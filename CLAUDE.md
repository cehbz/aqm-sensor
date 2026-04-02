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

## Notes

- SK6812 VDD is powered from 3.3V (below 3.7V datasheet min) — works in practice.
- SK6812 pin 1 (marked with triangle/chamfer) is VSS/GND, not VDD.
- PMS5003 cable mirrors pin order (pin 1↔8). J1 wiring accounts for this.

## Pin Mapping

| GPIO | Net | Function |
|------|-----|----------|
| GPIO0 | /LED | SK6812 DIN (WS2812 protocol) |
| GPIO1 | /PMS_RESET | PMS5003 RESET (active low) |
| GPIO2 | /PMS_TX | PMS5003 TXD → ESP RX (through R1 1k) |
| GPIO3 | /PMS_RX | ESP TX → PMS5003 RXD |
| GPIO4 | /PMS_SET | PMS5003 SET (high=normal, low=sleep) |
| GPIO8 | /I2C_SDA | QWIIC SDA (also onboard blue LED) |
| GPIO9 | /I2C_SCL | QWIIC SCL |

## Firmware

ESP-IDF v6 project in `firmware/`. Currently running WiFi + NTP + MQTT + PMS5003 + HA Discovery (direct WiFi, smoke test architecture).

- Build: `idf.py build` (wrapper at ~/bin/idf.py handles environment)
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

## Network Architecture and Domain Model

See `../aqm_fan_controller/CLAUDE.md` for the full architectural analysis and domain model. Summary:
- Moving to ESP-WIFI-MESH with T-Display S3 as root node and MQTT bridge
- Current direct WiFi firmware is for smoke testing / bring-up
- This board hosts two Devices: a **Pms5003** (Sensor) that produces **PmsMeasurement** values, and a **StatusLed** (RGB light, visible to HA). They are independent — coordination between them is application policy, not entity behavior
- The Gateway timestamps measurements on arrival; mesh nodes don't run NTP

## Related Projects
- aqm_fan_controller at ../aqm_fan_controller — fan controller board (TPS562201, MOSFET, QWIIC)
- aqm_sen55_tdisplays3 at ../aqm_sen55_tdisplays3 — T-Display S3 gateway + display + SEN55 sensor
- cyd-aqm at ../cyd-aqm — CYD gateway (original plan, blocked)
