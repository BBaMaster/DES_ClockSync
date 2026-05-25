#pragma once

#define GPIO_SYNC_PULSE  18
#define GPIO_HEALTH      23

void gpio_init(void);
void gpio_set(int pin, int val);
void gpio_cleanup(void);
