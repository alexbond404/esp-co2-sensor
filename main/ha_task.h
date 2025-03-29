#pragma once

#include <stdbool.h>
#include <stdint.h>


typedef enum
{
     HA_EVENT_CALIBRATE,
     HA_EVENT_CALIBRATE_ABORT,
} ha_event_t;

typedef void (*ha_event_cb_t)(ha_event_t event, void *param);

typedef struct
{
     float temp;
     float hum;
     uint16_t co2;
} sensor_output_t;

void ha_init(ha_event_cb_t event_cb);
void ha_start(void);
void ha_stop(void);

void ha_update_data(sensor_output_t *output, bool calibrate);
void ha_pusblish_data(void);
