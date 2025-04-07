#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <DHT.h>
#include <ESP8266HTTPClient.h>

#define EEPROM_SIZE 512
#define ONE_WIRE_BUS D3
#define DHTPIN D4
#define DHTTYPE DHT11

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DHT dht(DHTPIN, DHTTYPE);
MAX30105 particleSensor;

const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

const char* fixedServerUrl = "http://your-server-url.com";

struct Config {
  char ssid[32];
  char password[32];
};
Config config;

unsigned long lastStreamTime = 0;
const unsigned long streamInterval = 2000;

void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);
  EEPROM.end();
}

void saveConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, config);
  EEPROM.commit();
  EEPROM.end();
}

bool isConfigValid() {
  return strlen(config.ssid) > 0;
}

bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

bool fetchWiFiCredentials() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  delay(1000);

  WiFiClient client;
  HTTPClient http;
  String configUrl = String(fixedServerUrl) + "/configure";
  http.begin(client, configUrl);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      strncpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
      strncpy(config.password, doc["password"] | "", sizeof(config.password));
      saveConfig();
      http.end();
      return true;
    }
  }
  http.end();
  return false;
}

void setupSensors() {
  sensors.begin();
  dht.begin();
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not found. Check wiring.");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeGreen(0);
}

void streamSensorData() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  float dhtTemp = dht.readTemperature();
  float humidity = dht.readHumidity();

  StaticJsonDocument<512> doc;
  doc["ds18b20_temp"] = tempC;
  doc["dht_temp"] = dhtTemp;
  doc["humidity"] = humidity;
  doc["ir"] = particleSensor.getIR();
  doc["bpm"] = beatsPerMinute;
  doc["bpm_avg"] = beatAvg;

  WiFiClient client;
  HTTPClient http;
  String postUrl = String(fixedServerUrl) + "/stream";
  http.begin(client, postUrl);
  http.addHeader("Content-Type", "application/json");
  String payload;
  serializeJson(doc, payload);
  int httpResponseCode = http.POST(payload);
  http.end();

  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  loadConfig();

  if (!isConfigValid() && !fetchWiFiCredentials()) {
    Serial.println("Failed to fetch WiFi credentials");
    return;
  }

  if (!connectToWiFi()) {
    Serial.println("WiFi connection failed");
    return;
  }

  setupSensors();
}

void loop() {
  // Sample MAX30102 as frequently as possible
  long irValue = particleSensor.getIR();

  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;

      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  // Periodic data streaming every 2 seconds
  if (millis() - lastStreamTime >= streamInterval) {
    lastStreamTime = millis();
    streamSensorData();
  }
}
