# AQM Sensor Board - TODO for Next Revision

## Critical

- [ ] **Fix ESP32-C3 SuperMini footprint rotation** — Footprint is rotated 180 degrees. The antenna cutout is next to pin 5 but should be next to pin 21. All pin assignments are mirrored. Currently works by inserting the module flipped, but antenna doesn't benefit from the keepout zone.

- [ ] **Fix PMS5003 connector (J3) pin order** — Two issues:
  1. VCC (pin 1) and GND (pin 2) are swapped vs PMS5003 standard pinout. J3 pin 1 = GND, J3 pin 2 = +5V, but PMS5003 expects pin 1 = VCC, pin 2 = GND.
  2. The PMS5003 cable reverses pin order (pin 1 ↔ pin 8 mirror). Layout the PCB to match the actual cables that ship with the sensor.

## Recommended

- [ ] **Add I2C pull-up resistors** — Add 4.7k pull-ups on SDA and SCL to +3.3V for QWIIC connector reliability.

- [ ] **Add test points** — TP_5V, TP_3V3, TP_GND for power verification. TP_PMS_TX, TP_PMS_RX for UART debugging.

- [ ] **Add silkscreen labels** — Pin 1 indicator and GND/5V labels on J3. Board name and revision number. Polarity marking on C4.

- [ ] **Add more GND vias** — 2-3 additional vias near J3 and D1 for better return path.

- [ ] **Check passive footprints** — Verify 1206 parts on hand vs 0805. Consider standardizing on whichever is in stock.

## Nice to Have

- [ ] **Add 10uF ceramic cap on 3.3V rail** — Helps with WiFi TX current transients.

- [ ] **Verify C4 footprint** — PCB has CP_Elec_6.3x7.7, confirm schematic matches.

## Confirmed Working

- SK6812 (RGB variant) works at 3.3V VDD with WS2812 protocol (24-bit GRB)
- WiFi connects at reduced TX power (8.5 dBm) even with bad xtal ESP32-C3 variant
- QWIIC connector pinout is correct (GND, 3.3V, SDA, SCL)
