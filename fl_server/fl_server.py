import paho.mqtt.client as mqtt
from dotenv import dotenv_values
import os
import time
import json
import numpy as np

config = dotenv_values(os.path.join(os.path.dirname(__file__), ".env"))
NUM_BATCHES = int(int(config["NN_WEIGHTS_AMT"]) / int(config["BATCH_SIZE"]))
clients = {22240: {"weights": [0] * NUM_BATCHES}} # This should be empty, and clients should be discovered by the server
mqtt_client = mqtt.Client("fl-server")

GET_WEIGHTS_COMMAND = 0
GET_STATUS_COMMAND = 1
SEND_WEIGHTS_COMMAND = 2

def get_weights():
    for i in range(NUM_BATCHES):
        for client_id in clients:
            message = buildCommand({"flCommand": GET_WEIGHTS_COMMAND, 'data': {"batch": i}})
            print(f"[SERVER] Requesting batch {i} of weights from: {client_id}")
            mqtt_client.publish(config["MQTT_FROM_SERVER_TOPIC"] + '/' + str(client_id), message)
        time.sleep(15)


def calculate_fed_avg_batches():
    weight_batches = [client['weights'] for client in clients.values()]
    epochs = [client["epochs"] for client in clients.values()]
    return np.average(weight_batches, axis=0, weights=epochs)


def send_weights(weight_batches):
    for i in range(NUM_BATCHES):
        mqtt_client.publish(config["MQTT_FROM_SERVER_TOPIC"], json.dumps({
            'flCommand': 'update_weights',
            'batch': i,
            'weights': weight_batches[i].tolist()
        }))


def on_connect(client, userdata, flags, rc):
    print("[SERVER] Connected with result code " + str(rc))
    client.subscribe(config["MQTT_TO_SERVER_TOPIC"] + '/+')

def on_message(client, userdata, msg):
    print("[SERVER] Message received: ", msg.topic + " " + str(msg.payload))
    payload = json.loads(msg.payload)['data']
    client = payload['addrSrc']
    if payload['flCommand'] == 'status':
        clients[client] = {'weights': [0] * NUM_BATCHES, 'epochs': payload['data']['epochs']}
    if payload['flCommand'] == SEND_WEIGHTS_COMMAND:
        batch = payload['data']['batch']
        clients[client]['weights'][batch] = payload['data']['weights']


def buildCommand(data):
    message = {
        "data": {
            "appPortDst": 13,
            "appPortSrc": 13,
            "addrDst": 22240,
            "client_id": 22240,
        }
    }
    message["data"].update(data)
    return json.dumps(message)

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

mqtt_client.connect(config["MQTT_HOST"], int(config["MQTT_PORT"]), 60)
mqtt_client.loop_start()

while True:
    # input("Pres enter to start FL process...")
    # mqtt_client.publish(config["MQTT_FROM_SERVER_TOPIC"], buildCommand({"command": GET_STATUS_COMMAND}))
    time.sleep(15)
    get_weights()
    # weights = calculate_fed_avg_batches()
    # send_weights(weights)
