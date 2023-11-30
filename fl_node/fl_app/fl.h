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

    void initFl(bool receive_model);

    void receiveSampleAndTrain();
    void train(int nb, bool only_forward);
    //void startFl();
    void receiveModel();

    String sendWeights(DynamicJsonDocument data);
    String sendStatus();

    String flGetWeights(FlMessage* flMessage);
    String flGetStatus(FlMessage* flMessage);

    String getJSON(DataMessage* message);

    FlCommandService* flCommandService = new FlCommandService();

    DataMessage* getDataMessage(JsonObject data);

    FlMessage* getFlMessage(FlCommand command, uint16_t dst);

    void processReceivedMessage(messagePort port, DataMessage* message);

private:
    NeuralNetwork* myNetwork;
    uint16_t num_epochs = 0;

    Fl(): MessageService(FL_APP_PORT, "Fl") {
        commandService = flCommandService;
    };

    void getJSONDataObject(JsonObject& doc, FlMessage* fldataMessage);
};
