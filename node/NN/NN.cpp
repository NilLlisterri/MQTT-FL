// This code is inspired by http://robotics.hobbizine.com/arduinoann.html

#include <arduino.h>
#include "NN.h"
#include <math.h>

float sigmoid(float x) {
    return 1.0 / (1.0 + exp(-x));
}

void NeuralNetwork::initialize(float LearningRate, float Momentum) {
    this->LearningRate = LearningRate;
    this->Momentum = Momentum;

    this->activation = &sigmoid;
}

float NeuralNetwork::forward(const float Input[], const float Target[]){
    float error = 0;

    // Compute hidden layer activations
    for (int i = 0; i < NN_HIDDEN_NEURONS; i++) {
        float Accum = HiddenWeights[InputNodes*NN_HIDDEN_NEURONS + i];
        for (int j = 0; j < InputNodes; j++) {
            Accum += Input[j] * HiddenWeights[j*NN_HIDDEN_NEURONS + i];
        }
        Hidden[i] = this->activation(Accum);
    }
    
    // Compute output layer activations and calculate errors
    for (int i = 0; i < OutputNodes; i++) {
        float Accum = OutputWeights[NN_HIDDEN_NEURONS*OutputNodes + i];
        for (int j = 0; j < NN_HIDDEN_NEURONS; j++) {
            Accum += Hidden[j] * OutputWeights[j*OutputNodes + i];
        }
        Output[i] = this->activation(Accum);
        error += (1.0/OutputNodes) * (Target[i] - Output[i]) * (Target[i] - Output[i]);
    }
    
    return error;
}

float NeuralNetwork::backward(const float Input[], const float Target[]){
    float error = 0;

    // Forward
    // Compute hidden layer activations
    for (int i = 0; i < NN_HIDDEN_NEURONS; i++) {
        float Accum = HiddenWeights[InputNodes*NN_HIDDEN_NEURONS + i];
        for (int j = 0; j < InputNodes; j++) {
            Accum += Input[j] * HiddenWeights[j*NN_HIDDEN_NEURONS + i];
        }
        Hidden[i] = this->activation(Accum);
    }

    // Compute output layer activations and calculate errors
    for (int i = 0; i < OutputNodes; i++) {
        float Accum = OutputWeights[NN_HIDDEN_NEURONS*OutputNodes + i];
        for (int j = 0; j < NN_HIDDEN_NEURONS; j++) {
            Accum += Hidden[j] * OutputWeights[j*OutputNodes + i];
        }
        Output[i] = this->activation(Accum);
        OutputDelta[i] = (Target[i] - Output[i]) * Output[i] * (1.0 - Output[i]);
        error += (1.0/OutputNodes) * (Target[i] - Output[i]) * (Target[i] - Output[i]);
    }
    // End forward

    // Backward
    // Backpropagate errors to hidden layer
    for(int i = 0 ; i < NN_HIDDEN_NEURONS ; i++ ) {    
        float Accum = 0.0 ;
        for(int j = 0 ; j < OutputNodes ; j++ ) {
            Accum += OutputWeights[i*OutputNodes + j] * OutputDelta[j] ;
        }
        HiddenDelta[i] = Accum * Hidden[i] * (1.0 - Hidden[i]) ;
    }

    // Update Inner-->Hidden Weights
    for(int i = 0 ; i < NN_HIDDEN_NEURONS ; i++ ) {     
        ChangeHiddenWeights[InputNodes*NN_HIDDEN_NEURONS + i] = LearningRate * HiddenDelta[i] + Momentum * ChangeHiddenWeights[InputNodes*NN_HIDDEN_NEURONS + i] ;
        HiddenWeights[InputNodes*NN_HIDDEN_NEURONS + i] += ChangeHiddenWeights[InputNodes*NN_HIDDEN_NEURONS + i] ;
        for(int j = 0 ; j < InputNodes ; j++ ) { 
            ChangeHiddenWeights[j*NN_HIDDEN_NEURONS + i] = LearningRate * Input[j] * HiddenDelta[i] + Momentum * ChangeHiddenWeights[j*NN_HIDDEN_NEURONS + i];
            HiddenWeights[j*NN_HIDDEN_NEURONS + i] += ChangeHiddenWeights[j*NN_HIDDEN_NEURONS + i] ;
        }
    }

    // Update Hidden-->Output Weights
    for(int i = 0 ; i < OutputNodes ; i ++ ) {    
        ChangeOutputWeights[NN_HIDDEN_NEURONS*OutputNodes + i] = LearningRate * OutputDelta[i] + Momentum * ChangeOutputWeights[NN_HIDDEN_NEURONS*OutputNodes + i] ;
        OutputWeights[NN_HIDDEN_NEURONS*OutputNodes + i] += ChangeOutputWeights[NN_HIDDEN_NEURONS*OutputNodes + i] ;
        for(int j = 0 ; j < NN_HIDDEN_NEURONS ; j++ ) {
            ChangeOutputWeights[j*OutputNodes + i] = LearningRate * Hidden[j] * OutputDelta[i] + Momentum * ChangeOutputWeights[j*OutputNodes + i] ;
            OutputWeights[j*OutputNodes + i] += ChangeOutputWeights[j*OutputNodes + i] ;
        }
    }

    return error;
}


float* NeuralNetwork::get_output(){
    return Output;
}

float* NeuralNetwork::get_HiddenWeights(){
    return HiddenWeights;
}

float* NeuralNetwork::get_OutputWeights(){
    return OutputWeights;
}

float NeuralNetwork::get_error(){
    return Error;
}