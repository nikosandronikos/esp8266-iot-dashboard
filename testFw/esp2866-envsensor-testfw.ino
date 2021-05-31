#include <Wire.h>
#include "SSD1306Wire.h"

#include "DHTesp.h"

// Available: D0, D1, D2, D3, D4, D5, D6, D7, D8
//                 IO  IO  O   O   IO  IO IO  ~O
//                                        S
//                        D   D       TH
//             L  S    S          S       S   L


#define NUM_SWITCH  5
int switchAddr[NUM_SWITCH] = {D2, D5, D6, D7, D1};//, D6, D2, D5};

#define NUM_LED 1
int ledAddr[NUM_LED] = {D8};

#define DISPLAY_SCL D4
#define DISPLAY_SDA D3

#define DHT11_S D0

boolean switchStates[NUM_SWITCH];
boolean ledStates[NUM_LED];

DHTesp dht;
int dhtPeriod;

SSD1306Wire display(0x3c, DISPLAY_SDA, DISPLAY_SCL);

unsigned long lastMs = 0, lastTelemAccum = 0;
float temperature, humidity;

void updateLeds() {
    for (int i = 0; i < NUM_LED; i++) {
        digitalWrite(ledAddr[i], ledStates[i] ? 0xFF : 0);
    }
}

void updateDisplay() {
    display.clear();

    display.setColor(WHITE);

    char str[(NUM_SWITCH + NUM_LED) * 2];

    int x = 10;

    for(int i = 0; i < NUM_LED; i++) {
        display.drawString(x, 10, ledStates[i] ? "*" : ".");
        x += 10;
    }

    for(int i = 0; i < NUM_SWITCH; i++) {
        display.drawString(x, 10, switchStates[i] ? "+" : "-");
        x += 10;
    }

    char tempStr[20], humStr[20];

    sprintf(tempStr, "Temperature : %0.2f", temperature);
    sprintf(humStr, "Humidity    : %0.2f", humidity);

    display.drawString(10, 20, tempStr);
    display.drawString(10, 30, humStr);

    display.drawRect(0,0,128,64);

    display.display();
}

void ICACHE_RAM_ATTR handleSwitchChange() {
    Serial.println("Switch interrupt");
    for (int i = 0; i < NUM_SWITCH; i++) {
        switchStates[i] = digitalRead(switchAddr[i]);
        Serial.printf("  %d: %s\n", i, switchStates[i] ? "on" : "off");
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    for (int i = 0; i < NUM_LED; i++) {
        pinMode(ledAddr[i], OUTPUT);
    }

    for (int i = 0; i < NUM_SWITCH; i++) {
        pinMode(switchAddr[i], INPUT);
        attachInterrupt(digitalPinToInterrupt(switchAddr[i]), handleSwitchChange, CHANGE);
    }

    display.init();
    Serial.printf("Display initialised.\n");

    dht.setup(DHT11_S, DHTesp::DHT11);
    Serial.printf("DHT initialised.\n");

    dhtPeriod = dht.getMinimumSamplingPeriod() * 4;
    Serial.printf("DHT minimum sampling period: %d\n", dhtPeriod);

    handleSwitchChange();
}

void loop() {
    unsigned long currentMs = millis();
    unsigned long elapsed = currentMs - lastMs;
    lastMs = currentMs;
    lastTelemAccum += elapsed;

    for (int i = 0; i < NUM_LED; i++) {
        ledStates[i] = !ledStates[i];
    }

    if (lastTelemAccum > dhtPeriod) {
        lastTelemAccum = 0;

        humidity = dht.getHumidity();
        temperature = dht.getTemperature();

        Serial.printf("humidity: %f\n", humidity);
        Serial.printf("temperature: %f\n", temperature);
    }

    updateDisplay();
    updateLeds();

    delay(200);
}
