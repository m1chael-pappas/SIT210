/*
  Task 8.1HD - Voice Activated Lighting System (Arduino side)
  SIT210 Embedded Systems Development
*/

#include <ArduinoBLE.h>
#include <Wire.h>
#include <BH1750.h>
#include <Servo.h>

// Pin map
const int BATHROOM_LED = 3;
const int HALLWAY_LED  = 4;
const int FAN_SERVO    = 9;

const float DARK_THRESHOLD_LUX = 50.0;

BLEService voiceService("19B10000-E8F2-537E-4F6C-D104768A1214");

BLEStringCharacteristic commandChar(
  "19B10001-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLEWrite, 20
);

BLEStringCharacteristic statusChar(
  "19B10002-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLENotify, 40
);

BH1750 lightMeter;
Servo fanServo;

bool bathroomOn = false;
bool hallwayOn  = false;
bool fanOn      = false;

float readLux() {
  return lightMeter.readLightLevel();
}

void setFan(bool on) {
  
  if (on == fanOn) return;
  fanOn = on;

  fanServo.attach(FAN_SERVO);
  if (on) {
    fanServo.write(180);
  } else {
    fanServo.write(0);
  }
  delay(500);          
  fanServo.detach();   
}

void publishStatus() {
  String s = String("bath=") + (bathroomOn ? "1" : "0")
           + ",hall="       + (hallwayOn  ? "1" : "0")
           + ",fan="        + (fanOn      ? "1" : "0");
  statusChar.writeValue(s);
  Serial.println("Status: " + s);
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  Serial.println("Command received: " + cmd);

  float lux = readLux();
  Serial.print("Ambient lux: "); Serial.println(lux);
  bool isDark = lux < DARK_THRESHOLD_LUX;

  if (cmd == "bathroom_on") {
    if (isDark) {
      bathroomOn = true;
      digitalWrite(BATHROOM_LED, HIGH);
    } else {
      Serial.println("Bathroom already bright enough, skipping LED.");
    }
  }
  else if (cmd == "hallway_on") {
    if (isDark) {
      hallwayOn = true;
      digitalWrite(HALLWAY_LED, HIGH);
    } else {
      Serial.println("Hallway already bright enough, skipping LED.");
    }
  }
  else if (cmd == "all_on") {
    if (isDark) {
      bathroomOn = true; hallwayOn = true;
      digitalWrite(BATHROOM_LED, HIGH);
      digitalWrite(HALLWAY_LED, HIGH);
    }
    setFan(true);
  }
  else if (cmd == "bathroom_off") {
    bathroomOn = false;
    digitalWrite(BATHROOM_LED, LOW);
  }
  else if (cmd == "hallway_off") {
    hallwayOn = false;
    digitalWrite(HALLWAY_LED, LOW);
  }
  else if (cmd == "all_off") {
    bathroomOn = false; hallwayOn = false;
    digitalWrite(BATHROOM_LED, LOW);
    digitalWrite(HALLWAY_LED, LOW);
    setFan(false);
  }
  else if (cmd == "fan_on")  { setFan(true);  }
  else if (cmd == "fan_off") { setFan(false); }
  else {
    Serial.println("Unknown command, ignoring.");
    return;
  }

  publishStatus();
}

void setup() {
  Serial.begin(9600);
  unsigned long start = millis();
  while (!Serial && millis() - start < 2000) { }

  pinMode(BATHROOM_LED, OUTPUT);
  pinMode(HALLWAY_LED,  OUTPUT);
  digitalWrite(BATHROOM_LED, LOW);
  digitalWrite(HALLWAY_LED,  LOW);

  // Move servo to OFF position once on startup, then detach
  fanServo.attach(FAN_SERVO);
  fanServo.write(0);
  delay(500);
  fanServo.detach();

  Wire.begin();
  if (!lightMeter.begin()) {
    Serial.println("BH1750 not found. Check wiring.");
  }

  if (!BLE.begin()) {
    Serial.println("BLE failed to start.");
    while (1) { }
  }

  BLE.setLocalName("VoiceLights");
  BLE.setAdvertisedService(voiceService);

  voiceService.addCharacteristic(commandChar);
  voiceService.addCharacteristic(statusChar);
  BLE.addService(voiceService);

  commandChar.writeValue("");
  statusChar.writeValue("bath=0,hall=0,fan=0");

  BLE.advertise();
  Serial.println("BLE advertising as 'VoiceLights'. Waiting for Pi...");
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected: "); Serial.println(central.address());

    while (central.connected()) {
      if (commandChar.written()) {
        String cmd = commandChar.value();
        handleCommand(cmd);
      }
    }

    Serial.println("Disconnected.");
  }
}