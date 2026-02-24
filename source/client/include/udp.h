#ifndef UDP_H
#define UDP_H

#include "fsm.h"
#include <stdint.h>

uint32_t get_local_ip(uint32_t dest_ip);

int send_message(struct ip_info ip_ctx, uint16_t sec_payload);
int recv_message(struct ip_info ip_ctx, uint16_t *sec_payload);

int send_file(struct ip_info ip_ctx, const char *path);
int send_string(struct ip_info ip_ctx, const char *s);

int receive_string(struct ip_info ip_ctx, char **out_str);
int receive_file(struct ip_info ip_ctx);

#endif // UDP_H
