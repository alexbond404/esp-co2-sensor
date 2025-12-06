#ifndef __SCD40_H_
#define __SCD40_H_

#include <stdbool.h>
#include <stdint.h>

typedef bool (*sdc_io_fptr_t)(uint8_t *buf_wr, uint8_t len_wr, uint8_t *buf_rd, uint8_t len_rd);

void scd40_init(sdc_io_fptr_t io_func_);
bool scd40_start_measurement(void);
bool scd40_stop_measurement(void);
bool scd40_read_measurement(uint16_t *co2, float *temp, float *hum);
bool scd40_get_data_ready_status(bool *ready);
bool scd40_set_automatic_self_calibration_enabled(bool enable);
bool scd40_get_ambient_pressure(uint32_t *pressure);
bool scd40_set_ambient_pressure(uint32_t pressure);
bool scd40_force_calibration(uint16_t co2);

#endif
