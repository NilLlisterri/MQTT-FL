#include "secrets.h"

#define NN_HIDDEN_NEURONS 8

// This should not be necessary
#define LED LED_BUILTIN

// MQTT configuration
#define MQTT_ENABLED
#define MQTT_SERVER "192.168.0.82"
#define MQTT_PORT 1883
#define MQTT_USERNAME "admin"
#define MQTT_PASSWORD "public"

#define FL_APP_PORT 13

#define SEND_STATUS_DELAY_SECONDS 5

#define SAMPLE_SIZE 16000