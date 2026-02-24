#include "fsm.h"
#include <crypt.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#%^&()_+-=.,:;?";
#define CHARSET_SIZE (strlen(charset))

static atomic_uint_fast64_t task_counter;
static atomic_bool          found;
static char                 found_candidate[64];
static pthread_mutex_t      found_mutex = PTHREAD_MUTEX_INITIALIZER;

char *index_to_password(uint64_t index);
void *worker(void *arg);
int   create_threads(size_t number_of_threads, struct worker_state *ws);
