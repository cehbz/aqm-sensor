#pragma once

#include <cstdint>

struct PmsMeasurement {
    uint16_t pm1_0;    // µg/m³ (atmospheric)
    uint16_t pm2_5;
    uint16_t pm10;
    uint16_t cnt_03;   // particles/0.1L >0.3µm
    uint16_t cnt_05;
    uint16_t cnt_10;
    uint16_t cnt_25;
    uint16_t cnt_50;
    uint16_t cnt_100;
};

using pms_callback_t = void (*)(const PmsMeasurement &m);

/// Initialise PMS5003: SET/RESET GPIOs, UART on GPIO2 (RX) / GPIO3 (TX).
/// Starts a FreeRTOS task that calls `cb` on each valid frame (~1/s).
void pms5003_init(pms_callback_t cb);
