#include <Arduino.h>
#include <ArduinoLog.h>
#include "message/messageService.h"
#include "message/messageManager.h"
#include "flMessage.h"
#include "flCommandService.h"
#include "settings.h"
#include "NN/NN.h"

class Fl: public MessageService {
public:
    static Fl& getInstance() {
        static Fl instance;
        return instance;
    }

    void initFl();

    void receiveSampleAndTrain();
    void train(int nb, bool only_forward);
    void startFl();
    void receiveModel();

    String flSendWeights();

    String flGetWeights(uint16_t dst);
    
    void createAndSendFl();

    String getJSON(DataMessage* message);

    FlCommandService* flCommandService = new FlCommandService();

    DataMessage* getDataMessage(JsonObject data);

    DataMessage* getFlMessage(FlCommand command, uint16_t dst);

    void processReceivedMessage(messagePort port, DataMessage* message);

private:
    NeuralNetwork* myNetwork;

    Fl(): MessageService(FL_APP_PORT, "Fl") {
        commandService = flCommandService;
    };

    void getJSONDataObject(JsonObject& doc, FlMessage* fldataMessage);
};
