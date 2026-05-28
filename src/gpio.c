#include "gpio.h"

#ifdef PLATFORM_rpi

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

/* BCM2711 GPIO peripheral block, addressed as 32-bit words from the
 * /dev/gpiomem base. Layout (only the registers we touch):
 *   word 0..5  GPFSEL0..5   function select; 10 pins × 3 mode bits each
 *   word 7     GPSET0       write 1-bit to drive pin HIGH (no read-mod-write)
 *   word 10    GPCLR0       write 1-bit to drive pin LOW
 *
 * GPSET/GPCLR are atomic from the CPU's view: each edge is a single
 * STR instruction with no preceding load. The two-register split is
 * what makes that possible without a critical section. */
#define GPFSEL(pin)   ((pin) / 10)
#define GPFSHIFT(pin) (((pin) % 10) * 3)
#define GPSET0        7
#define GPCLR0        10

#define GPIO_MAP_SIZE 0x1000  /* 4 KB page covers all GPFSEL/GPSET/GPCLR */

static volatile uint32_t *g_gpio = NULL;

static void cfg_output(int pin)
{
    if (!g_gpio || pin < 0 || pin > 53) return;
    int      reg   = GPFSEL(pin);
    int      shift = GPFSHIFT(pin);
    uint32_t v     = g_gpio[reg];
    v &= ~(0x7u << shift);
    v |=  (0x1u << shift);   /* 001 = output */
    g_gpio[reg] = v;
}

void gpio_init(void)
{
    int fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("gpio_init: open /dev/gpiomem");
        return;
    }
    void *p = mmap(NULL, GPIO_MAP_SIZE,
                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) {
        perror("gpio_init: mmap");
        return;
    }
    g_gpio = (volatile uint32_t *)p;

    cfg_output(GPIO_SYNC_PULSE);
    cfg_output(GPIO_HEALTH);

    /* Boot state: pulse line LOW, health LED HIGH (not yet synced). */
    g_gpio[GPCLR0] = 1u << GPIO_SYNC_PULSE;
    g_gpio[GPSET0] = 1u << GPIO_HEALTH;
}

void gpio_set(int pin, int val)
{
    if (!g_gpio || pin < 0 || pin > 31) return;
    if (val) g_gpio[GPSET0] = 1u << pin;
    else     g_gpio[GPCLR0] = 1u << pin;
}

void gpio_cleanup(void)
{
    if (!g_gpio) return;
    g_gpio[GPCLR0] = 1u << GPIO_SYNC_PULSE;
    g_gpio[GPSET0] = 1u << GPIO_HEALTH;
    munmap((void *)g_gpio, GPIO_MAP_SIZE);
    g_gpio = NULL;
}

#else /* host stub */

void gpio_init(void)    {}
void gpio_set(int pin, int val) { (void)pin; (void)val; }
void gpio_cleanup(void) {}

#endif
