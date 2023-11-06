#include <Arduino.h>
#include "config.h"


// #include <training_kws_inference.h>
#include "NN.h"

#define SAMPLE_COUNT 16000

bool mixed_precision = true;
typedef uint16_t scaledType;
uint scaled_weights_bits = 16;

short sample[SAMPLE_COUNT];

static bool debug_nn = false;
static NeuralNetwork myNetwork;
uint16_t num_epochs = 0;

// LoRaChat
#include "ArduinoLog.h"
#include "helpers/helper.h"
#include "message/messageManager.h"
#include "display.h"
#include "loramesh/loraMeshService.h"
#include "mqtt/mqttService.h"
#include "wifi/wifiServerService.h"
#include "led/led.h"

Led& led = Led::getInstance();
void initLed() {
    led.init();
}

WiFiServerService& wiFiService = WiFiServerService::getInstance();
void initWiFi() {
    wiFiService.initWiFi();
}

LoRaMeshService& loraMeshService = LoRaMeshService::getInstance();
void initLoRaMesher() {
    loraMeshService.initLoraMesherService();
}

MqttService& mqttService = MqttService::getInstance();
void initMqtt() {
    mqttService.initMqtt(String(loraMeshService.getLocalAddress()));
}

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

    manager.addMessageService(&led);
    Log.verboseln("Led service added to manager");

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

int readInt() {
    byte res[4];
    while(Serial.available() < 4) {}
    for (int n = 0; n < 4; n++) res[n] = Serial.read();
    return *(int *)&res;
}

float readFloat() {
    byte res[4];
    while(Serial.available() < 4) {}
    for (int n = 0; n < 4; n++) res[n] = Serial.read();
    return *(float *)&res;
}

void init_network_model() {
    char startChar;
    do {
        startChar = Serial.read();
        Serial.println("Waiting for new model...");
        delay(1000);
    } while(startChar != 's'); // s -> START

    Serial.println("start");
    int seed = readInt();
    srand(seed);
    float learningRate = readFloat();
    float momentum = readFloat();

    myNetwork.initialize(learningRate, momentum);

    char* myHiddenWeights = (char*) myNetwork.get_HiddenWeights();
    for (uint16_t i = 0; i < (InputNodes+1) * NN_HIDDEN_NEURONS; ++i) {
        Serial.write('n');
        while(Serial.available() < 4) {}
        for (int n = 0; n < 4; n++) {
            myHiddenWeights[i*4+n] = Serial.read();
        }
    }

    char* myOutputWeights = (char*) myNetwork.get_OutputWeights();
    for (uint16_t i = 0; i < (NN_HIDDEN_NEURONS+1) * OutputNodes; ++i) {
        Serial.write('n');
        while(Serial.available() < 4) {}
        for (int n = 0; n < 4; n++) {
            myOutputWeights[i*4+n] = Serial.read();
        }
    }

    Serial.println("Received new model.");
}


void setup() {
    Serial.begin(115200);

    // LoRaChat
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    initDisplay();
    initManager();
    initLoRaMesher();
    initWiFi();
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    initMqtt();
    initLed();
    
    init_network_model();
}

/*
static scaledType microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
    return 0;
}
*/
void train(int nb, bool only_forward) {
    float myTarget[OutputNodes] = {0};
    myTarget[nb-1] = 1.f; // button 1 -> {1,0,0};  button 2 -> {0,1,0};  button 3 -> {0,0,1}

    // FORWARD
    float forward_error = myNetwork.forward(sample, myTarget);
    
    // BACKWARD
    if (!only_forward) {
        myNetwork.backward(sample, myTarget);
        ++num_epochs;
    }

    // Info to plot
    Serial.println("graph");

    // Print outputs
    float* output = myNetwork.get_output();
    for (size_t i = 0; i < OutputNodes; i++) {
        Serial.println(output[i]);
        // ei_printf_float(output[i]);
        Serial.print(" ");
    }
    Serial.print("\n");

    // Print error
    // ei_printf_float(forward_error);
    Serial.println(forward_error);
    Serial.print("\n");
    Serial.println(num_epochs, DEC);
    char* myError = (char*) &forward_error;
    Serial.write(myError, sizeof(float));
    Serial.println(nb, DEC);
}

void receiveSampleAndTrain() {
    Serial.println("ok");

    while(Serial.available() < 1) {}
    uint8_t num_button = Serial.read();
    Serial.println("Button " + String(num_button));

    while(Serial.available() < 1) {}
    bool only_forward = Serial.read() == 1;
    Serial.println("Only forward " + String(only_forward));
    
    byte ref[2];
    for(int i = 0; i < SAMPLE_COUNT; i++) {
        while(Serial.available() < 2) {}
        Serial.readBytes(ref, 2);
        sample[i] = (ref[1] << 8) | ref[0];
    }
    Serial.println("Sample received for button " + String(num_button));
    train(num_button, only_forward);
}

void getScaleRange(float &a, float &b) {
    a = 0;
    b = std::pow(2, scaled_weights_bits)-1;
} 

scaledType scaleWeight(float min_w, float max_w, float weight) {
    float a, b;
    getScaleRange(a, b);
    return round(a + ( (weight-min_w)*(b-a) / (max_w-min_w) ));
}

float deScaleWeight(float min_w, float max_w, scaledType weight) {
    float a, b;
    getScaleRange(a, b);
    return min_w + ( (weight-a)*(max_w-min_w) / (b-a) );
}

void startFL() {
    Serial.write('<');
    while(!Serial.available()) {}
    if (Serial.read() == 's') {
        Serial.println("start");
        Serial.println(num_epochs);
        num_epochs = 0;

        // Find min and max weights
        float* float_hidden_weights = myNetwork.get_HiddenWeights();
        float* float_output_weights = myNetwork.get_OutputWeights();
        float min_weight = float_hidden_weights[0];
        float max_weight = float_hidden_weights[0];
        for(uint i = 0; i < hiddenWeightsAmt; i++) {
            if (min_weight > float_hidden_weights[i]) min_weight = float_hidden_weights[i];
            if (max_weight < float_hidden_weights[i]) max_weight = float_hidden_weights[i];
        }
        for(uint i = 0; i < outputWeightsAmt; i++) {
            if (min_weight > float_output_weights[i]) min_weight = float_output_weights[i];
            if (max_weight < float_output_weights[i]) max_weight = float_output_weights[i];
        }

        Serial.write((byte *) &min_weight, sizeof(float));
        Serial.write((byte *) &max_weight, sizeof(float));

        // Sending hidden layer
        char* hidden_weights = (char*) myNetwork.get_HiddenWeights();
        for (uint16_t i = 0; i < hiddenWeightsAmt; ++i) {
            if (mixed_precision) {
                scaledType weight = scaleWeight(min_weight, max_weight, float_hidden_weights[i]);
                Serial.write((byte*) &weight, sizeof(scaledType));
            } else {
                Serial.write((byte*) &float_hidden_weights[i], sizeof(float)); // debug
            }
        }

        // Sending output layer
        char* output_weights = (char*) myNetwork.get_OutputWeights();
        for (uint16_t i = 0; i < outputWeightsAmt; ++i) {
            if (mixed_precision) {
                scaledType weight = scaleWeight(min_weight, max_weight, float_output_weights[i]);
                Serial.write((byte*) &weight, sizeof(scaledType));
            } else {
                Serial.write((byte*) &float_output_weights[i], sizeof(float)); // debug
            }
        }

        while (!Serial.available()) {
            delay(100);
        }

        float min_received_w = readFloat();
        float max_received_w = readFloat();

        // Receiving hidden layer
        for (uint16_t i = 0; i < hiddenWeightsAmt; ++i) {
            if (mixed_precision) {
                scaledType val;
                Serial.readBytes((byte*) &val, sizeof(scaledType));
                float_hidden_weights[i] = deScaleWeight(min_received_w, max_received_w, val);
            } else {
                while(Serial.available() < 4) {}
                for (int n = 0; n < 4; n++) {
                    hidden_weights[i*4+n] = Serial.read();
                }
            }
        }
        // Receiving output layer
        for (uint16_t i = 0; i < outputWeightsAmt; ++i) {
            if (mixed_precision) {
                scaledType val;
                Serial.readBytes((byte*) &val, sizeof(scaledType));
                float_output_weights[i] = deScaleWeight(min_received_w, max_received_w, val);
            } else {
                while(Serial.available() < 4) {}
                for (int n = 0; n < 4; n++) {
                    output_weights[i*4+n] = Serial.read();
                }
            }
        }
        Serial.println("Model received");
    }
}


void loop() {
    if (Serial.available()) {
        char read = Serial.read();
        if (read == '>') {
            startFL();
        } else if (read == 't') {
            receiveSampleAndTrain();
        } else { // Error
            while(true) {
                Serial.println("Unknown command " + String(read));
                delay(100);
            }
        }
    }
}

