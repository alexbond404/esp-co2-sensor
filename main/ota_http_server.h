#ifndef __OTA_HTTP_SERVER_H_
#define __OTA_HTTP_SERVER_H_

#include <stdbool.h>


void ota_http_server_init(void);
bool ota_http_server_start(void);
void ota_http_server_stop(void);

#endif // __OTA_HTTP_SERVER_H_
