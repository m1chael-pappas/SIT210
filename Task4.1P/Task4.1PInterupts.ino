#include <Wire.h>
#include <BH1750.h>

// Pin definitions
const int pirPin = 2;
const int switchPin = 3;
const int led1Pin = 4;
const int led2Pin = 5;

// Light threshold (lux) - below this is considered "dark"
const float luxThreshold = 50.0;

// Auto-off timer (milliseconds)
const unsigned long autoOffDelay = 30000; // 30 seconds

// State tracking
volatile bool motionTriggered = false;
volatile bool switchTriggered = false;
bool lightsOn = false;
bool motionActive = false;
unsigned long lastMotionDetected = 0;

// Debounce timing
volatile unsigned long lastMotionTime = 0;
volatile unsigned long lastSwitchTime = 0;
const unsigned long debounceDelay = 300;

// Light sensor
BH1750 lightMeter;

// ISR for PIR motion sensor
void motionISR()
{
    unsigned long now = millis();
    if (now - lastMotionTime > debounceDelay)
    {
        motionTriggered = true;
        lastMotionTime = now;
    }
}

// ISR for slider switch
void switchISR()
{
    unsigned long now = millis();
    if (now - lastSwitchTime > debounceDelay)
    {
        switchTriggered = true;
        lastSwitchTime = now;
    }
}

void setLights(bool state)
{
    lightsOn = state;
    digitalWrite(led1Pin, lightsOn ? HIGH : LOW);
    digitalWrite(led2Pin, lightsOn ? HIGH : LOW);
}

void setup()
{
    Serial.begin(9600);
    Wire.begin();

    // Initialise BH1750
    if (lightMeter.begin())
    {
        Serial.println("BH1750 initialised successfully.");
    }
    else
    {
        Serial.println("Error initialising BH1750!");
    }

    // Pin modes
    pinMode(pirPin, INPUT);
    pinMode(switchPin, INPUT_PULLUP);
    pinMode(led1Pin, OUTPUT);
    pinMode(led2Pin, OUTPUT);

    // LEDs off initially
    setLights(false);

    // Attach interrupts
    attachInterrupt(digitalPinToInterrupt(pirPin), motionISR, RISING);
    attachInterrupt(digitalPinToInterrupt(switchPin), switchISR, CHANGE);

    Serial.println("System ready. Waiting for PIR warmup (30s)...");
    delay(30000);
    Serial.println("System active. Monitoring for motion and switch input.");
}

void loop()
{
    // Handle motion interrupt
    if (motionTriggered)
    {
        motionTriggered = false;

        float lux = lightMeter.readLightLevel();
        Serial.print("Motion detected! Light level: ");
        Serial.print(lux);
        Serial.println(" lux");

        if (lux < luxThreshold)
        {
            if (!lightsOn)
            {
                setLights(true);
                Serial.println("It is dark - motion detected - lights ON");
            }
            // Reset the timer on every motion event
            motionActive = true;
            lastMotionDetected = millis();
        }
        else
        {
            Serial.println("It is bright - lights not needed.");
        }
    }

    // Handle switch interrupt
    if (switchTriggered)
    {
        switchTriggered = false;

        // Toggle lights and cancel any motion timer
        lightsOn = !lightsOn;
        setLights(lightsOn);
        motionActive = false;
        Serial.print("Switch used - lights toggled ");
        Serial.println(lightsOn ? "ON" : "OFF");
    }

    // Auto-off after no motion for 30 seconds
    if (motionActive && lightsOn && (millis() - lastMotionDetected > autoOffDelay))
    {
        motionActive = false;
        setLights(false);
        Serial.println("No motion for 30s - lights OFF");
    }
}