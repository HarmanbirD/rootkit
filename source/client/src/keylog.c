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

/**
 * Check if a device is a keyboard by examining its capabilities
 * Returns 1 if device is a keyboard, 0 otherwise
 */
static int is_keyboard(int fd)
{
    unsigned long evbit[NBITS(EV_MAX)];
    unsigned long keybit[NBITS(KEY_MAX)];
    int           keyboard_keys = 0;
    int           i;

    // Get supported event types
    memset(evbit, 0, sizeof(evbit));
    if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit) < 0)
    {
        return 0;
    }

    // Must support key events
    if (!test_bit(EV_KEY, evbit))
    {
        return 0;
    }

    // Get supported keys
    memset(keybit, 0, sizeof(keybit));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keybit) < 0)
    {
        return 0;
    }

    // Check for typical keyboard keys
    // A real keyboard should have alphanumeric keys

    // Check for letter keys (Q-P row as a sample)
    for (i = KEY_Q; i <= KEY_P; i++)
    {
        if (test_bit(i, keybit))
        {
            keyboard_keys++;
        }
    }

    // Also check for common keys
    if (test_bit(KEY_ENTER, keybit))
        keyboard_keys++;
    if (test_bit(KEY_SPACE, keybit))
        keyboard_keys++;

    // If we found several keyboard keys, it's likely a keyboard
    return keyboard_keys > 5;
}

/**
 * Get detailed device information
 */
static void print_device_info(const char *path, int fd)
{
    char            name[256] = "Unknown";
    char            phys[256] = "Unknown";
    char            uniq[256] = "Unknown";
    struct input_id device_info;
    unsigned long   evbit[NBITS(EV_MAX)];

    // Get device name
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);

    // Get physical location
    if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) < 0)
    {
        strcpy(phys, "N/A");
    }

    // Get unique identifier
    if (ioctl(fd, EVIOCGUNIQ(sizeof(uniq)), uniq) < 0)
    {
        strcpy(uniq, "N/A");
    }

    // Get device ID (vendor, product, etc.)
    if (ioctl(fd, EVIOCGID, &device_info) >= 0)
    {
        printf("  Device:   %s\n", path);
        printf("  Name:     %s\n", name);
        printf("  Physical: %s\n", phys);
        printf("  Unique:   %s\n", uniq);
        printf("  Vendor:   0x%04x\n", device_info.vendor);
        printf("  Product:  0x%04x\n", device_info.product);
        printf("  Version:  0x%04x\n", device_info.version);
        printf("  Bus Type: %d\n", device_info.bustype);
    }
    else
    {
        printf("  Device: %s\n", path);
        printf("  Name:   %s\n", name);
    }

    // Show supported event types
    memset(evbit, 0, sizeof(evbit));
    if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit) >= 0)
    {
        printf("  Events:   ");
        if (test_bit(EV_KEY, evbit))
            printf("KEY ");
        if (test_bit(EV_REL, evbit))
            printf("REL ");
        if (test_bit(EV_ABS, evbit))
            printf("ABS ");
        if (test_bit(EV_LED, evbit))
            printf("LED ");
        if (test_bit(EV_SND, evbit))
            printf("SND ");
        printf("\n");
    }
}

static void discover_keyboards(void)
{
    DIR           *dir;
    struct dirent *entry;
    char           path[PATH_MAX_LEN];
    int            fd;
    int            keyboard_count = 0;

    printf("Linux Keyboard Device Discovery\n");
    printf("================================\n\n");

    // Open /dev/input directory
    dir = opendir("/dev/input");
    if (!dir)
    {
        fprintf(stderr, "Error: Cannot open /dev/input: %s\n", strerror(errno));
        fprintf(stderr, "Hint: You may need to run this program with sudo\n");
        return;
    }

    printf("Scanning /dev/input for keyboard devices...\n\n");

    // Iterate through all entries
    while ((entry = readdir(dir)) != NULL)
    {
        // Only check event devices
        if (strncmp(entry->d_name, "event", 5) != 0)
        {
            continue;
        }

        // Build full path
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        // Try to open device
        fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            // Skip devices we can't access
            continue;
        }

        // Check if it's a keyboard
        if (is_keyboard(fd))
        {
            keyboard_count++;
            printf("Keyboard #%d:\n", keyboard_count);
            print_device_info(path, fd);
            printf("\n");
        }

        close(fd);
    }

    closedir(dir);

    // Summary
    printf("================================\n");
    printf("Total keyboards found: %d\n", keyboard_count);

    if (keyboard_count == 0)
    {
        printf("\nNo keyboards detected. Possible reasons:\n");
        printf("  - Insufficient permissions (try running with sudo)\n");
        printf("  - No physical keyboard connected\n");
        printf("  - Keyboard drivers not loaded\n");
    }
    else
    {
        printf("\nNote: Multiple interfaces from the same physical device may be listed.\n");
        printf("This is normal for devices with separate standard/multimedia interfaces.\n");
    }
}

int start_keylogging(void)
{
    discover_keyboards();

    return 0;
}
