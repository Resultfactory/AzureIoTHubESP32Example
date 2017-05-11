# AzureIoTHubESP32Example
Example project for the ESP32 connected to the Azure IoT hub

To add this repository to esp-idf as a component change directory to your esp-idf directory.
``git submodule add https://github.com/Resultfactory/AzureIoTHubESP32.git components/AzureIoTHub``   

To build this project make sure you are able to build the esp-idf/examples.
Run ``make menuconfig``
In menuconfig set the wifi ssid, password and the Azure IoT Hub connection String.
Run ``make flash``