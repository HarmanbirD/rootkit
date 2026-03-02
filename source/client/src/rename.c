#include "rename.h"

#include <sys/prctl.h>

static int is_all_digits(const char *s)
{
    if (!s || !*s)
        return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (!isdigit(*p))
            return 0;
    return 1;
}

static char *xstrdup_or_null(const char *s)
{
    if (!s)
        return NULL;
    char *d = strdup(s);
    return d;
}

static void rstrip_newline(char *s)
{
    if (!s)
        return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
    {
        s[n - 1] = '\0';
        n--;
    }
}

static char *try_read_text(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    char   *buf   = NULL;
    size_t  cap   = 0;
    ssize_t nread = getdelim(&buf, &cap, '\0', f);
    fclose(f);

    if (nread < 0)
    {
        free(buf);
        return NULL;
    }
    return buf;
}

static unsigned char *try_read_bytes(const char *path, size_t *out_len)
{
    if (out_len)
        *out_len = 0;

    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return NULL;
    }
    rewind(f);

    unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (got != (size_t)sz && ferror(f))
    {
        free(buf);
        return NULL;
    }

    buf[got] = '\0';
    if (out_len)
        *out_len = got;
    return buf;
}

static char *try_readlink_path(const char *path)
{
    char    tmp[PATH_MAX];
    ssize_t n = readlink(path, tmp, sizeof(tmp) - 1);
    if (n < 0)
        return NULL;
    tmp[n] = '\0';
    return strdup(tmp);
}

static int records_push(proc_records_t *rs, proc_record_t rec)
{
    if (rs->len == rs->cap)
    {
        size_t         new_cap = rs->cap ? rs->cap * 2 : 128;
        proc_record_t *p       = realloc(rs->items, new_cap * sizeof(*p));
        if (!p)
            return -1;
        rs->items = p;
        rs->cap   = new_cap;
    }
    rs->items[rs->len++] = rec;
    return 0;
}

static void record_free(proc_record_t *r)
{
    if (!r)
        return;
    free(r->comm);
    free(r->argv0);
    free(r->cmdline);
    free(r->exe);
    memset(r, 0, sizeof(*r));
}

void proc_records_free(proc_records_t *rs)
{
    if (!rs)
        return;
    for (size_t i = 0; i < rs->len; i++)
        record_free(&rs->items[i]);
    free(rs->items);
    rs->items = NULL;
    rs->len = rs->cap = 0;
}

static void parse_cmdline(const unsigned char *raw, size_t raw_len,
                          char **out_argv0, char **out_cmdline)
{
    *out_argv0   = NULL;
    *out_cmdline = NULL;

    if (!raw || raw_len == 0)
        return;

    size_t parts_count = 0;
    size_t out_len     = 0;

    size_t i = 0;
    while (i < raw_len)
    {
        size_t start = i;
        while (i < raw_len && raw[i] != '\0')
            i++;
        size_t seg_len = i - start;

        if (seg_len > 0)
        {
            parts_count++;
            out_len += seg_len;
            if (parts_count > 1)
                out_len += 1;
        }

        while (i < raw_len && raw[i] == '\0')
            i++;
    }

    if (parts_count == 0)
        return;

    char *joined = (char *)malloc(out_len + 1);
    if (!joined)
        return;

    size_t w          = 0;
    i                 = 0;
    size_t part_index = 0;

    while (i < raw_len)
    {
        size_t start = i;
        while (i < raw_len && raw[i] != '\0')
            i++;
        size_t seg_len = i - start;

        if (seg_len > 0)
        {
            part_index++;
            if (part_index > 1)
                joined[w++] = ' ';
            memcpy(joined + w, raw + start, seg_len);
            w += seg_len;

            if (part_index == 1)
            {
                char *a0 = (char *)malloc(seg_len + 1);
                if (a0)
                {
                    memcpy(a0, raw + start, seg_len);
                    a0[seg_len] = '\0';
                    *out_argv0  = a0;
                }
            }
        }

        while (i < raw_len && raw[i] == '\0')
            i++;
    }

    joined[w]    = '\0';
    *out_cmdline = joined;
}

static int collect_proc_records(proc_records_t *out)
{
    if (!out)
        return -1;

    memset(out, 0, sizeof(*out));

    DIR *d = opendir("/proc");
    if (!d)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL)
    {
        if (!is_all_digits(ent->d_name))
            continue;

        errno         = 0;
        long pid_long = strtol(ent->d_name, NULL, 10);
        if (errno != 0 || pid_long <= 0 || pid_long > INT_MAX)
            continue;

        pid_t pid = (pid_t)pid_long;

        char path_comm[PATH_MAX];
        char path_cmdl[PATH_MAX];
        char path_exe[PATH_MAX];

        snprintf(path_comm, sizeof(path_comm), "/proc/%ld/comm", pid_long);
        snprintf(path_cmdl, sizeof(path_cmdl), "/proc/%ld/cmdline", pid_long);
        snprintf(path_exe, sizeof(path_exe), "/proc/%ld/exe", pid_long);

        proc_record_t r;
        memset(&r, 0, sizeof(r));
        r.pid = pid;

        char *comm = try_read_text(path_comm);
        if (comm)
        {
            rstrip_newline(comm);
            r.comm = comm;
        }

        size_t         raw_len = 0;
        unsigned char *raw     = try_read_bytes(path_cmdl, &raw_len);
        if (raw)
        {
            parse_cmdline(raw, raw_len, &r.argv0, &r.cmdline);
            free(raw);
        }

        r.exe = try_readlink_path(path_exe);

        if (records_push(out, r) != 0)
        {
            record_free(&r);
            closedir(d);
            proc_records_free(out);
            return -1;
        }
    }

    closedir(d);
    return 0;
}

static int cmp_comm_ref(const void *a, const void *b)
{
    const comm_ref_t *ra = (const comm_ref_t *)a;
    const comm_ref_t *rb = (const comm_ref_t *)b;
    // NULLs shouldn't be present, but be defensive
    if (!ra->comm && !rb->comm)
        return 0;
    if (!ra->comm)
        return 1;
    if (!rb->comm)
        return -1;
    return strcmp(ra->comm, rb->comm);
}

static int most_common_comm(const proc_records_t *rs,
                            const char          **out_comm,
                            size_t               *out_count,
                            int                  *out_example_pid)
{
    if (!rs || !out_comm || !out_count)
        return -1;

    *out_comm  = NULL;
    *out_count = 0;
    if (out_example_pid)
        *out_example_pid = -1;

    // Build list of comm references (skip NULL comms)
    comm_ref_t *refs = malloc(rs->len * sizeof(*refs));
    if (!refs)
        return -1;

    size_t n = 0;
    for (size_t i = 0; i < rs->len; i++)
    {
        if (rs->items[i].comm && rs->items[i].comm[0] != '\0')
        {
            refs[n].comm = rs->items[i].comm;
            refs[n].pid  = rs->items[i].pid;
            n++;
        }
    }

    if (n == 0)
    {
        free(refs);
        return -1;
    }

    qsort(refs, n, sizeof(*refs), cmp_comm_ref);

    const char *best_comm  = refs[0].comm;
    size_t      best_count = 1;
    int         best_pid   = refs[0].pid;

    const char *cur_comm  = refs[0].comm;
    size_t      cur_count = 1;
    int         cur_pid   = refs[0].pid;

    for (size_t i = 1; i < n; i++)
    {
        if (strcmp(refs[i].comm, cur_comm) == 0)
        {
            cur_count++;
        }
        else
        {
            if (cur_count > best_count)
            {
                best_count = cur_count;
                best_comm  = cur_comm;
                best_pid   = cur_pid;
            }
            cur_comm  = refs[i].comm;
            cur_count = 1;
            cur_pid   = refs[i].pid;
        }
    }

    if (cur_count > best_count)
    {
        best_count = cur_count;
        best_comm  = cur_comm;
        best_pid   = cur_pid;
    }

    free(refs);

    *out_comm  = best_comm;
    *out_count = best_count;
    if (out_example_pid)
        *out_example_pid = best_pid;

    return 0;
}

static void print_self_info(const proc_records_t *rs)
{
    pid_t self = getpid();

    for (size_t i = 0; i < rs->len; i++)
    {
        const proc_record_t *r = &rs->items[i];

        if (r->pid == self)
        {
            printf("\n=== Current Process ===\n");
            printf("PID:     %d\n", r->pid);
            printf("comm:    %s\n", r->comm ? r->comm : "(null)");
            // printf("argv0:   %s\n", r->argv0 ? r->argv0 : "(null)");
            // printf("cmdline: %s\n", r->cmdline ? r->cmdline : "(null)");
            printf("exe:     %s\n", r->exe ? r->exe : "(null)");
            return;
        }
    }

    printf("\nCurrent process not found in /proc records (race condition?)\n");
}

static int set_comm_name(const char *name)
{
    if (!name)
        return -1;

    char buf[16] = {0};
    strncpy(buf, name, 15);

    if (prctl(PR_SET_NAME, buf, 0, 0, 0) != 0)
    {
        fprintf(stderr, "prctl(PR_SET_NAME) failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static void print_self_comm_now(void)
{
    FILE *f = fopen("/proc/self/comm", "r");
    if (!f)
    {
        perror("fopen /proc/self/comm");
        return;
    }

    char line[256];
    if (fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\r\n")] = '\0';
        printf("NOW /proc/self/comm = '%s'\n", line);
    }
    fclose(f);
}

int rename_process(void)
{
    proc_records_t rs;
    if (collect_proc_records(&rs) != 0)
    {
        perror("collect_proc_records");
        return 1;
    }

    const char *comm        = NULL;
    size_t      count       = 0;
    int         example_pid = -1;

    if (most_common_comm(&rs, &comm, &count, &example_pid) == 0)
    {
        printf("Most common comm: '%s' (count=%zu, example pid=%d)\n",
               comm, count, example_pid);
    }
    else
    {
        printf("No comm values found.\n");
    }

    print_self_info(&rs);

    printf("\nChanging COMM name\n");
    // print_self_comm_now();
    set_comm_name(comm);
    // print_self_comm_now();

    proc_records_free(&rs);

    if (collect_proc_records(&rs) != 0)
    {
        perror("collect_proc_records");
        return 1;
    }

    print_self_info(&rs);

    proc_records_free(&rs);
    return 0;
}
