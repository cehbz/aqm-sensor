# Air Quality Monitor Fan Controller System

## Project Goals

Build a distributed air quality monitoring system for a home with five DIY air filters. Each filter consists of a XIAOMI filter refill, a 12V PC fan mounted on a plastic disk, and a 12V power supply with 5.5×2.1mm barrel connector.

**Primary objectives:**
- Sample PM2.5 levels near filters
- Automatically adjust fan speed based on air quality readings
- Display current air quality via RGB LED color coding
- Integrate with Home Assistant for centralized monitoring and control

**Secondary objectives:**
- Modular design allowing flexible sensor/controller placement
- Stemma QT (I2C) expansion capability on both modules
- Minimal cost per unit
- PCBs panelized on 50×50mm boards for manufacturing efficiency

## Architecture

Split into two module types rather than a single integrated unit:

### Sensor Module
Monitors PM2.5 and reports to Home Assistant. Powered via USB-C.

### Controller Module  
Receives commands from Home Assistant to control fan speed. Inline between existing 12V wall wart and fan—no modification to existing filter hardware required.

**Rationale for split design:**
- Greater installation flexibility (sensor can be placed optimally for air sampling, controller stays inline with power)
- Sensors are expensive (~$15); may not need 1:1 ratio with filters
- Each module is simpler and more reusable
- Home Assistant handles control logic centrally

## Hardware Design

### Sensor Module

**Components:**

| Ref | Part | Value/Type | Package | Notes |
|-----|------|------------|---------|-------|
| U1 | ESP32-C3 SuperMini | — | 22.5×18.5mm module | USB-C power |
| U2 | PMS5003 | PM2.5 sensor | — | ~2 year lifespan continuous |
| J1 | JST-ZH | 5-pin | 1.25mm pitch | PMS5003 connector |
| J2 | JST-SH | 4-pin | 1.0mm pitch | Stemma QT / I2C expansion |
| LED1 | RGB LED | 5050 | 6-pin independent | AQI status indicator |
| R1 | Resistor | 150Ω | 0805 | Red LED current limit |
| R2 | Resistor | 100Ω | 0805 | Green LED current limit |
| R3 | Resistor | 100Ω | 0805 | Blue LED current limit |

**Netlist:**

```
# Power (from USB-C via SuperMini)
U1.5V           → J1.1 (PMS VCC), R1.1, R2.1, R3.1
U1.3V3          → J2.1 (Stemma 3V3)
U1.GND          → J1.2 (PMS GND), J2.4 (Stemma GND)

# PMS5003 UART
J1.3 (PMS TX)   → U1.GPIO20 (RX)
J1.4 (PMS RX)   → NC (or U1.GPIO21 if commands needed)
J1.5 (PMS SET)  → NC (or 3V3 to keep awake)

# RGB LED (5050, independent anodes/cathodes, active-low drive)
R1.2            → LED1.1 (R anode)
R2.2            → LED1.2 (G anode)
R3.2            → LED1.3 (B anode)
LED1.6 (R K)    → U1.GPIO3
LED1.5 (G K)    → U1.GPIO4
LED1.4 (B K)    → U1.GPIO5

# I2C Expansion (Stemma QT)
J2.2 (SDA)      → U1.GPIO6
J2.3 (SCL)      → U1.GPIO7
```

**Estimated cost:** ~$18.50/unit (PMS5003 dominates at ~$15)

---

### Controller Module

**Components:**

| Ref | Part | Value/Type | Package | Notes |
|-----|------|------------|---------|-------|
| U1 | ESP32-C3 SuperMini | — | 22.5×18.5mm module | |
| U2 | Mini-360 | Buck converter | 17×11mm module | 12V→5V, pre-adjusted |
| Q1 | AO3400A | N-MOSFET | SOT-23 | Low-side fan switch |
| D1 | SS14 | Schottky diode | SMA | Flyback protection |
| J1 | Barrel jack | 5.5×2.1mm | PCB mount | 12V input |
| J2 | Barrel plug pigtail | 5.5×2.1mm | Wire lead | 12V output to fan |
| J3 | JST-SH | 4-pin | 1.0mm pitch | Stemma QT / I2C expansion |
| LED1 | RGB LED | 5050 | 6-pin independent | Status indicator |
| C1 | Capacitor | 100µF 16V | 6.3×5.4mm electrolytic | Input bulk |
| C2 | Capacitor | 10µF | 0805 | 5V bulk |
| C3 | Capacitor | 100nF | 0805 | 5V decoupling |
| R1 | Resistor | 100Ω | 0805 | MOSFET gate series |
| R2 | Resistor | 10kΩ | 0805 | Gate pull-down |
| R3 | Resistor | 150Ω | 0805 | Red LED |
| R4 | Resistor | 100Ω | 0805 | Green LED |
| R5 | Resistor | 100Ω | 0805 | Blue LED |

**Netlist:**

```
# 12V Input
J1.+ (12V)      → C1.+, U2.IN+, D1.K, J2.+ (FAN+)
J1.- (GND)      → C1.-, U2.IN-

# 5V Rail (from Mini-360)
U2.OUT+         → C2.1, C3.1, U1.5V, R3.1, R4.1, R5.1
U2.OUT-         → C2.2, C3.2, U1.GND

# Stemma QT (I2C expansion)
U1.3V3          → J3.1 (3V3)
U1.GND          → J3.4 (GND)
U1.GPIO8        → J3.2 (SDA)
U1.GPIO9        → J3.3 (SCL)

# Fan Driver (low-side switch)
U1.GPIO6        → R1.1
R1.2            → Q1.G, R2.1
R2.2            → GND
Q1.S            → GND
Q1.D            → D1.A, J2.- (FAN-)

# RGB LED (5050, independent anodes/cathodes, active-low drive)
R3.2            → LED1.1 (R anode)
R4.2            → LED1.2 (G anode)
R5.2            → LED1.3 (B anode)
LED1.6 (R K)    → U1.GPIO3
LED1.5 (G K)    → U1.GPIO4
LED1.4 (B K)    → U1.GPIO5
```

**Note:** PWM uses GPIO6 to avoid GPIO8 strapping pin conflict. I2C moved to GPIO8/9 (boot strapping not affected by I2C pull-ups).

**Estimated cost:** ~$4.30/unit

---

## Design Decisions

### Split vs Integrated Modules
Originally designed as a single unit. Split approach chosen for installation flexibility and cost optimization (fewer expensive sensors needed).

### ESP32-C3 SuperMini vs ESP8266 D1 Mini
ESP32-C3 selected for USB-C, BLE capability, modern toolchain, and native USB. Marginal cost difference.

### PMS5003 vs Sharp GP2Y1010AU0F
PMS5003 selected for digital interface and proven accuracy. Sharp sensor is cheaper but requires analog signal conditioning and calibration.

### Mini-360 vs Integrated Buck (AP63205)
Mini-360 module chosen for simplicity—drop-in, pre-adjusted, no layout concerns. AP63205 would be smaller and cleaner but requires careful PCB layout and additional passives.

### Low-Side vs High-Side Switching
Low-side N-MOSFET switch for simplicity. GPIO can drive gate directly without level shifting since source is at ground.

### RGB LED Current Limiting
150Ω for red (2.0V Vf), 100Ω for green/blue (3.1V Vf). Targets ~19mA per channel at 5V. Green is inherently brighter; can reduce via PWM in software if needed for color balance.

### GPIO Assignments

**Sensor module:**
- GPIO20: UART RX (PMS5003 TX)
- GPIO3/4/5: LED R/G/B cathodes
- GPIO6/7: I2C SDA/SCL (Stemma)

**Controller module:**
- GPIO6: PWM fan control
- GPIO3/4/5: LED R/G/B cathodes
- GPIO8/9: I2C SDA/SCL (Stemma)

GPIO8 avoided for PWM due to strapping pin requirement (must be HIGH at boot). Safe for I2C since external pull-ups hold it HIGH.

### Trace Widths
- 12V traces: 1mm (0.5A with 2× headroom)
- Standard signal traces: 0.25mm
- Ground: plane with multiple vias near Q1 and Mini-360

---

## Hardware Testing Plan

### Sensor Module

**Power-on test:**
1. Connect USB-C, verify 5V present on PMS5003 VCC pin
2. Verify 3.3V on Stemma connector
3. Check current draw: ~100mA idle, ~150mA with PMS5003 active

**PMS5003 communication:**
1. Connect USB, open serial monitor at 9600 baud
2. Verify periodic data frames from sensor (starts automatically)
3. Parse PM2.5 values, confirm reasonable readings (indoor typically 5-50 µg/m³)

**LED test:**
1. Drive each GPIO (3, 4, 5) LOW individually
2. Verify corresponding color illuminates
3. Test PWM dimming on each channel
4. Test combined colors (yellow = R+G, cyan = G+B, magenta = R+B, white = all)

**I2C bus (if Stemma populated):**
1. Connect known I2C device (e.g., SHT40, BMP280)
2. Run I2C scanner, verify device detected at expected address
3. Read sensor values

---

### Controller Module

**Power-on test (no fan connected):**
1. Connect 12V to barrel jack
2. Verify 5V on Mini-360 output (±0.1V)
3. Verify ESP32-C3 boots (onboard LED or serial output)
4. Measure current draw: ~50mA idle

**5V rail stability:**
1. Monitor 5V with scope during WiFi connection (current spike)
2. Verify no dropout below 4.5V

**Fan driver test (resistive load first):**
1. Connect 12V bulb or power resistor (~24Ω, 6W) to fan output
2. Set GPIO6 HIGH, verify load receives 12V
3. Set GPIO6 LOW, verify load off
4. PWM at 50%, verify ~6V average on multimeter (or scope)

**Fan driver test (actual fan):**
1. Connect 12V fan to barrel plug pigtail
2. PWM sweep 0-100%, verify smooth speed control
3. Rapid on/off cycling, verify no glitches or resets (flyback diode working)

**LED test:**
Same as sensor module.

**Thermal check:**
1. Run fan at 100% for 10 minutes
2. Check Mini-360 and MOSFET temperature (should be <50°C)

**I2C bus (if Stemma populated):**
Same as sensor module.

---

## Software Overview

### Framework
ESPHome recommended for both modules. Native support for:
- PMS5003 (`pmsx003` sensor component)
- PWM output (`ledc` component)
- RGB LED (`rgb` light component with PWM outputs)
- Home Assistant integration (automatic discovery)

Alternative: Custom firmware with Arduino or ESP-IDF if more control needed.

### Sensor Module Firmware

**Core functionality:**
1. Initialize UART for PMS5003 (GPIO20, 9600 baud)
2. Parse PM2.5 readings from sensor data frames
3. Publish PM2.5 to Home Assistant via WiFi
4. Set LED color based on AQI thresholds

**LED color mapping (US EPA AQI breakpoints for PM2.5):**

| PM2.5 (µg/m³) | AQI Category | LED Color |
|---------------|--------------|-----------|
| 0-12.0 | Good | Green |
| 12.1-35.4 | Moderate | Yellow |
| 35.5-55.4 | Unhealthy (sensitive) | Orange |
| 55.5-150.4 | Unhealthy | Red |
| 150.5-250.4 | Very Unhealthy | Purple |
| 250.5+ | Hazardous | Flashing red |

**Optional features:**
- PMS5003 duty cycling (sleep/wake) to extend sensor life
- Smoothing/averaging of readings
- I2C sensor support via Stemma (temperature, humidity)

---

### Controller Module Firmware

**Core functionality:**
1. Connect to Home Assistant
2. Expose fan as a controllable entity (speed 0-100%)
3. Receive speed commands from HA
4. Output PWM on GPIO6 to control fan
5. Set LED color to indicate current state

**PWM configuration:**
- Frequency: 25kHz (inaudible, standard for PC fans)
- Resolution: 8-bit (256 steps) sufficient
- Duty cycle: 0% = off, 100% = full speed

**LED states:**

| State | LED Color |
|-------|-----------|
| Idle (fan off) | Off or dim blue |
| Fan running | Green (brightness ∝ speed) |
| Error/disconnected | Red flashing |

**Optional features:**
- Local fallback mode if HA unavailable (fixed speed or last commanded)
- Soft start (ramp PWM to avoid inrush)
- I2C sensor input for local closed-loop control

---

### Home Assistant Integration

**Automations (configured in HA, not on device):**

1. **Basic threshold control:**
   - IF PM2.5 > 35 THEN set nearby fan to 100%
   - IF PM2.5 < 12 THEN set fan to 0%
   - Linear interpolation between thresholds

2. **Zone-based control:**
   - Multiple sensors report to HA
   - HA calculates which fans to activate based on sensor locations
   - Allows N sensors to control M fans

3. **Schedule integration:**
   - Reduce fan speed at night
   - Boost during cooking hours

4. **Dashboard:**
   - Per-room PM2.5 graphs
   - Fan status indicators
   - Manual override controls

---

### ESPHome Example Configurations

**Sensor module (sensor.yaml):**

```yaml
esphome:
  name: aqm-sensor-1
  platform: ESP32
  board: esp32-c3-devkitm-1

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret api_key

uart:
  rx_pin: GPIO20
  baud_rate: 9600

sensor:
  - platform: pmsx003
    type: PMSX003
    pm_2_5:
      name: "PM2.5"
      id: pm25
      on_value:
        then:
          - script.execute: update_led

light:
  - platform: rgb
    name: "AQI LED"
    id: aqi_led
    red: pwm_red
    green: pwm_green
    blue: pwm_blue

output:
  - platform: ledc
    pin: GPIO3
    id: pwm_red
    inverted: true
  - platform: ledc
    pin: GPIO4
    id: pwm_green
    inverted: true
  - platform: ledc
    pin: GPIO5
    id: pwm_blue
    inverted: true

script:
  - id: update_led
    then:
      - if:
          condition:
            lambda: 'return id(pm25).state <= 12.0;'
          then:
            - light.turn_on:
                id: aqi_led
                red: 0%
                green: 100%
                blue: 0%
      - if:
          condition:
            lambda: 'return id(pm25).state > 12.0 && id(pm25).state <= 35.4;'
          then:
            - light.turn_on:
                id: aqi_led
                red: 100%
                green: 100%
                blue: 0%
      - if:
          condition:
            lambda: 'return id(pm25).state > 35.4 && id(pm25).state <= 55.4;'
          then:
            - light.turn_on:
                id: aqi_led
                red: 100%
                green: 50%
                blue: 0%
      - if:
          condition:
            lambda: 'return id(pm25).state > 55.4;'
          then:
            - light.turn_on:
                id: aqi_led
                red: 100%
                green: 0%
                blue: 0%
```

**Controller module (controller.yaml):**

```yaml
esphome:
  name: aqm-controller-1
  platform: ESP32
  board: esp32-c3-devkitm-1

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret api_key

output:
  - platform: ledc
    pin: GPIO6
    id: fan_pwm
    frequency: 25000Hz

  - platform: ledc
    pin: GPIO3
    id: pwm_red
    inverted: true
  - platform: ledc
    pin: GPIO4
    id: pwm_green
    inverted: true
  - platform: ledc
    pin: GPIO5
    id: pwm_blue
    inverted: true

fan:
  - platform: speed
    name: "Air Filter Fan"
    output: fan_pwm

light:
  - platform: rgb
    name: "Controller LED"
    red: pwm_red
    green: pwm_green
    blue: pwm_blue
```

---

## Open Issues

1. **PMS5003 lifespan:** ~2 years continuous operation. Consider duty cycling in firmware (wake, sample, sleep) to extend life.

2. **Fan minimum speed:** Some 12V fans won't spin below ~30% PWM. May need per-fan calibration in HA.

3. **LED brightness balance:** Green is ~3× brighter than red/blue at equal current. Software PWM compensation may be needed for accurate color mixing.

4. **GPIO9 strapping (controller I2C SCL):** GPIO9 is a boot strapping pin. I2C pull-up should keep it HIGH which is correct for normal boot. Verify during testing.
