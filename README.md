# ESP8266 Home Status Dashboard

Firmware for a device designed to sit on a disk and provide status details
of various Home IoT things.

Currently hooks into:
* Cognian Syncromesh
* Homeassistant

Hardware consists of:
* ESP8266 via NodeMCU Devkit v1.0
* 4 pin 128x64 OLED display module
* DHT11 Temperature & Humidity sensor
* 4 switches
* 2 LEDs

## credentials-secret.h

Must define:
```
const char *ssid;
const char *password;

const char *mqttUser;
const char *mqttPass;

const char *mqttBroker;
const int mqttPort;
```

## Build and flash
Load into the Arduino IDE, or use the makefile.

The make file depends on the ESP8266 makefile from https://github.com/plerup/makeEspArduino.
Check out the above project and update the local makefile to point to `makeEspArduino.mk` from
that repository.

Then:
`make flash`

### HW test firmware
```
cd testFw
make -f ../Makefile flash
```
