#include "keylog.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

void watch_path(const char *path)
{
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        perror("inotify_init");
        return;
    }

    int wd = inotify_add_watch(fd, path,
                               IN_MODIFY |
                                   IN_CREATE |
                                   IN_DELETE |
                                   IN_MOVED_FROM |
                                   IN_MOVED_TO);

    if (wd < 0)
    {
        perror("inotify_add_watch");
        close(fd);
        return;
    }

    printf("Watching: %s\n", path);

    char buffer[EVENT_BUF_LEN];

    while (1)
    {
        int length = read(fd, buffer, EVENT_BUF_LEN);

        if (length < 0)
        {
            usleep(100000);
            continue;
        }

        int i = 0;
        while (i < length)
        {
            struct inotify_event *event =
                (struct inotify_event *)&buffer[i];

            if (event->len)
            {
                if (event->mask & IN_CREATE)
                    printf("Created: %s\n", event->name);

                if (event->mask & IN_DELETE)
                    printf("Deleted: %s\n", event->name);

                if (event->mask & IN_MODIFY)
                    printf("Modified: %s\n", event->name);

                if (event->mask & IN_MOVED_FROM)
                    printf("Moved from: %s\n", event->name);

                if (event->mask & IN_MOVED_TO)
                    printf("Moved to: %s\n", event->name);
            }
            else
            {
                if (event->mask & IN_MODIFY)
                    printf("File modified\n");
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}
