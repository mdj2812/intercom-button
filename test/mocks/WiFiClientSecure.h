#pragma once
#include <WiFi.h>

inline bool _secure_client_insecure = false;

class WiFiClientSecure : public WiFiClient {
public:
    void setInsecure() {
        _secure_client_insecure = true;
    }
};
