/*
 * Task 2.1P – Temperature & Soil Moisture to ThingSpeak
 * Hardware : Arduino Nano 33 IoT
 * Sensors  : DHT22 on D2  |  Soil moisture (SIG) on A0
 * Fields   : Field 1 = Temperature (C), Field 2 = Soil Moisture (raw)
 */

#include <WiFiNINA.h>
#include <ThingSpeak.h>
#include <DHT.h>

// ── Pin definitions ────────────────────────────────────────
#define DHT_PIN 2 // D2  — DHT22 data line
#define DHT_TYPE DHT22
#define SOIL_PIN A0 // A0  — Soil sensor SIG pin

// ── WiFi credentials ───────────────────────────────────────
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";

// ── ThingSpeak config ──────────────────────────────────────
const unsigned long CHANNEL_ID = ;
const char *WRITE_API_KEY = "";
const unsigned long SEND_INTERVAL = 30000UL; // 30 seconds

// ── Objects ────────────────────────────────────────────────
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient client;

// ── Prototypes ─────────────────────────────────────────────
void connectToWiFi();
float readTemperature();
int readSoilMoisture();
void sendToThingSpeak(float temp, int soil);

// ──────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(9600);
    while (!Serial)
        ;

    Serial.println("=== Task 2.1P — Room Conditions Monitor ===");
    dht.begin();
    connectToWiFi();
    ThingSpeak.begin(client);
    Serial.println("Ready. Sending data every 30 seconds.");
}

// ──────────────────────────────────────────────────────────
void loop()
{
    static unsigned long lastSend = 0;

    if (millis() - lastSend >= SEND_INTERVAL)
    {
        lastSend = millis();

        float temp = readTemperature();
        int soil = readSoilMoisture();

        if (isnan(temp))
        {
            Serial.println("[ERROR] DHT22 read failed — skipping.");
            return;
        }

        Serial.print("[DATA] Temp: ");
        Serial.print(temp, 1);
        Serial.print(" C  |  Soil (raw): ");
        Serial.println(soil);

        sendToThingSpeak(temp, soil);
    }
}

// ──────────────────────────────────────────────────────────
// Connects to WiFi, retrying every 5s until successful
void connectToWiFi()
{
    Serial.print("Connecting to WiFi");
    while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED)
    {
        Serial.print(".");
        delay(5000);
    }
    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
}

// ──────────────────────────────────────────────────────────
// Returns temperature in Celsius from DHT22 (NaN on failure)
float readTemperature()
{
    return dht.readTemperature();
}

// ──────────────────────────────────────────────────────────
// Returns raw analog value 0–1023 (low = wet, high = dry)
int readSoilMoisture()
{
    return analogRead(SOIL_PIN);
}

// ──────────────────────────────────────────────────────────
// Sends both sensor readings to ThingSpeak via HTTP POST
void sendToThingSpeak(float temp, int soil)
{
    ThingSpeak.setField(1, temp);
    ThingSpeak.setField(2, soil);

    int code = ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);

    if (code == 200)
    {
        Serial.println("[THINGSPEAK] Update successful.");
    }
    else
    {
        Serial.print("[THINGSPEAK] Error, HTTP code: ");
        Serial.println(code);
    }
}