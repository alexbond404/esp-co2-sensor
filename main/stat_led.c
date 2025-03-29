#include "stat_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "driver/gpio.h"


static TimerHandle_t tmr;
static uint8_t t_on = 5;
static uint8_t t_off = 5;


static void stat_led_tmr_cb(TimerHandle_t arg)
{
    static uint8_t t = 0;

    if ( (t == 0) && (t_on > 0) )
    {
        gpio_set_level(STAT_LED_GPIO, !STAT_LED_INV);
    }
    else if ( (t == t_on) && (t_off > 0) )
    {
        gpio_set_level(STAT_LED_GPIO, STAT_LED_INV);
    }

    if (++t >= (t_on + t_off))
    {
        t = 0;
    }
}


void stat_led_init(void)
{
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL << STAT_LED_GPIO);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    tmr = xTimerCreate("statLedTmr", (100 / portTICK_PERIOD_MS), pdTRUE, NULL, stat_led_tmr_cb);
    xTimerStart(tmr, portMAX_DELAY);
}

void stat_led_set(uint8_t on, uint8_t period)
{
    if ( (on <= period) && (period > 0) )
    {
        t_on = on;
        t_off = period - on;
    }
}
