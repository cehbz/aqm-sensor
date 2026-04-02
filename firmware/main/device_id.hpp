#pragma once

/// Initialise device identity from the base MAC address.
/// Call once from app_main before anything that needs the ID.
void device_id_init();

/// 6-char hex string from the last 3 MAC bytes, e.g. "a1b2c3".
const char *device_id_get();

/// Full 12-char hex MAC, e.g. "a1b2c3d4e5f6".
const char *device_id_mac_hex();
