#ifndef KEYLOG_H
#define KEYLOG_H

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + NAME_MAX + 1))

void watch_path(const char *path);

#endif // KEYLOG_H
