import paho.mqtt.client as mqtt
from dotenv import dotenv_values
import os
import time
import json
import numpy as np
from tqdm import tqdm
from tqdm import trange
from constants import *
import math

MQTT_HOST = "localhost"
MQTT_PORT = 1883

MQTT_FROM_SERVER_TOPIC = 'from-server'
MQTT_TO_SERVER_TOPIC = 'to-server'

GET_WEIGHTS_COMMAND = 0
GET_STATUS_COMMAND = 1
SEND_WEIGHTS_COMMAND = 2
SEND_STATUS_COMMAND = 3
UPDATE_WEIGHTS_COMMAND = 4

MQTT_NUM_BATCHES = math.ceil((SIZE_HIDDEN_LAYER + SIZE_OUTPUT_LAYER) / MQTT_WEIGHTS_BATCH_SIZE)

class FLServer:

    def __init__(self):
        self.clients = {} # This should be empty, and clients should be discovered by the server
        
        self.connectToMQTTServer()

    def connectToMQTTServer(self):
        self.mqttClient = mqtt.Client("fl-server")
        self.mqttClient.on_connect = self.on_connect
        self.mqttClient.on_message = self.on_message
        self.mqttClient.connect(MQTT_HOST, int(MQTT_PORT), 60)
        self.mqttClient.loop_start()

    def getWeights(self):
        for i in trange(MQTT_NUM_BATCHES, desc="Getting weights from clients"):
            for client_id in self.clients:
                message = self.buildCommand({"flCommand": GET_WEIGHTS_COMMAND, 'data': {"batch": i, "batch_size": MQTT_WEIGHTS_BATCH_SIZE}})
                print(f"[SERVER] Requesting batch {i} of weights from: {client_id}")
                self.mqttClient.publish(MQTT_FROM_SERVER_TOPIC + '/' + str(client_id), message)
            time.sleep(3)


    def calculateFedAvgBatches(self):
        weight_batches = [client['weights'] for client in self.clients.values()]
        epochs = [client["epochs"] for client in self.clients.values()]
        return np.average(weight_batches, axis=0, weights=epochs)


    def sendWeights(self, weight_batches):
        for i in trange(MQTT_NUM_BATCHES, desc="Sending weights to clients"):
            self.mqttClient.publish(MQTT_FROM_SERVER_TOPIC, json.dumps({
                'flCommand': UPDATE_WEIGHTS_COMMAND,
                'data': {
                    'batch': i,
                    'weights': weight_batches[i].tolist()
                }
            }))
            time.sleep(2)


    def on_connect(self, client, userdata, flags, rc):
        print("[SERVER] Connected with result code " + str(rc))
        client.subscribe(MQTT_TO_SERVER_TOPIC + '/+')

    def on_message(self, client, userdata, msg):
        print("[SERVER] Message received on topic ", msg.topic + ": " + msg.payload.decode('utf8'))
        payload = json.loads(msg.payload)['data']
        client = payload['addrSrc']
        if client not in self.clients:
            self.addClient(client)
        if payload['flCommand'] == SEND_STATUS_COMMAND:
            # clients[client]['epochs'] = payload['data']['epochs'] // TODO: This will be used when nodes can be trained via serial
            self.clients[client]['epochs'] = 1
        if payload['flCommand'] == SEND_WEIGHTS_COMMAND:
            batch = payload['data']['batch']
            self.clients[client]['weights'][batch] = payload['data']['weights']

    def addClient(self, id):
        print(f"New client added with id {id}")
        self.clients[id] = {"weights": None, 'epochs': None}
        self.resetClientWeights(id)

    def resetClientWeights(self, id = None):
        if id == None:
            for clientId in self.clients: 
                self.clients[clientId]["weights"] = [[] for _ in range(MQTT_WEIGHTS_BATCH_SIZE)]
        else: 
            self.clients[id]["weights"] = [[] for _ in range(MQTT_WEIGHTS_BATCH_SIZE)]

    def buildCommand(self, data):
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

    def startFL(self):
        self.resetClientWeights()
        self.getWeights()
        weights = self.calculateFedAvgBatches()
        self.sendWeights(weights)

