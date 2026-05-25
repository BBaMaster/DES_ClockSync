#include "gpio.h"

#ifdef PLATFORM_rpi

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static void gpio_export(int pin)
{
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) return;
    char buf[8];
    int n = snprintf(buf, sizeof(buf), "%d", pin);
    (void)write(fd, buf, (size_t)n);
    close(fd);
}

static void gpio_set_direction(int pin, const char *dir)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    (void)write(fd, dir, strlen(dir));
    close(fd);
}

void gpio_init(void)
{
    gpio_export(GPIO_SYNC_PULSE);
    gpio_export(GPIO_HEALTH);
    gpio_set_direction(GPIO_SYNC_PULSE, "out");
    gpio_set_direction(GPIO_HEALTH, "out");
}

void gpio_set(int pin, int val)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
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
