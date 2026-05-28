#include "gpio.h"

#ifdef PLATFORM_rpi

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int g_gpio_base = 0;

/* Find the sysfs GPIO base for the Pi's main GPIO bank (pinctrl-bcm2711). */
static int find_gpio_base(void)
{
    DIR *dir = opendir("/sys/class/gpio");
    if (!dir) return 0;
    struct dirent *e;
    int base = 0;
    while ((e = readdir(dir)) != NULL) {
        if (strncmp(e->d_name, "gpiochip", 8) != 0) continue;
        char lpath[320], bpath[320];
        snprintf(lpath, sizeof(lpath), "/sys/class/gpio/%s/label", e->d_name);
        snprintf(bpath, sizeof(bpath), "/sys/class/gpio/%s/base",  e->d_name);
        char label[64] = {0};
        int fd = open(lpath, O_RDONLY);
        if (fd >= 0) { (void)read(fd, label, sizeof(label) - 1); close(fd); }
        if (strstr(label, "pinctrl") || strstr(label, "fe200000") || strstr(label, "bcm2")) {
            char buf[16] = {0};
            fd = open(bpath, O_RDONLY);
            if (fd >= 0) { (void)read(fd, buf, sizeof(buf) - 1); close(fd); base = atoi(buf); }
            break;
        }
    }
    closedir(dir);
    return base;
}

static void gpio_export(int bcm)
{
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) return;
    char buf[8];
    int n = snprintf(buf, sizeof(buf), "%d", g_gpio_base + bcm);
    (void)write(fd, buf, (size_t)n);
    close(fd);
}

static void gpio_set_direction(int bcm, const char *dir)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", g_gpio_base + bcm);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    (void)write(fd, dir, strlen(dir));
    close(fd);
}

void gpio_init(void)
{
    g_gpio_base = find_gpio_base();
    gpio_export(GPIO_SYNC_PULSE);
    gpio_export(GPIO_HEALTH);
    /* udev creates sysfs entries asynchronously; poll until ready */
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction",
             g_gpio_base + GPIO_SYNC_PULSE);
    for (int i = 0; i < 50; i++) {
        int fd = open(path, O_WRONLY);
        if (fd >= 0) { close(fd); break; }
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 20000000LL };
        nanosleep(&ts, NULL);
    }
    gpio_set_direction(GPIO_SYNC_PULSE, "out");
    gpio_set_direction(GPIO_HEALTH, "out");
}

void gpio_set(int bcm, int val)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", g_gpio_base + bcm);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    (void)write(fd, val ? "1" : "0", 1);
    close(fd);
}

void gpio_cleanup(void)
{
    gpio_set(GPIO_SYNC_PULSE, 0);
    gpio_set(GPIO_HEALTH, 1);
}

#else /* host stub */

void gpio_init(void)    {}
void gpio_set(int pin, int val) { (void)pin; (void)val; }
void gpio_cleanup(void) {}

#endif
