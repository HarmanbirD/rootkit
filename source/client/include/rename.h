#ifndef RENAME_H
#define RENAME_H

#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct proc_record
{
    pid_t pid;
    char *comm;    // /proc/<pid>/comm (short name)
    char *argv0;   // first element of cmdline
    char *cmdline; // cmdline joined with spaces
    char *exe;     // readlink(/proc/<pid>/exe)
} proc_record_t;

typedef struct proc_records
{
    proc_record_t *items;
    size_t         len;
    size_t         cap;
} proc_records_t;

typedef struct comm_ref
{
    const char *comm;
    int         pid; // optional, just to show an example PID
} comm_ref_t;

int rename_process(void);

#endif // RENAME_H
