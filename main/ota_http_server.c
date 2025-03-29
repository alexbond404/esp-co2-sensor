#include "ota_http_server.h"

#include "defs.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "ota_http";


/* Empty handle to esp_http_server */
static httpd_handle_t server = NULL;

static esp_err_t get_handler(httpd_req_t *req);
static esp_err_t reboot_handler(httpd_req_t *req);
static esp_err_t post_handler(httpd_req_t *req);


/* URI handler structure for GET /ota */
static const httpd_uri_t uri_ota_get = {
    .uri      = "/ota",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

/* URI handler structure for GET /reboot */
static const httpd_uri_t uri_reboot_get = {
    .uri      = "/reboot",
    .method   = HTTP_GET,
    .handler  = reboot_handler,
    .user_ctx = NULL
};

/* URI handler structure for POST /ota */
static const httpd_uri_t uri_ota_post = {
    .uri      = "/ota",
    .method   = HTTP_POST,
    .handler  = post_handler,
    .user_ctx = NULL
};


/* Our URI handler function to be called during GET /uri request */
esp_err_t get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "name", cJSON_CreateString(PROJECT_TITLE));
    cJSON_AddItemToObject(root, "hostname", cJSON_CreateString(HOSTNAME));
    cJSON_AddItemToObject(root, "idf_version", cJSON_CreateString(IDF_VER));
    cJSON_AddItemToObject(root, "version", cJSON_CreateString(PROJECT_VER));
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

esp_err_t reboot_handler(httpd_req_t *req)
{
    esp_restart();
    return ESP_OK;
}

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char *buf = (char*)malloc(4096);

    int res = 0;
    esp_err_t err = ESP_OK;
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    if (NULL == ota_partition)
    {
        res = -1;
        goto exit;
    }

    esp_ota_handle_t ota;
    err = esp_ota_begin(ota_partition, req->content_len, &ota);
    if (ESP_OK != err)
    {
        res = -2;
        goto exit;
    }

    uint32_t bytes_left = req->content_len;
    while (bytes_left > 0)
    {
        size_t recv_size = MIN(bytes_left, 4096);
        int ret = httpd_req_recv(req, buf, recv_size);
        if (ret <= 0){  /* 0 return value indicates connection closed */
            /* Check if timeout occurred */
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* In case of timeout one can choose to retry calling
                * httpd_req_recv(), but to keep it simple, here we
                * respond with an HTTP 408 (Request Timeout) error */
                httpd_resp_send_408(req);
            }
            /* In case of error, returning ESP_FAIL will
            * ensure that the underlying socket is closed */
            goto exit1;
        }

        ESP_LOGI(TAG, "recv bytes: %d\n", ret);
        bytes_left -= ret;

        err = esp_ota_write(ota, buf, ret);
        if (ESP_OK != err)
        {
            res = -2;
            goto exit1;
        }
    }

exit1:
    if (0 != res)
    {
        esp_ota_abort(ota);
    }
    else
    {
        err = esp_ota_end(ota);
        if (ESP_OK != err)
        {
            res = -3;
            goto exit;
        }
        
        err = esp_ota_set_boot_partition(ota_partition);
        if (ESP_OK != err)
        {
            res = -4;
            goto exit;
        }
    }

exit:
    int len = snprintf(buf, 4096, "{\"res\": %d, \"esp_err\": %d}", res, (int)err);
    httpd_resp_send(req, buf, len);
    free(buf);
    return ESP_OK;
}

void ota_http_server_init(void)
{
    esp_ota_mark_app_valid_cancel_rollback();
}

bool ota_http_server_start(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_ota_get);
        httpd_register_uri_handler(server, &uri_ota_post);
        httpd_register_uri_handler(server, &uri_reboot_get);
    }

    /* If server failed to start, handle will be NULL */
    return (NULL != server);
}

void ota_http_server_stop(void)
{
    if (server)
    {
        /* Stop the httpd server */
        httpd_stop(server);
        server = NULL;
    }
}
