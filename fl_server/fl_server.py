import paho.mqtt.client as mqtt
from dotenv import dotenv_values
import os
import time
import json
import numpy as np

config = dotenv_values(os.path.join(os.path.dirname(__file__), ".env"))
clients = {}
NUM_BATCHES = int(int(config["NN_WEIGHTS_AMT"]) / int(config["BATCH_SIZE"]))
mqtt_client = mqtt.Client("fl-server")


def get_weights():
    for i in range(NUM_BATCHES):
        for client_id in clients:
            mqtt_client.publish(config["MQTT_FROM_SERVER_TOPIC"] + '/' + str(client_id), json.dumps({
                'type': 'get_weights',
                'batch': i
            }))
        time.sleep(1)


def calculate_fed_avg_batches():
    weight_batches = [client['weights'] for client in clients.values()]
    epochs = [client["epochs"] for client in clients.values()]
    return np.average(weight_batches, axis=0, weights=epochs)


def send_weights(weight_batches):
    for i in range(NUM_BATCHES):
        mqtt_client.publish(config["MQTT_FROM_SERVER_TOPIC"], json.dumps({
            'type': 'update_weights',
            'batch': i,
            'weights': weight_batches[i].tolist()
        }))


def on_connect(client, userdata, flags, rc):
    print("[SERVER] Connected with result code " + str(rc))
    client.subscribe(config["MQTT_TO_SERVER_TOPIC"] + '/+')


def on_message(client, userdata, msg):
    print("[SERVER] Message received: ", msg.topic + " " + str(msg.payload))
    payload = json.loads(msg.payload)
    if payload['type'] == 'status':
        clients[payload['client_id']] = {'weights': [0] * NUM_BATCHES, 'epochs': payload['epochs']}
    if payload['type'] == 'weights':
        clients[payload['client_id']]['weights'][payload['batch']] = payload['weights']


mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

mqtt_client.connect(config["MQTT_HOST"], int(config["MQTT_PORT"]), 60)
mqtt_client.loop_start()

while True:
    input("Pres enter to start FL process...")
    mqtt_client.publish(config["MQTT_FROM_SERVER_TOPIC"], json.dumps({'type': 'get_status'}))
    time.sleep(1)
    get_weights()
    weights = calculate_fed_avg_batches()
    send_weights(weights)
