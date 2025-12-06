#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "defs.h"
#include "driver/i2c_master.h"
#include "ha_task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ota_http_server.h"
#include "scd30.h"
#include "scd40.h"
#include "stat_led.h"
#include "wifi_task.h"


#define DATA_SEND_INTERVAL  30

#define SCL_IO_PIN         4
#define SDA_IO_PIN         3


typedef enum
{
    DEV_UNKNOWN,
    DEV_SCD30,
    DEV_SCD40,
} i2c_device_t;


static const char *TAG = "main";
static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t scd_bus_handle = NULL;
static i2c_device_t dev_type = DEV_UNKNOWN;
static bool scd_calibrating = false;
static TickType_t scd_calibrate_start;
static uint32_t scd_calibrate_ppm;

static i2c_device_t init_i2c(void);


static bool scdxx_io_func(uint8_t *buf_wr, uint8_t len_wr, uint8_t *buf_rd, uint8_t len_rd)
{
    bool res = false;
    if (NULL != buf_wr && 0 != len_wr && NULL != buf_rd && 0 != len_rd)
    {
        res = (ESP_OK == i2c_master_transmit_receive(scd_bus_handle, buf_wr, len_wr, buf_rd, len_rd, 500));
    }
    else if (NULL != buf_wr && 0 != len_wr)
    {
        res = (ESP_OK == i2c_master_transmit(scd_bus_handle, buf_wr, len_wr, 500));
    }
    else if (NULL != buf_rd && 0 != len_rd)
    {
        res = (ESP_OK == i2c_master_receive(scd_bus_handle, buf_rd, len_rd, 500));
    }

    return res;
}

static i2c_device_t init_i2c(void)
{
    // initialize i2c bus
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1, // any available
        .scl_io_num = SCL_IO_PIN,
        .sda_io_num = SDA_IO_PIN,
        .glitch_ignore_cnt = 7,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    // probe devices
    i2c_device_t dev_type = DEV_UNKNOWN;

    if (ESP_OK == i2c_master_probe(bus_handle, 0x61, 100))
    {
        // add scd30
        i2c_device_config_t scd30_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x61,
            .scl_speed_hz = 50000,
            .scl_wait_us = 200*1000,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &scd30_config, &scd_bus_handle));
        dev_type = DEV_SCD30;
    }
    else if (ESP_OK == i2c_master_probe(bus_handle, 0x62, 100))
    {
        // add scd40
        i2c_device_config_t scd40_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x62,
            .scl_speed_hz = 100000,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &scd40_config, &scd_bus_handle));
        dev_type = DEV_SCD40;
    }
    else
    {
        for (uint8_t addr = 0; addr < 0x7f; addr++)
        {
            if (ESP_OK == i2c_master_probe(bus_handle, addr, 10))
            {
                ESP_LOGI(TAG, "found device, addr: 0x%02x", addr);
            }
        }
    }

    return dev_type;
}

static void wifi_event_handler(wifi_event_e event, void *user_data)
{
    switch (event)
    {
        case WIFI_READY:
            stat_led_set(1, 30);
            ha_start();
            ESP_LOGI(TAG, "ota http server start result: %d\n", (int)ota_http_server_start());
            break;

        case WIFI_DISCONNECTED:
            stat_led_set(5, 10);
            ha_stop();
            ota_http_server_stop();
            break;
    }
}

void ha_event_cb(ha_event_t event, void *param)
{
    ESP_LOGI(TAG, "ha_event: %d, param: %08x", (int)event, (unsigned int)param);
    switch (event)
    {
        case HA_EVENT_CALIBRATE:
            if (DEV_SCD30 == dev_type)
            {
                scd30_set_measurement_interval(2);
            }
            scd_calibrate_start = xTaskGetTickCount();
            scd_calibrate_ppm = (uint32_t)param;
            scd_calibrating = true;
            break;

        case HA_EVENT_CALIBRATE_ABORT:
            if (DEV_SCD30 == dev_type)
            {
                scd30_set_measurement_interval(DATA_SEND_INTERVAL);
            }
            scd_calibrating = false;
            break;

        default:
            break;
    }
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    stat_led_init();
    stat_led_set(5, 10);

    ha_init(ha_event_cb);

    wifi_task_init(HOSTNAME);
    wifi_register_handler(wifi_event_handler, NULL);

    wifi_start();
    ota_http_server_init();

    ESP_LOGI(TAG, "Minimum free heap size: %"PRIu32" bytes\n", esp_get_minimum_free_heap_size());

    dev_type = init_i2c();
    switch (dev_type)
    {
        case DEV_SCD30:
            ESP_LOGI(TAG, "device type is scd30");
            scd30_init(scdxx_io_func);
            scd30_set_measurement_interval(DATA_SEND_INTERVAL);
            scd30_set_automatic_self_calibration_enabled(false);
            scd30_start_cont_measurement(0);
            break;

        case DEV_SCD40:
            ESP_LOGI(TAG, "device type is scd40");
            scd40_init(scdxx_io_func);
            scd40_set_automatic_self_calibration_enabled(false);
            scd40_start_measurement();
            break;

        default:
            ESP_LOGI(TAG, "device type is unknown");
            break;
    }

    while (1)
    {
        bool scd_done = false;
        while (!scd_done)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);

            switch (dev_type)
            {
                case DEV_SCD30:
                {
                    bool rdy = false;
                    scd30_get_data_ready_status(&rdy);
                    if (rdy)
                    {
                        sensor_output_t output;
                        scd30_read_measurement(&output.co2, &output.temp, &output.hum);
                        ESP_LOGI(TAG, "scd30 co2: %d, temp: %d, hum: %d",
                            (int)output.co2, (int)output.temp, (int)output.hum);

                        // check if calibration is ongoing
                        if (scd_calibrating && (xTaskGetTickCount() - scd_calibrate_start >= 2 * 60 * 1000 / portTICK_PERIOD_MS))
                        {
                            scd30_force_calibration(scd_calibrate_ppm);
                            scd30_set_measurement_interval(DATA_SEND_INTERVAL);
                            scd_calibrating = false;
                        }

                        ha_update_data(&output, scd_calibrating);

                        scd_done = true;
                    }
                }
                break;

                case DEV_SCD40:
                {
                    bool rdy = false;
                    scd40_get_data_ready_status(&rdy);
                    if (rdy)
                    {
                        sensor_output_t output;
                        scd40_read_measurement(&output.co2, &output.temp, &output.hum);
                        ESP_LOGI(TAG, "scd40 co2: %d, temp: %d, hum: %d",
                            (int)output.co2, (int)output.temp, (int)output.hum);

                        // check if calibration is ongoing
                        if (scd_calibrating)
                        {
                            if (xTaskGetTickCount() - scd_calibrate_start >= 2 * 60 * 1000 / portTICK_PERIOD_MS)
                            {
                                scd40_force_calibration(scd_calibrate_ppm);
                                scd_calibrating = false;
                            }
                        }
                        else
                        {
                            // do delay - no need to send measures too often
                            vTaskDelay(DATA_SEND_INTERVAL * 1000 / portTICK_PERIOD_MS);
                        }

                        ha_update_data(&output, scd_calibrating);

                        scd_done = true;
                    }
                }
                break;

                default:
                {
                    vTaskDelay(DATA_SEND_INTERVAL * 1000 / portTICK_PERIOD_MS);
                    scd_calibrating = false;

                    sensor_output_t output;
                    output.temp = 0;
                    output.hum = 0;
                    output.co2 = 0;
                    ha_update_data(&output, scd_calibrating);
                    scd_done = true;
                }
                break;
            }
        }

        ha_pusblish_data();
    }
}
