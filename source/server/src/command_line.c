#include "command_line.h"
#include "udp.h"
#include "utils.h"
#include <arpa/inet.h>

int parse_arguments(int argc, char *argv[], arguments *args, struct fsm_error *err)
{
    //     int opt;
    //     int H_flag, c_flag, p_flag, s_flag, w_flag, t_flag;
    //
    //     opterr = 0;
    //     H_flag = 0;
    //     c_flag = 0;
    //     p_flag = 0;
    //     s_flag = 0;
    //     w_flag = 0;
    //     t_flag = 0;
    //
    //     static struct option long_opts[] = {
    //         {"hash",       required_argument, 0, 'H'},
    //         {"checkpoint", required_argument, 0, 'c'},
    //         {"port",       required_argument, 0, 'p'},
    //         {"server",     required_argument, 0, 's'},
    //         {"work-size",  required_argument, 0, 'w'},
    //         {"timeout",    required_argument, 0, 't'},
    //         {"help",       no_argument,       0, 'h'},
    //         {0,            0,                 0, 0  },
    //     };
    //
    //     while ((opt = getopt_long(argc, argv, "H:c:p:s:w:t:h", long_opts, NULL)) != -1)
    //     {
    //         switch (opt)
    //         {
    //             case 'H':
    //             {
    //                 if (H_flag)
    //                 {
    //                     usage(argv[0]);
    //
    //                     SET_ERROR(err, "option '-H' can only be passed in once.");
    //
    //                     return -1;
    //                 }
    //
    //                 H_flag++;
    //                 args->crack_ctx.hash = optarg;
    //                 break;
    //             }
    //             case 'c':
    //             {
    //                 if (c_flag)
    //                 {
    //                     usage(argv[0]);
    //
    //                     SET_ERROR(err, "option '-c' can only be passed in once.");
    //
    //                     return -1;
    //                 }
    //
    //                 c_flag++;
    //                 args->checkpoint_str = optarg;
    //                 break;
    //             }
    //             case 'p':
    //             {
    //                 if (p_flag)
    //                 {
    //                     usage(argv[0]);
    //
    //                     SET_ERROR(err, "option '-p' can only be passed in once.");
    //
    //                     return -1;
    //                 }
    //
    //                 p_flag++;
    //                 args->server_port_str = optarg;
    //                 break;
    //             }
    //             case 's':
    //             {
    //                 if (s_flag)
    //                 {
    //                     usage(argv[0]);
    //
    //                     SET_ERROR(err, "option '-s' can only be passed in once.");
    //
    //                     return -1;
    //                 }
    //
    //                 s_flag++;
    //                 args->server_addr = optarg;
    //                 break;
    //             }
    //             case 't':
    //             {
    //                 if (t_flag)
    //                 {
    //                     usage(argv[0]);
    //
    //                     SET_ERROR(err, "option '-t' can only be passed in once.");
    //
    //                     return -1;
    //                 }
    //
    //                 t_flag++;
    //                 args->timeout_str = optarg;
    //                 break;
    //             }
    //             case 'w':
    //             {
    //                 if (w_flag)
    //                 {
    //                     usage(argv[0]);
    //
    //                     SET_ERROR(err, "option '-w' can only be passed in once.");
    //
    //                     return -1;
    //                 }
    //
    //                 w_flag++;
    //                 args->work_size_str = optarg;
    //                 break;
    //             }
    //             case 'h':
    //             {
    //                 usage(argv[0]);
    //
    //                 SET_ERROR(err, "user called for help");
    //
    //                 return -1;
    //             }
    //             case '?':
    //             {
    //                 char message[24];
    //
    //                 snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
    //                 usage(argv[0]);
    //                 SET_ERROR(err, message);
    //
    //                 return -1;
    //             }
    //             default:
    //             {
    //                 usage(argv[0]);
    //             }
    //         }
    //     }
    //
    //     if (optind < argc)
    //     {
    //         usage(argv[0]);
    //
    //         SET_ERROR(err, "Too many options.");
    //
    //         return -1;
    //     }
    //
    return 0;
}
//
// void usage(const char *program_name)
// {
//     fprintf(stderr,
//             "Usage: %s [OPTIONS]\n\n"
//             "Required options:\n"
//             "  -s, --server <addr>       Server IP address or hostname (required)\n"
//             "  -p, --port <num>          Server listen port (required)\n"
//             "  -H, --hash <hash>         Hashed password to crack (required)\n\n"
//             "Optional options:\n"
//             "  -w, --work-size <num>     Number of passwords assigned per node request\n"
//             "                             (default: 1000)\n"
//             "  -c, --checkpoint <num>    Number of attempts before a node sends a checkpoint\n"
//             "                             (default: work-size / 4)\n"
//             "  -t, --timeout <num>       Seconds to wait for a checkpoint from a client\n"
//             "                             (default: 600)\n"
//             "  -h, --help                Display this help message and exit\n\n"
//             "Examples:\n"
//             "  %s --server 192.168.1.10 --port 5000 --hash $6$... --work-size 1000\n"
//             "  %s -s example.com -p 5000 -H <hash> -c 500 -t 300\n\n",
//             program_name, program_name, program_name);
//
//     fputs("Notes:\n", stderr);
//     fputs("  • Long and short forms may be used interchangeably (e.g. --port or -p).\n", stderr);
//     fputs("  • If work-size is omitted it defaults to 1000.\n", stderr);
//     fputs("  • If checkpoint is omitted it defaults to work-size / 4.\n", stderr);
//     fputs("  • The program will validate numeric ranges (e.g. port must fit in uint16).\n", stderr);
// }
//
int handle_arguments(const char *binary_name, arguments *args, struct fsm_error *err)
{
    //     if (args->server_addr == NULL)
    //     {
    //         SET_ERROR(err, "The server IP address is required.");
    //         usage(binary_name);
    //
    //         return -1;
    //     }
    //
    //     if (args->server_port_str == NULL)
    //     {
    //         SET_ERROR(err, "The server port is required.");
    //         usage(binary_name);
    //
    //         return -1;
    //     }
    //
    //     if (args->crack_ctx.hash == NULL)
    //     {
    //         SET_ERROR(err, "The Hash is required!");
    //         usage(binary_name);
    //
    //         return -1;
    //     }
    //
    //     if (parse_in_port_t(binary_name, args->server_port_str, &args->server_port, err) == -1)
    //     {
    //         printf("for port: %s\n", args->server_port_str);
    //         return -1;
    //     }
    //
    //     if (args->work_size_str == NULL)
    //         args->crack_ctx.work_size = 1000;
    //     else
    //     {
    //         if (string_to_uint64(args->work_size_str, &args->crack_ctx.work_size, err) != 0)
    //             return -1;
    //     }
    //
    //     if (args->checkpoint_str == NULL)
    //         args->crack_ctx.checkpoint = args->crack_ctx.work_size / 4;
    //     else
    //     {
    //         if (string_to_uint64(args->checkpoint_str, &args->crack_ctx.checkpoint, err) != 0)
    //             return -1;
    //     }
    //
    //     if (args->crack_ctx.checkpoint > args->crack_ctx.work_size)
    //     {
    //         SET_ERROR(err, "Checkpoint must be less than work size!");
    //         usage(binary_name);
    //
    //         return -1;
    //     }
    //
    //     if (args->timeout_str == NULL)
    //         args->crack_ctx.timeout = 600;
    //     else
    //     {
    //         if (string_to_uint64(args->timeout_str, &args->crack_ctx.timeout, err) != 0)
    //             return -1;
    //     }
    //
    uint32_t dest_ip;
    inet_pton(AF_INET, "8.8.8.8", &dest_ip);

    args->ip_info.local_ip  = get_local_ip(dest_ip);
    args->ip_info.src_port  = 3500;
    args->ip_info.dest_port = 3600;

    return 0;
}

int parse_in_port_t(const char *binary_name, const char *str, in_port_t *port, struct fsm_error *err)
{
    //     char     *endptr;
    //     uintmax_t parsed_value;
    //
    //     errno        = 0;
    //     parsed_value = strtoumax(str, &endptr, 10);
    //
    //     if (errno != 0)
    //     {
    //         SET_ERROR(err, strerror(errno));
    //
    //         return -1;
    //     }
    //
    //     // Check if there are any non-numeric characters in the input string
    //     if (*endptr != '\0')
    //     {
    //         SET_ERROR(err, "Invalid characters in input.");
    //         usage(binary_name);
    //
    //         return -1;
    //     }
    //
    //     // Check if the parsed value is within the valid range for in_port_t
    //     if (parsed_value > UINT16_MAX)
    //     {
    //         SET_ERROR(err, "in_port_t value out of range.");
    //         usage(binary_name);
    //
    //         return -1;
    //     }
    //
    //     *port = (in_port_t)parsed_value;
    return 0;
}
