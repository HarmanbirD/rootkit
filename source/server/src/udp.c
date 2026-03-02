#include "udp.h"
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int    sum = 0;
    for (; len > 1; len -= 2)
    {
        sum += *buf++;
    }
    if (len == 1)
    {
        sum += *(unsigned char *)buf;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

uint32_t get_local_ip(uint32_t dest_ip)
{
    uint32_t result = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 0;
    }

    struct sockaddr_in dest = {0};
    dest.sin_family         = AF_INET;
    dest.sin_port           = htons(53);
    dest.sin_addr.s_addr    = dest_ip;

    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) < 0)
    {
        perror("connect");
        close(sock);
        return 0;
    }

    struct sockaddr_in local = {0};
    socklen_t          len   = sizeof(local);

    if (getsockname(sock, (struct sockaddr *)&local, &len) < 0)
    {
        perror("getsockname");
        close(sock);
        return 0;
    }

    result = local.sin_addr.s_addr;

    close(sock);
    return result;
}

int send_message(struct ip_info ip_ctx, uint16_t sec_payload)
{
    char packet[4096];
    int  sock;
    memset(packet, 0, sizeof(packet));
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }
    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
    {
        perror("setsockopt");
        return 1;
    }
    struct iphdr  *ip          = (struct iphdr *)packet;
    struct udphdr *udp         = (struct udphdr *)(packet + sizeof(struct iphdr));
    char          *payload     = packet + sizeof(struct iphdr) + sizeof(struct udphdr);
    int            payload_len = 2;
    strcpy(payload, "hi");
    ip->ihl     = 5;
    ip->version = 4;
    ip->tos     = 0;
    ip->tot_len =
        htons(sizeof(struct iphdr) + sizeof(struct udphdr) + payload_len);
    ip->id       = htons(sec_payload);
    ip->frag_off = 0;
    ip->ttl      = 64;
    ip->protocol = IPPROTO_UDP;
    ip->saddr    = ip_ctx.local_ip;
    ip->daddr    = ip_ctx.dest_ip;
    ip->check    = checksum(ip, sizeof(struct iphdr));
    udp->source  = htons(ip_ctx.src_port);
    udp->dest    = htons(ip_ctx.dest_port);
    udp->len     = htons(sizeof(struct udphdr) + payload_len);
    udp->check   = 0;
    struct sockaddr_in dest;
    dest.sin_family      = AF_INET;
    dest.sin_port        = udp->dest;
    dest.sin_addr.s_addr = ip->daddr;
    sendto(sock, packet,
           sizeof(struct iphdr) + sizeof(struct udphdr) + payload_len, 0,
           (struct sockaddr *)&dest, sizeof(dest));

    char local_ip_str[INET_ADDRSTRLEN];
    char dest_ip_str[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &ip_ctx.local_ip, local_ip_str, sizeof(local_ip_str));
    inet_ntop(AF_INET, &ip_ctx.dest_ip, dest_ip_str, sizeof(dest_ip_str));

    printf("ip_info {\n");
    printf("  local_ip  : %s\n", local_ip_str);
    printf("  dest_ip   : %s\n", dest_ip_str);
    printf("  src_port  : %d\n", ip_ctx.src_port);
    printf("  dest_port : %d\n", ip_ctx.dest_port);
    printf("  payload   : %d\n", sec_payload);
    printf("}\n");
    usleep(150000);
    return 0;
}

void print_udp_packet(unsigned char *buffer, ssize_t len)
{
    struct iphdr *ip            = (struct iphdr *)buffer;
    int           ip_header_len = ip->ihl * 4;

    if (len < ip_header_len + sizeof(struct udphdr))
        return;

    struct udphdr *udp = (struct udphdr *)(buffer + ip_header_len);

    struct in_addr src, dst;
    src.s_addr = ip->saddr;
    dst.s_addr = ip->daddr;

    printf("\n=== IP HEADER ===\n");
    printf("Version        : %d\n", ip->version);
    printf("Header Length  : %d bytes\n", ip_header_len);
    printf("Type of Service: %d\n", ip->tos);
    printf("Total Length   : %d\n", ntohs(ip->tot_len));
    printf("Identification  : %d\n", ntohs(ip->id));
    printf("TTL            : %d\n", ip->ttl);
    printf("Protocol       : %d\n", ip->protocol);
    printf("Checksum       : %d\n", ntohs(ip->check));
    printf("Source IP      : %s\n", inet_ntoa(src));
    printf("Destination IP : %s\n", inet_ntoa(dst));

    printf("\n=== UDP HEADER ===\n");
    printf("Source Port    : %d\n", ntohs(udp->source));
    printf("Destination Port: %d\n", ntohs(udp->dest));
    printf("Length         : %d\n", ntohs(udp->len));
    printf("Checksum       : %d\n", ntohs(udp->check));

    // Payload
    int            udp_header_len = sizeof(struct udphdr);
    int            payload_len    = len - ip_header_len - udp_header_len;
    unsigned char *payload        = buffer + ip_header_len + udp_header_len;

    printf("\n=== PAYLOAD (%d bytes) ===\n", payload_len);
    for (int i = 0; i < payload_len; i++)
    {
        printf("%02x ", payload[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n===========================\n");
}

int recv_message(struct ip_info ip_ctx, uint16_t *sec_payload)
{
    printf("in receive message\n");
    if (!sec_payload)
    {
        printf("no sec payload\n");
        return 0;
    }

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock < 0)
    {
        perror("socket");
        return 0;
    }

    unsigned char buffer[65536];

    while (1)
    {
        ssize_t len = recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);
        if (len <= 0)
            continue;

        if ((size_t)len < sizeof(struct iphdr))
            continue;

        struct iphdr *ip = (struct iphdr *)buffer;

        size_t ip_header_len = ip->ihl * 4;
        if (ip_header_len < sizeof(struct iphdr) ||
            (size_t)len < ip_header_len + sizeof(struct udphdr))
            continue;

        if (ip->protocol != IPPROTO_UDP)
            continue;

        if (ip_ctx.local_ip != 0 && ip->daddr != ip_ctx.local_ip)
            continue;

        struct udphdr *udp = (struct udphdr *)(buffer + (ip->ihl * 4));

        if (ip_ctx.src_port != 0 && ntohs(udp->dest) != ip_ctx.src_port)
            continue;

        *sec_payload = ntohs(ip->id);

        printf("Recieved: %d\n", ntohs(ip->id));

        print_udp_packet(buffer, len);

        close(sock);
        return 1;
    }
}

static void send_bytes_exact_be(struct ip_info ip_ctx, const uint8_t *data,
                                size_t nbytes)
{
    size_t i = 0;

    while (i + 1 < nbytes)
    {
        uint16_t w = ((uint16_t)data[i] << 8) | (uint16_t)data[i + 1];
        send_message(ip_ctx, w);
        i += 2;
    }

    if (i < nbytes)
    {
        uint16_t w = ((uint16_t)data[i] << 8);
        send_message(ip_ctx, w);
    }
}

static const char *path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? (slash + 1) : path;
}

static void send_u64_as_u16_be(struct ip_info ip_ctx, uint64_t v)
{
    send_message(ip_ctx, (uint16_t)((v >> 48) & 0xFFFFu));
    send_message(ip_ctx, (uint16_t)((v >> 32) & 0xFFFFu));
    send_message(ip_ctx, (uint16_t)((v >> 16) & 0xFFFFu));
    send_message(ip_ctx, (uint16_t)((v >> 0) & 0xFFFFu));
}

int send_file(struct ip_info ip_ctx, const char *path)
{
    if (!path)
        return -1;

    struct stat st;
    if (stat(path, &st) != 0)
        return -1;

    if (!S_ISREG(st.st_mode))
        return -1;

    uint64_t file_len = (uint64_t)st.st_size;

    const char *name     = path_basename(path);
    size_t      name_len = strlen(name);
    if (name_len > 0xFFFFu)
        return -1;

    send_message(ip_ctx, (uint16_t)name_len);
    send_u64_as_u16_be(ip_ctx, file_len);

    send_bytes_exact_be(ip_ctx, (const uint8_t *)name, name_len);

    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    uint8_t buf[4096];
    size_t  n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        send_bytes_exact_be(ip_ctx, buf, n);
    }

    if (ferror(f))
    {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

int send_string(struct ip_info ip_ctx, const char *s)
{
    if (!s)
        return -1;

    uint64_t len = (uint64_t)strlen(s);

    printf("sending len: %zd\n", len);
    send_u64_as_u16_be(ip_ctx, len);
    send_bytes_exact_be(ip_ctx, (const uint8_t *)s, (size_t)len);

    return 0;
}

static int recv_u16(struct ip_info ip_ctx, uint16_t *out)
{
    return recv_message(ip_ctx, out);
}

static int recv_u64_from_u16_be(struct ip_info ip_ctx, uint64_t *out)
{
    printf("recv_u64_from_u16_be\n");
    uint16_t w0, w1, w2, w3;

    if (recv_message(ip_ctx, &w0) != 1)
    {
        printf("err 0: %d\n", w0);
        return -1;
    }
    printf("0: %d\n", w0);
    if (recv_message(ip_ctx, &w1) != 1)
    {
        printf("err 1: %d\n", w1);
        return -1;
    }
    printf("1: %d\n", w1);
    if (recv_message(ip_ctx, &w2) != 1)
    {
        printf("err 2: %d\n", w2);
        return -1;
    }
    printf("2: %d\n", w2);
    if (recv_message(ip_ctx, &w3) != 1)
    {
        printf("err 3: %d\n", w3);
        return -1;
    }
    printf("3: %d\n", w3);

    *out = ((uint64_t)w0 << 48) | ((uint64_t)w1 << 32) | ((uint64_t)w2 << 16) |
           ((uint64_t)w3 << 0);

    printf("NUMBERS: %d %d %d %d\n", w0, w1, w2, w3);
    *out = (uint64_t)w3;

    return 0;
}

static int recv_bytes_to_buffer_be(struct ip_info ip_ctx, uint8_t *out,
                                   size_t nbytes)
{
    size_t remaining = nbytes;

    while (remaining > 0)
    {
        uint16_t w;
        if (recv_message(ip_ctx, &w) != 1)
            return -1;

        uint8_t b0 = (uint8_t)((w >> 8) & 0xFF);
        uint8_t b1 = (uint8_t)(w & 0xFF);

        if (remaining >= 1)
        {
            *out++ = b0;
            remaining--;
        }
        if (remaining >= 1)
        {
            *out++ = b1;
            remaining--;
        }
    }

    return 0;
}

static int recv_bytes_to_file_be(struct ip_info ip_ctx, FILE *f,
                                 uint64_t nbytes)
{
    uint64_t remaining = nbytes;

    while (remaining > 0)
    {
        uint16_t w;
        if (recv_message(ip_ctx, &w) != 1)
            return -1;

        uint8_t b0 = (uint8_t)((w >> 8) & 0xFF);
        uint8_t b1 = (uint8_t)(w & 0xFF);

        if (remaining >= 1)
        {
            if (fputc((int)b0, f) == EOF)
                return -1;
            remaining--;
        }
        if (remaining >= 1)
        {
            if (fputc((int)b1, f) == EOF)
                return -1;
            remaining--;
        }
    }

    return 0;
}

int receive_string(struct ip_info ip_ctx, char **out_str)
{
    if (!out_str)
        return -1;
    char local_ip_str[INET_ADDRSTRLEN];
    char dest_ip_str[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &ip_ctx.local_ip, local_ip_str, sizeof(local_ip_str));
    inet_ntop(AF_INET, &ip_ctx.dest_ip, dest_ip_str, sizeof(dest_ip_str));

    printf("ip_info {\n");
    printf("  local_ip  : %s\n", local_ip_str);
    printf("  dest_ip   : %s\n", dest_ip_str);
    printf("  src_port  : %d\n", ip_ctx.src_port);
    printf("  dest_port : %d\n", ip_ctx.dest_port);
    printf("}\n");

    *out_str = NULL;

    printf("Waiting for len\n");
    uint64_t len;
    if (recv_u64_from_u16_be(ip_ctx, &len) != 0)
        return -1;

    printf("recieved len: %zd\n", len);
    if (len > (1024ull * 1024ull * 64ull))
    {
        printf("Len is wrong!!!");
        return -1;
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf)
    {
        printf("buff wrong");
        return -1;
    }

    if (recv_bytes_to_buffer_be(ip_ctx, (uint8_t *)buf, (size_t)len) != 0)
    {
        printf("recvs error");
        free(buf);
        return -1;
    }

    buf[len] = '\0';
    *out_str = buf;

    printf("Recieved frm akad: %s\n", *out_str);
    return 0;
}

int receive_file(struct ip_info ip_ctx)
{
    uint16_t name_len_u16;
    uint64_t file_len;

    if (recv_u16(ip_ctx, &name_len_u16) != 1)
        return -1;
    // printf("name len: %d\n", name_len_u16);

    if (recv_u64_from_u16_be(ip_ctx, &file_len) != 0)
        return -1;

    // printf("file len: %zd\n", file_len);

    size_t name_len = (size_t)name_len_u16;

    char *filename = (char *)malloc(name_len + 1);
    if (!filename)
        return -1;

    if (recv_bytes_to_buffer_be(ip_ctx, (uint8_t *)filename, name_len) != 0)
    {
        free(filename);
        return -1;
    }
    filename[name_len] = '\0';

    // printf("filename: %s\n", filename);

    size_t path_len = strlen("./Downloaded/") + strlen(filename) + 1;
    char  *fullpath = malloc(path_len);
    if (!fullpath)
    {
        free(filename);
        return -1;
    }

    snprintf(fullpath, path_len, "./Downloaded/%s", filename);

    FILE *f = fopen(fullpath, "wb");
    if (!f)
    {
        free(filename);
        free(fullpath);
        return -1;
    }

    int rc = recv_bytes_to_file_be(ip_ctx, f, file_len);

    fclose(f);

    printf("Received File %d: %s\n", rc, filename);

    free(filename);
    free(fullpath);

    return (rc == 0) ? 0 : -1;
}
