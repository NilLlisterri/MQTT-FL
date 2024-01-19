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

void getScaleRange(float &a, float &b, int bits) {
    a = 0;
    b = std::pow(2, bits)-1;
} 

int scaleWeight(float min_w, float max_w, float weight, int bits) {
    float a, b;
    getScaleRange(a, b, bits);
    return round(a + ( (weight-min_w)*(b-a) / (max_w-min_w) ));
}

float deScaleWeight(float min_w, float max_w, int weight, int bits) {
    float a, b;
    getScaleRange(a, b, bits);
    return min_w + ( (weight-a)*(max_w-min_w) / (b-a) );
}
