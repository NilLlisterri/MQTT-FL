from node_manager import NodeManager
from fl_server import FLServer
from dotenv import dotenv_values
import os
from constants import *


def main():
    flServer = FLServer()
    nodeManager = NodeManager()

    input("Press enter to start...")
    nodeManager.startExperiment(flServer)

if __name__ == "__main__":
    main()