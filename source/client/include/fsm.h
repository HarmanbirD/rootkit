#ifndef CLIENT_FSM_H
#define CLIENT_FSM_H

#include <glob.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

typedef enum
{
    FSM_IGNORE = -1,
    FSM_INIT,
    FSM_EXIT,
    FSM_USER_START
} fsm_state;

typedef struct ip_info
{
    uint32_t local_ip, dest_ip;
    int      src_port, dest_port;
} ip_info;

typedef struct arguments
{
    struct ip_info ip_info;
    int            connected;
} arguments;

typedef struct fsm_context
{
    int               argc;
    char            **argv;
    struct arguments *args;
} fsm_context;

typedef struct fsm_error
{
    char       *err_msg;
    const char *function_name;
    const char *file_name;
    int         error_line;
} fsm_error;

typedef int (*fsm_state_func)(struct fsm_context *context,
                              struct fsm_error   *err);

struct fsm_transition
{
    int            from_id;
    int            to_id;
    fsm_state_func perform;
};

static inline void fsm_error_init(struct fsm_error *e)
{
    if (!e)
        return;
    e->err_msg       = NULL;
    e->error_line    = 0;
    e->function_name = NULL;
    e->file_name     = NULL;
}

static inline void fsm_error_clear(struct fsm_error *e)
{
    if (!e)
        return;
    free(e->err_msg);
    e->err_msg       = NULL;
    e->error_line    = 0;
    e->function_name = NULL;
    e->file_name     = NULL;
}

static inline char *fsm_strdup_or_null(const char *s)
{
    if (!s)
        return NULL;
    char *d = strdup(s);
    return d;
}

int fsm_run(struct fsm_context *context, struct fsm_error *err,
            const struct fsm_transition transitions[]);

#define SET_ERROR(err, msg)                                 \
    do                                                      \
    {                                                       \
        if (err)                                            \
        {                                                   \
            free((err)->err_msg);                           \
            (err)->err_msg     = fsm_strdup_or_null((msg)); \
            err->error_line    = __LINE__;                  \
            err->function_name = __func__;                  \
            err->file_name     = __FILENAME__;              \
        }                                                   \
    } while (0)

#define SET_TRACE(ctx, msg, curr_state)                     \
    do                                                      \
    {                                                       \
        printf("TRACE: %s \nEntered state at line %d.\n\n", \
               curr_state, __LINE__);                       \
        fflush(stdout);                                     \
    } while (0)

#endif // CLIENT_FSM_H
