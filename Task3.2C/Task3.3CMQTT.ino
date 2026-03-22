#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>

// WiFi credentials
const char *ssid = "";
const char *password = "";

// EMQX public broker (no auth needed)
const char *mqttBroker = "broker.emqx.io";
int mqttPort = 1883;

// Pin definitions
const int trigPin = 2;
const int echoPin = 3;
const int hallwayLED = 4;
const int bathroomLED = 5;

const float DETECT_DISTANCE = 12.0;
const unsigned long PAT_THRESHOLD = 1500;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char *myName = "Michael";

// Gesture state tracking
bool objectPresent = false;
unsigned long gestureStartTime = 0;

void setup()
{
    Serial.begin(9600);
    delay(2000);

    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    pinMode(hallwayLED, OUTPUT);
    pinMode(bathroomLED, OUTPUT);

    Serial.print("Connecting to WiFi");
    while (WiFi.begin(ssid, password) != WL_CONNECTED)
    {
        Serial.print(".");
        delay(5000);
    }
    Serial.println(" connected!");

    mqttClient.setId("michael-arduino-210");

    mqttClient.onMessage(onMqttMessage);

    Serial.print("Connecting to MQTT broker");
    while (!mqttClient.connect(mqttBroker, mqttPort))
    {
        Serial.print(".");
        delay(5000);
    }
    Serial.println(" connected!");

    mqttClient.subscribe("ES/Wave");
    mqttClient.subscribe("ES/Pat");
    Serial.println("Subscribed to ES/Wave and ES/Pat");

    delay(1000);
    mqttClient.beginMessage("ES/Wave");
    mqttClient.print("Startup test");
    mqttClient.endMessage();
    Serial.println("Published startup test to ES/Wave");
}

void loop()
{
    mqttClient.poll();

    if (!mqttClient.connected())
    {
        Serial.println("MQTT disconnected, reconnecting...");
        if (mqttClient.connect(mqttBroker, mqttPort))
        {
            mqttClient.subscribe("ES/Wave");
            mqttClient.subscribe("ES/Pat");
        }
    }

    detectGesture();
    delay(50);
}

float getDistance()
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 30000);
    if (duration == 0)
        return 999.0;
    return (duration * 0.0343) / 2.0;
}

void detectGesture()
{
    float distance = getDistance();

    if (distance < DETECT_DISTANCE && !objectPresent)
    {
        objectPresent = true;
        gestureStartTime = millis();
        Serial.print("Object detected at ");
        Serial.print(distance);
        Serial.println(" cm");
    }
    else if (distance >= DETECT_DISTANCE && objectPresent)
    {
        unsigned long holdTime = millis() - gestureStartTime;
        objectPresent = false;

        Serial.print("Hold time: ");
        Serial.print(holdTime);
        Serial.println(" ms");

        if (holdTime >= PAT_THRESHOLD)
        {
            Serial.println(">>> PAT detected!");
            String message = String(myName) + " - Pat detected";
            mqttClient.beginMessage("ES/Pat");
            mqttClient.print(message);
            mqttClient.endMessage();
            Serial.println("Published to ES/Pat");
        }
        else if (holdTime > 50)
        {
            Serial.println(">>> WAVE detected!");
            String message = String(myName) + " - Wave detected";
            mqttClient.beginMessage("ES/Wave");
            mqttClient.print(message);
            mqttClient.endMessage();
            Serial.println("Published to ES/Wave");
        }

        delay(1500); // Debounce
    }
}

void onMqttMessage(int messageSize)
{
    String topic = mqttClient.messageTopic();
    String message = "";
    while (mqttClient.available())
    {
        message += (char)mqttClient.read();
    }

    Serial.print(">>> Received on ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(message);

    if (topic == "ES/Wave")
    {
        digitalWrite(hallwayLED, HIGH);
        digitalWrite(bathroomLED, HIGH);
        Serial.println("LEDs ON (Wave)");
    }
    else if (topic == "ES/Pat")
    {
        digitalWrite(hallwayLED, LOW);
        digitalWrite(bathroomLED, LOW);
        Serial.println("LEDs OFF (Pat)");
    }
}