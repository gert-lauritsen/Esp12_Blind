# ESP12 Blind Controller

## Oversigt

ESP12 Blind Controller er en WiFi-baseret rullegardinstyring udviklet til IKEA-rullegardiner med et 3D-printet gearsystem og en stepmotor. Enheden styres via MQTT og understøtter automatisk registrering i Home Assistant gennem MQTT Discovery.

Projektet er baseret på en ESP8266 (ESP-12) og kan drive både:

* 28BYJ-48 stepmotor via ULN2003 driver
* Kraftigere stepmotor via DRV8825 driver

Positionen gemmes i EEPROM, så gardinet bevarer sin position efter genstart.

---

## Funktioner

* WiFi-forbindelse med fast IP-adresse
* MQTT styring
* Home Assistant MQTT Discovery
* Gemmer top-, bund- og aktuel position i EEPROM
* Ikke-blokerende stepmotorstyring
* Understøtter procentvis positionering (0-100%)
* Understøtter OPEN, CLOSE og STOP kommandoer
* MQTT Last Will & Testament (Online/Offline status)
* Understøttelse af endestopkontakter
* Automatisk Home Assistant Device-oprettelse

---

## Hardware

### ESP8266

Projektet er udviklet til:

* ESP-12E
* ESP-12F
* NodeMCU
* Wemos D1 Mini

### Stepmotor

Anbefalet:

* 28BYJ-48 5V stepmotor

### Motor Driver

#### ULN2003

Standardkonfiguration:

| ESP8266 GPIO | ULN2003 |
| ------------ | ------- |
| GPIO5        | IN1     |
| GPIO0        | IN2     |
| GPIO4        | IN3     |
| GPIO2        | IN4     |

#### DRV8825

Alternativt kan projektet konfigureres til DRV8825 ved at deaktivere:

```cpp
#define ULN2003
```

og aktivere:

```cpp
#define DRV8825
```

---

## Endestopkontakter

Følgende indgange anvendes:

| Funktion      | GPIO |
| ------------- | ---- |
| Top Endestop  | D3   |
| Bund Endestop | D6   |

Begge anvender:

```cpp
INPUT_PULLUP
```

---

## Konfiguration

### secrets.h

Opret filen:

```cpp
#pragma once

#define WIFI_SSID   "MyWifi"
#define WIFI_PASS   "MyPassword"

#define MQTT_SERVER "192.168.1.10"
#define MQTT_USER   "mqttuser"
#define MQTT_PASS   "mqttpassword"

#define LOCAL_IP    192,168,1,50
#define GATEWAY_IP  192,168,1,1
#define SUBNET_MASK 255,255,255,0
```

---

## Valg af Gardin

Hvert gardin skal have et unikt navn.

Eksempel:

```cpp
const char* room = "bed_room_right";
```

Andre eksempler:

```cpp
const char* room = "living_room";
const char* room = "office";
const char* room = "kitchen";
```

Dette navn anvendes til:

* MQTT topics
* Home Assistant Device ID
* Home Assistant Entity Name

---

## MQTT Topics

Hvis:

```cpp
room = "bed_room_right"
```

oprettes følgende topics.

### Kommandoer

#### Åbn

Topic:

```text
home/blind/bed_room_right/set
```

Payload:

```text
OPEN
```

#### Luk

```text
CLOSE
```

#### Stop

```text
STOP
```

#### Gå til position

```text
50
```

Flytter gardinet til 50%.

---

### Direkte positionering

Topic:

```text
home/blind/bed_room_right/set_position
```

Payload:

```text
35000
```

Flytter direkte til step-position.

---

### Gem bundposition

Topic:

```text
home/blind/bed_room_right/save_bottom
```

Payload kan være tom.

Aktuel position gemmes som bundposition.

---

### Gem topposition

Topic:

```text
home/blind/bed_room_right/save_top
```

Payload kan være tom.

Aktuel position gemmes som topposition.

---

## Status Topics

### Position i procent

```text
home/blind/bed_room_right/state
```

Eksempel:

```text
75
```

### Aktuel step-position

```text
home/blind/bed_room_right/CurrentPosition
```

Eksempel:

```text
52341
```

### Tilgængelighed

```text
home/blind/bed_room_right/availability
```

Mulige værdier:

```text
online
offline
```

---

## Home Assistant

Projektet anvender MQTT Discovery.

Discovery Topic:

```text
homeassistant/cover/esp8266_blind_<room>/config
```

Ved opstart registreres gardinet automatisk som:

* Cover Entity
* Device
* Availability Sensor

Ingen manuel YAML-konfiguration er nødvendig.

---

## EEPROM

Følgende data gemmes permanent:

| Adresse | Data             |
| ------- | ---------------- |
| 0       | Bottom Position  |
| 4       | Top Position     |
| 8       | Current Position |

Data gemmes automatisk når:

* Top position ændres
* Bund position ændres
* Gardinet når sin destination

---

## Første Opstart

Ved første opstart initialiseres:

```cpp
bottomPosition = 0;
topPosition = 70000;
currentPosition = 0;
```

Disse værdier bør efterfølgende kalibreres.

---

## Kalibrering

1. Kør gardinet til helt lukket position.
2. Send MQTT kommando til:

```text
home/blind/<room>/save_bottom
```

3. Kør gardinet til helt åben position.
4. Send MQTT kommando til:

```text
home/blind/<room>/save_top
```

5. Positionerne gemmes i EEPROM.

---

## Netværk

Projektet anvender statisk IP-adresse.

Konfiguration:

```cpp
WiFi.config(local_IP, gateway, subnet);
```

Hvis DHCP ønskes kan denne linje fjernes.

---

## Seriel Debug

Baudrate:

```text
115200
```

Ved opstart vises blandt andet:

```text
Connecting to WiFi
MQTT host: 192.168.1.10
Setup Completed
AA:BB:CC:DD:EE:FF
```

---

## Projektstruktur

```text
Esp12_Blind/
│
├── README.md
├── Schematic.png
│
├── Esp8266_blindController_HA/
│   ├── Esp8266_blindController_HA.ino
│   └── secrets.h
│
└── ES3P2_test/
    └── Esp32_blindController_HA/
```

---

## Kendte Begrænsninger

* Automatisk kalibrering er ikke implementeret.
* Endestopkontakter anvendes endnu ikke aktivt under normal drift.
* Positionering er baseret på step-tælling og ikke absolut feedback.
* Ved mekanisk slip kan positionen afvige.

---

## Fremtidige Forbedringer

* Automatisk kalibrering via endestop
* Web-interface
* OTA firmwareopdatering
* Hastighedsramper (Acceleration/Deceleration)
* Batteridrift
* Position backup til MQTT/Home Assistant

## Noter
Simple rullegardin styring baseret på Ikea rulle gardin og et 3D printet gear. Driver en lille stepper vha ULN2003.
I den nye hardware har jeg lagt ind at man også kan bruge en almindelig stepper driver til lidt mere highpower stepper

- Video [On Smarthome](https://www.facebook.com/Askob.dk/photos/a.409725012772869/409724829439554/?type=3&theater)
- [608ZZ kugleleje](https://arduinotech.dk/shop/kuglelejer/)
- [28BYJ-48](https://arduinotech.dk/shop/step-gear-motor-28byj-48-5v-4-phase-5-wire-dc-5v/) stepper motor

- [Schematic](https://github.com/gert-lauritsen/Esp12_Blind/blob/master/ESP12DualStepperPCB/RulleGardin.pdf)

---

## Referencer

* ESP8266
* Home Assistant MQTT Discovery
* PubSubClient
* ArduinoJson
* 28BYJ-48 Stepper Motor
* ULN2003 Driver
* DRV8825 Driver

## Licens

Projektet stilles til rådighed som open source. Tilpas og anvend frit på eget ansvar.

