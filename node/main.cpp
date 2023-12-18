#include <Arduino.h>
#include "config.h"
#include "LoRaChat.cpp"
#include "fl_app/fl.h"

Fl& flApp = Fl::getInstance();
LC lc;
TaskHandle_t sendStatusTaskHandle = NULL;

void sendStatusToServer(void* pvParameters) {
    while(true) {
        flApp.sendStatus();
        vTaskDelay(SEND_STATUS_DELAY_SECONDS * 1000 / portTICK_PERIOD_MS); // 10s
    }
}

void setup() {
    Serial.begin(115200);

    lc.init(String(WIFI_SSID), String(WIFI_PASSWORD), String(MQTT_SERVER), MQTT_PORT, String(MQTT_USERNAME), String(MQTT_PASSWORD));
    lc.registerApplication(&flApp);
    // lc.printCommands();

    flApp.initFl(false);

    // TODO: This task randomly crashes
    // int res = xTaskCreate(sendStatusToServer, "SEND_STATUS_TASK", 1024, (void*) 1, 2, &sendStatusTaskHandle);
    // if (res != pdPASS) {
    //     while(true) {
    //         ESP_LOGE("MAIN", "Send status task creation error: %d", res);
    //         delay(1000);
    //     }
    // }
}

uint32_t lastStatusBroadcast = millis();
uint32_t lastDisplayUpdate = millis();
void loop() {
    if (millis() - lastStatusBroadcast > 5000) { // 5s
        lastStatusBroadcast = millis();
        flApp.sendStatus();
    }

    if (millis() - lastDisplayUpdate > 500) { // 5s
        lastDisplayUpdate = millis();
        lc.displayStatus();
    }

    if (Serial.available()) {
        char read = Serial.read();
        if (read == 't') flApp.receiveSampleAndTrain();
        else if (read == 's') flApp.receiveModel();
        else { // Error
            while(true) {
                Serial.println("Unknown command " + String(read));
                delay(1000);
            }
        }
    }
}
