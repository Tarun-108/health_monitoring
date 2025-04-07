#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is connected to GPIO 0 (D3 on NodeMCU)
#define ONE_WIRE_BUS 0

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  sensors.begin();
}

void loop() {
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempC = sensors.getTempCByIndex(0); // Get temperature in Celsius
  float tempF = sensors.getTempFByIndex(0); // Get temperature in Fahrenheit

  Serial.print("Temperature: ");
  Serial.print(tempC);
  Serial.print("°C  |  ");
  Serial.print(tempF);
  Serial.println("°F");

  delay(2000); // Wait 2 seconds before next reading
}
