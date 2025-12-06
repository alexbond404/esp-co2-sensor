configure_file("${CMAKE_CURRENT_LIST_DIR}/env.h.in" "${CMAKE_CURRENT_LIST_DIR}/env.h" @ONLY)

idf_component_register(SRCS "ha_task.c" "wifi_task.c" "stat_led.c" "scd30.c" "scd40.c" "ota_http_server.c" "main.c"
                    INCLUDE_DIRS ".")