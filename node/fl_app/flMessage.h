#pragma once

#include <Arduino.h>
#include <ArduinoLog.h>
#include "message/dataMessage.h"

enum FlCommand: uint8_t {
    GetWeights = 0,
    GetStatus = 1,
    SendWeights = 2,
    SendStatus = 3,
    UpdateWeights = 4
};

class FlMessage: public DataMessageGeneric {
public:
    static const int MAX_JSON_SIZE = 2048;
    FlCommand flCommand;
    String data;

    void serialize(JsonObject& doc) {
        // Call the base class serialize function
        ((DataMessageGeneric*) (this))->serialize(doc);

        // Add the derived class data to the JSON object
        doc["flCommand"] = this->flCommand;
        doc["data"] = this->getData();
    }

    void deserialize(JsonObject& doc) {
        // Call the base class deserialize function
        ((DataMessageGeneric*) (this))->deserialize(doc);

        // Add the derived class data to the JSON object
        this->flCommand = doc["flCommand"];
        String extraData = doc["data"];
        this->data = extraData;
    }

    DynamicJsonDocument getData() {
        DynamicJsonDocument doc = DynamicJsonDocument(MAX_JSON_SIZE);
        deserializeJson(doc, this->data);
        return doc;
    }
};
#pragma pack()
