#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <BH1750.h>
#include <Wire.h>

// WiFi credentials
const char *ssid = "";
const char *password = "";

// HiveMQ credentials
const char *mqttBroker = "";
int mqttPort = 8883;
const char *mqttUser = "";
const char *mqttPass = "";
const char *mqttTopic = "terrarium/light";

// Light threshold in lux
const float SUNLIGHT_THRESHOLD = 100.0;

BH1750 lightMeter;
WiFiSSLClient wifiClient;
MqttClient mqttClient(wifiClient);

// Track state changes
bool wasSunlit = false;
bool firstReading = true;

void setup()
{
    Serial.begin(9600);
    delay(2000);
    Wire.begin();
    lightMeter.begin();

    // Connect WiFi
    Serial.print("Connecting to WiFi");
    while (WiFi.begin(ssid, password) != WL_CONNECTED)
    {
        Serial.print(".");
        delay(5000);
    }
    Serial.println(" connected!");

    // Connect MQTT
    mqttClient.setUsernamePassword(mqttUser, mqttPass);
    Serial.print("Connecting to MQTT broker");
    while (!mqttClient.connect(mqttBroker, mqttPort))
    {
        Serial.print(".");
        Serial.println(mqttClient.connectError());
        delay(5000);
    }
    Serial.println(" connected!");
}

void loop()
{
    // Keep MQTT connection alive
    mqttClient.poll();

    // Reconnect if needed
    if (!mqttClient.connected())
    {
        Serial.println("MQTT disconnected, reconnecting...");
        mqttClient.connect(mqttBroker, mqttPort);
    }

    float lux = lightMeter.readLightLevel();
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");

    bool isSunlit = (lux >= SUNLIGHT_THRESHOLD);

    if (firstReading)
    {
        wasSunlit = isSunlit;
        firstReading = false;
        Serial.println(isSunlit ? "Initial state: SUNLIT" : "Initial state: DARK");
    }
    else if (isSunlit && !wasSunlit)
    {
        Serial.println(">>> Sunlight detected!");
        publishMessage("Sunlight detected on your terrarium!");
        wasSunlit = true;
    }
    else if (!isSunlit && wasSunlit)
    {
        Serial.println(">>> Sunlight stopped!");
        publishMessage("Sunlight has stopped hitting your terrarium.");
        wasSunlit = false;
    }

    delay(10000);
}

void publishMessage(const char *message)
{
    mqttClient.beginMessage(mqttTopic);
    mqttClient.print(message);
    mqttClient.endMessage();
    Serial.print("Published: ");
    Serial.println(message);
}