#pragma once

#include "esp_err.h"

using mqtt_data_cb_t = void (*)(const char *topic, int topic_len,
                                const char *data, int data_len);
using mqtt_connect_cb_t = void (*)();

esp_err_t mqtt_init(const char *device_id,
                    mqtt_connect_cb_t on_connect,
                    mqtt_data_cb_t on_data);

bool mqtt_is_connected();

int mqtt_publish(const char *topic, const char *data,
                 int qos = 0, bool retain = false);

int mqtt_subscribe(const char *topic, int qos = 0);
