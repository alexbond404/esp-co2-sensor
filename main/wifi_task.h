#ifndef __WIFI_TASK_H
#define __WIFI_TASK_H

#include "passwords.h"


#ifndef WIFI_SSID
    #define WIFI_SSID                "test"
#endif //WIFI_SSID
#ifndef WIFI_PASS
    #define WIFI_PASS                "test"
#endif //WIFI_PASS

#define WIFI_HANDLERS_MAX_COUNT         3

typedef enum
{
    WIFI_READY,
    WIFI_DISCONNECTED,
} wifi_event_e;

typedef void (*wifi_handler_cb)(wifi_event_e event, void *user_data);

void wifi_task_init(const char *hostname);
void wifi_start(void);
int wifi_register_handler(wifi_handler_cb handler, void *user_data);

#endif //__WIFI_TASK_H