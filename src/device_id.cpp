#include "device_id.h"

#include <Arduino.h>
#include <WiFi.h>

const char* DeviceId::mac() {
    static String s_mac;
    s_mac = WiFi.macAddress();
    s_mac.toUpperCase();
    return s_mac.c_str();
}
