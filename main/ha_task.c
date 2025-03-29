#include "ha_task.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "esp_log.h"
#include "esp_mac.h"

#include "ha_types.h"
#include "ha_device.h"
#include "ha_number.h"
#include "ha_sensor.h"
#include "ha_switch.h"


static ha_event_cb_t _event_cb;
static SemaphoreHandle_t sem = NULL;

static ha_device_handle_t ha_dev;

static ha_config_handle_t ha_temp_sensor;
static ha_config_handle_t ha_hum_sensor;
static ha_config_handle_t ha_co2_sensor;
static ha_config_handle_t ha_calibrate_switch;
static ha_config_handle_t ha_calibrate_ppm_number;


//static const char *TAG = "ha_task";
static bool calibrate_switch_on_change_cb(ha_config_handle_t config, bool state)
{
    _event_cb(state ? HA_EVENT_CALIBRATE : HA_EVENT_CALIBRATE_ABORT, NULL);
    return true;
}

// convert MAC from binary format to string
static inline char* gen_mac_str(const uint8_t* mac, char* pref, char* mac_str)
{
    sprintf(mac_str, "%s%02x%02x%02x%02x%02x%02x", pref, MAC2STR(mac));
    return mac_str;
}


void ha_init(ha_event_cb_t event_cb)
{
    _event_cb = event_cb;

    ha_device_config_t ha_config = {
        .mqtt_server = "192.168.0.220",
        .mqtt_username = "test",
        .mqtt_password = "test",
        .device_unique_prefix = "co2_sensor",
        .ha_topic_prefix = NULL,
        .model = "SCDx0 co2 sensor",
        .manufacturer = "AlexBond",
    };

    char device_name[HA_NAME_MAX_SIZE];
    uint8_t sta_mac[6] = {0};
    esp_read_mac(sta_mac, ESP_MAC_WIFI_STA);
    ha_config.device_name = gen_mac_str(sta_mac, "co2_", device_name);

    ha_dev = ha_device_init(&ha_config);

    ha_temp_sensor = ha_sensor_init("temp", SENSOR_TEMPERATURE, 0);
    ha_sensor_set_units_of_measurement(ha_temp_sensor, "°C");
    ha_sensor_set_value_precision(ha_temp_sensor, 1);
    ha_hum_sensor = ha_sensor_init("hum", SENSOR_HUMIDITY, 0);
    ha_sensor_set_units_of_measurement(ha_hum_sensor, "%");
    ha_sensor_set_value_precision(ha_hum_sensor, 0);
    ha_co2_sensor = ha_sensor_init("co2", SENSOR_CARBON_DIOXIDE, 0);
    ha_sensor_set_units_of_measurement(ha_co2_sensor, "ppm");
    ha_calibrate_switch = ha_switch_init("calibrate", false, calibrate_switch_on_change_cb);
    number_settings_t calibrate_number_setttings = 
    {
        .min = 400,
        .max = 2000,
        .step = 50,
        .mode = NUMBER_MODE_AUTO,
        .cls = DEV_CLASS_NONE,
        .on_change_cb = NULL, /// TODO
    };
    ha_calibrate_ppm_number = ha_number_init("calibrate ppm", 400, &calibrate_number_setttings);

    ha_device_add_config(ha_dev, ha_temp_sensor);
    ha_device_add_config(ha_dev, ha_hum_sensor);
    ha_device_add_config(ha_dev, ha_co2_sensor);
    ha_device_add_config(ha_dev, ha_calibrate_switch);
    ha_device_add_config(ha_dev, ha_calibrate_ppm_number);

    sem = xSemaphoreCreateBinary();
    xSemaphoreGive(sem);
}

void ha_start(void)
{
    ha_device_start(ha_dev);
}

void ha_stop(void)
{
    ha_device_stop(ha_dev);
}

void ha_update_data(sensor_output_t *output, bool calibrate)
{
    xSemaphoreTake(sem, portMAX_DELAY);
    ha_sensor_set_value(ha_temp_sensor, output->temp);
    ha_sensor_set_value(ha_hum_sensor, output->hum);
    ha_sensor_set_value(ha_co2_sensor, output->co2);
    ha_switch_set_value(ha_calibrate_switch, calibrate);
    xSemaphoreGive(sem);
}

void ha_pusblish_data(void)
{
    xSemaphoreTake(sem, portMAX_DELAY);
    ha_device_commit(ha_dev);
    xSemaphoreGive(sem);
}