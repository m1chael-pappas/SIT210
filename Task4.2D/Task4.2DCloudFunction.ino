#include <WiFiNINA.h>
#include <PubSubClient.h>

// WiFi credentials
const char *ssid = "Your_SSID";
const char *password = "Your_PASSWORD";

// HiveMQ broker credentials
const char *mqttServer = "broker.hivemq.com";
const int mqttPort = 8883;
const char *mqttUser = "";
const char *mqttPassword = "";

// LED pins
const int livingRoomPin = 3;
const int bathroomPin = 4;
const int closetPin = 5;

// LED states
bool livingRoomState = false;
bool bathroomState = false;
bool closetState = false;

WiFiSSLClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setup()
{
    Serial.begin(9600);
    delay(1500);

    pinMode(livingRoomPin, OUTPUT);
    pinMode(bathroomPin, OUTPUT);
    pinMode(closetPin, OUTPUT);

    // Connect to WiFi
    Serial.print("Connecting to WiFi");
    while (WiFi.begin(ssid, password) != WL_CONNECTED)
    {
        Serial.print(".");
        delay(3000);
    }
    Serial.println("\nWiFi connected!");

    // Connect to MQTT broker
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    connectMQTT();
}

void loop()
{
    if (!mqttClient.connected())
    {
        connectMQTT();
    }
    mqttClient.loop();
}

void connectMQTT()
{
    while (!mqttClient.connected())
    {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect("ArduinoNano33", mqttUser, mqttPassword))
        {
            Serial.println("connected!");
            mqttClient.subscribe("linda/lights");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.println(mqttClient.state());
            delay(3000);
        }
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    // Convert payload to string
    String message = "";
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    Serial.println("Received: " + message);

    // Toggle the corresponding LED
    if (message == "living room")
    {
        livingRoomState = !livingRoomState;
        digitalWrite(livingRoomPin, livingRoomState ? HIGH : LOW);
        Serial.println("Living Room: " + String(livingRoomState ? "ON" : "OFF"));
    }
    else if (message == "bathroom")
    {
        bathroomState = !bathroomState;
        digitalWrite(bathroomPin, bathroomState ? HIGH : LOW);
        Serial.println("Bathroom: " + String(bathroomState ? "ON" : "OFF"));
    }
    else if (message == "closet")
    {
        closetState = !closetState;
        digitalWrite(closetPin, closetState ? HIGH : LOW);
        Serial.println("Closet: " + String(closetState ? "ON" : "OFF"));
    }
}