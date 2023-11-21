import paho.mqtt.client as mqtt
from dotenv import dotenv_values
import os
import time
import json 
import random

client_id = 123

config = dotenv_values(os.path.join(os.path.dirname(__file__), ".env"))
mqtt_client = mqtt.Client("fl-client")

def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    client.subscribe(config["MQTT_FROM_SERVER_TOPIC"]) # Broadcast messages
    client.subscribe(config["MQTT_FROM_SERVER_TOPIC"] + '/' + str(client_id)) # Direct messages

def on_message(client, userdata, msg):
    print("[NODE] Message received: ", msg.topic+" "+str(msg.payload))
    payload = json.loads(msg.payload)

    if payload['command'] == 'get_status':
        mqtt_client.publish(config["MQTT_TO_SERVER_TOPIC"] + '/' + str(client_id), json.dumps({
            'command': 'status',
            'client_id': client_id,
            'epochs': 1
        }))
    elif payload['command'] == 'get_weights':
        mqtt_client.publish(config["MQTT_TO_SERVER_TOPIC"] + '/' + str(client_id), json.dumps({
            'command': 'weights', 
            'client_id': client_id,
            'batch': payload['batch'], 
            'weights': random.sample(range(10, 30), 5)
        })) # Max: 268435455 bytes
    elif payload['command'] == 'update_weights':
        print(f"[NODE] Batch {payload['batch']} of weights updated")

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

mqtt_client.connect(config["MQTT_HOST"], int(config["MQTT_PORT"]), 60)

mqtt_client.loop_start()


while True:
    time.sleep(1)
