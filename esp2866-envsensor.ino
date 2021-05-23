#include <ESP8266WiFi.h>

#include <Wire.h>
#include "SSD1306Wire.h"

#include "DHTesp.h"

#include <PubSubClient.h>

#include "utils/string.h"

#include "credentials-secret.h"

/// WIFI ///////////////////////////////////
WiFiClient espClient;

/// MQTT ////////////////////////////////////
String mqttClientId = "ESP8266-";
String temperatureTopic = "esp8266/";
String humidityTopic = "esp8266/";
String willTopic = "esp8266/";
PubSubClient client(espClient);

#define MAX_PAYLOAD_PERIOD_MS (unsigned long)300000

/// HARDWARE //////////////////////////////
#define NUM_SWITCH  4
int switchAddr[NUM_SWITCH] = {D1, D2, D5, D7};//, D6, D2, D5};
bool switchStates[NUM_SWITCH];
bool switchToggle[NUM_SWITCH];
long switchTime[NUM_SWITCH];

#define NUM_LED 1
int ledAddr[NUM_LED] = {D8}; //{D0, D8};
bool ledStates[NUM_LED];

#define DISPLAY_SCL D4
#define DISPLAY_SDA D3

#define DHT11_S D6

DHTesp dht;
int dhtPeriod;

SSD1306Wire display(0x3c, DISPLAY_SDA, DISPLAY_SCL);

unsigned long lastMs = 0;

/// IOT STATE ////////////////////////////////////

bool displayOn = false, ledsOn = false;

unsigned long garageOccupancyTopic = strHash("homeassistant/garageOccupancy");
bool garageOccupancy = false;

unsigned long hassStatusTopic = strHash("homeassistant/status");
bool hassOnline = false;

unsigned long gwStatusTopic = strHash("syncromesh/status/nikosHowson");
bool gwOnline = false;

float humidity = 0.0, temperature = 0.0;
float lastPubTemp = 0, lastPubHumidity = 0;
unsigned long lastPubTempTime = 0, lastPubHumidityTime = 0, lastTelemAccum = 0;


// Things to track and display
// homeassistant/status (online|offline)
// Garage occupancy - better if I just standardised the topic structure so any occupancy works
// door switches once I have them
// Basically we want a boolean indicator for anything with category, name, and state.


bool checkMQTTConnection() {
    int state = client.state();

    if (state != MQTT_CONNECTED) {
        Serial.printf("MQTT status: %d\n", state);

        if (client.connect(mqttClientId.c_str(), mqttUser, mqttPass, willTopic.c_str(), 1, true, "offline")) {
            Serial.printf("MQTT connected\n");
            client.publish(willTopic.c_str(), "online", true);

            client.subscribe("homeassistant/garageOccupancy", 1);
            client.subscribe("homeassistant/status", 1);
            client.subscribe("syncromesh/status/nikosHowson", 1);

        } else {
            Serial.printf("MQTT failed. State: %d\n", client.state());
            return false;
        }
    }

    return true;
}

void mqttSubCallback(const char *topic, byte *payload, unsigned int length) {
    Serial.printf("topic: %s (%d bytes)\n", topic, length);

    unsigned long topicHash = strHash(topic);

    if (topicHash == garageOccupancyTopic) {
        garageOccupancy = strStartsWith((char *)payload, "on") ? true : false; // on or off

    } else if (topicHash == hassStatusTopic) {
        hassOnline = strStartsWith((char *)payload, "on") ? true : false; // online or offline
        Serial.printf("hassOnline: %d\n", hassOnline);

    } else if (topicHash == gwStatusTopic) {
        gwOnline = strStartsWith((char *)payload, "on") ? true : false; // online or offline, both with timestamp
        Serial.printf("gwOnline: %d\n", gwOnline);
    }
}

void updateLeds() {
    for (int i = 0; i < NUM_LED; i++) {
        digitalWrite(ledAddr[i], ledStates[i] ? 0xFF : 0);
    }
}

void setupLeds() {
    for (int i = 0; i < NUM_LED; i++) {
        pinMode(ledAddr[i], OUTPUT);
    }
}

void ICACHE_RAM_ATTR handleSwitchChange() {
    Serial.println("Switch interrupt");
    for (int i = 0; i < NUM_SWITCH; i++) {
        bool curState = digitalRead(switchAddr[i]);
        if (curState != switchStates[i]) {
            // state changed
            if (!curState) {
                // Toggle on release
                switchToggle[i] = !switchToggle[i];
            }
            switchStates[i] = curState;

            Serial.printf("Switch state: %d: %s\n", i, switchStates[i] ? "on" : "off");
            Serial.printf("      toggle: %d: %s\n", i, switchToggle[i] ? "on" : "off");
         }
   }
}

void setupSwitches() {
    for (int i = 0; i < NUM_SWITCH; i++) {
        pinMode(switchAddr[i], INPUT);
        attachInterrupt(digitalPinToInterrupt(switchAddr[i]), handleSwitchChange, CHANGE);
    }
}

void updateDisplay() {
    display.clear();

    if (displayOn) {
        display.setColor(WHITE);

        char tempStr[20], humStr[20];

        sprintf(tempStr, "Temperature : %0.2f", temperature);
        sprintf(humStr, "Humidity    : %0.2f", humidity);

        display.drawString(10, 6, tempStr);
        display.drawString(10, 16, humStr);

        if (garageOccupancy) display.drawString(10, 30, "@ Garage");

        int x = -30;

        if (!ledsOn) display.drawString(x += 40,44, "[!]LED");
        if (!hassOnline) display.drawString(x += 40,44, "[!]hass");
        if (!gwOnline) display.drawString(x += 40,44, "[!]gw");

        display.drawRect(0,0,128,64);
  }

  display.display();
}

void setup() {
    Serial.begin(115200);
    delay(500);

    setupLeds();

    setupSwitches();
    handleSwitchChange();

    WiFi.begin(ssid, password);

    display.init();
    Serial.printf("Display initialised.\n");

    dht.setup(DHT11_S, DHTesp::DHT11);
    Serial.printf("DHT initialised.\n");

    dhtPeriod = dht.getMinimumSamplingPeriod() * 4;
    Serial.printf("DHT minimum sampling period: %d\n", dhtPeriod);

    // Once wifi is connected, the connection is maintained by the library
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nWiFi connected to %s\n", ssid);
    Serial.println(WiFi.localIP());
    Serial.println(String(WiFi.macAddress()));

    mqttClientId += String(WiFi.macAddress());
    temperatureTopic += String(WiFi.macAddress()) + "/temperatureC";
    humidityTopic += String(WiFi.macAddress()) + "/humidityRelative";
    willTopic += String(WiFi.macAddress()) + "/status";

    Serial.printf("Connecting to MQTT broker %s\n", mqttBroker);

    client.setServer(mqttBroker, mqttPort);
    client.setCallback(mqttSubCallback);
}

void loop() {
    unsigned long currentMs = millis();
    unsigned long elapsed = currentMs - lastMs;
    lastMs = currentMs;
    lastTelemAccum += elapsed;

    if (lastTelemAccum > dhtPeriod) {
        lastTelemAccum = 0;

        humidity = dht.getHumidity();
        temperature = dht.getTemperature();

        if (checkMQTTConnection()) {
            if ((unsigned long)(currentMs - lastPubTempTime) > MAX_PAYLOAD_PERIOD_MS || fabs(lastPubTemp - temperature) > 0.15) {
                char pubStr[6];
                sprintf(pubStr, "%0.2f", temperature);
                client.publish(temperatureTopic.c_str(), pubStr);
                Serial.printf("Publish temperature - %s\n", pubStr);
                lastPubTemp = temperature;
                lastPubTempTime = currentMs;
            }

            if ((unsigned long)(currentMs - lastPubHumidityTime) > MAX_PAYLOAD_PERIOD_MS || fabs(lastPubHumidity - humidity) > 0.2) {
                char pubStr[6];
                sprintf(pubStr, "%0.2f", humidity);
                client.publish(humidityTopic.c_str(), pubStr);
                Serial.printf("Publish humidity - %s\n", pubStr);
                lastPubHumidity = humidity;
                lastPubHumidityTime = currentMs;
            }
        }
    }

    boolean alert = !gwOnline || !hassOnline || garageOccupancy;

    displayOn = switchStates[0] || alert;
    ledsOn = switchToggle[1];

    ledStates[0] = ledsOn && alert;

    updateDisplay();
    updateLeds();

    client.loop();

    delay(200);
}
