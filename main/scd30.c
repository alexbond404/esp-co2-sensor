#include "scd30.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"


#define CMD_START_CONT_MEAS             0x0010
#define CMD_STOP_CONT_MEAS              0x0104
#define CMD_SET_MEAS_INTERVAL           0x4600
#define CMD_READ_MEAS                   0x0300
#define CMD_GET_DATA_READY_STATUS       0x0202
#define CMD_SET_AUTO_SELF_CAL_EN        0x5306
// #define CMD_SET_AMBIENT_PRESSURE        0xe000
#define CMD_FORCE_RECALIBRATION         0x5204
#define CMD_GET_ALTITUDE_COMPENSATION   0x5102
#define CMD_GET_TEMPERATURE_COMPENSATION 0x5403
#define CMD_GET_FIRMWARE_VERSION        0xd100

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

void scd30_init(sdc_io_fptr_t io_func_)
{
    io_func = io_func_;
}

bool scd30_start_cont_measurement(uint32_t preassure)
{
    uint16_t param = preassure / 100;
    return send_command(CMD_START_CONT_MEAS, &param, 1);
}

bool scd30_stop_cont_measurement(void)
{
    return send_command(CMD_STOP_CONT_MEAS, NULL, 0);
}

bool scd30_set_measurement_interval(uint16_t sec)
{
    return send_command(CMD_SET_MEAS_INTERVAL, &sec, 1);    
}

bool scd30_read_measurement(uint16_t *co2, float *temp, float *hum)
{
    if (!send_command(CMD_READ_MEAS, NULL, 0))
    {
        return false;
    }

    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint16_t resp[2*3];
    if (!read_responce(resp, 6))
    {
        return false;
    }
    uint32_t x = ((uint32_t)resp[0] << 16) | resp[1];
    *co2 = *(float*)&x;
    x = ((uint32_t)resp[2] << 16) | resp[3];
    *temp = *(float*)&x;
    x = ((uint32_t)resp[4] << 16) | resp[5];
    *hum = *(float*)&x;

    return true;
}

bool scd30_get_data_ready_status(bool *ready)
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

    *ready = (resp != 0);
    return true;
}

bool scd30_set_automatic_self_calibration_enabled(bool enable)
{
    uint16_t param = enable;
    return send_command(CMD_SET_AUTO_SELF_CAL_EN, &param, 1);
}

bool scd30_get_altitute_compensation(uint16_t *altitude)
{
    if (!send_command(CMD_GET_ALTITUDE_COMPENSATION, NULL, 0))
    {
        return false;
    }

    uint16_t resp[1];
    if (!read_responce(resp, 1))
    {
        return false;
    }

    *altitude = resp[0];
    return true;
}

bool scd30_get_temperature_offset(uint16_t *offset)
{
    if (!send_command(CMD_GET_TEMPERATURE_COMPENSATION, NULL, 0))
    {
        return false;
    }

    uint16_t resp[1];
    if (!read_responce(resp, 1))
    {
        return false;
    }

    *offset = resp[0];
    return true;
}

bool scd30_force_calibration(uint16_t co2)
{
    return send_command(CMD_FORCE_RECALIBRATION, &co2, 1);
}

bool scd30_get_firmware_version(uint8_t *major, uint8_t *minor)
{
    if (!send_command(CMD_GET_FIRMWARE_VERSION, NULL, 0))
    {
        return false;
    }

    uint16_t resp[1];
    if (!read_responce(resp, 1))
    {
        return false;
    }

    *major = resp[0] >> 8;
    *minor = resp[0] >> 0;
    return true;
}
