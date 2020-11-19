 #include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Stepper_28BYJ_48.h>
#include <String.h>
//For at kunne kører med OTA skal den sætte til 4M/1M, ellers er der ikke plads til koden
//HW_ver1 er forbunder D0, D2, D12, D13
//HW_ver2 er forbunde D5, D0, D4, D2

#include <ArduinoJson.h>
#include "FS.h"
//For at få OTA til at fungere skal den være programmet første gang som wemos, og derefter kan man skifte

bool omvendt=false; //få den til at kører den anden vej "køkken" og "kontor" er omvendt mens de andre ikke er det

/* MQTT Settings */
char mqttTopicStatus[30];
char ID[8];

char IPadr[25];
bool SendSetting=false;
bool moving=false;
unsigned int currentPosition, endPosition, SetPosition;
char Save2NV=0; //Signal om at gemme til JSON
 
Stepper_28BYJ_48 small_stepper(0, 2, 12, 13); //Initiate stepper driver HW ver 1
//Stepper_28BYJ_48 small_stepper(2, 4, 0, 5); //Initiate stepper driver HW med dual stepperdriver
const int pins[4]={2,4,0,5};
//''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
WiFiClient wificlient;
PubSubClient client(wificlient);

/**
 * MQTT callback to process messages
 * Start, EndPoint : start og slutpunkt
 * Posplus, PosMinus: Manuel frem og tilbage
 */
void callback(char* topic, byte* payload, unsigned int length) {
 int i = 0; 
 char message_buff[250]; 
 for(i=0; i<length; i++) {
 message_buff[i] = payload[i];
 }
 message_buff[i] = '\0';
 Serial.println(message_buff);
 //if (message_buff[0]=='{') saveconfig(payload,length);
 if (String(message_buff) == "OPEN")  {
  SetPosition=endPosition;
 } 
 if (String(message_buff) == "CLOSE")  {
  SetPosition=0;
 } 

 if (String(message_buff) == "Start")  {
  currentPosition=0; SetPosition=0;
  Save2NV=1;
 }
 if (String(message_buff) == "EndPoint") {
   endPosition=currentPosition;
   SetPosition=currentPosition; //Stopper
   Save2NV=1;
 }
 if (String(message_buff) == "PosPlus")  { //Manuel mode
  SetPosition=0xffffffff;
 }
 if (String(message_buff) == "PosMinus") {
  SetPosition=0;
 }
 if (String(message_buff) == "settings") {
  SendSetting=true;
 }
 if (String(message_buff) == "setup") {
  SetPosition=9428;
  currentPosition=9428;
  endPosition=9428;
  Save2NV=1;
  SendSetting=true;
 }
 
 //UpdateState=true;
}


/**
 * Setup
 */
void setup() {
  Serial.begin(115200);
  Serial.print("Booting ");
  initIOT();
}

void SendStatus() {
  char msg[150];
  char temp[25];
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["endPosition"]=endPosition;    //endpos giver hvor langt op det skal rulle. Så man starter med at rulle det ned, sætte det som start og derefter rulle op.
  json["currentPosition"]=currentPosition;
  if (SetPosition==0) json["state"]="ON"; else json["state"]="OFF"; //om den er åben eller lukket
  json["RSSI"]=WiFi.RSSI();
  json["IPAdr"]=IPadr;
  json.printTo(Serial);
  Serial.println("");
  json.printTo(msg);
  client.publish(mqttTopicStatus, msg);
}


/**
 * Main
 */
void loop() {
  char str[80];
  checkWifi();
  if (Save2NV) {
    Save2NV=0;
    saveConfig();
  }
  if (moving && (currentPosition == SetPosition)) {
    moving=false;
    Save2NV=1;
    SendStatus();
    for (int i=0; i<4; i++) digitalWrite(pins[i],LOW);
  }
  if (SendSetting) {
    SendSetting=false;
    client.publish(mqttTopicStatus,GetConfig().c_str());
   // SendStatus();
  }
  if (currentPosition > SetPosition){ //kør baglæns
      if (omvendt) small_stepper.step(1); else small_stepper.step(-1);
      moving=true;
      currentPosition = currentPosition - 1;
  } else if (currentPosition < SetPosition){ //kør fremad
      if (omvendt) small_stepper.step(-1); else small_stepper.step(1);
      moving=true;
      currentPosition = currentPosition + 1;
  }  
}
