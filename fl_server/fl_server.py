import paho.mqtt.client as mqtt
from dotenv import dotenv_values
import os
import time
import json
import numpy as np
from tqdm import tqdm
from tqdm import trange

config = dotenv_values(os.path.join(os.path.dirname(__file__), ".env"))
NUM_BATCHES = int(int(config["NN_WEIGHTS_AMT"]) / int(config["BATCH_SIZE"]))
clients = {} # This should be empty, and clients should be discovered by the server
mqttClient = mqtt.Client("fl-server")

GET_WEIGHTS_COMMAND = 0
GET_STATUS_COMMAND = 1
SEND_WEIGHTS_COMMAND = 2
SEND_STATUS_COMMAND = 3
UPDATE_WEIGHTS_COMMAND = 4

def getWeights():
    for i in trange(NUM_BATCHES, desc="Getting weights from clients"):
        for client_id in clients:
            message = buildCommand({"flCommand": GET_WEIGHTS_COMMAND, 'data': {"batch": i, "batch_size": int(config["BATCH_SIZE"])}})
            print(f"[SERVER] Requesting batch {i} of weights from: {client_id}")
            mqttClient.publish(config["MQTT_FROM_SERVER_TOPIC"] + '/' + str(client_id), message)
        time.sleep(3)


def calculate_fed_avg_batches():
    weight_batches = [client['weights'] for client in clients.values()]
    epochs = [client["epochs"] for client in clients.values()]
    return np.average(weight_batches, axis=0, weights=epochs)


def sendWeights(weight_batches):
    for i in trange(NUM_BATCHES, desc="Sending weights to clients"):
        mqttClient.publish(config["MQTT_FROM_SERVER_TOPIC"], json.dumps({
            'flCommand': UPDATE_WEIGHTS_COMMAND,
            'data': {
                'batch': i,
                'weights': weight_batches[i].tolist()
            }
        }))
        time.sleep(2)


def on_connect(client, userdata, flags, rc):
    print("[SERVER] Connected with result code " + str(rc))
    client.subscribe(config["MQTT_TO_SERVER_TOPIC"] + '/+')

def on_message(client, userdata, msg):
    print("[SERVER] Message received on topic ", msg.topic + ": " + msg.payload.decode('utf8'))
    payload = json.loads(msg.payload)['data']
    client = payload['addrSrc']
    if client not in clients:
        addClient(client)
    if payload['flCommand'] == SEND_STATUS_COMMAND:
        # clients[client]['epochs'] = payload['data']['epochs'] // TODO: This will be used when nodes can be trained via serial
        clients[client]['epochs'] = 1
    if payload['flCommand'] == SEND_WEIGHTS_COMMAND:
        batch = payload['data']['batch']
        clients[client]['weights'][batch] = payload['data']['weights']

def addClient(id):
    print(f"New client added with id {id}")
    clients[id] = {"weights": None, 'epochs': None}
    resetClientWeights(id)

def resetClientWeights(id = None):
    if id == None:
        for clientId in clients: 
            clients[clientId]["weights"] = [[] for _ in range(NUM_BATCHES)]
    else: 
        clients[id]["weights"] = [[] for _ in range(NUM_BATCHES)]

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

def startFL():
    resetClientWeights()
    getWeights()
    weights = calculate_fed_avg_batches()
    sendWeights(weights)

mqttClient.on_connect = on_connect
mqttClient.on_message = on_message

mqttClient.connect(config["MQTT_HOST"], int(config["MQTT_PORT"]), 60)
mqttClient.loop_start()

while True:
    input("Press enter to start the FL process...")
    startFL()
