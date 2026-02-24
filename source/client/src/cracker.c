#include "cracker.h"
#include "fsm.h"
#include "server_config.h"
#include <stdatomic.h>

char *index_to_password(uint64_t index)
{
    size_t i     = 0;
    size_t len   = 1;
    size_t range = CHARSET_SIZE;
    size_t total = range;

    while (index >= total)
    {
        len++;
        range *= CHARSET_SIZE;
        total += range;
    }

    char *buffer = malloc(len + 1);
    char *temp   = malloc(len + 1);
    if (!buffer || !temp)
        return NULL;

    size_t start_index = total - range;
    size_t local_index = index - start_index;

    for (i = 0; i < len; i++)
    {
        temp[len - i - 1] = charset[local_index % CHARSET_SIZE];
        local_index /= CHARSET_SIZE;
    }

    temp[len] = '\0';
    strcpy(buffer, temp);
    free(temp);
    return buffer;
}

void *worker(void *arg)
{
    struct worker_state *ws = (struct worker_state *)arg;

    struct crypt_data cdata;
    cdata.initialized = 0;

    while (!atomic_load(&found))
    {
        uint64_t idx = (uint64_t)atomic_fetch_add(&task_counter, 1);

        if (idx > ws->work_size - 1)
        {
            break;
        }

        if (idx % ws->checkpoint_interval == 0)
        {
            if (send_checkpoint(ws, ws->start_index + idx) == -1)
            {
                atomic_store(&found, true);
                break;
            }
        }

        char *pass = index_to_password(ws->start_index + idx);

        char *result = crypt_r(pass, ws->hash, &cdata);
        if (result != NULL)
        {
            if (strcmp(ws->hash, result) == 0)
            {
                if (!atomic_exchange(&found, true))
                {
                    pthread_mutex_lock(&found_mutex);
                    strncpy(found_candidate, pass, sizeof(found_candidate) - 1);
                    found_candidate[sizeof(found_candidate) - 1] = '\0';
                    pthread_mutex_unlock(&found_mutex);
                }
                printf("Password found!\nPassword is: %s\n", pass);
                send_found(ws->sockfd, pass);

                free(pass);
                break;
            }
        }
        free(pass);
    }

    return NULL;
}

int create_threads(size_t number_of_threads, struct worker_state *ws)
{
    pthread_t *threads = malloc(number_of_threads * sizeof(pthread_t));
    if (!threads)
        return -1;

    atomic_store(&task_counter, 0);
    atomic_store(&found, false);
    found_candidate[0] = '\0';

    for (size_t i = 0; i < number_of_threads; i++)
    {
        int rc = pthread_create(&threads[i], NULL, worker, (void *)ws);

        if (rc != 0)
        {
            fprintf(stderr, "pthread_create failed: %d\n", rc);
            for (size_t j = 0; j < i; ++j)
                pthread_join(threads[j], NULL);

            free(threads);
            return -1;
        }
    }

    for (size_t i = 0; i < number_of_threads; ++i)
        pthread_join(threads[i], NULL);

    bool got = atomic_load(&found);
    free(threads);
    return (got) ? 0 : -1;
}
