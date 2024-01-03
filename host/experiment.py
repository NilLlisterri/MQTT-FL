from node_manager import NodeManager
from fl_server import FLServer
from dotenv import dotenv_values
import os
import time
from constants import *
import serial


def main():
    devices = [
        serial.Serial("com20", 115200, timeout=2),
        serial.Serial("com3", 115200, timeout=2)
    ]

    flServer = FLServer()
    nodeManager = NodeManager(devices)

    print("Waiting for clients to begin experiment")
    while(len(flServer.clients) < len(devices)):
        time.sleep(0.5)

    nodeManager.startExperiment(flServer)

if __name__ == "__main__":
    main()