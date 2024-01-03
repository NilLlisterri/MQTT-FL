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
import utils

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
        self.clients = {}
        self.connectToMQTTServer()

    def connectToMQTTServer(self):
        self.mqttClient = mqtt.Client("fl-server")
        self.mqttClient.on_connect = self.on_connect
        self.mqttClient.on_message = self.on_message
        self.mqttClient.connect(MQTT_HOST, int(MQTT_PORT), 60)
        self.mqttClient.loop_start()

    def getWeights(self):
        for i in trange(MQTT_NUM_BATCHES, desc="[SERVER] Getting weights from clients"):
            for clientId in self.clients:
                missingWeights = lambda: len(self.clients[clientId]['weights'][i]) == 0
                lastRequestTime = None
                while missingWeights():
                    if lastRequestTime == None or time.time() - lastRequestTime >= 10:
                        print(f"[SERVER] Requesting batch {i} of weights from: {clientId}")
                        message = self.buildCommand(clientId, {
                            "flCommand": GET_WEIGHTS_COMMAND, 
                            'data': {"batch": i, "batch_size": MQTT_WEIGHTS_BATCH_SIZE}
                        })
                        self.mqttClient.publish(MQTT_FROM_SERVER_TOPIC + '/' + str(clientId), message)
                        lastRequestTime = time.time()
                    else:
                        time.sleep(0.1)

    def calculateFedAvgBatches(self):
        # TODO: Ugly and unintuitive, improve
        all_weights = [([w for ws in client['weights'] for w in ws]) for client in self.clients.values()]
        epochs = [client["epochs"] for client in self.clients.values()]
        weight_averages = np.average(all_weights, axis=0, weights=epochs)
        return np.array_split(weight_averages, MQTT_WEIGHTS_BATCH_SIZE)

    def sendWeights(self, weight_batches):
        # TODO: Verify the weights arrived correctly, ack?
        for i in trange(MQTT_NUM_BATCHES, desc="Sending weights to clients"):
            min, max, weights = utils.scaleWeights(weight_batches[i].tolist(), SCALED_WEIGHT_BITS)
            #weights = weight_batches[i].tolist()
            self.mqttClient.publish(MQTT_FROM_SERVER_TOPIC, json.dumps({
                'flCommand': UPDATE_WEIGHTS_COMMAND,
                'data': {
                    'batch': i,
                    'batch_size': MQTT_WEIGHTS_BATCH_SIZE,
                    'weights': weights,
                    'min': min, 
                    'max': max
                }
            }))
            time.sleep(1)

    def on_connect(self, client, userdata, flags, rc):
        print("[SERVER] Connected with result code " + str(rc))
        client.subscribe(MQTT_TO_SERVER_TOPIC + '/+')

    def on_message(self, client, userdata, msg):
        print("[SERVER] Message received on topic ", msg.topic + ": " + msg.payload.decode('utf8'))
        jsonMessage = json.loads(msg.payload)
        if not 'data' in jsonMessage:
            print("[SERVER] Payload not found in data, this is an ERROR!")
            return
        payload = jsonMessage['data']
        client = payload['addrSrc']
        if client not in self.clients:
            self.addClient(client)
        if payload['flCommand'] == SEND_STATUS_COMMAND:
            self.clients[client]['epochs'] = payload['data']['epochs']
        if payload['flCommand'] == SEND_WEIGHTS_COMMAND:
            self.weightsReceived(client, payload['data'])

    def weightsReceived(self, client, data):
        batch = data['batch']
        min_w = data['min']
        max_w = data['max']
        self.clients[client]['weights'][batch] = [utils.deScaleWeight(min_w, max_w, w, SCALED_WEIGHT_BITS) for w in data['weights']]

    def addClient(self, id):
        print(f"New client added with id {id}")
        self.clients[id] = {"weights": None, 'epochs': None}
        self.resetClientWeights(id)

    def resetClientWeights(self, id = None):
        if id == None:
            for clientId in self.clients: 
                self.clients[clientId]["weights"] = [[] for _ in range(MQTT_NUM_BATCHES)]
        else: 
            self.clients[id]["weights"] = [[] for _ in range(MQTT_NUM_BATCHES)]

    def buildCommand(self, clientId, data):
        message = {
            "data": {
                "appPortDst": FL_APP_PORT,
                "appPortSrc": FL_APP_PORT,
                "addrDst": clientId,
                "client_id": clientId,
            }
        }
        message["data"].update(data)
        return json.dumps(message)

    def startFL(self):
        self.resetClientWeights()
        self.getWeights()
        weights = self.calculateFedAvgBatches()
        self.sendWeights(weights)

