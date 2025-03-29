#pragma once

#include <stdint.h>


#define STAT_LED_GPIO       8
#define STAT_LED_INV        1


void stat_led_init(void);

void stat_led_set(uint8_t on, uint8_t period);
