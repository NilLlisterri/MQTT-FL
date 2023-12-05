#include <Arduino.h>

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

void getScaleRange(float &a, float &b) {
    a = 0;
    b = std::pow(2, SCALED_WEIGHT_BITS)-1;
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
