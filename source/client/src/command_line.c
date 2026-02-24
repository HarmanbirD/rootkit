#include "command_line.h"
#include "utils.h"

int parse_arguments(int argc, char *argv[], arguments *args, struct fsm_error *err)
{
    // int opt;
    // int p_flag, s_flag, t_flag;
    //
    // opterr = 0;
    // p_flag = 0;
    // s_flag = 0;
    // t_flag = 0;
    //
    // static struct option long_opts[] = {
    //     {"port",    required_argument, 0, 'p'},
    //     {"server",  required_argument, 0, 's'},
    //     {"threads", required_argument, 0, 't'},
    //     {"help",    no_argument,       0, 'h'},
    //     {0,         0,                 0, 0  },
    // };
    //
    // while ((opt = getopt_long(argc, argv, "p:s:t:h", long_opts, NULL)) != -1)
    // {
    //     switch (opt)
    //     {
    //         case 'p':
    //         {
    //             if (p_flag)
    //             {
    //                 usage(argv[0]);
    //
    //                 SET_ERROR(err, "option '-p' can only be passed in once.");
    //
    //                 return -1;
    //             }
    //
    //             p_flag++;
    //             args->server_port_str = optarg;
    //             break;
    //         }
    //         case 's':
    //         {
    //             if (s_flag)
    //             {
    //                 usage(argv[0]);
    //
    //                 SET_ERROR(err, "option '-s' can only be passed in once.");
    //
    //                 return -1;
    //             }
    //
    //             s_flag++;
    //             args->server_addr = optarg;
    //             break;
    //         }
    //         case 't':
    //         {
    //             if (t_flag)
    //             {
    //                 usage(argv[0]);
    //
    //                 SET_ERROR(err, "option '-t' can only be passed in once.");
    //
    //                 return -1;
    //             }
    //
    //             t_flag++;
    //             args->threads_str = optarg;
    //
    //             break;
    //         }
    //         case 'h':
    //         {
    //             usage(argv[0]);
    //             SET_ERROR(err, "user called for help");
    //
    //             return -1;
    //         }
    //         case '?':
    //         {
    //             char message[24];
    //
    //             snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
    //             usage(argv[0]);
    //             SET_ERROR(err, message);
    //
    //             return -1;
    //         }
    //         default:
    //         {
    //             usage(argv[0]);
    //         }
    //     }
    // }
    //
    // if (optind < argc)
    // {
    //     usage(argv[0]);
    //
    //     SET_ERROR(err, "Too many options.");
    //
    //     return -1;
    // }

    return 0;
}

void usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s [OPTIONS]\n\n"
            "Required options:\n"
            "  -s, --server <addr>       Server IP address or hostname (required)\n"
            "  -p, --port <num>          Server listen port (required)\n"
            "Optional options:\n"
            "  -t, --threads <num>       Number of threads the worker will use\n"
            "                             (default: 4)\n"
            "  -h, --help                Display this help message and exit\n\n"
            "Examples:\n"
            "  %s --server 192.168.1.10 --port 5000\n"
            "  %s -s example.com -p 5000 -t 8\n\n",
            program_name, program_name, program_name);

    fputs("Notes:\n", stderr);
    fputs("  • Long and short forms may be used interchangeably (e.g. --port or -p).\n", stderr);
    fputs("  • If threads is omitted it defaults to 4.\n", stderr);
    fputs("  • The program will validate numeric ranges (e.g. port must fit in uint16).\n", stderr);
}

int handle_arguments(const char *binary_name, arguments *args, struct fsm_error *err)
{
    // if (args->server_addr == NULL)
    // {
    //     SET_ERROR(err, "The server IP address is required.");
    //     usage(binary_name);
    //
    //     return -1;
    // }
    //
    // if (args->server_port_str == NULL)
    // {
    //     SET_ERROR(err, "The server port is required.");
    //     usage(binary_name);
    //
    //     return -1;
    // }
    //
    // if (parse_in_port_t(binary_name, args->server_port_str, &args->server_port, err) == -1)
    // {
    //     printf("for port: %s\n", args->server_port_str);
    //     return -1;
    // }
    //
    // if (args->threads_str == NULL)
    // {
    //     args->threads = 4;
    // }
    // else
    // {
    //     if (string_to_int(args->threads_str, &args->threads, err) != 0)
    //         return -1;
    // }

    return 0;
}

int parse_in_port_t(const char *binary_name, const char *str, in_port_t *port, struct fsm_error *err)
{
    char     *endptr;
    uintmax_t parsed_value;

    errno        = 0;
    parsed_value = strtoumax(str, &endptr, 10);

    if (errno != 0)
    {
        SET_ERROR(err, strerror(errno));

        return -1;
    }

    // Check if there are any non-numeric characters in the input string
    if (*endptr != '\0')
    {
        SET_ERROR(err, "Invalid characters in input.");

        usage(binary_name);

        return -1;
    }

    // Check if the parsed value is within the valid range for in_port_t
    if (parsed_value > UINT16_MAX)
    {
        SET_ERROR(err, "in_port_t value out of range.");

        usage(binary_name);

        return -1;
    }

    *port = (in_port_t)parsed_value;
    return 0;
}
