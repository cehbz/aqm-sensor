#include "mqtt.hpp"
#include "secrets.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "mqtt_client.h"

namespace {

const char *TAG = "mqtt";

esp_mqtt_client_handle_t s_client{};
bool s_connected{false};
mqtt_connect_cb_t s_on_connect{};
mqtt_data_cb_t s_on_data{};

char s_availability_topic[64]{};

void event_handler(void * /*arg*/, esp_event_base_t /*base*/,
                   int32_t event_id, void *event_data)
{
    auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        s_connected = true;
        esp_mqtt_client_publish(s_client, s_availability_topic,
                                "online", 0, 1, 1);
        if (s_on_connect) {
            s_on_connect();
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker");
        s_connected = false;
        break;

    case MQTT_EVENT_DATA:
        if (s_on_data) {
            s_on_data(event->topic, event->topic_len,
                      event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error type: %d", event->error_handle->error_type);
        break;

    default:
        break;
    }
}

} // namespace

esp_err_t mqtt_init(const char *device_id,
                    mqtt_connect_cb_t on_connect,
                    mqtt_data_cb_t on_data)
{
    s_on_connect = on_connect;
    s_on_data = on_data;

    std::snprintf(s_availability_topic, sizeof(s_availability_topic),
                  "aqm/%s/availability", device_id);

    esp_mqtt_client_config_t cfg{};
    cfg.broker.address.uri = MQTT_BROKER_URI;
    cfg.session.last_will.topic = s_availability_topic;
    cfg.session.last_will.msg = "offline";
    cfg.session.last_will.msg_len = 7;
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = 1;
    cfg.session.keepalive = 60;

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        s_client, MQTT_EVENT_ANY, event_handler, nullptr));

    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));
    ESP_LOGI(TAG, "MQTT client started, broker: %s", MQTT_BROKER_URI);

    return ESP_OK;
}

bool mqtt_is_connected()
{
    return s_connected;
}

int mqtt_publish(const char *topic, const char *data,
                 int qos, bool retain)
{
    if (!s_client) return -1;
    return esp_mqtt_client_publish(s_client, topic, data, 0, qos, retain ? 1 : 0);
}

int mqtt_subscribe(const char *topic, int qos)
{
    if (!s_client) return -1;
    return esp_mqtt_client_subscribe(s_client, topic, qos);
}
