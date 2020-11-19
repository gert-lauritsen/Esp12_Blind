//#define CLIENT_ID "SensorOut"         // Client ID to send to the broker
#include "password.h"


char UniqID[8];

String GetConfig() {
  String str;
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    //return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
   // return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);
  for (int i=0; i<size; i++) str+=buf[i];
  str+="\0";
  //str=buf.get();
  Serial.println(str);
  configFile.close();  
  return str;
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);
  configFile.close();
  
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }
  json.printTo(Serial);
 /*  mqtt_server=json["mqtt_server"];
    mqtt_port=json["mqtt_port"];
    mqtt_username=json["username"];
    mqtt_password=json["password"];
    endPosition = json["endPosition"];
    ssid= json["ssid"];
    password= json["wifipassword"];*/
  currentPosition = json["currentPosition"];
  endPosition = json["endPosition"];
  SetPosition = currentPosition; //Stopper
  Serial.println(' ');
  
  return true;
}

void readconfig() {
  String str;
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) Serial.println("Failed to open config file");
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  //return str;
}

/*void saveconfig(byte* payload, unsigned int length) {
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) Serial.println("Failed to open config file for writing");
  configFile.write(payload, length);
  configFile.close();
  Serial.println("Config write done");
}*/

//Save Config
bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["mqtt_server"]=mqtt_server;
  json["mqtt_port"]=mqtt_port;
  json["username"]=username;
  json["password"]=password;
  json["ssid"]=ssid;
  json["wifipassword"]=password;

  json["endPosition"] = endPosition; //Hvor langt der er til endpoint
  json["currentPosition"] = currentPosition; //Gemmer hvor den er
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  json.printTo(configFile);
  return true;
}

/**
   Attempt connection to MQTT broker and subscribe to command topic
*/
void reconnect() {
  // Loop until we're reconnected
  char str[25];
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(ID)) {
      Serial.println("connected");
      sprintf(&str[0], "/%08X/cmd", ESP.getChipId());
      client.subscribe(str);
      ipToString(WiFi.localIP()).toCharArray(IPadr, 25);
      SendStatus();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

String ipToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}


void initIOT() {
  sprintf(&UniqID[0], "%08X", ESP.getChipId());

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  Serial.println("Mount file system Done!!");

  if (!loadConfig()) {
    Serial.println("Failed to load config");
  } else {
    Serial.println("Config loaded");
  }


  WiFi.mode(WIFI_STA);
  WiFi.begin(&ssid[0], &wifipassword[0]);
  WiFi.hostname(UniqID);
  Serial.println("WiFi begun");
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Proceeding");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(UniqID);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR   ) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR  ) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR    ) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(mqtt_server);
  // Prepare MQTT client
  sprintf(mqttTopicStatus, "/%08X/Status", ESP.getChipId());
  Serial.println(mqttTopicStatus);
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback);
}

void checkWifi() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) reconnect(); else client.loop();
  }
}
