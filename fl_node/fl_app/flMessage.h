#pragma once

#include <Arduino.h>
#include <ArduinoLog.h>
#include "message/dataMessage.h"

enum FlCommand: uint8_t {
    GetWeights = 0,
};

class FlMessage: public DataMessageGeneric {
public:
    FlCommand flCommand;

    void serialize(JsonObject& doc) {
        // Call the base class serialize function
        ((DataMessageGeneric*) (this))->serialize(doc);

        // Add the derived class data to the JSON object
        doc["flCommand"] = "state";
    }

    void deserialize(JsonObject& doc) {
        // Call the base class deserialize function
        ((DataMessageGeneric*) (this))->deserialize(doc);

        // Add the derived class data to the JSON object
        flCommand = doc["flCommand"];
    }
};
#pragma pack()
