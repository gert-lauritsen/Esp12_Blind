// ESP8266 Blind Controller with ULN2003 and MQTT Auto-Discovery for Home Assistant (non-blocking)
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#endif
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "secrets.h"
//#include <Stepper_28BYJ_48.h>

#ifdef ESP8266
#define IN1_PIN 5
#define IN2_PIN 0
#define IN3_PIN 4
#define IN4_PIN 2
#define LIMIT_TOP_PIN D3
#define LIMIT_BOTTOM_PIN D6
#elif defined(ESP32)
#define IN1_PIN 5
#define IN2_PIN 0
#define IN3_PIN 4
#define IN4_PIN 2
#define LIMIT_TOP_PIN 3
#define LIMIT_BOTTOM_PIN 15
#endif
#define ReverseSetup true
#define UpdateStatusTimeout 10000
#define ULN2003
//#define DRV8825

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* mqtt_server = MQTT_SERVER;
const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PASS;

const char* room = "bed_room_rigth"; //Has to uniq
//const char* room = "bed_room_left"; //Has to uniq

// Build a stable device identifier for Home Assistant "device" grouping (ESP8266-only)
String haDeviceId() {
  uint32_t chip = ESP.getChipId();
  char buf[32];
  // include room to avoid collisions if you ever clone flash to another unit without changing room
  snprintf(buf, sizeof(buf), "esp8266_blind_%06X_%s", chip, room);
  return String(buf);
}

//const char* room = "bed_room_left";
//const char* room = "main_room_test";

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

const uint8_t fullStepSequence[4][4] = {
  {1, 1, 0, 0},  // Step 1
  {0, 1, 1, 0},  // Step 2
  {0, 0, 1, 1},  // Step 3
  {1, 0, 0, 1}   // Step 4
};


IPAddress local_IP(LOCAL_IP);
IPAddress gateway(GATEWAY_IP);
IPAddress subnet(SUBNET_MASK);

WiFiClient espClient;
PubSubClient client(espClient);

long topPosition = 0;
long bottomPosition = 0;
long currentPosition = 0;
long targetPosition = 0;
long lastupdatestatus = 0; 

bool calibrated = false;
bool moving = false;
bool direction = true;
bool StepValue = false;
//Stepper_28BYJ_48 small_stepper(IN1_PIN, IN2_PIN, IN3_PIN, IN4_PIN); //Initiate stepper driver HW ver 1

#define EEPROM_SIZE 64


int stepIndex = 0;
unsigned long lastStepTime = 0;
unsigned long stepInterval = 1200; // microseconds between steps

void setup_wifi() {
  Serial.println("Connecting to: "+String(ssid)); 
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }  
  Serial.println("");
    // Set hostname, e.g., "myesp"
  if (!MDNS.begin(String(room))) {
    Serial.println("Error starting mDNS");
    return;
  }
}

void publishDiscoveryConfig() {
  // Home Assistant MQTT Discovery for a Cover entity (blind)
  // Discovery topic format: homeassistant/<component>/<unique_id>/config
  String uniqueId = "esp8266_blind_" + String(room);
  String topic = "homeassistant/cover/" + uniqueId + "/config";

  String base = "home/blind/" + String(room);
  // Use a dedicated availability topic. If you previously published to
  // "home/blind/<room>/availability" (common pattern) and an old retained
  // "offline" is still there, Home Assistant will keep the entity unavailable
  // until it receives "online" on the *same* topic.
  //
  // So we standardize on "/availability" here.
  String statusTopic = base + "/availability";

  StaticJsonDocument<768> doc;

  // Friendly name
  String displayName = String(room);
  displayName.replace("_", " ");
  doc["name"] = displayName + " Blind";

  // Topics
  doc["command_topic"]       = base + "/set";           // OPEN/CLOSE/STOP (and also used by your code)
  doc["position_topic"]      = base + "/state";         // 0-100 position reports
  doc["set_position_topic"]  = base + "/set_position";  // numeric 0-100 set

  // Availability (shows device online/offline)
  doc["availability_topic"]   = statusTopic;
  doc["payload_available"]    = "online";
  doc["payload_not_available"]= "offline";

  // Entity identity
  doc["unique_id"] = uniqueId;
  doc["device_class"] = "blind";

  // ---- Device info (this is what makes it show up as a Device in HA) ----
  JsonObject dev = doc.createNestedObject("device");
  JsonArray ids = dev.createNestedArray("identifiers");
  ids.add(haDeviceId());                 // stable ID per unit
  dev["name"] = displayName + " Blind Controller";
  dev["manufacturer"] = "DIY";
  dev["model"] = "ESP8266 Blind Controller (ULN2003/DRV8825)";
  dev["sw_version"] = "1.0";
  dev["suggested_area"] = displayName;

  char buffer[768];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));
  bool success = client.publish(topic.c_str(), (const uint8_t*)buffer, n, true);

  Serial.print("Discovery publish result: ");
  Serial.println(success ? "SUCCESS" : "FAILED");
}
/*
void stepMotor(bool dir) {
  if (dir) small_stepper.step(-1);
  else small_stepper.step(1);
}
*/
#ifdef ULN2003
void stepMotor(bool dir) {
  if (ReverseSetup) dir=!dir;
  if (dir) stepIndex = (stepIndex + 1) % 8;
  else stepIndex = (stepIndex + 7) % 8;
  digitalWrite(IN1_PIN, stepSequence[stepIndex][0]);
  digitalWrite(IN2_PIN, stepSequence[stepIndex][1]);
  digitalWrite(IN3_PIN, stepSequence[stepIndex][2]);
  digitalWrite(IN4_PIN, stepSequence[stepIndex][3]);
}
#endif

#ifdef DRV8825
void stepMotor(bool dir) {
  if (ReverseSetup) dir=!dir;
  StepValue=!StepValue;
  digitalWrite(IN4_PIN, 0); //Set reset low
  digitalWrite(IN2_PIN, StepValue);
  digitalWrite(IN3_PIN, dir);
}
#endif

/*
void stepMotor(bool dir) {
  if (dir) stepIndex = (stepIndex + 1) % 4;
  else stepIndex = (stepIndex + 3) % 4;
  digitalWrite(IN1_PIN, fullStepSequence[stepIndex][0]);
  digitalWrite(IN2_PIN, fullStepSequence[stepIndex][1]);
  digitalWrite(IN3_PIN, fullStepSequence[stepIndex][2]);
  digitalWrite(IN4_PIN, fullStepSequence[stepIndex][3]);
}*/

void moveTo(long target) {
  targetPosition = target;
  direction = (targetPosition > currentPosition);
  if (target != currentPosition) {
    moving = true;
    String StatusTopic= "home/blind/" + String(room) + "/target";
    client.publish(StatusTopic.c_str(), String(target).c_str(), true);
    Serial.println("New Target: "+String(target));
  }  
}

void stop() {
 moving=false;
 digitalWrite(IN1_PIN,0);
 digitalWrite(IN2_PIN,0);
 digitalWrite(IN3_PIN,0);
 digitalWrite(IN4_PIN,0);
}

void updateMotor() {
  if (!moving) return;
  if ((micros() - lastStepTime) >= stepInterval) {
    lastStepTime = micros();
    stepMotor(direction);
    currentPosition += direction ? 1 : -1;
    if (currentPosition == targetPosition) {
      stop();
      publishState();
      saveLimits();
    }
  }
  if ((millis() - lastupdatestatus) >= UpdateStatusTimeout) {
    lastupdatestatus=millis();
    publishState();
  }    
}

void calibrate() {
 /* while (digitalRead(LIMIT_BOTTOM_PIN) == HIGH) {
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
  publishState();*/
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg+="\0";
  Serial.print(topic);
  Serial.print(" ");
  Serial.println(msg);
  String setTopic = "home/blind/" + String(room) + "/set";
  String calibrateTopic = "home/blind/" + String(room) + "/calibrate";
  if (String(topic) == setTopic) {
    if (strstr(msg.c_str(),"OPEN")!=NULL) {
      moveTo(topPosition);
      Serial.println("Opens blinds");
    }  
    else if (strstr(msg.c_str(),"CLOSE")!=NULL) {
      moveTo(bottomPosition);
      Serial.println("Closing blinds");
    }  
    else if (strstr(msg.c_str(),"STOP")!=NULL) {
      stop();
    }  
    else {
      int percent = msg.toInt();
      long range = topPosition-bottomPosition;
      if (range == 0) return;
      long target = bottomPosition + ((range * percent) / 100);
      moveTo(target);
    }
    Serial.println("Bottom:"+String(bottomPosition)+" Top: "+String(topPosition)+" Current "+String(currentPosition));  
  }  
  if (String(topic) == calibrateTopic) {
    calibrate();
  }

  if (String(topic) == "home/blind/" + String(room) + "/save_bottom") {
    bottomPosition = currentPosition;
    saveLimits();
    setTopic ="home/blind/" + String(room) + "/bottom_value";
    client.publish(setTopic.c_str(), String(bottomPosition).c_str(), true);
    Serial.println("Bottom position saved.");
  }
  if (String(topic) == "home/blind/" + String(room) + "/save_top") {
    topPosition = currentPosition;
    saveLimits();
    setTopic ="home/blind/" + String(room) + "/top_value";
    client.publish(setTopic.c_str(), String(topPosition).c_str(), true);
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
    String clientName = "Blind_" + String(room);

    // Must match discovery availability_topic
    String willTopic = String("home/blind/") + room + "/availability";

    bool ok = false;

    // If no MQTT username is configured, use the connect() overload without auth
    if (mqtt_user == nullptr || mqtt_user[0] == '\0') {
      ok = client.connect(
        clientName.c_str(),
        willTopic.c_str(),   // will topic
        0,                   // will QoS
        true,                // will retained
        "offline"            // will message
      );
    } else {
      ok = client.connect(
        clientName.c_str(),
        mqtt_user,
        mqtt_pass,
        willTopic.c_str(),   // will topic
        0,                   // will QoS
        true,                // will retained
        "offline"            // will message
      );
    }

    if (ok) {
      // Publish online retained so HA immediately marks entity available
      bool success = client.publish(willTopic.c_str(), "online", true);
      Serial.print("Mqtt Online result: ");
      Serial.println(success ? "SUCCESS" : "FAILED");

      // Subscribe to your control topics (unchanged)
      String setTopic            = String("home/blind/") + room + "/set";
      String calibrateTopic      = String("home/blind/") + room + "/calibrate";
      String setPositionTopic    = String("home/blind/") + room + "/set_position";
      String saveTopTopic        = String("home/blind/") + room + "/save_top";
      String saveBottomTopic     = String("home/blind/") + room + "/save_bottom";

      client.subscribe(setTopic.c_str());
      client.subscribe(calibrateTopic.c_str());
      client.subscribe(setPositionTopic.c_str());
      client.subscribe(saveTopTopic.c_str());
      client.subscribe(saveBottomTopic.c_str());

      client.publish("home/blind/status", "online", true);

      publishDiscoveryConfig();
    } else {
      // Optional: print state to diagnose auth/timeout issues
      Serial.print("MQTT connect failed, state=");
      Serial.println(client.state());

      delay(5000);
    }
  }
}

void saveLimits() {
  EEPROM.put(0, bottomPosition);
  EEPROM.put(sizeof(long), topPosition);
  EEPROM.put(sizeof(long)*2, currentPosition);
  EEPROM.commit();
  Serial.println("Save Bottom:"+String(bottomPosition)+" Top: "+String(topPosition)+" Current "+String(currentPosition));
}

void loadLimits() {
  EEPROM.get(0, bottomPosition);
  EEPROM.get(sizeof(long), topPosition);
  EEPROM.get(sizeof(long)*2, currentPosition);
  calibrated = true;
  Serial.println("Load Bottom:"+String(bottomPosition)+" Top: "+String(topPosition)+" Current "+String(currentPosition));
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
  stateTopic = "home/blind/" + String(room) + "/CurrentPosition";
  client.publish(stateTopic.c_str(), String(currentPosition).c_str(), true);
}

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("-------------------------------------------------------------------");  
  pinMode(LIMIT_TOP_PIN, INPUT_PULLUP);
  pinMode(LIMIT_BOTTOM_PIN, INPUT_PULLUP);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);

  EEPROM.begin(EEPROM_SIZE);
  loadLimits();
  if (topPosition<=0) {
    bottomPosition=0;
    topPosition=70000;
    currentPosition=0;
    saveLimits();
    Serial.println("Init value loaded");
    Serial.println("Bottom:"+String(bottomPosition)+" Top: "+String(topPosition)+" Current "+String(currentPosition));
  }
  setup_wifi();
  Serial.print("MQTT host: "); Serial.println(mqtt_server);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.setBufferSize(1024);

  publishState();
  Serial.println("Setup Completed");
  Serial.println(WiFi.macAddress());
  Serial.println("-------------------------------------------------------------------");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
  updateMotor();
}
