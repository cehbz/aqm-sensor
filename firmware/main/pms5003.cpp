#include "pms5003.hpp"

#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

namespace {

const char *TAG = "pms5003";

constexpr gpio_num_t PIN_SET   = GPIO_NUM_4;
constexpr gpio_num_t PIN_RESET = GPIO_NUM_1;
constexpr int        RX_GPIO   = 2;  // PMS TX → ESP RX
constexpr int        TX_GPIO   = 3;  // ESP TX → PMS RX

constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int BAUD        = 9600;
constexpr int FRAME_LEN   = 32;
constexpr int BUF_SIZE    = 256;

pms_callback_t s_callback{};

bool parse_frame(const uint8_t *buf, int len, PmsMeasurement &out)
{
    for (int i = 0; i <= len - FRAME_LEN; ++i) {
        if (buf[i] != 0x42 || buf[i + 1] != 0x4D)
            continue;

        uint16_t frame_len = (buf[i + 2] << 8) | buf[i + 3];
        if (frame_len != 28)
            continue;

        uint16_t checksum = 0;
        for (int j = 0; j < FRAME_LEN - 2; ++j)
            checksum += buf[i + j];

        uint16_t expected = (buf[i + 30] << 8) | buf[i + 31];
        if (checksum != expected) {
            ESP_LOGW(TAG, "Checksum mismatch: calc=%04x expected=%04x",
                     checksum, expected);
            continue;
        }

        // Atmospheric readings (bytes 10-15)
        out.pm1_0   = (buf[i + 10] << 8) | buf[i + 11];
        out.pm2_5   = (buf[i + 12] << 8) | buf[i + 13];
        out.pm10    = (buf[i + 14] << 8) | buf[i + 15];

        // Particle counts (bytes 16-29)
        out.cnt_03  = (buf[i + 16] << 8) | buf[i + 17];
        out.cnt_05  = (buf[i + 18] << 8) | buf[i + 19];
        out.cnt_10  = (buf[i + 20] << 8) | buf[i + 21];
        out.cnt_25  = (buf[i + 22] << 8) | buf[i + 23];
        out.cnt_50  = (buf[i + 24] << 8) | buf[i + 25];
        out.cnt_100 = (buf[i + 26] << 8) | buf[i + 27];

        return true;
    }
    return false;
}

void read_task(void * /*arg*/)
{
    uint8_t buf[BUF_SIZE];
    PmsMeasurement m{};

    while (true) {
        int bytes = uart_read_bytes(UART_PORT, buf, BUF_SIZE,
                                    pdMS_TO_TICKS(3000));
        if (bytes > 0 && parse_frame(buf, bytes, m)) {
            ESP_LOGI(TAG, "PM1.0: %u  PM2.5: %u  PM10: %u µg/m³",
                     m.pm1_0, m.pm2_5, m.pm10);
            if (s_callback) {
                s_callback(m);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

} // namespace

void pms5003_init(pms_callback_t cb)
{
    s_callback = cb;

    // SET and RESET control pins
    gpio_config_t io{};
    io.pin_bit_mask = (1ULL << PIN_SET) | (1ULL << PIN_RESET);
    io.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io);

    gpio_set_level(PIN_SET, 1);     // normal mode
    gpio_set_level(PIN_RESET, 0);   // hold reset
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RESET, 1);   // release reset
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "SET=high, RESET released");

    // UART
    uart_config_t uart_cfg{};
    uart_cfg.baud_rate = BAUD;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity    = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, TX_GPIO, RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE, 0, 0, nullptr, 0));

    xTaskCreate(read_task, "pms5003", 4096, nullptr, 5, nullptr);
    ESP_LOGI(TAG, "UART started on RX=GPIO%d TX=GPIO%d", RX_GPIO, TX_GPIO);
}
