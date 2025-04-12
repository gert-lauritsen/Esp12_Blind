// ESP8266 Blind Controller with ULN2003 and MQTT Auto-Discovery for Home Assistant
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "secrets.h"

#define IN1_PIN D1
#define IN2_PIN D2
#define IN3_PIN D3
#define IN4_PIN D4
#define LIMIT_TOP_PIN D5
#define LIMIT_BOTTOM_PIN D6

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* mqtt_server = MQTT_SERVER;
const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PASS;

const char* room = "living_room"; // <--- Change this to the desired room name

WiFiClient espClient;
PubSubClient client(espClient);

long topPosition = 0;
long bottomPosition = 0;
long currentPosition = 0;

bool calibrated = false;

#define EEPROM_SIZE 64

const int stepsPerRevolution = 4096; // for 28BYJ-48

const uint8_t stepSequence[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void publishDiscoveryConfig() {
  String topic = "homeassistant/cover/" + String(room) + "_blind/config";

  StaticJsonDocument<512> doc;
  String displayName = String(room);
  displayName.replace("_", " ");

  doc["name"] = displayName + " Blind";
  doc["command_topic"] = "home/blind/" + String(room) + "/set";
  doc["position_topic"] = "home/blind/" + String(room) + "/state";
  doc["set_position_topic"] = "home/blind/" + String(room) + "/set";
  doc["unique_id"] = "esp8266_blind_" + String(room);
  doc["device_class"] = "blind";

  char buffer[512];
  size_t len = serializeJson(doc, buffer);

  bool success = client.publish(topic.c_str(), buffer, true);
  Serial.print("Discovery publish result: ");
  Serial.println(success ? "SUCCESS" : "FAILED");
}

void stepMotor(bool direction) {
  for (int step = 0; step < 8; step++) {
    int index = direction ? step : (7 - step);
    digitalWrite(IN1_PIN, stepSequence[index][0]);
    digitalWrite(IN2_PIN, stepSequence[index][1]);
    digitalWrite(IN3_PIN, stepSequence[index][2]);
    digitalWrite(IN4_PIN, stepSequence[index][3]);
    delayMicroseconds(1000);
  }
}

void moveTo(long target) {
  if (topPosition == bottomPosition) return; // avoid div by zero
  bool direction = target > currentPosition;
  long steps = abs(target - currentPosition);

  for (long i = 0; i < steps; i++) {
    stepMotor(direction);
    currentPosition += direction ? 1 : -1;
  }

  publishState();
}

void calibrate() {
  while (digitalRead(LIMIT_BOTTOM_PIN) == HIGH) {
    stepMotor(false);
    currentPosition--;
  }
  bottomPosition = currentPosition;

  delay(500);

  while (digitalRead(LIMIT_TOP_PIN) == HIGH) {
    stepMotor(true);
    currentPosition++;
  }
  topPosition = currentPosition;

  saveLimits();
  calibrated = true;
  publishState();
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  String setTopic = String("home/blind/") + room + "/set";
  String calibrateTopic = String("home/blind/") + room + "/calibrate";

  if (String(topic) == setTopic) {
    if (msg == "open") moveTo(topPosition);
    else if (msg == "close") moveTo(bottomPosition);
    else {
      int percent = msg.toInt();
      long range = topPosition - bottomPosition;
      if (range == 0) return;
      long target = bottomPosition + ((range * percent) / 100);
      moveTo(target);
    }
  } else if (String(topic) == calibrateTopic) {
    calibrate();
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP8266Blind", mqtt_user, mqtt_pass)) {
      client.publish("home/blind/status", "online", true);
      String setTopic = String("home/blind/") + room + "/set";
      String calibrateTopic = String("home/blind/") + room + "/calibrate";

      client.subscribe(setTopic.c_str());
      client.subscribe(calibrateTopic.c_str());
      client.publish("home/blind/status", "online", true);
      publishDiscoveryConfig();
    } else {
      delay(5000);
    }
  }
}

void saveLimits() {
  EEPROM.put(0, bottomPosition);
  EEPROM.put(sizeof(long), topPosition);
  EEPROM.commit();
}

void loadLimits() {
  EEPROM.get(0, bottomPosition);
  EEPROM.get(sizeof(long), topPosition);
  calibrated = true;
}

void publishState() {
  String stateTopic = String("home/blind/") + room + "/state";
  long range = topPosition - bottomPosition;
  if (range == 0) {
    client.publish(stateTopic.c_str(), "0", true);
    return;
  }
  int percent = (currentPosition - bottomPosition) * 100 / range;
  percent = constrain(percent, 0, 100);
  client.publish(stateTopic.c_str(), String(percent).c_str(), true);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("-------------------------------------------------------------------");
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  pinMode(LIMIT_TOP_PIN, INPUT_PULLUP);
  pinMode(LIMIT_BOTTOM_PIN, INPUT_PULLUP);

  EEPROM.begin(EEPROM_SIZE);
  loadLimits();

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  //client.publish("home/blind/status", "online", true);
  Serial.println("Setup Completed");
  Serial.println("-------------------------------------------------------------------");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
}

