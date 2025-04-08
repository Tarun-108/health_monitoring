#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

// -------------- Sensors --------------
#define DHTPIN 2  // D4
#define DHTTYPE DHT11
#define ONE_WIRE_BUS 0  // D3

DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
MAX30105 particleSensor;

// -------------- Server Config --------------
String HOST_IP = "172.22.60.195:8080";  // IP of healthmonitoring-production.up.railway.app
const int HTTP_PORT = 80;
const String CONFIG_PATH = "/sensor/configure";
const String POST_PATH = "/sensor/data";

// -------------- Heart Rate Variables --------------
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

// -------------- State --------------
bool wifiConnected = false;
unsigned long lastPost = 0;
const unsigned long postInterval = 10000;

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();
  tempSensor.begin();

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not found. Check wiring.");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeGreen(0x0A);

  getWiFiConfigAndConnect();
}

void loop() {
  if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Retrying config...");
    getWiFiConfigAndConnect();
    delay(5000);
    return;
  }

  long irValue = particleSensor.getIR();
  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);
    if (beatsPerMinute > 20 && beatsPerMinute < 255) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
      beatAvg = 0;
      for (byte i = 0; i < RATE_SIZE; i++) beatAvg += rates[i];
      beatAvg /= RATE_SIZE;
    }
  }

  unsigned long now = millis();
  if (now - lastPost >= postInterval) {
    lastPost = now;
    postSensorData(irValue, beatsPerMinute, beatAvg);
  }

  delay(10);
}

void getWiFiConfigAndConnect() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin("LAPTOP-N3KG5V4G", "5c1;Q031"); // TEMP connection to get config

  Serial.println("Connecting temporarily to fetch Wi-Fi config...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  IPAddress gateway = WiFi.gatewayIP();
  Serial.println("Router IP: " + gateway.toString());
  HOST_IP = gateway.toString()+":8080";

  HTTPClient http;
  WiFiClient client;

  String url = "http://" + HOST_IP + CONFIG_PATH;
  http.begin(client, url);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload);
    const char* ssid = doc["ssid"];
    const char* password = doc["password"];

    Serial.print("\nConnecting to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
      Serial.print(".");
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("\nConnected to WiFi. IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("\nFailed to connect to new WiFi.");
    }

  } else {
    Serial.println("\nFailed to fetch config. HTTP code: " + String(httpCode));
  }

  http.end();
}

void postSensorData(long ir, float bpm, int bpm_avg) {
  tempSensor.requestTemperatures();
  float dsTemp = tempSensor.getTempCByIndex(0);
  float dhtTemp = dht.readTemperature();
  float humidity = dht.readHumidity();

  HTTPClient http;
  WiFiClient client;

  StaticJsonDocument<256> json;
  json["ds18b20_temp"] = dsTemp;
  json["dht11_temp"] = dhtTemp;
  json["humidity"] = humidity;
  json["ir"] = ir;
  json["bpm"] = bpm;
  json["bpm_avg"] = bpm_avg;

  Serial.println("=== Sensor Data ===");
  Serial.print("DS18B20 Temp: "); Serial.println(dsTemp);
  Serial.print("DHT11 Temp: "); Serial.print(dhtTemp); Serial.print(" °C | Humidity: "); Serial.println(humidity);
  Serial.print("IR: "); Serial.print(ir);
  Serial.print(" | BPM: "); Serial.print(bpm);
  Serial.print(" | Avg BPM: "); Serial.println(bpm_avg);
  if (ir < 50000) Serial.println("No finger on sensor.");
  Serial.println("=====================");

  String payload;
  serializeJson(json, payload);

  String url = "http://" + HOST_IP + POST_PATH;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(payload);
  Serial.println("POST /sensor/data => HTTP " + String(code));

  http.end();
}
