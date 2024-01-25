#include <training_kws_inference.h>
#include "fl.h"
#include "mfcc.cpp"

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
    network = new NeuralNetwork<NN_HIDDEN_NEURONS>();
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
    for (uint16_t i = 0; i < network->getHiddenWeightsAmt(); ++i) {
        Serial.write('n');
        while(Serial.available() < 4) {}
        for (int n = 0; n < 4; n++) myHiddenWeights[i*4+n] = Serial.read();
    }

    char* myOutputWeights = (char*) network->get_OutputWeights();
    for (uint16_t i = 0; i < network->getOutputWeightsAmt(); ++i) {
        Serial.write('n');
        while(Serial.available() < 4) {}
        for (int n = 0; n < 4; n++) myOutputWeights[i*4+n] = Serial.read();
    }

    Serial.println("Received new model");
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
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
        Serial.println("ERR: Failed to get features ("+String(r)+")");
        return;
    }

    float myTarget[network->OutputNodes] = {0};
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
    for (size_t i = 0; i < network->OutputNodes; i++) {
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


// start of commands for FL
String Fl::sendWeights(DynamicJsonDocument requestData) {
    ESP_LOGI("FL", "Sending weights for batch %d (batch size: %d)...", requestData["batch"], requestData["batch_size"]);
    FlMessage* message = getFlMessage(FlCommand::SendWeights, 1);

    DynamicJsonDocument data = DynamicJsonDocument(1024);

    data["batch"] = requestData["batch"];
    JsonArray weights = data.createNestedArray("weights");

    
    if (weights.isNull()) {
        ESP_LOGE("FL", "Could not create the weights nested array");
        delete message;
        return "Weights could not be sent";
    }

    float* hidden_weights = network->get_HiddenWeights();
    float* output_weights = network->get_OutputWeights();

    float min = hidden_weights[0];
    float max = hidden_weights[0];
    for(uint i = 0; i < network->getHiddenWeightsAmt(); i++) {
        if (min > hidden_weights[i]) min = hidden_weights[i];
        if (max < hidden_weights[i]) max = hidden_weights[i];
    }
    for(uint i = 0; i < network->getOutputWeightsAmt(); i++) {
        if (min > output_weights[i]) min = output_weights[i];
        if (max < output_weights[i]) max = output_weights[i];
    }

    data["min"] = min;
    data["max"] = max;

    int sent_at = data["sent_at"];
    // TODO: Calculate RTT
    int rtt = 0;

    int bit_width = this->getBitWidth(rtt);
    data["bit_width"] = bit_width;

    int start = int(requestData["batch"]) * int(requestData["batch_size"]);
    for (int current = start; current - start < requestData["batch_size"]; current++) {
        if (current >= network->getHiddenWeightsAmt() + network->getOutputWeightsAmt()) break;
        float weight = (current < network->getHiddenWeightsAmt()) ? hidden_weights[current] : output_weights[current - network->getOutputWeightsAmt()];
        bool success = weights.add(scaleWeight(min, max, weight, bit_width));
        if (!success) {
            ESP_LOGE("FL", "Could not add all of the weights");
            delete message;
            return "Weights could not be sent";
        }
    }


    serializeJson(data, message->data);

    MessageManager::getInstance().sendMessage(messagePort::MqttPort, (DataMessage*) message);
    delete message;

    return "Fl wait";
}

int Fl::getBitWidth(int rtt) {
    return SCALED_WEIGHT_BITS;
}

String Fl::updateWeights(DynamicJsonDocument requestData) {
    ESP_LOGI("FL", "Updat weights for batch %d (batch size: %d)...", requestData["batch"], requestData["batch_size"]);

    float* hidden_weights = network->get_HiddenWeights();
    float* output_weights = network->get_OutputWeights();
    int start = int(requestData["batch"]) * int(requestData["batch_size"]);
    int weights_count = requestData["weights"].size();

    for (int current = start; current < start + weights_count; current++) {
        float weight = deScaleWeight(requestData["min"], requestData["max"], requestData["weights"][current], requestData["bit_width"]);
        if (current < network->getHiddenWeightsAmt()) hidden_weights[current] = weight;
        else output_weights[current - network->getHiddenWeightsAmt()] = weight;
    }

    return "Weights updated";
}

String Fl::sendStatus() {
    ESP_LOGI("FL", "Sending status...");
    FlMessage* message = getFlMessage(FlCommand::SendStatus, 1);
    message->data = "{\"epochs\": " + String(this->num_epochs) +"}";
    MessageManager::getInstance().sendMessage(messagePort::MqttPort, (DataMessage*) message);
    delete message;
    return "Fl wait";
}

String Fl::flGetWeights(FlMessage* flMessage) {
    sendWeights(flMessage->getData());
    return "Fl wait";
}

String Fl::flUpdateWeights(FlMessage* flMessage) {
    updateWeights(flMessage->getData());
    return "Fl wait";
}

String Fl::flGetStatus(FlMessage* flMessage) {
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
            ESP_LOGV("FL", "Get weights command received");
            flGetWeights(flMessage);
            break;
        case FlCommand::GetStatus:
            ESP_LOGV("FL", "Get status command received");
            flGetStatus(flMessage);
            break;
        case FlCommand::UpdateWeights:
            ESP_LOGV("FL", "Update weights command received");
            flUpdateWeights(flMessage);
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
    if (dataObj.isNull()) {
        ESP_LOGE("FL", "Could not add the data object to the JSON message");
    }

    flMessage->serialize(dataObj);

    String json;
    serializeJson(doc, json);
    return json;
}
