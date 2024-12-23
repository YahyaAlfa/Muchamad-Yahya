#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define DHTPIN 15  // Pin DHT11
#define DHTTYPE DHT22  // Ganti DHT22 dengan DHT11

// Pin Definitions
const int ldrPin = 17;
const int waterLevelPin = 8;  // Pin untuk sensor water level
const int mq135Pin = 35;
const int trigPin = 6;
const int echoPin = 18;
const int buzzerPin = 12;
const int ledPin = 16;
const int relayWaterPumpPin = 7;
const int relayFanPin = 14;

DHT dht(DHTPIN, DHTTYPE);

// Ganti dengan informasi WiFi Anda
const char* ssid = "OPPO A17k";
const char* password = "w5mx9cvf";

// Ganti dengan token perangkat ThingsBoard Anda
const char* thingsboardServer = "demo.thingsboard.io"; // Ganti dengan URL ThingsBoard Anda
const char* deviceToken = "Ed1FAQs6yUdeo7jXRdpp";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(relayWaterPumpPin, OUTPUT);
  pinMode(relayFanPin, OUTPUT);
  pinMode(ldrPin, INPUT);
  pinMode(waterLevelPin, INPUT);
  pinMode(mq135Pin, INPUT);
  
  dht.begin();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Configure ThingsBoard server
  client.setServer(thingsboardServer, 1883);
}

void loop() {
  // Connect to ThingsBoard
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

 // --- DHT22 Sensor ---
  float temperature = dht.readTemperature();
  if (!isnan(temperature)) {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

   

    // Send temperature data to ThingsBoard
    sendDataToThingsBoard("temperature", temperature);

    if (temperature <= 26.0) {
      digitalWrite(relayFanPin, HIGH);  // Fan on
      digitalWrite(ledPin, HIGH);       // LED on if temp ≤ 26.0°C
    } else {
      digitalWrite(relayFanPin, LOW);   // Fan off
      // LED depends on LDR if temp > 26.0°C
      int ldrValue = analogRead(ldrPin);
      Serial.print("LDR Value: ");
      Serial.println(ldrValue);
      
      // Send light level data to ThingsBoard
      sendDataToThingsBoard("light_level", ldrValue);

      if (ldrValue > 900) {   // LED on if dark
        digitalWrite(ledPin, HIGH);
      } else {                // LED off if bright
        digitalWrite(ledPin, LOW);
      }
    }
  }

  // --- HC-SR04 Ultrasonic Sensor for Plant Height ---
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float plantHeight = (duration / 2.0) / 29.1;  // Convert to cm

  Serial.print("Plant Height: ");
  Serial.print(plantHeight);
  Serial.println(" cm");

  // Send plant height data to ThingsBoard
  sendDataToThingsBoard("plant_height", plantHeight);

  if (plantHeight >= 30.0) {
    digitalWrite(buzzerPin, HIGH);   // Buzzer on
  } else {
    digitalWrite(buzzerPin, LOW);    // Buzzer off
  }

  // --- Water Level Sensor ---
  int waterLevelValue = analogRead(waterLevelPin);
  float waterLevel = (waterLevelValue / 1023.0) * 60.0;
  Serial.print("Water Level: ");
  Serial.print(waterLevel);
  Serial.println(" cm");

  // Send water level data to ThingsBoard
  sendDataToThingsBoard("water_level", waterLevel);

  if (waterLevel > 12) {
    digitalWrite(relayWaterPumpPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
  } else if (waterLevel >= 24) {
    digitalWrite(relayWaterPumpPin, HIGH);
    digitalWrite(buzzerPin, LOW);
  } else if (waterLevel >= 36) {
    digitalWrite(relayWaterPumpPin, HIGH);
    digitalWrite(buzzerPin, LOW);
  } else if (waterLevel >= 48) {
    digitalWrite(relayWaterPumpPin, LOW);
    digitalWrite(buzzerPin, LOW);
  } else {
    digitalWrite(relayWaterPumpPin, LOW);
    digitalWrite(buzzerPin, HIGH);
  }

  // --- MQ-135 Gas Sensor ---
  int mq135Value = analogRead(mq135Pin);
  float ppm = map(mq135Value, 0, 1023, 0, 200);  // Map value to ppm
  Serial.print("Gas Concentration: ");
  Serial.print(ppm);
  Serial.println(" ppm");

  // Send gas concentration data to ThingsBoard
  sendDataToThingsBoard("gas_concentration", ppm);

  // Fan control based on gas concentration
  if (ppm > 50.0) {
    digitalWrite(relayFanPin, HIGH);  // Fan on if ppm > 50.0
  } 

  // If gas concentration >= 75 ppm, activate buzzer
  if (ppm >= 75.0) {
    digitalWrite(buzzerPin, HIGH);    // Buzzer on if dangerous gas detected
  } else {
    digitalWrite(buzzerPin, LOW);     // Buzzer off if gas < 75 ppm
  }

  // Send device status to ThingsBoard
  sendDataToThingsBoard("fan_status", digitalRead(relayFanPin));
  sendDataToThingsBoard("pump_status", digitalRead(relayWaterPumpPin));
  sendDataToThingsBoard("led_status", digitalRead(ledPin));
  sendDataToThingsBoard("buzzer_status", digitalRead(buzzerPin));

  delay(1000);  // Wait before next reading
}

void reconnect() {
  // Loop until connected
  while (!client.connected()) {
    Serial.print("Attempting to connect to ThingsBoard...");
    // Attempt to connect
    if (client.connect("ESP32Client", deviceToken, NULL)) {
      Serial.println("Connected to ThingsBoard");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void sendDataToThingsBoard(const char* key, float value) {
  // Create JSON payload
  String payload = "{\"";
  payload += key;
  payload += "\":";
  payload += value;
  payload += "}";

  // Send data to ThingsBoard
  client.publish("v1/devices/me/telemetry", payload.c_str());
}