#include <Arduino.h>
#include "config.h"

// LoRaChat
#include "ArduinoLog.h"
#include "helpers/helper.h"
#include "message/messageManager.h"
#include "display.h"
#include "loramesh/loraMeshService.h"
#include "mqtt/mqttService.h"
#include "wifi/wifiServerService.h"
#include "fl_app/fl.h"

Fl& flApp = Fl::getInstance();
WiFiServerService& wiFiService = WiFiServerService::getInstance();
LoRaMeshService& loraMeshService = LoRaMeshService::getInstance();
MqttService& mqttService = MqttService::getInstance();
MessageManager& manager = MessageManager::getInstance();

void initManager() {
    manager.init();
    Log.verboseln("Manager initialized");
    manager.addMessageService(&loraMeshService);
    Log.verboseln("LoRaMesher service added to manager");
    manager.addMessageService(&wiFiService);
    Log.verboseln("WiFi service added to manager");
    manager.addMessageService(&mqttService);
    Log.verboseln("Mqtt service added to manager");
    manager.addMessageService(&flApp);
    Log.verboseln("FL App added to manager");
    Serial.println(manager.getAvailableCommands());
}

TaskHandle_t display_TaskHandle = NULL;

#define DISPLAY_TASK_DELAY 50          // ms
#define DISPLAY_LINE_TWO_DELAY 10000   // ms
#define DISPLAY_LINE_THREE_DELAY 50000 // ms

void display_Task(void* pvParameters) {
    uint32_t lastLineTwoUpdate = 0;
    uint32_t lastLineThreeUpdate = 0;
    char lineThree[25];
    while (true) {
        if (millis() - lastLineTwoUpdate > DISPLAY_LINE_TWO_DELAY) {
            lastLineTwoUpdate = millis();
            String lineTwo = String(loraMeshService.getLocalAddress()) + " | " + wiFiService.getIP();
            sprintf(lineThree, "Free ram: %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            Screen.changeLineTwo(lineTwo);
            Screen.changeLineThree(lineThree);
        }
        Screen.drawDisplay();
        vTaskDelay(DISPLAY_TASK_DELAY / portTICK_PERIOD_MS);
    }
}

void createUpdateDisplay() {
    int res = xTaskCreate( display_Task, "Display Task", 4096, (void*) 1, 2, &display_TaskHandle);
    if (res != pdPASS) {
        Log.errorln(F("Display Task creation gave error: %d"), res);
    }
}

void initDisplay() {
    Screen.initDisplay();
    createUpdateDisplay();
}

void setup() {
    Serial.begin(115200);

    // LoRaChat
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    initDisplay();
    initManager();
    loraMeshService.initLoraMesherService();
    wiFiService.initWiFi();
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Wait for Wifi
    mqttService.initMqtt(String(loraMeshService.getLocalAddress()));
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
