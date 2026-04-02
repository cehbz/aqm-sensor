// AQM Sensor — WiFi + MQTT + PMS5003 + HA Discovery

#include "device_id.hpp"
#include "mqtt.hpp"
#include "ha_discovery.hpp"
#include "pms5003.hpp"
#include "secrets.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "led_strip.h"

namespace {

const char *TAG = "aqm_sensor";

// --- Pin assignments (from schematic) ---
constexpr gpio_num_t PIN_SK6812 = GPIO_NUM_0;

// --- State ---
led_strip_handle_t s_strip{};

// ============================================================
// SK6812 on GPIO0
// ============================================================

void init_sk6812()
{
    led_strip_config_t cfg{};
    cfg.strip_gpio_num          = PIN_SK6812;
    cfg.max_leds                = 1;
    cfg.led_model               = LED_MODEL_WS2812;
    cfg.color_component_format  = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    cfg.flags.invert_out        = false;

    led_strip_rmt_config_t rmt{};
    rmt.clk_src        = RMT_CLK_SRC_DEFAULT;
    rmt.resolution_hz  = 0;
    rmt.flags.with_dma = false;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&cfg, &rmt, &s_strip));
    led_strip_clear(s_strip);
    led_strip_refresh(s_strip);
}

void set_led(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
}

// ============================================================
// WiFi
// ============================================================

EventGroupHandle_t s_wifi_events{};
constexpr int WIFI_CONNECTED_BIT = BIT0;
constexpr int WIFI_FAIL_BIT      = BIT1;

void wifi_event_handler(void * /*arg*/, esp_event_base_t base,
                        int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected — reconnecting");
        esp_wifi_connect();
        xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *event = static_cast<ip_event_got_ip_t *>(data);
        ESP_LOGI(TAG, "Connected — IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

bool wifi_connect()
{
    s_wifi_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr));

    wifi_config_t wifi_cfg{};
    std::strncpy(reinterpret_cast<char *>(wifi_cfg.sta.ssid),
                 WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    std::strncpy(reinterpret_cast<char *>(wifi_cfg.sta.password),
                 WIFI_PASS, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Reduce TX power for bad-crystal SuperMini variants
    esp_wifi_set_max_tx_power(34);  // 8.5 dBm

    ESP_LOGI(TAG, "Connecting to %s ...", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_events,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE, pdFALSE,
        pdMS_TO_TICKS(15000));

    return (bits & WIFI_CONNECTED_BIT) != 0;
}

// ============================================================
// MQTT callbacks
// ============================================================

void on_mqtt_connect()
{
    ha_discovery_publish(device_id_get());
}

void on_mqtt_data(const char * /*topic*/, int /*topic_len*/,
                  const char * /*data*/, int /*data_len*/)
{
}

void publish_pms(const PmsMeasurement &m)
{
    if (!mqtt_is_connected()) return;

    const char *id = device_id_get();
    char topic[64];
    char value[16];

    struct { uint16_t val; const char *key; } fields[] = {
        {m.pm1_0,   "pm1_0"},
        {m.pm2_5,   "pm2_5"},
        {m.pm10,    "pm10"},
        {m.cnt_03,  "cnt_03"},
        {m.cnt_05,  "cnt_05"},
        {m.cnt_10,  "cnt_10"},
        {m.cnt_25,  "cnt_25"},
        {m.cnt_50,  "cnt_50"},
        {m.cnt_100, "cnt_100"},
    };

    for (auto &f : fields) {
        std::snprintf(topic, sizeof(topic), "aqm/%s/sensor/%s", id, f.key);
        std::snprintf(value, sizeof(value), "%u", f.val);
        mqtt_publish(topic, value);
    }

    // Update LED: green if PM2.5 < 12, yellow < 35, red >= 35
    if (m.pm2_5 < 12) {
        set_led(0, 20, 0);
    } else if (m.pm2_5 < 35) {
        set_led(20, 20, 0);
    } else {
        set_led(20, 0, 0);
    }
}

void publish_rssi()
{
    if (!mqtt_is_connected()) return;

    wifi_ap_record_t ap{};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        const char *id = device_id_get();
        char topic[64];
        char value[8];
        std::snprintf(topic, sizeof(topic), "aqm/%s/sensor/rssi", id);
        std::snprintf(value, sizeof(value), "%d", ap.rssi);
        mqtt_publish(topic, value);
    }
}

} // namespace

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());

    // 1. SK6812 — blue = alive
    init_sk6812();
    set_led(0, 0, 40);
    ESP_LOGI(TAG, "SK6812 initialized on GPIO0");

    // 2. Device ID
    device_id_init();
    const char *id = device_id_get();
    ESP_LOGI(TAG, "Device ID: %s", id);

    // 3. WiFi — amber while connecting
    set_led(40, 25, 0);
    bool connected = wifi_connect();
    if (connected) {
        ESP_LOGI(TAG, "WiFi OK");
        set_led(0, 60, 0);
    } else {
        ESP_LOGW(TAG, "WiFi not connected yet — will auto-reconnect");
        set_led(60, 0, 0);
    }

    // 4. NTP — init regardless of WiFi state; syncs once connected
    setenv("TZ", TIMEZONE, 1);
    tzset();
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t sntp_err = esp_netif_sntp_init(&sntp_cfg);
    if (sntp_err == ESP_OK) {
        ESP_LOGI(TAG, "SNTP configured, TZ=%s", TIMEZONE);
    } else {
        ESP_LOGE(TAG, "SNTP init failed: %s", esp_err_to_name(sntp_err));
    }

    // 5. MQTT
    mqtt_init(id, on_mqtt_connect, on_mqtt_data);

    // 6. PMS5003
    pms5003_init(publish_pms);
    ESP_LOGI(TAG, "PMS5003 initialized");

    // Idle — publish RSSI periodically, PMS publishes via callback
    set_led(0, 10, 0);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        publish_rssi();

        auto now = std::time(nullptr);
        struct tm tm_buf{};
        localtime_r(&now, &tm_buf);
        ESP_LOGI(TAG, "Heartbeat %04d-%02d-%02d %02d:%02d:%02d | mqtt %s",
                 tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                 tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                 mqtt_is_connected() ? "connected" : "disconnected");
    }
}
