#ifndef __SCD30_H_
#define __SCD30_H_

#include <stdbool.h>
#include <stdint.h>

typedef bool (*sdc_io_fptr_t)(uint8_t *buf_wr, uint8_t len_wr, uint8_t *buf_rd, uint8_t len_rd);

void scd30_init(sdc_io_fptr_t io_func_);
bool scd30_start_cont_measurement(uint32_t preassure);
bool scd30_stop_cont_measurement(void);
bool scd30_set_measurement_interval(uint16_t sec);
bool scd30_read_measurement(uint16_t *co2, float *temp, float *hum);
bool scd30_get_data_ready_status(bool *ready);
bool scd30_set_automatic_self_calibration_enabled(bool enable);
// bool scd40_set_ambient_pressure(uint32_t pressure);
bool scd30_force_calibration(uint16_t co2);

#endif
