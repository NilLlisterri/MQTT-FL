#pragma once

#include <Arduino.h>
#include <ArduinoLog.h>
#include "message/dataMessage.h"

enum FlCommand: uint8_t {
    GetWeights = 0,
    GetStatus = 1,
    SendWeights = 2,
    SendStatus = 3
};

class FlMessage: public DataMessageGeneric {
public:
    static const int MAX_JSON_SIZE = 2048;
    FlCommand flCommand;
    DynamicJsonDocument data = DynamicJsonDocument(MAX_JSON_SIZE);

    void serialize(JsonObject& doc) {
        // Call the base class serialize function
        ((DataMessageGeneric*) (this))->serialize(doc);

        // Add the derived class data to the JSON object
        doc["flCommand"] = flCommand;
        doc["data"] = this->data;
    }

    void deserialize(JsonObject& doc) {
        // Call the base class deserialize function
        ((DataMessageGeneric*) (this))->deserialize(doc);

        // Add the derived class data to the JSON object
        flCommand = doc["flCommand"];
        data = doc["data"];
    }
};
#pragma pack()
