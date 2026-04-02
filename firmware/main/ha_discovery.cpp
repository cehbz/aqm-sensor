#include "ha_discovery.hpp"
#include "mqtt.hpp"

#include <cstdio>
#include <cstddef>
#include "cJSON.h"
#include "esp_log.h"

namespace {

const char *TAG = "ha_disc";

struct SensorMeta {
    const char *key;           // topic suffix and unique_id component
    const char *name;          // human-readable
    const char *device_class;  // HA device_class (nullptr if none)
    const char *unit;          // unit_of_measurement (nullptr if none)
    const char *entity_cat;    // entity_category (nullptr for primary)
};

constexpr SensorMeta kEntities[] = {
    {"pm1_0",   "PM1.0",            "pm1",              "µg/m³", nullptr},
    {"pm2_5",   "PM2.5",            "pm25",             "µg/m³", nullptr},
    {"pm10",    "PM10",             "pm10",             "µg/m³", nullptr},
    {"cnt_03",  "Particles >0.3µm", nullptr,            "/0.1L", nullptr},
    {"cnt_05",  "Particles >0.5µm", nullptr,            "/0.1L", nullptr},
    {"cnt_10",  "Particles >1.0µm", nullptr,            "/0.1L", nullptr},
    {"cnt_25",  "Particles >2.5µm", nullptr,            "/0.1L", nullptr},
    {"cnt_50",  "Particles >5.0µm", nullptr,            "/0.1L", nullptr},
    {"cnt_100", "Particles >10µm",  nullptr,            "/0.1L", nullptr},
    {"rssi",    "WiFi RSSI",        "signal_strength",  "dBm",   "diagnostic"},
};

constexpr size_t kEntityCount = sizeof(kEntities) / sizeof(kEntities[0]);

cJSON *make_device_block(const char *device_id)
{
    cJSON *dev = cJSON_CreateObject();
    cJSON *ids = cJSON_CreateArray();
    cJSON_AddItemToArray(ids, cJSON_CreateString(device_id));
    cJSON_AddItemToObject(dev, "identifiers", ids);

    char name[32];
    std::snprintf(name, sizeof(name), "AQM Sensor %s", device_id);
    cJSON_AddStringToObject(dev, "name", name);
    cJSON_AddStringToObject(dev, "model", "AQM Sensor");
    cJSON_AddStringToObject(dev, "manufacturer", "haynes");
    cJSON_AddStringToObject(dev, "sw_version", "0.2.0");

    return dev;
}

void publish_sensor_config(const char *device_id, const SensorMeta &meta)
{
    char topic[128];
    std::snprintf(topic, sizeof(topic),
                  "homeassistant/sensor/%s/%s/config", device_id, meta.key);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "name", meta.name);

    char unique_id[64];
    std::snprintf(unique_id, sizeof(unique_id),
                  "sensor_%s_%s", device_id, meta.key);
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON_AddStringToObject(root, "object_id", unique_id);

    char state_topic[64];
    std::snprintf(state_topic, sizeof(state_topic),
                  "aqm/%s/sensor/%s", device_id, meta.key);
    cJSON_AddStringToObject(root, "state_topic", state_topic);

    if (meta.device_class)
        cJSON_AddStringToObject(root, "device_class", meta.device_class);
    if (meta.unit)
        cJSON_AddStringToObject(root, "unit_of_measurement", meta.unit);
    if (meta.entity_cat)
        cJSON_AddStringToObject(root, "entity_category", meta.entity_cat);

    cJSON_AddStringToObject(root, "state_class", "measurement");

    char avail_topic[64];
    std::snprintf(avail_topic, sizeof(avail_topic),
                  "aqm/%s/availability", device_id);
    cJSON_AddStringToObject(root, "availability_topic", avail_topic);

    cJSON_AddItemToObject(root, "device", make_device_block(device_id));

    char *json = cJSON_PrintUnformatted(root);
    mqtt_publish(topic, json, 1, true);
    ESP_LOGI(TAG, "Published: %s", topic);

    cJSON_free(json);
    cJSON_Delete(root);
}

} // namespace

void ha_discovery_publish(const char *device_id)
{
    for (size_t i = 0; i < kEntityCount; ++i) {
        publish_sensor_config(device_id, kEntities[i]);
    }
}
