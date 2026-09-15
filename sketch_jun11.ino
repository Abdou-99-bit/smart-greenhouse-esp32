#include <WiFi.h>
#include <AsyncMqttClient.h>

const char* WIFI_SSID = "Abdou";
const char* WIFI_PASSWORD = "Taza$&@0610";
const char* MQTT_HOST = "172.20.10.14";
const uint16_t MQTT_PORT = 1883;

// --- PINS ---
const int MOISTURE_PIN = 34;
const int PUMP_LED = 25;
const int FAN_LED = 26; 
const int STATUS_LED = 32;

// --- CALIBRATION & THRESHOLDS ---
const int DRY_VALUE = 3500;
const int WET_VALUE = 1200;
const int MOISTURE_LOW_THRESHOLD = 65;
const int MOISTURE_HIGH_THRESHOLD = 85;

AsyncMqttClient mqttClient;
unsigned long lastSensorReadTime = 0;
const unsigned long s_interval = 2000;
bool pumpState = false;
bool fanState = false; // keeps track of the ventilator status

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to Mosquitto MQTT Broker...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("Wi-Fi Connected! IP address: ");
      Serial.println(WiFi.localIP());
      digitalWrite(STATUS_LED, HIGH);
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("Wi-Fi connection lost. Reconnecting...");
      digitalWrite(STATUS_LED, LOW);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Successfully connected to Mosquitto MQTT Broker!");
  
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT Broker.");
  if (WiFi.isConnected()) {
    connectToMqtt();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(PUMP_LED, OUTPUT);
  pinMode(FAN_LED, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(MOISTURE_PIN, INPUT);
  
  
// Start with everything off

  digitalWrite(PUMP_LED, LOW);
  digitalWrite(FAN_LED, LOW);
  digitalWrite(STATUS_LED, LOW);
  
  WiFi.onEvent(WiFiEvent);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  
  connectToWifi();
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastSensorReadTime >= s_interval) {
    lastSensorReadTime = currentMillis;
    
    int rawAnalog = analogRead(MOISTURE_PIN);
    int moisturePercent = map(rawAnalog, DRY_VALUE, WET_VALUE, 0, 100);
    moisturePercent = constrain(moisturePercent, 0, 100);
    
    Serial.print("Raw Analog: ");
    Serial.print(rawAnalog);
    Serial.print(" -> Moisture Level: ");
    Serial.print(moisturePercent);
    Serial.println("%");
    
    // --- PUMP LOGIC ---
    
    if (moisturePercent < MOISTURE_LOW_THRESHOLD) {
      pumpState = true;
    } else if (moisturePercent > MOISTURE_HIGH_THRESHOLD) {
      pumpState = false;
    }
    
    // --- FAN LOGIC ---

    // The fan now runs automatically when the pump delivers water
    
    if (pumpState == true) {
      fanState = true;
    } else {
      fanState = false;
    }
    
    // --- LEDS PRECISION SYNCHRONIZED ---
    
    digitalWrite(PUMP_LED, pumpState ? HIGH : LOW);
    digitalWrite(FAN_LED, fanState ? HIGH : LOW);
    
    Serial.print("Pomp: "); Serial.println(pumpState ? "ON" : "OFF");
    Serial.print("Fan: ");  Serial.println(fanState ? "ON" : "OFF");
    
    // --- SEND MQTT MESSAGES ---
    
    if (mqttClient.connected()) {
      // Send the humidity as text
      String moistureStr = String(moisturePercent);
      mqttClient.publish("tun/sensor/fochtigens", 0, false, moistureStr.c_str());
      
      // Send the status of the devices
      
      mqttClient.publish("tun/status/pomp", 0, false, pumpState ? "ON" : "OFF");
      mqttClient.publish("tun/status/fan", 0, false, fanState ? "ON" : "OFF");
      
      Serial.println("MQTT data sent to broker!");
    } else {
      Serial.println("MQTT not connected. No message sent.");
    }
    
    Serial.println("--------------------------------------");
  }
}
