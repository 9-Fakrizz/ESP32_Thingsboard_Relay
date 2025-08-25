#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>

// ====== WiFi Config ======
#define WIFI_SSID "CARETUNU123"
#define WIFI_PASSWORD "q12345678"

// ====== ThingsBoard Config ======
#define TOKEN "4k0UHuHe1Ad7NzSq0yc0"   // เอาจาก Device ใน ThingsBoard Demo
#define THINGSBOARD_SERVER "demo.thingsboard.io"

// ====== Telegram Config ======
#define BOT_TOKEN "7571172904:AAG1VgHgjK6yBHXl0Un6Hmga2_Z9xGkkEI0"
#define CHAT_ID "7176116450"

// ====== API key for GEMINI 2.5 flash ======
//AIzaSyArg_x4SphohCxI3fF-09Ii56IktVcMYCk

// ====== Pins ======
#define MQ137_PIN 34
#define WATER_PIN 35
#define DHTPIN 4
#define RELAY1 16
#define RELAY2 17
#define RELAY3 18
#define RELAY4 19

bool manual_mode = false;
bool online_mode = false;

// ====== Threshold ======
float ammoniaThreshold = 30;
float tempHigh = 35.0;
float tempLow = 20.0;
float humidityThreshold = 80.0;

// ====== Lib Init ======
WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHT22);

// ====== Send Telegram ======
void sendMessage(String message) {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  HTTPClient http;

  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage";

  if (http.begin(secureClient, url)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "chat_id=" + String(CHAT_ID) + "&text=" + message;

    int httpResponseCode = http.POST(postData);
    Serial.println("Telegram sent. Code: " + String(httpResponseCode));
    http.end();
  } else {
    Serial.println("Failed to connect for text message");
  }
}


// ====== Callback ThingsBoard ======
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("Message from TB: " + msg);

  // Manual control จาก Dashboard
  if (msg.indexOf("relay1_on") != -1) digitalWrite(RELAY1, HIGH);
  if (msg.indexOf("relay1_off") != -1) digitalWrite(RELAY1, LOW);
  if (msg.indexOf("relay2_on") != -1) digitalWrite(RELAY2, HIGH);
  if (msg.indexOf("relay2_off") != -1) digitalWrite(RELAY2, LOW);
  if (msg.indexOf("relay3_on") != -1) digitalWrite(RELAY3, HIGH);
  if (msg.indexOf("relay3_off") != -1) digitalWrite(RELAY3, LOW);
  if (msg.indexOf("relay4_on") != -1) digitalWrite(RELAY4, HIGH);
  if (msg.indexOf("relay4_off") != -1) digitalWrite(RELAY4, LOW);
  if (msg.indexOf("manual_off") != -1) manual_mode = false;
  if (msg.indexOf("manual_on") != -1) manual_mode = true;
}

void displayStatus(float mq137, float temp, float hum, int waterLevel) {
  Serial.println("===== System Status =====");

  // Sensor values
  Serial.println("Ammonia (MQ137): " + String(mq137));
  Serial.println("Temperature (DHT22): " + String(temp) + " *C");
  Serial.println("Humidity (DHT22): " + String(hum) + " %");
  Serial.println("Water Sensor: " + String(waterLevel));

  // Relay states
  Serial.print("Relay1 (Water Pump): ");
  Serial.println(digitalRead(RELAY1) ? "ON" : "OFF");

  Serial.print("Relay2 (Fan/Alarm): ");
  Serial.println(digitalRead(RELAY2) ? "ON" : "OFF");

  Serial.print("Relay3 (Heater): ");
  Serial.println(digitalRead(RELAY3) ? "ON" : "OFF");

  Serial.print("Relay4 (Manual/Extra): ");
  Serial.println(digitalRead(RELAY4) ? "ON" : "OFF");

  Serial.println("=========================");
}

long timer1;
long timer_connect;

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  pinMode(WATER_PIN, INPUT_PULLDOWN);

  timer_connect = millis();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if(millis() - timer_connect >= 10000) break;
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    online_mode = true;
    client.setServer(THINGSBOARD_SERVER, 1883);
    client.setCallback(callback);
  }else{
    Serial.println("\nWiFi cannot connect");
    Serial.println("Run Offline Mode !");
    online_mode = false;
  }
  
  timer1 = millis();
  timer_connect = millis();
}

// ====== Loop ======
void loop() {
  // check wifi every 5 second
  if(millis() - timer_connect >= 5000 && WiFi.status() != WL_CONNECTED){
    while (WiFi.status() != WL_CONNECTED) {
      delay(500); Serial.print(".");
      if(millis() - timer_connect >= 10000) break; // start 5 then count 5 total 10
    }
    if(WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected");
      online_mode = true;
      client.setServer(THINGSBOARD_SERVER, 1883);
      client.setCallback(callback);
    }else{
      Serial.println("\nWiFi cannot connect");
      Serial.println("Run Offline Mode !");
      online_mode = false;
    }
      timer_connect = millis();
  }


  if(online_mode){
    if (!client.connected()) {
      while (!client.connected()) {
        if (client.connect("ESP32_Device", TOKEN, NULL)) {
          Serial.println("Connected to ThingsBoard");
          client.subscribe("v1/devices/me/rpc/request/+"); // รับคำสั่ง manual
        } else {
          delay(2000);
        }
      }
    }
    client.loop();
  }

  // ===== อ่านค่าจาก Sensor =====
  float mq137 = analogRead(MQ137_PIN);
  mq137 = mq137/500.0;
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int waterLevel = digitalRead(WATER_PIN);

  displayStatus(mq137, temp, hum, waterLevel);


  if (millis() - timer1 >= 5000 && online_mode ){
    // ===== ส่งค่าไป ThingsBoard =====
    String payload = "{";
    payload += "\"ammonia\":" + String(mq137) + ",";
    payload += "\"temperature\":" + String(temp) + ",";
    payload += "\"humidity\":" + String(hum) + ",";
    payload += "\"waterLevel\":" + String(waterLevel);
    payload += "}";
    client.publish("v1/devices/me/telemetry", payload.c_str());
    Serial.println("Data sent: " + payload);
    timer1 = millis();
  }

  if(manual_mode == false){
    // ===== Auto Control =====
    if (waterLevel == LOW) {  // ไม่มีน้ำ
      digitalWrite(RELAY1, HIGH);
    } else {
      digitalWrite(RELAY1, LOW);
    }

    if (mq137 > ammoniaThreshold) {
      digitalWrite(RELAY2, HIGH);
      sendMessage("Ammonia High: " + String(mq137));
    }

    if (temp > tempHigh) {
      digitalWrite(RELAY2, HIGH);
      sendMessage("Temp High: " + String(temp));
    }

    if (hum > humidityThreshold) {
      digitalWrite(RELAY2, HIGH);
    }

    if (temp < tempLow) {
      digitalWrite(RELAY3, HIGH);
    } else {
      digitalWrite(RELAY3, LOW);
    }

  }
  
  delay(1000);
}

