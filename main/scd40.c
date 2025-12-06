#include "scd30.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"


#define CMD_START_PERIODIC_MEAS         0x21b1
#define CMD_STOP_PERIODIC_MEAS          0x3f86
#define CMD_READ_MEAS                   0xec05
#define CMD_GET_DATA_READY_STATUS       0xe4b8
#define CMD_SET_AUTO_SELF_CAL_EN        0x2416
#define CMD_GET_AMBIENT_PRESSURE        0xe000
#define CMD_SET_AMBIENT_PRESSURE        0xe000
#define CMD_PERFORM_FORCE_RECALIBRAITON 0x362f

#define CRC8_POLYNOMIAL                 0x31
#define CRC8_INIT                       0xff

static sdc_io_fptr_t io_func;


static uint8_t sensirion_common_generate_crc(const uint8_t* data, uint16_t count)
{
    uint16_t current_byte;
    uint8_t crc = CRC8_INIT;
    uint8_t crc_bit;

    /* calculates 8-Bit checksum with given polynomial */
    for (current_byte = 0; current_byte < count; ++current_byte)
    {
        crc ^= (data[current_byte]);
        for (crc_bit = 8; crc_bit > 0; --crc_bit)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ CRC8_POLYNOMIAL;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

static bool send_command(uint16_t cmd, uint16_t *params, uint8_t params_cnt)
{
    const uint8_t buf_len = 2 + 3*params_cnt;
    uint8_t *buf = malloc(buf_len);
    buf[0] = cmd >> 8;
    buf[1] = cmd >> 0;
    for (uint8_t i = 0; i < params_cnt; i++)
    {
        buf[2 + 3*i + 0] = params[i] >> 8;
        buf[2 + 3*i + 1] = params[i] >> 0;
        buf[2 + 3*i + 2] = sensirion_common_generate_crc(&buf[2 + 3*i + 0], 2);
    }
    bool res = io_func(buf, buf_len, NULL, 0);
    free(buf);
    return res;
}

static bool read_responce(uint16_t *val, uint8_t len)
{
    uint8_t *buf = malloc(3*len);
    bool res;
    res = io_func(NULL, 0, buf, 3*len);

    for (uint8_t i = 0; i < len && res; i++)
    {
        val[i] = ((uint16_t)buf[3*i + 0] << 8) | (uint16_t)buf[3*i + 1];
        res = (sensirion_common_generate_crc(buf, 2) == buf[2]);
    }
    free(buf);
    return res;
}

void scd40_init(sdc_io_fptr_t io_func_)
{
    io_func = io_func_;
}

bool scd40_start_measurement(void)
{
    return send_command(CMD_START_PERIODIC_MEAS, NULL, 0);
}

bool scd40_stop_measurement(void)
{
    bool res = send_command(CMD_STOP_PERIODIC_MEAS, NULL, 0);
    if (res)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    return res;
}

bool scd40_read_measurement(uint16_t *co2, float *temp, float *hum)
{
    if (!send_command(CMD_READ_MEAS, NULL, 0))
    {
        return false;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);

    uint16_t resp[3];
    if (!read_responce(resp, 3))
    {
        return false;
    }
    *co2 = resp[0];
    *temp = -45 + (float)175 * resp[1] / 65535;
    *hum = (float)100 * resp[2] / 65535;

    return true;
}

bool scd40_get_data_ready_status(bool *ready)
{
    if (!send_command(CMD_GET_DATA_READY_STATUS, NULL, 0))
    {
        return false;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);

    uint16_t resp;
    if (!read_responce(&resp, 1))
    {
        return false;
    }

    *ready = (bool)(resp & 0x7ff);
    return true;
}

bool scd40_set_automatic_self_calibration_enabled(bool enable)
{
    uint16_t param = enable;
    return send_command(CMD_SET_AUTO_SELF_CAL_EN, &param, 1);
}

bool scd40_get_ambient_pressure(uint32_t *pressure)
{
    if (!send_command(CMD_GET_AMBIENT_PRESSURE, NULL, 0))
    {
        return false;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);

    uint16_t resp;
    if (!read_responce(&resp, 1))
    {
        return false;
    }

    *pressure = (uint32_t)resp * 100;
    return true;
}

bool scd40_set_ambient_pressure(uint32_t pressure)
{
    uint16_t param = pressure / 100;
    return send_command(CMD_SET_AMBIENT_PRESSURE, &param, 1);
}

bool scd40_force_calibration(uint16_t co2)
{
    bool res = send_command(CMD_PERFORM_FORCE_RECALIBRAITON, &co2, 1);
    if (!res)
    {
        return false;
    }

    vTaskDelay(400 / portTICK_PERIOD_MS);

    uint16_t frc_corr;
    res = read_responce(&frc_corr, 1);
    if (!res)
    {
        return false;
    }

    return frc_corr != 0xffff;
}
