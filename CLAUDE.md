# AQM Sensor Board

KiCad 9 project for an air quality monitor sensor board.

## Prefer kct tool
When reasoning about the KiCad files, use the kct tool whenever possible for analysis rather than hand crafted bash command lines or ad-hoc python scripts.

## Architecture

- **MCU**: ESP32-C3 SuperMini (through-hole, socketed)
- **Sensor**: PMS5003 particulate matter sensor (UART, 9600 baud)
- **I2C**: QWIIC/Stemma QT connector (JST-SH 4-pin) for expansion
- **LED**: SK6812 RGB (3-channel, NOT RGBW) — use WS2812 / 24-bit GRB protocol
- **Power**: 5V via ESP32 USB-C, 3.3V from ESP32 onboard regulator
- **PCB**: 2-layer, ~45x30mm, +3.3V pour on F.Cu, GND plane on B.Cu

## Known Issues (v1)

- ESP32-C3 SuperMini footprint is **rotated 180 degrees** — antenna cutout is at the wrong end. Module must be inserted flipped to match pin assignments.
- PMS5003 connector (J3) has **VCC/GND swapped** (pin 1=GND, pin 2=5V, but PMS5003 expects pin 1=VCC, pin 2=GND).
- PMS5003 cable that ships with the sensor **mirrors the pin order** (pin 1↔8). The PCB layout must account for this in the next rev.
- SK6812 VDD is powered from 3.3V (below 3.7V datasheet min) — works in practice.
- SK6812 pin 1 (marked with triangle/chamfer) is VSS/GND, not VDD.

## Pin Mapping (with module inserted flipped to match footprint)

| GPIO | Net | Function |
|------|-----|----------|
| GPIO0 | /LED | SK6812 DIN (WS2812 protocol) |
| GPIO1 | /PMS_SET | PMS5003 SET (high=normal, low=sleep) |
| GPIO2 | /PMS_RX | ESP TX → PMS5003 RXD |
| GPIO3 | /PMS_TX | PMS5003 TXD → ESP RX (through R5 1k) |
| GPIO4 | /PMS_RESET | PMS5003 RESET (active low) |
| GPIO8 | /I2C_SDA | QWIIC SDA (also onboard blue LED) |
| GPIO9 | /I2C_SCL | QWIIC SCL |

## Firmware

ESP-IDF v5.x project in `firmware/`. Uses `espressif/led_strip` component (v3.x) from ESP component registry.

- Build: `source ~/esp/esp-idf/export.sh && cd firmware && idf.py build`
- Flash + monitor: `idf.py -p /dev/cu.usbmodem1101 flash monitor`
- WiFi credentials: `firmware/main/secrets.h` (gitignored). Copy `secrets.h.example` to get started.
- WiFi tested at reduced TX power (8.5 dBm) for bad-xtal ESP32-C3 SuperMini variants

## Build Notes

- ESP32-C3 RMT has no DMA — set `flags.with_dma = false` for led_strip
- WiFi auth mode enum: `WIFI_AUTH_WPA2_PSK` (not `WPA2_PSK`)
- VS Code IntelliSense: `compile_commands.json` is at `firmware/build/compile_commands.json`, referenced from `.vscode/c_cpp_properties.json` at project root

### KiCad ERC Notes
- GND net needs a `PWR_FLAG` symbol — KiCad requires a `power_output` on every net with `power_input` pins.
- Any power rail not directly output by a component pin (e.g., regulator output through an inductor) needs `PWR_FLAG`.

## Related Projects
- aqm_fan_controller at ../aqm_fan_controller — fan controller board (TPS562201, MOSFET, QWIIC)
- cyd-aqm at ../cyd-aqm — display
