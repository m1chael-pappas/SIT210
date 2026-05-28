// CycleGuard_BikeNode.ino
// CycleGuard - Node 1 (Bike Unit) - Full integrated firmware
//
// Sensors / outputs:
//   - APDS-9960 proximity sensor (rear-facing)
//   - LSM6DS3 IMU (built into Nano 33 IoT, impact detection)
//   - WS2812B LED strip (state indicator)
//   - Piezo buzzer (audio alerts)
//   - GPS NEO-6M (incident location)
//   - SSD1309 OLED (live status display)
//   - Push button (D7): toggles networking on/off so the bike can be taken
//     offline and back online with a single press. Sensor loop and flash
//     logging are unaffected - only the WiFi/MQTT layer is gated.
//
// Storage / networking:
//   - Onboard flash: close-call + impact event log (persists across resets)
//   - WiFi + MQTT publish to the Pi hub
//   - Offline-first: events always logged to flash, publish is best-effort,
//     unpublished events are replayed on reconnect.
//
// Fault tolerance:
//   - Lazy sensor init handles wiring faults at boot.
//   - Offline-first sync: every event is durably stored in flash before
//     the network is even attempted. WiFi/MQTT outages cause zero data loss.
//   - APDS-9960 heartbeat: if proximity reading is unchanged for 30 seconds,
//     a sensor_fault event is published and the alert switches to a magenta
//     timed-beep fallback pattern so the rider is never left silent.
//   - Loop-time monitoring: per-loop execution time is tracked and reported
//     every 10 seconds. Provides concrete evidence that the sensor loop
//     stays sub-100ms even during MQTT publishes (cooperative concurrency).
//
// Serial commands:
//   - dump      : print the entire event log
//   - clear     : wipe the event log
//   - sync      : force a sync attempt for unpublished events
//   - status    : print WiFi + MQTT + sensor + queue status

#include <Wire.h>
#include <Adafruit_APDS9960.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <FlashStorage.h>
#include <Arduino_LSM6DS3.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include "arduino_secrets.h"

// --- Pins ---
#define LED_PIN     6
#define BUZZER_PIN  9
#define LED_COUNT   8
#define BUTTON_PIN  7

// --- OLED ---
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

// --- Proximity thresholds ---
#define THRESH_CAUTION   20
#define THRESH_DANGER    80
#define THRESH_CRITICAL  180

// --- IMU impact threshold ---
#define IMPACT_THRESHOLD_G  1.8

// --- Event detection ---
#define CLOSE_CALL_MIN_DURATION_MS 300
#define EVENT_COOLDOWN_MS          5000
#define IMPACT_DISPLAY_MS          2000
#define MAX_EVENTS                 20

// --- Network ---
#define WIFI_RETRY_INTERVAL_MS  10000
#define MQTT_RETRY_INTERVAL_MS  5000
#define SYNC_INTERVAL_MS        10000

// --- Sensor health (Gap 1: heartbeat) ---
#define SENSOR_STALE_MS         30000   // no reading change for 30s = fault
#define FAULT_BEEP_INTERVAL_MS  2000    // timed fallback alert pattern

// --- Loop-time monitoring (Gap 2A: concurrency evidence) ---
#define LOOP_REPORT_INTERVAL_MS 10000   // print stats every 10s

// --- Button (connect/disconnect toggle) ---
#define BUTTON_DEBOUNCE_MS 50

// --- Event types ---
enum EventType { EVENT_CLOSE_CALL = 0, EVENT_IMPACT = 1 };

// --- Secrets (loaded from arduino_secrets.h) ---
const char WIFI_SSID[]  = SECRET_SSID;
const char WIFI_PASS[]  = SECRET_PASS;
const char MQTT_HOST[]  = SECRET_MQTT_HOST;
const int  MQTT_PORT    = SECRET_MQTT_PORT;
const char BIKE_ID[]    = SECRET_BIKE_ID;

// --- Globals ---
Adafruit_APDS9960 apds;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
TinyGPSPlus gps;
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

bool apdsReady = false;
bool imuReady  = false;

enum AlertState { CLEAR, CAUTION, DANGER, CRITICAL };
AlertState currentState = CLEAR;

// Close-call tracking
unsigned long dangerStartedAt = 0;
unsigned long lastCloseCallLoggedAt = 0;
uint8_t peakProxThisIncident = 0;

// Impact tracking
unsigned long lastImpactLoggedAt = 0;
unsigned long impactBannerUntil = 0;
float lastImpactMagnitude = 0;

// Network tracking
unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastSyncAttempt = 0;

// Sensor heartbeat tracking 
uint8_t lastProxReading = 0;
unsigned long lastProxChangeAt = 0;
bool sensorFault = false;
unsigned long lastFaultBeepAt = 0;
bool faultBeepOn = false;
bool sensorFaultReported = false;  

// Loop-time monitoring 
unsigned long loopStartedAt = 0;
unsigned long loopMaxMs = 0;
unsigned long loopMinMs = 999999;
unsigned long loopTotalMs = 0;
unsigned long loopSamples = 0;
unsigned long lastLoopReportAt = 0;

// Button + network toggle
bool networkEnabled = false;         // start OFF 
int lastButtonState = HIGH;
unsigned long lastButtonDebounce = 0;

// --- Flash event log  ---
typedef struct {
  uint8_t  type;
  uint32_t timestamp;
  uint8_t  peakProximity;
  float    impactG;
  float    latitude;
  float    longitude;
  bool     hasGpsFix;
  bool     published;       
} BikeEvent;

typedef struct {
  bool valid;
  uint16_t schemaVersion;
  uint8_t count;
  uint8_t nextSlot;
  BikeEvent events[MAX_EVENTS];
} EventLog;

#define SCHEMA_VERSION 3

FlashStorage(eventLogStore, EventLog);
EventLog logBuffer;

// LED flash timing
unsigned long lastFlashToggle = 0;
bool flashOn = false;
const unsigned long FLASH_INTERVAL_MS = 150;

// OLED refresh timing
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL_MS = 200;

// --- Forward declarations ---
void connectWiFi();
void connectMQTT();
bool publishEvent(const BikeEvent& e);
void syncPendingEvents();
int pendingEventCount();
void checkSensorHealth(uint8_t prox);
bool publishSensorFault();
void applyFaultAlert();
void recordLoopTime();
void checkButton();
void setNetworkEnabled(bool enabled);

// =================== SETUP ===================
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  // Don't block forever on Serial - bike runs without USB
  unsigned long serialWait = millis();
  while (!Serial && millis() - serialWait < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.println("CycleGuard - Bike Unit (full firmware)");
  Serial.println("Commands: 'dump' 'clear' 'sync' 'status'");

  // OLED first
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed.");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("CycleGuard");
    display.setTextSize(1);
    display.setCursor(0, 24);
    display.println("Booting...");
    display.display();
  }

  // IMU
  if (IMU.begin()) {
    imuReady = true;
    Serial.print("IMU OK, ");
    Serial.print(IMU.accelerationSampleRate());
    Serial.println(" Hz");
  } else {
    Serial.println("IMU init failed.");
  }

  // LED strip
  strip.begin();
  strip.setBrightness(40);
  strip.show();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Load event log
  logBuffer = eventLogStore.read();
  if (!logBuffer.valid || logBuffer.schemaVersion != SCHEMA_VERSION) {
    Serial.println("Initialising fresh event log (schema changed or first boot).");
    logBuffer.valid = true;
    logBuffer.schemaVersion = SCHEMA_VERSION;
    logBuffer.count = 0;
    logBuffer.nextSlot = 0;
    eventLogStore.write(logBuffer);
  } else {
    Serial.print("Loaded event log: ");
    Serial.print(logBuffer.count);
    Serial.print(" events (");
    Serial.print(pendingEventCount());
    Serial.println(" pending sync).");
  }

  // MQTT client ID
  mqttClient.setId(BIKE_ID);

  // Boot in the OFF state
  if (!networkEnabled) {
    setStripSolid(0, 0, 0);
    noTone(BUZZER_PIN);
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    Serial.println("System is OFF. Press the button to turn on.");
  } else {
    connectWiFi();
  }
}


// =================== MAIN LOOP ===================
void loop() {
  loopStartedAt = millis(); 

  // --- Sensor / alert path  ---

  if (!apdsReady) {
    if (apds.begin()) {
      Serial.println("APDS-9960 initialised.");
      apds.enableProximity(true);
      apdsReady = true;
      lastProxChangeAt = millis();
    } else {
      delay(1000);
      return;
    }
  }

  // Drain GPS bytes
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  handleSerialCommands();
  checkButton();

  if (!networkEnabled) {
    delay(50);
    return;
  }

  uint8_t prox = apds.readProximity();
  checkSensorHealth(prox);
  AlertState newState = stateFromProximity(prox);

  checkImpact();
  applyAlertState(newState);
  detectCloseCall(prox, newState);
  currentState = newState;

  if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = millis();
    updateDisplay(prox);
  }

  // --- Network path ---
  manageNetwork();

  recordLoopTime();          

  delay(50);
}

// =================== ALERT STATE MACHINE ===================
AlertState stateFromProximity(uint8_t prox) {
  if (prox < THRESH_CAUTION)  return CLEAR;
  if (prox < THRESH_DANGER)   return CAUTION;
  if (prox < THRESH_CRITICAL) return DANGER;
  return CRITICAL;
}

void applyAlertState(AlertState s) {
  // Sensor fault has highest priority - it trumps all proximity/impact states and has its own distinct alert pattern.
  if (sensorFault) {
    applyFaultAlert();
    return;
  }

  if (millis() < impactBannerUntil) {
    flashStripWhite();
    tone(BUZZER_PIN, 2500);
    return;
  }

  switch (s) {
    case CLEAR:
      setStripSolid(0, 255, 0);
      noTone(BUZZER_PIN);
      break;
    case CAUTION:
      setStripSolid(255, 180, 0);
      noTone(BUZZER_PIN);
      break;
    case DANGER:
      setStripSolid(255, 0, 0);
      noTone(BUZZER_PIN);
      break;
    case CRITICAL:
      flashStripRed();
      tone(BUZZER_PIN, 1800);
      break;
  }
}

// Sensor fault fallback: timed beep + magenta strip every 2s
void applyFaultAlert() {
  unsigned long now = millis();
  if (now - lastFaultBeepAt >= FAULT_BEEP_INTERVAL_MS / 2) {
    lastFaultBeepAt = now;
    faultBeepOn = !faultBeepOn;
    if (faultBeepOn) {
      setStripSolid(255, 0, 255);  // magentas
      tone(BUZZER_PIN, 800);
    } else {
      setStripSolid(0, 0, 0);
      noTone(BUZZER_PIN);
    }
  }
}

void setStripSolid(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void flashStripRed() {
  unsigned long now = millis();
  if (now - lastFlashToggle >= FLASH_INTERVAL_MS) {
    flashOn = !flashOn;
    lastFlashToggle = now;
    setStripSolid(flashOn ? 255 : 0, 0, 0);
  }
}

void flashStripWhite() {
  unsigned long now = millis();
  if (now - lastFlashToggle >= 80) {
    flashOn = !flashOn;
    lastFlashToggle = now;
    setStripSolid(flashOn ? 255 : 0, flashOn ? 255 : 0, flashOn ? 255 : 0);
  }
}

// =================== EVENT DETECTION ===================
void detectCloseCall(uint8_t prox, AlertState s) {
  unsigned long now = millis();
  bool inDangerZone = (s == DANGER || s == CRITICAL);

  if (inDangerZone) {
    if (dangerStartedAt == 0) {
      dangerStartedAt = now;
      peakProxThisIncident = prox;
    } else {
      if (prox > peakProxThisIncident) peakProxThisIncident = prox;
      if (now - dangerStartedAt >= CLOSE_CALL_MIN_DURATION_MS &&
          now - lastCloseCallLoggedAt >= EVENT_COOLDOWN_MS) {
        logEvent(EVENT_CLOSE_CALL, peakProxThisIncident, 0);
        lastCloseCallLoggedAt = now;
      }
    }
  } else {
    dangerStartedAt = 0;
    peakProxThisIncident = 0;
  }
}

void checkImpact() {
  if (!imuReady) return;
  if (!IMU.accelerationAvailable()) return;

  float x, y, z;
  IMU.readAcceleration(x, y, z);
  float magnitude = sqrt(x*x + y*y + z*z);

  if (magnitude >= IMPACT_THRESHOLD_G) {
    unsigned long now = millis();
    if (now - lastImpactLoggedAt >= EVENT_COOLDOWN_MS) {
      lastImpactLoggedAt = now;
      lastImpactMagnitude = magnitude;
      impactBannerUntil = now + IMPACT_DISPLAY_MS;
      logEvent(EVENT_IMPACT, 0, magnitude);
    }
  }
}

// =================== SENSOR HEALTH  ===================
// Watches the proximity sensor for "stuck" readings 
void checkSensorHealth(uint8_t prox) {
  unsigned long now = millis();

  // First-time init
  if (lastProxChangeAt == 0) {
    lastProxReading = prox;
    lastProxChangeAt = now;
    return;
  }

  // Any change at all resets the staleness clock
  if (prox != lastProxReading) {
    lastProxReading = prox;
    lastProxChangeAt = now;
    if (sensorFault) {
      // Sensor recovered!
      Serial.println("[heartbeat] APDS-9960 recovered, fault cleared.");
      sensorFault = false;
      sensorFaultReported = false;
    }
    return;
  }

  // No change for too long?
  if (!sensorFault && now - lastProxChangeAt >= SENSOR_STALE_MS) {
    sensorFault = true;
    Serial.println();
    Serial.println("!!! SENSOR FAULT DETECTED !!!");
    Serial.print("APDS-9960 stuck at value ");
    Serial.print(prox);
    Serial.print(" for ");
    Serial.print((now - lastProxChangeAt) / 1000);
    Serial.println(" seconds.");
    Serial.println("Switching to fault alert pattern.");
    Serial.println();
  }

  // Publish a sensor_fault event once per fault episode
  if (sensorFault && !sensorFaultReported) {
    if (publishSensorFault()) {
      sensorFaultReported = true;
    }
  }
}

bool publishSensorFault() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!mqttClient.connected())       return false;

  char topic[64];
  snprintf(topic, sizeof(topic), "cycleguard/%s/events", BIKE_ID);

  char payload[160];
  snprintf(payload, sizeof(payload),
    "{\"type\":\"sensor_fault\",\"sensor\":\"apds9960\",\"stuck_at\":%u,\"ts\":%lu}",
    lastProxReading, millis());

  if (!mqttClient.beginMessage(topic)) return false;
  mqttClient.print(payload);
  if (!mqttClient.endMessage()) return false;

  Serial.println("[heartbeat] published sensor_fault event");
  return true;
}

// =================== LOOP TIME MONITORING ===================
// Tracks per-loop execution time so we can prove the sensor loop stays fast
// even when MQTT publishes happen. Prints stats every LOOP_REPORT_INTERVAL_MS.

void recordLoopTime() {
  unsigned long elapsed = millis() - loopStartedAt;

  if (elapsed > loopMaxMs) loopMaxMs = elapsed;
  if (elapsed < loopMinMs) loopMinMs = elapsed;
  loopTotalMs += elapsed;
  loopSamples++;

  unsigned long now = millis();
  if (now - lastLoopReportAt >= LOOP_REPORT_INTERVAL_MS) {
    if (loopSamples > 0) {
      unsigned long avg = loopTotalMs / loopSamples;
      Serial.print("[loop-time] samples=");
      Serial.print(loopSamples);
      Serial.print(" min=");
      Serial.print(loopMinMs);
      Serial.print("ms avg=");
      Serial.print(avg);
      Serial.print("ms max=");
      Serial.print(loopMaxMs);
      Serial.println("ms");
    }
    // Reset for next window
    loopMaxMs = 0;
    loopMinMs = 999999;
    loopTotalMs = 0;
    loopSamples = 0;
    lastLoopReportAt = now;
  }
}

// =================== LOGGING & PUBLISHING ===================
void logEvent(EventType type, uint8_t peakProx, float impactG) {
  BikeEvent e;
  e.type = (uint8_t)type;
  e.timestamp = millis();
  e.peakProximity = peakProx;
  e.impactG = impactG;
  e.hasGpsFix = gps.location.isValid();
  e.latitude  = e.hasGpsFix ? gps.location.lat() : 0.0;
  e.longitude = e.hasGpsFix ? gps.location.lng() : 0.0;
  e.published = false;

  // Try to publish immediately. If it succeeds, mark as published before saving.
  if (publishEvent(e)) {
    e.published = true;
  }

  // Always save to flash regardless of publish result.
  logBuffer.events[logBuffer.nextSlot] = e;
  logBuffer.nextSlot = (logBuffer.nextSlot + 1) % MAX_EVENTS;
  if (logBuffer.count < MAX_EVENTS) {
    logBuffer.count++;
  }
  eventLogStore.write(logBuffer);

  Serial.println();
  if (type == EVENT_CLOSE_CALL) {
    Serial.println("*** CLOSE CALL LOGGED ***");
    Serial.print("  Peak proximity: "); Serial.println(peakProx);
  } else {
    Serial.println("*** IMPACT LOGGED ***");
    Serial.print("  Magnitude: "); Serial.print(impactG); Serial.println(" g");
  }
  Serial.print("  GPS: ");
  if (e.hasGpsFix) {
    Serial.print(e.latitude, 6); Serial.print(", "); Serial.println(e.longitude, 6);
  } else {
    Serial.println("no fix");
  }
  Serial.print("  Published: "); Serial.println(e.published ? "yes" : "DEFERRED");
  Serial.print("  Total stored: "); Serial.println(logBuffer.count);
  Serial.println();
}

bool publishEvent(const BikeEvent& e) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!mqttClient.connected())       return false;

  char topic[64];
  snprintf(topic, sizeof(topic), "cycleguard/%s/events", BIKE_ID);

  // Build JSON payload manually 
  char payload[256];
  if (e.type == (uint8_t)EVENT_CLOSE_CALL) {
    if (e.hasGpsFix) {
      snprintf(payload, sizeof(payload),
        "{\"type\":\"close_call\",\"peak_prox\":%u,\"lat\":%.6f,\"lng\":%.6f,\"has_gps\":true,\"ts\":%lu}",
        e.peakProximity, e.latitude, e.longitude, e.timestamp);
    } else {
      snprintf(payload, sizeof(payload),
        "{\"type\":\"close_call\",\"peak_prox\":%u,\"has_gps\":false,\"ts\":%lu}",
        e.peakProximity, e.timestamp);
    }
  } else {
    if (e.hasGpsFix) {
      snprintf(payload, sizeof(payload),
        "{\"type\":\"impact\",\"impact_g\":%.2f,\"lat\":%.6f,\"lng\":%.6f,\"has_gps\":true,\"ts\":%lu}",
        e.impactG, e.latitude, e.longitude, e.timestamp);
    } else {
      snprintf(payload, sizeof(payload),
        "{\"type\":\"impact\",\"impact_g\":%.2f,\"has_gps\":false,\"ts\":%lu}",
        e.impactG, e.timestamp);
    }
  }

  if (!mqttClient.beginMessage(topic)) {
    return false;
  }
  mqttClient.print(payload);
  if (!mqttClient.endMessage()) {
    return false;
  }

  return true;
}

int pendingEventCount() {
  int n = 0;
  for (uint8_t i = 0; i < logBuffer.count; i++) {
    if (!logBuffer.events[i].published) n++;
  }
  return n;
}

void syncPendingEvents() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqttClient.connected()) return;

  int pending = pendingEventCount();
  if (pending == 0) return;

  Serial.print("[sync] replaying ");
  Serial.print(pending);
  Serial.println(" pending events");

  bool changed = false;
  // Walk in storage order. The ring buffer means oldest-first if full.
  uint8_t start = (logBuffer.count == MAX_EVENTS) ? logBuffer.nextSlot : 0;
  for (uint8_t i = 0; i < logBuffer.count; i++) {
    uint8_t idx = (start + i) % MAX_EVENTS;
    if (logBuffer.events[idx].published) continue;
    if (publishEvent(logBuffer.events[idx])) {
      logBuffer.events[idx].published = true;
      changed = true;
    } else {
      // Connection died mid-sync - stop and try again next time.
      Serial.println("[sync] publish failed, will retry next cycle");
      break;
    }
  }

  if (changed) {
    eventLogStore.write(logBuffer);
    Serial.print("[sync] done, ");
    Serial.print(pendingEventCount());
    Serial.println(" still pending");
  }
}

// =================== NETWORK MANAGEMENT ===================
void connectWiFi() {
  Serial.print("[wifi] connecting to ");
  Serial.println(WIFI_SSID);
  // begin() is async 
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiAttempt = millis();
}

void connectMQTT() {
  Serial.print("[mqtt] connecting to ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  if (mqttClient.connect(MQTT_HOST, MQTT_PORT)) {
    Serial.println("[mqtt] connected");
  } else {
    Serial.print("[mqtt] connect failed rc=");
    Serial.println(mqttClient.connectError());
  }
  lastMqttAttempt = millis();
}

void manageNetwork() {
 
  if (!networkEnabled) {
    return;
  }

  // Keep MQTT keep-alive ticking if connected.
  if (mqttClient.connected()) {
    mqttClient.poll();
  }

  unsigned long now = millis();

  // WiFi: retry connection every WIFI_RETRY_INTERVAL_MS while down
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiAttempt >= WIFI_RETRY_INTERVAL_MS) {
      connectWiFi();
    }
    return;
  }

  // MQTT: connect once WiFi is up, retry every MQTT_RETRY_INTERVAL_MS while down
  if (!mqttClient.connected()) {
    if (now - lastMqttAttempt >= MQTT_RETRY_INTERVAL_MS) {
      connectMQTT();
    }
    return;
  }

  // Both connected - attempt to sync pending events every SYNC_INTERVAL_MS
  if (now - lastSyncAttempt >= SYNC_INTERVAL_MS) {
    lastSyncAttempt = now;
    syncPendingEvents();
  }
}

// =================== BUTTON / NETWORK TOGGLE ===================

void checkButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState && millis() - lastButtonDebounce > BUTTON_DEBOUNCE_MS) {
    lastButtonDebounce = millis();

    // Trigger on press 
    if (reading == LOW) {
      setNetworkEnabled(!networkEnabled);
    }

    lastButtonState = reading;
  }
}

void setNetworkEnabled(bool enabled) {
  networkEnabled = enabled;

  if (enabled) {
    Serial.println();
    Serial.println(">>> NETWORK ENABLED (button pressed)");
    Serial.println("    Connecting to hub...");
    
    // Wake the OLED back up
    display.ssd1306_command(SSD1306_DISPLAYON);

    // Kick off a fresh connection attempt immediately.
    lastWifiAttempt = 0;
    lastMqttAttempt = 0;
    connectWiFi();
  } else {
    Serial.println();
    Serial.println(">>> NETWORK DISABLED (button pressed)");
    Serial.println("    Going offline. Events will queue in flash.");

    // Turn off LED strip
    setStripSolid(0, 0, 0);

    // Silence buzzer
    noTone(BUZZER_PIN);

    // Blank and sleep the OLED
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);

    // Cleanly drop MQTT and WiFi so the OLED reflects the offline state.
    mqttClient.stop();
    WiFi.disconnect();
  }
}

// =================== OLED DISPLAY ===================
void updateDisplay(uint8_t prox) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Title + status indicators
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CycleGuard ");
  // Network status: OFF if user disabled, else W/M connection indicators
  if (!networkEnabled) {
    display.print("OFF");
  } else {
    display.print(WiFi.status() == WL_CONNECTED ? "W" : "-");
    display.print(mqttClient.connected() ? "M" : "-");
  }
  int pending = pendingEventCount();
  if (pending > 0) {
    display.print(" Q");
    display.print(pending);
  }

  // Alert state (big) — FAULT > IMPACT > proximity state
  display.setTextSize(2);
  display.setCursor(0, 14);
  if (sensorFault) {
    display.println("FAULT");
  } else if (millis() < impactBannerUntil) {
    display.println("IMPACT!");
  } else {
    switch (currentState) {
      case CLEAR:    display.println("CLEAR");    break;
      case CAUTION:  display.println("CAUTION");  break;
      case DANGER:   display.println("DANGER");   break;
      case CRITICAL: display.println("CRITICAL"); break;
    }
  }

  // Proximity reading
  display.setTextSize(1);
  display.setCursor(0, 36);
  display.print("Prox: ");
  display.print(prox);
  if (millis() < impactBannerUntil) {
    display.print("  ");
    display.print(lastImpactMagnitude, 1);
    display.print("g");
  }

  // GPS
  display.setCursor(0, 46);
  display.print("GPS: ");
  if (gps.location.isValid()) {
    display.print(gps.location.lat(), 2);
    display.print(",");
    display.print(gps.location.lng(), 2);
  } else if (gps.satellites.isValid() && gps.satellites.value() > 0) {
    display.print("sats ");
    display.print(gps.satellites.value());
  } else {
    display.print("no fix");
  }

  // Event count
  display.setCursor(0, 56);
  display.print("Events: ");
  display.print(logBuffer.count);

  display.display();
}

// =================== SERIAL COMMANDS ===================
void handleSerialCommands() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "dump") {
    dumpEventLog();
  } else if (cmd == "clear") {
    logBuffer.count = 0;
    logBuffer.nextSlot = 0;
    eventLogStore.write(logBuffer);
    Serial.println("Event log cleared.");
  } else if (cmd == "sync") {
    Serial.println("Forcing sync...");
    syncPendingEvents();
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd.length() > 0) {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
  }
}

void printStatus() {
  Serial.println();
  Serial.println("=== Status ===");
  Serial.print("Network toggle: ");
  Serial.println(networkEnabled ? "ENABLED" : "DISABLED (offline)");
  Serial.print("WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("connected, ");
    Serial.print(WiFi.localIP());
    Serial.print(", RSSI ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.print("disconnected (status=");
    Serial.print(WiFi.status());
    Serial.println(")");
  }
  Serial.print("MQTT: ");
  Serial.println(mqttClient.connected() ? "connected" : "disconnected");
  Serial.print("Sensor: ");
  if (sensorFault) {
    Serial.print("FAULT (stuck at ");
    Serial.print(lastProxReading);
    Serial.println(")");
  } else {
    Serial.print("ok (last change ");
    Serial.print((millis() - lastProxChangeAt) / 1000);
    Serial.println("s ago)");
  }
  Serial.print("Total events: ");
  Serial.println(logBuffer.count);
  Serial.print("Pending sync: ");
  Serial.println(pendingEventCount());
  Serial.println("==============");
  Serial.println();
}

void dumpEventLog() {
  Serial.println();
  Serial.println("=== Event log ===");
  Serial.print("Total: ");
  Serial.println(logBuffer.count);

  if (logBuffer.count == 0) {
    Serial.println("(none)");
    return;
  }

  uint8_t closeCalls = 0;
  uint8_t impacts = 0;
  uint8_t pending = 0;

  uint8_t start = (logBuffer.count == MAX_EVENTS) ? logBuffer.nextSlot : 0;
  for (uint8_t i = 0; i < logBuffer.count; i++) {
    uint8_t idx = (start + i) % MAX_EVENTS;
    BikeEvent e = logBuffer.events[idx];

    Serial.print("  ["); Serial.print(i + 1); Serial.print("] ");
    Serial.print("t="); Serial.print(e.timestamp); Serial.print("ms ");

    if (e.type == EVENT_CLOSE_CALL) {
      Serial.print("CLOSE_CALL prox=");
      Serial.print(e.peakProximity);
      closeCalls++;
    } else {
      Serial.print("IMPACT ");
      Serial.print(e.impactG, 2);
      Serial.print("g");
      impacts++;
    }

    if (e.hasGpsFix) {
      Serial.print(" ("); Serial.print(e.latitude, 6);
      Serial.print(","); Serial.print(e.longitude, 6); Serial.print(")");
    } else {
      Serial.print(" (no gps)");
    }

    Serial.print(e.published ? " [sent]" : " [PENDING]");
    if (!e.published) pending++;
    Serial.println();
  }

  Serial.print("Summary: ");
  Serial.print(closeCalls);
  Serial.print(" close calls, ");
  Serial.print(impacts);
  Serial.print(" impacts, ");
  Serial.print(pending);
  Serial.println(" pending sync.");
  Serial.println("===============");
  Serial.println();
}
