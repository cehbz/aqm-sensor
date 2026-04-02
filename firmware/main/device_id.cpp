#include "device_id.hpp"

#include <cstdio>
#include <cstdint>
#include "esp_mac.h"

static char s_device_id[7];   // "a1b2c3\0"
static char s_mac_hex[13];    // "a1b2c3d4e5f6\0"

void device_id_init()
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);

    std::snprintf(s_mac_hex, sizeof(s_mac_hex), "%02x%02x%02x%02x%02x%02x",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    std::snprintf(s_device_id, sizeof(s_device_id), "%02x%02x%02x",
                  mac[3], mac[4], mac[5]);
}

const char *device_id_get()
{
    return s_device_id;
}

const char *device_id_mac_hex()
{
    return s_mac_hex;
}
