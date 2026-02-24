#ifndef MENU_H
#define MENU_H

#include "fsm.h"

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
