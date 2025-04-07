#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <DHT.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// -------------------- DS18B20 --------------------
#define ONE_WIRE_BUS 0 // D3 (GPIO0)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// -------------------- MAX30105 --------------------
MAX30105 particleSensor;
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;

// -------------------- DHT11 --------------------
#define DHTPIN 2 // D4 (GPIO2)
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// -------------------- WiFi --------------------
const char* ssid = "deshwal";
const char* password = "balabala";

// -------------------- Web Server --------------------
ESP8266WebServer server(80);

// -------------------- Timing --------------------
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000; // 2 seconds

// Sensor readings
float dsTemp = 0.0;
float dhtTemp = 0.0;
float humidity = 0.0;
long irValue = 0;

void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Patient Health Monitoring</title>";
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.7.2/css/all.min.css'>";
  html += "<link rel='stylesheet' type='text/css' href='styles.css'>";
  html += "<style>";
  html += "body {background-color: #fff; font-family: Helvetica, sans-serif; color: #333; font-size: 14px; box-sizing: border-box;}";
  html += "#page {margin: 20px; background-color: #fff;}";
  html += ".container {height: inherit; padding-bottom: 20px;}";
  html += ".header {padding: 20px; text-align: center;}";
  html += ".header h1 {color: #008080; font-size: 45px; font-weight: bold; font-family: Garamond, sans-serif;}";
  html += ".header h3 {font-weight: bold; font-family: Arial, sans-serif; font-size: 17px; color: #b6b6b6;}";
  html += "h2 {padding-bottom: 0.2em; border-bottom: 1px solid #eee; margin: 2px; text-align: left;}";
  html += ".box-full {padding: 20px; border: 1px solid #ddd; border-radius: 1em; box-shadow: 1px 7px 7px rgba(0, 0, 0, 0.4); background: #fff; margin: 20px; width: 400px;}";
  html += "@media (max-width: 494px) {#page {width: inherit; margin: 5px auto;} .box-full {margin: 8px; padding: 10px; width: inherit;}}";
  html += "@media (min-width: 494px) and (max-width: 980px) {#page {width: 465px; margin: 0 auto;} .box-full {width: 380px;}}";
  html += "@media (min-width: 980px) {#page {width: 930px; margin: auto;}}";
  html += ".sensor {margin: 12px 0; font-size: 2.5rem;}";
  html += ".sensor-labels {font-size: 1rem; vertical-align: middle; padding-bottom: 15px;}";
  html += ".units {font-size: 1.2rem;}";
  html += "hr {height: 1px; color: #eee; background-color: #eee; border: none;}";
  html += "</style>";
  html += "<script>";
  html += "setInterval(loadDoc, 1000);";
  html += "function loadDoc() {";
  html += "var xhttp = new XMLHttpRequest();";
  html += "xhttp.onreadystatechange = function() {";
  html += "if (this.readyState == 4 && this.status == 200) {";
  html += "document.body.innerHTML = this.responseText;";
  html += "}";
  html += "};";
  html += "xhttp.open('GET', '/', true);";
  html += "xhttp.send();";
  html += "}";
  html += "</script>";
  html += "</head>";
  html += "<body>";
  html += "<div id='page'>";
  html += "<div class='header'>";
  html += "<h1>Health Monitoring System</h1>";
  html += "</div>";
  html += "<div id='content' align='center'>";
  html += "<div class='box-full' align='left'>";
  html += "<h2>Sensors Readings</h2>";
  html += "<div class='sensors-container'>";
  html += "<div class='sensors'>";
  html += "<p class='sensor'>";
  html += "<i class='fas fa-thermometer-half' style='color:#0275d8'></i>";
  html += "<span class='sensor-labels'> Room Temperature </span>" + String(dhtTemp) + "<sup class='units'>°C</sup>";
  html += "</p>";
  html += "<hr>";
  html += "</div>";
  html += "<div class='sensors'>";
  html += "<p class='sensor'>";
  html += "<i class='fas fa-tint' style='color:#5bc0de'></i>";
  html += "<span class='sensor-labels'> Room Humidity </span>" + String(humidity) + "<sup class='units'>%</sup>";
  html += "</p>";
  html += "<hr>";
  html += "<p class='sensor'>";
  html += "<i class='fas fa-heartbeat' style='color:#cc3300'></i>";
  html += "<span class='sensor-labels'> Last Heart Rate </span>" + String(beatsPerMinute) + "<sup class='units'>BPM</sup>";
  html += "</p>";
  html += "<hr>";
  html += "<p class='sensor'>";
  html += "<i class='fas fa-heartbeat' style='color:#cc3300'></i>";
  html += "<span class='sensor-labels'> Heart Rate </span>" + String(beatAvg) + "<sup class='units'>BPM</sup>";
  html += "</p>";
  html += "<hr>";
  html += "<p class='sensor'>";
  html += "<i class='fas fa-thermometer-full' style='color:#d9534f'></i>";
  html += "<span class='sensor-labels'> Body Temperature </span>" + String(dsTemp) + "<sup class='units'>°C</sup>";
  html += "</p>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</body>";
  html += "</html>";


  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Init sensors
  tempSensor.begin();
  dht.begin();

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not found. Check wiring.");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeGreen(0x0A); // Just to show it’s alive

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  // Start server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Heart rate from MAX30105
  irValue = particleSensor.getIR();
  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60.0 / (delta / 1000.0);
    if (beatsPerMinute > 20 && beatsPerMinute < 255) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
      beatAvg = 0;
      for (byte i = 0; i < RATE_SIZE; i++) beatAvg += rates[i];
      beatAvg /= RATE_SIZE;
    }
  }

  // Read every 5 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    tempSensor.requestTemperatures();
    dsTemp = tempSensor.getTempCByIndex(0);
    dhtTemp = dht.readTemperature();
    humidity = dht.readHumidity();

    // Print to Serial
    Serial.println("=== Sensor Data ===");
    Serial.print("DS18B20 Temp: "); Serial.println(dsTemp);
    Serial.print("DHT11 Temp: "); Serial.print(dhtTemp); Serial.print(" °C | Humidity: "); Serial.println(humidity);
    Serial.print("IR: "); Serial.print(irValue);
    Serial.print(" | BPM: "); Serial.print(beatsPerMinute);
    Serial.print(" | Avg BPM: "); Serial.println(beatAvg);
    if (irValue < 50000) Serial.println("No finger on sensor.");
    Serial.println("=====================");
  }

  server.handleClient();
  delay(10);
}
