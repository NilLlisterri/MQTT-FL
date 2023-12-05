#include <training_kws_inference.h>
#include "fl.h"
#include "mfcc.cpp"

typedef uint16_t scaledType;
#define SCALED_WEIGHT_BITS 16
#include "../utils.h"

#define SAMPLE_COUNT 16000

// short sample[SAMPLE_COUNT];

/** Audio buffers, pointers and selectors */
typedef struct {
    int16_t buffer[EI_CLASSIFIER_RAW_SAMPLE_COUNT];
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;
static inference_t inference;


void Fl::initFl(bool receive_model) {
    Log.notice(F("Initializing fl" CR));
    network = new NeuralNetwork();
    if (receive_model) {
        receiveModel();
    }
}

void Fl::receiveModel() {
    char startChar;

    Serial.println("start");
    int seed = readInt();
    srand(seed);
    float learningRate = readFloat();
    float momentum = readFloat();

    network->initialize(learningRate, momentum);

    char* myHiddenWeights = (char*) network->get_HiddenWeights();
    for (uint16_t i = 0; i < (InputNodes+1) * NN_HIDDEN_NEURONS; ++i) {
        Serial.write('n');
        while(Serial.available() < 4) {}
        for (int n = 0; n < 4; n++) myHiddenWeights[i*4+n] = Serial.read();
    }

    char* myOutputWeights = (char*) network->get_OutputWeights();
    for (uint16_t i = 0; i < (NN_HIDDEN_NEURONS+1) * OutputNodes; ++i) {
        Serial.write('n');
        while(Serial.available() < 4) {}
        for (int n = 0; n < 4; n++) myOutputWeights[i*4+n] = Serial.read();
    }

    Serial.println("Received new model");
}

static scaledType microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
    return 0;
}

void Fl::train(int nb, bool only_forward) {
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei::matrix_t features_matrix(1, EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);

    EI_IMPULSE_ERROR r = get_one_second_features(&signal, &features_matrix, false);
    if (r != EI_IMPULSE_OK) {
        Serial.println("ERR: Failed to get features ("+String(r));
        return;
    }

    float myTarget[OutputNodes] = {0};
    myTarget[nb-1] = 1.f; // button 1 -> {1,0,0};  button 2 -> {0,1,0};  button 3 -> {0,0,1}

    // FORWARD
    float forward_error = network->forward(features_matrix.buffer, myTarget);

    // BACKWARD
    if (!only_forward) {
        network->backward(features_matrix.buffer, myTarget);
        ++this->num_epochs;
    }

    // Info to plot
    Serial.println("graph");

    // Print outputs
    float* output = network->get_output();
    for (size_t i = 0; i < OutputNodes; i++) {
        Serial.print(output[i]);
        // ei_printf_float(output[i]);
        Serial.print(" ");
    }
    Serial.println();

    // Print error
    // ei_printf_float(forward_error);
    Serial.println(forward_error);

    Serial.println(this->num_epochs, DEC);
    char* myError = (char*) &forward_error;
    Serial.write(myError, sizeof(float));
    Serial.println(nb, DEC);
}

void Fl::receiveSampleAndTrain() {
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
        inference.buffer[i] = 0;
        inference.buffer[i] = (ref[1] << 8) | ref[0];
    }
    Serial.println("Sample received for button " + String(num_button));
    train(num_button, only_forward);
}

/*
void Fl::startFl() {
    Serial.write('<');
    while(!Serial.available()) {}
    if (Serial.read() == 's') {
        Serial.println("start");
        Serial.println(this->num_epochs);
        this->num_epochs = 0;

        // Find min and max weights
        float* float_hidden_weights = network->get_HiddenWeights();
        float* float_output_weights = network->get_OutputWeights();
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
        char* hidden_weights = (char*) network->get_HiddenWeights();
        for (uint16_t i = 0; i < hiddenWeightsAmt; ++i) {
            scaledType weight = scaleWeight(min_weight, max_weight, float_hidden_weights[i]);
            Serial.write((byte*) &weight, sizeof(scaledType));
        }

        // Sending output layer
        char* output_weights = (char*) network->get_OutputWeights();
        for (uint16_t i = 0; i < outputWeightsAmt; ++i) {
            scaledType weight = scaleWeight(min_weight, max_weight, float_output_weights[i]);
            Serial.write((byte*) &weight, sizeof(scaledType));
        }

        while (!Serial.available()) delay(100);

        float min_received_w = readFloat();
        float max_received_w = readFloat();

        // Receiving hidden layer
        for (uint16_t i = 0; i < hiddenWeightsAmt; ++i) {
            scaledType val;
            Serial.readBytes((byte*) &val, sizeof(scaledType));
            float_hidden_weights[i] = deScaleWeight(min_received_w, max_received_w, val);
        }
        // Receiving output layer
        for (uint16_t i = 0; i < outputWeightsAmt; ++i) {
            scaledType val;
            Serial.readBytes((byte*) &val, sizeof(scaledType));
            float_output_weights[i] = deScaleWeight(min_received_w, max_received_w, val);
        }
        Serial.println("Model received");
    }
}
*/

// start of commands for FL
String Fl::sendWeights(DynamicJsonDocument requestData) {
    ESP_LOGI("FL", "Sending weights for batch %d (batch size: %d)...", requestData["batch"], requestData["batch_size"]);
    FlMessage* message = getFlMessage(FlCommand::SendWeights, 1);

    message->data["batch"] = requestData["batch"];
    JsonArray weights = message->data.createNestedArray("weights");

    float* hidden_weights = network->get_HiddenWeights();
    float* output_weights = network->get_OutputWeights();

    // float values[requestData["batch_size"]];
    int start = int(requestData["batch"]) * int(requestData["batch_size"]);
    for (int current = start; current - start < requestData["batch_size"]; current++) {
        if (current >= hiddenWeightsAmt + outputWeightsAmt) break;
        
        if (current < hiddenWeightsAmt) weights.add(hidden_weights[current]);
        else weights.add(output_weights[current - hiddenWeightsAmt]);
    }

    // for(int i = 0; i < requestData["batch_size"]; i++) {
    //     bool res = weights.add(i); // TODO: Send real weights
    // }

    // serializeJson(message->data, Serial);

    MessageManager::getInstance().sendMessage(messagePort::MqttPort, (DataMessage*) message);
    delete message;

    return "Fl wait";
}

String Fl::sendStatus() {
    ESP_LOGI("FL", "Sending status...");
    FlMessage* message = getFlMessage(FlCommand::SendStatus, 1);

    message->data["epochs"] = this->num_epochs;
    MessageManager::getInstance().sendMessage(messagePort::MqttPort, (DataMessage*) message);
    delete message;
    return "Fl wait";
}

String Fl::flGetWeights(FlMessage* flMessage) {
    // if (dst == LoraMesher::getInstance().getLocalAddress())
    // int batch = flMessage->data["batch"];
    sendWeights(flMessage->data);
    return "Fl wait";
}

String Fl::flGetStatus(FlMessage* flMessage) {
    // if (dst == LoraMesher::getInstance().getLocalAddress())
    return sendStatus();
    return "Fl wait";
}

DataMessage* Fl::getDataMessage(JsonObject data) {
    FlMessage* flMessage = new FlMessage();
    flMessage->deserialize(data);
    flMessage->messageSize = sizeof(FlMessage) - sizeof(DataMessageGeneric);
    return ((DataMessage*) flMessage);
}

FlMessage* Fl::getFlMessage(FlCommand command, uint16_t dst) {
    FlMessage* flMessage = new FlMessage();

    flMessage->messageSize = sizeof(FlMessage) - sizeof(DataMessageGeneric);
    flMessage->flCommand = command;

    flMessage->appPortSrc = static_cast<appPort>((uint8_t) FL_APP_PORT);
    flMessage->appPortDst = static_cast<appPort>((uint8_t) FL_APP_PORT);

    flMessage->addrSrc = LoraMesher::getInstance().getLocalAddress();
    flMessage->addrDst = dst;

    return flMessage;
}

void Fl::processReceivedMessage(messagePort port, DataMessage* message) {
    FlMessage* flMessage = (FlMessage*) message;
    ESP_LOGV("FL", "Message received, command: %d", flMessage->flCommand);
    switch (flMessage->flCommand) {
        case FlCommand::GetWeights:
            ESP_LOGV("FL", "Éxito");
            flGetWeights(flMessage);
            break;
        case FlCommand::GetStatus:
            ESP_LOGV("FL", "Éxito 2");
            flGetStatus(flMessage);
            break;
        default:
            break;
    }
}

String Fl::getJSON(DataMessage* message) {
    FlMessage* flMessage = (FlMessage*) message;
    DynamicJsonDocument doc(FlMessage::MAX_JSON_SIZE);
    JsonObject jsonObj = doc.to<JsonObject>();
    JsonObject dataObj = jsonObj.createNestedObject("data");

    getJSONDataObject(dataObj, flMessage);

    String json;
    serializeJson(doc, json);
    return json;
}

void Fl::getJSONDataObject(JsonObject& doc, FlMessage* flMessage) {
    flMessage->serialize(doc);
}
