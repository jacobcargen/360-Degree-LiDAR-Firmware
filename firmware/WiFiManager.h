#ifndef WIFI_H
#define WIFI_H

#include <WiFi.h>

class WiFiManager {
    public:
        WiFiManager(const char* ssid, const char* password);

        void begin();
        bool isConnected();

    private:
        const char* _ssid;
        const char* _password;
};

#endif
