#include <Arduino.h>
#include "config.h"
#include "LoRaChat.cpp"
#include "fl_app/fl.h"

Fl& flApp = Fl::getInstance();
LC lc;

void setup() {
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);

    lc.init(String(WIFI_SSID), String(WIFI_PASSWORD), String(MQTT_SERVER), MQTT_PORT, String(MQTT_USERNAME), String(MQTT_PASSWORD));
    lc.registerApplication(&flApp);
    lc.printCommands();

    flApp.initFl();
}

void loop() {
    if (Serial.available()) {
        char read = Serial.read();
        if (read == '>') flApp.startFl();
        else if (read == 't') flApp.receiveSampleAndTrain();
        else { // Error
            while(true) {
                Serial.println("Unknown command " + String(read));
                delay(1000);
            }
        }
    } else {
        delay(100);
    }
}
