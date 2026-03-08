// D2 = Porch Light, D3 = Hallway Light, D4 = Button

const int PORCH_PIN = 2;
const int HALLWAY_PIN = 3;
const int BUTTON_PIN = 4;

const long PORCH_DURATION = 30000;   // 30 seconds
const long HALLWAY_DURATION = 60000; // 60 seconds

unsigned long porchStartTime = 0;
unsigned long hallwayStartTime = 0;

bool porchActive = false;
bool hallwayActive = false;

void setup()
{
    pinMode(PORCH_PIN, OUTPUT);
    pinMode(HALLWAY_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void turnOnPorchLight()
{
    digitalWrite(PORCH_PIN, HIGH);
    porchStartTime = millis();
    porchActive = true;
}

void turnOnHallwayLight()
{
    digitalWrite(HALLWAY_PIN, HIGH);
    hallwayStartTime = millis();
    hallwayActive = true;
}

void updatePorchLight()
{
    if (porchActive && (millis() - porchStartTime >= PORCH_DURATION))
    {
        digitalWrite(PORCH_PIN, LOW);
        porchActive = false;
    }
}

void updateHallwayLight()
{
    if (hallwayActive && (millis() - hallwayStartTime >= HALLWAY_DURATION))
    {
        digitalWrite(HALLWAY_PIN, LOW);
        hallwayActive = false;
    }
}

void checkButton()
{
    if (digitalRead(BUTTON_PIN) == LOW)
    {
        turnOnPorchLight();
        turnOnHallwayLight();
        delay(300);
    }
}

void loop()
{
    checkButton();
    updatePorchLight();
    updateHallwayLight();
}