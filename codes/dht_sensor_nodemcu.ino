#include <DHT.h>

#define DHTPIN D2      // Define the pin where the sensor is connected
#define DHTTYPE DHT11 // Define the sensor type

DHT dht(DHTPIN, DHTTYPE);

void setup() {
    Serial.begin(115200);
    Serial.println("DHT11 Sensor Reading...");
    dht.begin();
}

void loop() {
    float temperature = dht.readTemperature(); // Read temperature in Celsius
    float humidity = dht.readHumidity();      // Read humidity

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" °C  |  Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    delay(10000); // Wait for 2 seconds before next reading
}
