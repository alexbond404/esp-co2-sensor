#include "wifi_task.h"

#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"


typedef struct
{
    wifi_handler_cb handler;
    void *user_data;
} wifi_handler_t;

static const char *TAG = "wifi_task";

static wifi_handler_t handlers[WIFI_HANDLERS_MAX_COUNT];
static uint8_t handlers_count;
static bool wifi_connected;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        // {
        esp_wifi_connect();
            // s_retry_num++;
        ESP_LOGI(TAG, "retry to connect to the AP '%s'", WIFI_SSID);
        // }
        // else
        // {
        //     xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        // }
        // ESP_LOGI(TAG,"connect to the AP fail");
        if (wifi_connected)
        {
            for (uint8_t i = 0; i < handlers_count; i++)
            {
                handlers[i].handler(WIFI_DISCONNECTED, handlers[i].user_data);
            }
        }
        wifi_connected = false;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    //     s_retry_num = 0;
//        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        for (uint8_t i = 0; i < handlers_count; i++)
        {
            handlers[i].handler(WIFI_READY, handlers[i].user_data);
        }
        wifi_connected = true;
    }
}

void wifi_task_init(const char *hostname)
{
    handlers_count = 0;
    wifi_connected = false;

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (hostname != NULL && hostname[0] != 0)
    {
        esp_netif_set_hostname(sta_netif, hostname);
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA_PSK,
         .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

//    ESP_LOGI(TAG, "wifi_task_init finished.");
}

void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_wifi_start() );
}

int wifi_register_handler(wifi_handler_cb handler, void *user_data)
{
    if (handlers_count >= WIFI_HANDLERS_MAX_COUNT)
    {
        return -1;
    }

    handlers[handlers_count].handler = handler;
    handlers[handlers_count].user_data = user_data;
    handlers_count++;

    return 0;
}
