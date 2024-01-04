# Getting Started  

Create the `secrets.h` file in the `node` directory to define the WiFi SSID and password:
```
#define WIFI_SSID "XXX"
#define WIFI_PASSWORD "XXX"
```

Upload the code to the nodes:  
`pio run --target upload -e ttgo-lora32-v1 --upload-port com3; pio run --target upload -e ttgo-lora32-v1 --upload-port com20;`  

Install the required Python libraries:  
`pip install -r host/requirements.txt`  

Start the MQTT server:  
`docker-compose up -d`  

Start the experiment:  
`python host/experiment.py`  

# Project structure  
## Host  
For the host machine there are 3 important files under the host folder.  
* `fl_server.py` connects to the MQTT server and communicates with the nodes to organize the FL rounds.  
* `node_manager.py` communicates with the nodes via Serial port, and is used to create reproductble experiments. It handles the initialization of the nodes NN, sends training samples to the devices, etc.  
* `experiment.py` is used to automate the experiments. It uses both previous files to start the FL server and then execute the experiment.  

## Nodes  
* `main.cpp` is the main file and it initializes the LoRaMesher library and listens to the Serial port for the experiment commands.  
* `fl.h` and `fl.cpp` contain the KWS application, encapsulated in a LoRaMesher app format.  
* `NN.h` and `NN.cpp` are the files that contain the Neural Network.  