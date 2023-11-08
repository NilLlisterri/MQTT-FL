#include "config.h"

static const int InputNodes = 650;
static const int OutputNodes = 4;
static const float InitialWeightMax = 0.5;

typedef unsigned int uint;
static const uint hiddenWeightsAmt = (InputNodes + 1) * NN_HIDDEN_NEURONS;
static const uint outputWeightsAmt = (NN_HIDDEN_NEURONS + 1) * OutputNodes;

class NeuralNetwork {
    public:
        void initialize(float LearningRate, float Momentum);
        float forward(const short Input[], const float Target[]);
        float backward(const short Input[], const float Target[]);
        float* get_output();
        float* get_HiddenWeights();
        float* get_OutputWeights();
        float get_error();
        
    private:
        float Hidden[NN_HIDDEN_NEURONS] = {};
        float Output[OutputNodes] = {};
        float HiddenWeights[(InputNodes+1) * NN_HIDDEN_NEURONS] = {};
        float OutputWeights[(NN_HIDDEN_NEURONS+1) * OutputNodes] = {};
        float HiddenDelta[NN_HIDDEN_NEURONS] = {};
        float OutputDelta[OutputNodes] = {};
        float ChangeHiddenWeights[(InputNodes+1) * NN_HIDDEN_NEURONS] = {};
        float ChangeOutputWeights[(NN_HIDDEN_NEURONS+1) * OutputNodes] = {};
        float (*activation)(float);
        float Error;
        float LearningRate;
        float Momentum;
};
