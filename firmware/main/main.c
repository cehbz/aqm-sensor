#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "led_strip.h"

static const char *TAG = "pms5003";

#define LED_GPIO        0
#define LED_COUNT       1
#define BRIGHTNESS      40

#define PMS_SET_GPIO    1
#define PMS_RESET_GPIO  4

#define PMS_UART        UART_NUM_1
#define PMS_BAUD        9600
#define PMS_FRAME_LEN   32
#define PMS_BUF_SIZE    256

static led_strip_handle_t s_strip = NULL;

static void set_led(uint8_t r, uint8_t g, uint8_t b)
{
    if (s_strip) {
        led_strip_set_pixel(s_strip, 0, r, g, b);
        led_strip_refresh(s_strip);
    }
}

static void init_led(void)
{
    led_strip_config_t cfg = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 0,
        .mem_block_symbols = 0,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&cfg, &rmt, &s_strip));
    led_strip_clear(s_strip);
    led_strip_refresh(s_strip);
}

static void init_pms_control(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PMS_SET_GPIO) | (1ULL << PMS_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);

    gpio_set_level(PMS_SET_GPIO, 1);
    gpio_set_level(PMS_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PMS_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "PMS5003 SET=high, RESET released");
}

static void try_uart_on_gpio(int rx_gpio)
{
    // Uninstall previous driver if any
    uart_driver_delete(PMS_UART);

    uart_config_t uart_cfg = {
        .baud_rate = PMS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(PMS_UART, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(PMS_UART,
        UART_PIN_NO_CHANGE, rx_gpio,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(PMS_UART, PMS_BUF_SIZE, 0, 0, NULL, 0));

    ESP_LOGW(TAG, "=== Listening on GPIO%d for 10 seconds ===", rx_gpio);

    uint8_t buf[PMS_BUF_SIZE];
    for (int attempt = 0; attempt < 5; attempt++) {
        int bytes = uart_read_bytes(PMS_UART, buf, PMS_BUF_SIZE,
                                    pdMS_TO_TICKS(2000));
        if (bytes > 0) {
            ESP_LOGI(TAG, "GPIO%d: got %d bytes!", rx_gpio, bytes);
            // Dump first 32 bytes as hex
            char hex[97];
            int pos = 0;
            for (int i = 0; i < bytes && i < 32; i++)
                pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", buf[i]);
            ESP_LOGI(TAG, "  raw: %s", hex);

            // Check for PMS start bytes
            for (int i = 0; i < bytes - 1; i++) {
                if (buf[i] == 0x42 && buf[i + 1] == 0x4D) {
                    ESP_LOGI(TAG, "  Found PMS5003 frame header at offset %d!", i);
                    return;
                }
            }
        } else {
            ESP_LOGW(TAG, "GPIO%d: timeout (attempt %d/5)", rx_gpio, attempt + 1);
        }
    }
}

static bool parse_pms_frame(const uint8_t *buf, int len)
{
    for (int i = 0; i <= len - PMS_FRAME_LEN; i++) {
        if (buf[i] != 0x42 || buf[i + 1] != 0x4D)
            continue;

        uint16_t frame_len = (buf[i + 2] << 8) | buf[i + 3];
        if (frame_len != 28)
            continue;

        uint16_t checksum = 0;
        for (int j = 0; j < PMS_FRAME_LEN - 2; j++)
            checksum += buf[i + j];

        uint16_t expected = (buf[i + 30] << 8) | buf[i + 31];
        if (checksum != expected) {
            ESP_LOGW(TAG, "Checksum mismatch: calc=%04x expected=%04x", checksum, expected);
            continue;
        }

        uint16_t pm1_atm  = (buf[i + 10] << 8) | buf[i + 11];
        uint16_t pm25_atm = (buf[i + 12] << 8) | buf[i + 13];
        uint16_t pm10_atm = (buf[i + 14] << 8) | buf[i + 15];

        uint16_t cnt_03 = (buf[i + 16] << 8) | buf[i + 17];
        uint16_t cnt_05 = (buf[i + 18] << 8) | buf[i + 19];
        uint16_t cnt_10 = (buf[i + 20] << 8) | buf[i + 21];
        uint16_t cnt_25 = (buf[i + 22] << 8) | buf[i + 23];

        ESP_LOGI(TAG, "PM1.0: %u  PM2.5: %u  PM10: %u ug/m3",
                 pm1_atm, pm25_atm, pm10_atm);
        ESP_LOGI(TAG, ">0.3um: %u  >0.5um: %u  >1.0um: %u  >2.5um: %u",
                 cnt_03, cnt_05, cnt_10, cnt_25);
        return true;
    }
    return false;
}

void app_main(void)
{
    init_led();
    set_led(0, 0, BRIGHTNESS);
    ESP_LOGI(TAG, "PMS5003 UART probe starting...");

    init_pms_control();

    // Try GPIO3 first (expected pin based on schematic)
    set_led(BRIGHTNESS, 0, BRIGHTNESS);  // purple = trying GPIO3
    try_uart_on_gpio(3);

    // Try GPIO2 as well (in case nets are swapped)
    set_led(0, BRIGHTNESS, BRIGHTNESS);  // cyan = trying GPIO2
    try_uart_on_gpio(2);

    // Now settle on GPIO3 for continuous reading
    ESP_LOGW(TAG, "=== Continuous read on GPIO3 ===");
    uart_driver_delete(PMS_UART);
    uart_config_t uart_cfg = {
        .baud_rate = PMS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(PMS_UART, &uart_cfg);
    uart_set_pin(PMS_UART, UART_PIN_NO_CHANGE, 3,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(PMS_UART, PMS_BUF_SIZE, 0, 0, NULL, 0);

    set_led(BRIGHTNESS, BRIGHTNESS, 0);  // yellow = waiting
    uint8_t buf[PMS_BUF_SIZE];

    while (1) {
        int bytes = uart_read_bytes(PMS_UART, buf, PMS_BUF_SIZE,
                                    pdMS_TO_TICKS(3000));
        if (bytes > 0 && parse_pms_frame(buf, bytes)) {
            set_led(0, BRIGHTNESS, 0);  // green
        } else if (bytes > 0) {
            char hex[97];
            int pos = 0;
            for (int i = 0; i < bytes && i < 32; i++)
                pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", buf[i]);
            ESP_LOGW(TAG, "Got %d bytes, no valid frame: %s", bytes, hex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
