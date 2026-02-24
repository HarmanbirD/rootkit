#include "knocker.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

int listen_sequence(char *final_timestamp, size_t ts_len, char *final_ip)
{
    if (!final_timestamp || ts_len == 0)
        return 0;

    int ports[3] = {3000, 4000, 5000};
    int socks[3];

    for (int i = 0; i < 3; i++)
    {
        socks[i] = socket(AF_INET, SOCK_DGRAM, 0);
        if (socks[i] < 0)
            return 0;

        int yes = 1;
        setsockopt(socks[i], SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        struct sockaddr_in addr = {0};
        addr.sin_family         = AF_INET;
        addr.sin_port           = htons(ports[i]);
        addr.sin_addr.s_addr    = htonl(INADDR_ANY);

        if (bind(socks[i], (struct sockaddr *)&addr, sizeof(addr)) < 0)
            return 0;
    }

    printf("Listening for knock sequence 3000→4000→5000\n");

    int    state                        = 0;
    time_t start_time                   = 0;
    char   expected_ip[INET_ADDRSTRLEN] = {0};

    unsigned char buf[2048];

    while (1)
    {
        fd_set rfds;
        FD_ZERO(&rfds);

        int maxfd = 0;
        for (int i = 0; i < 3; i++)
        {
            FD_SET(socks[i], &rfds);
            if (socks[i] > maxfd)
                maxfd = socks[i];
        }

        struct timeval tv = {1, 0};
        int            r  = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (r < 0)
            continue;

        if (state != 0)
        {
            time_t now = time(NULL);
            if (difftime(now, start_time) > 30)
            {
                printf("30s timeout — resetting\n");
                state          = 0;
                expected_ip[0] = '\0';
                continue;
            }
        }

        for (int i = 0; i < 3; i++)
        {
            if (!FD_ISSET(socks[i], &rfds))
                continue;

            struct sockaddr_in src;
            socklen_t          srclen = sizeof(src);
            ssize_t            n      = recvfrom(socks[i], buf, sizeof(buf), 0,
                                                 (struct sockaddr *)&src, &srclen);
            if (n <= 0)
                continue;

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));

            int port_index = i;

            if (state == 0 && port_index == 0)
            {
                strcpy(expected_ip, ip);
                start_time = time(NULL);
                state      = 1;
                printf("Step 1 OK from %s\n", ip);
            }
            else if (state == 1 && port_index == 1)
            {
                if (strcmp(ip, expected_ip) == 0)
                {
                    state = 2;
                    printf("Step 2 OK\n");
                }
                else
                {
                    printf("Wrong IP at step 2 — reset\n");
                    state = 0;
                }
            }
            else if (state == 2 && port_index == 2)
            {
                if (strcmp(ip, expected_ip) == 0)
                {
                    time_t    t = time(NULL);
                    struct tm tm;
                    localtime_r(&t, &tm);
                    strftime(final_timestamp, ts_len,
                             "%Y-%m-%d %H:%M:%S", &tm);

                    printf("Sequence complete from %s\n", ip);

                    for (int j = 0; j < 3; j++)
                        close(socks[j]);

                    strcpy(final_ip, ip);

                    return 1;
                }
                else
                {
                    printf("Wrong IP at step 3 — reset\n");
                    state = 0;
                }
            }
            else
            {
                state = 0;
            }
        }
    }
}

// int listen_port(int port, struct sockaddr_in *src_info, char *timestamp)
// {
//     if (!src_info || !timestamp)
//         return -1;
//
//     const char *host      = "0.0.0.0";
//     int         max_bytes = 2048;
//     int         poll_ms   = 200;
//
//     int fd = socket(AF_INET, SOCK_DGRAM, 0);
//     if (fd < 0)
//     {
//         perror("socket");
//         return 2;
//     }
//     int yes = 1;
//     if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) <
//         0)
//     {
//         perror("setsockopt(SO_REUSEADDR)");
//         close(fd);
//         return 2;
//     }
//     struct sockaddr_in addr;
//     memset(&addr, 0, sizeof(addr));
//     addr.sin_family = AF_INET;
//     addr.sin_port   = htons((uint16_t)port);
//     if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
//     {
//         fprintf(stderr, "invalid --host address: %s\n", host);
//         close(fd);
//         return 2;
//     }
//     if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
//     {
//         perror("bind");
//         close(fd);
//         return 2;
//     }
//     printf("Listening for UDP on %s:%d\n", host, port);
//
//     unsigned char *buf = (unsigned char *)malloc((size_t)max_bytes);
//     if (!buf)
//     {
//         fprintf(stderr, "malloc failed\n");
//         close(fd);
//         return 2;
//     }
//
//     while (1)
//     {
//         fd_set rfds;
//         FD_ZERO(&rfds);
//         FD_SET(fd, &rfds);
//         struct timeval tv;
//         tv.tv_sec  = poll_ms / 1000;
//         tv.tv_usec = (poll_ms % 1000) * 1000;
//         int r      = select(fd + 1, &rfds, NULL, NULL, &tv);
//
//         if (r < 0)
//         {
//             if (errno == EINTR)
//                 continue;
//             perror("select");
//             break;
//         }
//
//         if (r == 0)
//         {
//             // timeout: wake up, check g_stop, continue
//             continue;
//         }
//
//         if (FD_ISSET(fd, &rfds))
//         {
//             struct sockaddr_in src;
//             socklen_t          srclen = sizeof(src);
//             ssize_t            n      = recvfrom(fd, buf, (size_t)max_bytes, 0,
//                                                  (struct sockaddr *)&src, &srclen);
//             if (n < 0)
//             {
//                 if (errno == EINTR)
//                     continue;
//                 perror("recvfrom");
//                 break;
//             }
//
//             char      ts[32];
//             time_t    t = time(NULL);
//             struct tm tm;
//             localtime_r(&t, &tm);
//             strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
//             char src_ip[INET_ADDRSTRLEN];
//             inet_ntop(AF_INET, &src.sin_addr, src_ip,
//                       sizeof(src_ip));
//             printf("%s knock dst_port=%d src=%s:%u bytes=%zd\n",
//                    ts,
//                    port,
//                    src_ip,
//                    (unsigned)ntohs(src.sin_port),
//                    n);
//             fflush(stdout);
//
//             *src_info = src;
//             strcpy(timestamp, ts);
//             printf("Stopping.\n");
//             free(buf);
//             close(fd);
//
//             return 1;
//         }
//     }
//
//     printf("Stopping.\n");
//     free(buf);
//     close(fd);
//     return 0;
// }
