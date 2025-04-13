// ESP8266 Blind Controller with ULN2003 and MQTT Auto-Discovery for Home Assistant (non-blocking)
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "secrets.h"
//#include <Stepper_28BYJ_48.h>


#define IN1_PIN 5
#define IN2_PIN 0
#define IN3_PIN 4
#define IN4_PIN 2
#define LIMIT_TOP_PIN D3
#define LIMIT_BOTTOM_PIN D6

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* mqtt_server = MQTT_SERVER;
const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PASS;

const char* room = "bed_room_rigth";
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

WiFiClient espClient;
PubSubClient client(espClient);

long topPosition = 0;
long bottomPosition = 0;
long currentPosition = 0;
long targetPosition = 0;

bool calibrated = false;
bool moving = false;
bool direction = true;
//Stepper_28BYJ_48 small_stepper(IN1_PIN, IN2_PIN, IN3_PIN, IN4_PIN); //Initiate stepper driver HW ver 1

#define EEPROM_SIZE 64


int stepIndex = 0;
unsigned long lastStepTime = 0;
unsigned long stepInterval = 700; // microseconds between steps

void setup_wifi() {
  Serial.println("Connecting to: "+String(ssid)); 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void publishDiscoveryConfig() {
  String topic = "homeassistant/cover/" + String(room) + "_blind/config";
  StaticJsonDocument<512> doc;
  String displayName = String(room); displayName.replace("_", " ");
  doc["name"] = displayName + " Blind";
  doc["command_topic"] = "home/blind/" + String(room) + "/set";
  doc["position_topic"] = "home/blind/" + String(room) + "/state";
  doc["set_position_topic"] = "home/blind/" + String(room) + "/set";
  doc["unique_id"] = "esp8266_blind_" + String(room);
  doc["device_class"] = "blind";
  char buffer[512];
  serializeJson(doc, buffer);
  bool success = client.publish(topic.c_str(), buffer, true);
  Serial.print("Discovery publish result: ");
  Serial.println(success ? "SUCCESS" : "FAILED");  
}
/*
void stepMotor(bool dir) {
  if (dir) small_stepper.step(-1);
  else small_stepper.step(1);
}
*/
void stepMotor(bool dir) {
  if (dir) stepIndex = (stepIndex + 1) % 8;
  else stepIndex = (stepIndex + 7) % 8;
  digitalWrite(IN1_PIN, stepSequence[stepIndex][0]);
  digitalWrite(IN2_PIN, stepSequence[stepIndex][1]);
  digitalWrite(IN3_PIN, stepSequence[stepIndex][2]);
  digitalWrite(IN4_PIN, stepSequence[stepIndex][3]);
}

void moveTo(long target) {
  targetPosition = target;
  direction = (targetPosition > currentPosition);
  moving = true;
}

void updateMotor() {
  if (!moving) return;
  if (micros() - lastStepTime >= stepInterval) {
    lastStepTime = micros();
    stepMotor(direction);
    currentPosition += direction ? 1 : -1;
    if (currentPosition == targetPosition) {
      moving = false;
      publishState();
    }
  }
}

void calibrate() {
  while (digitalRead(LIMIT_BOTTOM_PIN) == HIGH) {
    stepMotor(false);
    delay(5);
    currentPosition--;
  }
  bottomPosition = currentPosition;
  delay(500);
  while (digitalRead(LIMIT_TOP_PIN) == HIGH) {
    stepMotor(true);
    delay(5);
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
  String setTopic = "home/blind/" + String(room) + "/set";
  String calibrateTopic = "home/blind/" + String(room) + "/calibrate";
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
  }  
  if (String(topic) == calibrateTopic) {
    calibrate();
  }
  if (String(topic) == "home/blind/" + String(room) + "/save_bottom") {
    bottomPosition = currentPosition;
    saveLimits();
    Serial.println("Bottom position saved.");
  }
  if (String(topic) == "home/blind/" + String(room) + "/save_top") {
    topPosition = currentPosition;
    saveLimits();
    Serial.println("Top position saved.");
  }
  if (String(topic) == "home/blind/" + String(room) + "/set_position") {
    Serial.println("set_position");
    int target = String((char*)payload).toInt();
    moveTo(target);
  } 
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP8266Blind", mqtt_user, mqtt_pass)) {
      String setStatus = String("home/blind/") + room + "/status";
      bool success = client.publish(setStatus.c_str(), "online",true);
      Serial.print("Mqtt Online result: ");
      Serial.println(success ? "SUCCESS" : "FAILED");
      String setTopic = "home/blind/" + String(room) + "/set";
      String calibrateTopic = "home/blind/" + String(room) + "/calibrate";
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
  String stateTopic = "home/blind/" + String(room) + "/state";
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
  Serial.println("-------------------------------------------------------------------");  
  pinMode(LIMIT_TOP_PIN, INPUT_PULLUP);
  pinMode(LIMIT_BOTTOM_PIN, INPUT_PULLUP);
   pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);

  EEPROM.begin(EEPROM_SIZE);
  loadLimits();
   bottomPosition=9765;
  topPosition=0;
  saveLimits(); 
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  Serial.println("Setup Completed");
  Serial.println("-------------------------------------------------------------------");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
  updateMotor();
}
