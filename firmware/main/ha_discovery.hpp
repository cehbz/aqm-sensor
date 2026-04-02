#pragma once

/// Publish MQTT Discovery config for all sensor entities.
/// Call on every MQTT connect (including reconnects).
void ha_discovery_publish(const char *device_id);
