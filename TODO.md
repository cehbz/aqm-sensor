# PMS5003 Sensor TODO

## ESP-WIFI-MESH integration

- [ ] Validate ESP-WIFI-MESH interop between T-Display S3 (root) and C3 SuperMini (node) — shared task with fan controller
- [ ] Extract `status_led.hpp/cpp` — SK6812 semantic status functions
- [ ] Replace WiFi + MQTT transport with mesh node firmware — sensor sends PMS5003 readings to root
- [ ] Move HA discovery + MQTT publishing to gateway (T-Display S3)
- [ ] Verify: 9 PMS5003 entities + RSSI appear in HA via mesh → gateway → MQTT
