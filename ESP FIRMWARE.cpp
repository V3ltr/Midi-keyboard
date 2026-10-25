
#include <Arduino.h>
#include <NimBLEDevice.h>

static const char* DEVICE_NAME = "S3_PIANO";
static const char* SERVICE_UUID = "8b7e1000-7c4a-4f3d-9a11-123456789abc";
static const char* CHAR_UUID = "8b7e1001-7c4a-4f3d-9a11-123456789abc";

const int touchPins[5] = {1, 2, 3, 4, 5};
const int bassPin = 6;
const int pitchPin = 7;
const int encoderA = 8;
const int encoderB = 9;
const int octaveButton = 10;

NimBLECharacteristic* eventCharacteristic = nullptr;

bool previousTouch[5] = {false, false, false, false, false};
unsigned long touchChangedAt[5] = {0, 0, 0, 0, 0};

const unsigned long debounceMs = 30;

int encoderPrevious = 0;
int buttonPrevious = HIGH;
int octaveValue = 0;

bool buttonStable = HIGH;
unsigned long buttonChangedAt = 0;

int previousBass = -1;
int previousPitch = -1;
int previousVolume = -1;

int volumeValue = 50;
int bassValue = 0;
int pitchValue = 50;

unsigned long lastControlRead = 0;

void sendEvent(const String& message) {
    if (eventCharacteristic == nullptr) {
        return;
    }

    NimBLEServer* server = NimBLEDevice::getServer();

    if (server != nullptr && server->getConnectedCount() > 0) {
        eventCharacteristic->setValue(message.c_str());
        eventCharacteristic->notify();
    }
}

void sendControls() {
    String message = "{\"type\":\"controls\",\"volume\":";
    message += String(volumeValue / 100.0f, 2);
    message += ",\"bass\":";
    message += String(bassValue / 100.0f, 2);
    message += ",\"pitch\":";
    message += String((pitchValue - 50) / 50.0f, 2);
    message += "}";

    sendEvent(message);
}

void setup() {
    Serial.begin(115200);

    analogReadResolution(12);

    for (int i = 0; i < 5; i++) {
        pinMode(touchPins[i], INPUT_PULLDOWN);
    }

    pinMode(encoderA, INPUT_PULLUP);
    pinMode(encoderB, INPUT_PULLUP);
    pinMode(octaveButton, INPUT_PULLUP);

    encoderPrevious =
        (digitalRead(encoderA) << 1) | digitalRead(encoderB);

    NimBLEDevice::init(DEVICE_NAME);

    NimBLEServer* server = NimBLEDevice::createServer();
    NimBLEService* service =
        server->createService(SERVICE_UUID);

    eventCharacteristic = service->createCharacteristic(
        CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    eventCharacteristic->setValue("{\"type\":\"status\",\"message\":\"Ready\"}");

    service->start();

    NimBLEAdvertising* advertising =
        NimBLEDevice::getAdvertising();

    advertising->addServiceUUID(SERVICE_UUID);
    advertising->start();

    Serial.println("S3 piano controller ready");
}

void loop() {
    unsigned long now = millis();

    for (int i = 0; i < 5; i++) {
        bool currentTouch = digitalRead(touchPins[i]) == HIGH;

        if (currentTouch != previousTouch[i] &&
            now - touchChangedAt[i] >= debounceMs) {

            previousTouch[i] = currentTouch;
            touchChangedAt[i] = now;

            String message = "{\"type\":\"touch\",\"id\":";
            message += String(i);
            message += ",\"pressed\":";
            message += currentTouch ? "true" : "false";
            message += "}";

            sendEvent(message);
        }
    }

    int encoderCurrent =
        (digitalRead(encoderA) << 1) | digitalRead(encoderB);

    if (encoderCurrent != encoderPrevious) {
        if ((encoderPrevious == 0 && encoderCurrent == 1) ||
            (encoderPrevious == 1 && encoderCurrent == 3) ||
            (encoderPrevious == 3 && encoderCurrent == 2) ||
            (encoderPrevious == 2 && encoderCurrent == 0)) {
            volumeValue = min(100, volumeValue + 3);
        } else if (
            (encoderPrevious == 0 && encoderCurrent == 2) ||
            (encoderPrevious == 2 && encoderCurrent == 3) ||
            (encoderPrevious == 3 && encoderCurrent == 1) ||
            (encoderPrevious == 1 && encoderCurrent == 0)) {
            volumeValue = max(0, volumeValue - 3);
        }

        encoderPrevious = encoderCurrent;
    }

    int buttonReading = digitalRead(octaveButton);

    if (buttonReading != buttonPrevious) {
        buttonPrevious = buttonReading;
        buttonChangedAt = now;
    }

    if (now - buttonChangedAt >= debounceMs &&
        buttonStable != buttonReading) {

        buttonStable = buttonReading;

        if (buttonStable == LOW) {
            octaveValue++;

            if (octaveValue > 3) {
                octaveValue = -3;
            }

            String message = "{\"type\":\"octave\",\"value\":";
            message += String(octaveValue);
            message += "}";

            sendEvent(message);
        }
    }

    if (now - lastControlRead >= 40) {
        lastControlRead = now;

        int rawBass = analogRead(bassPin);
        int rawPitch = analogRead(pitchPin);

        bassValue = map(rawBass, 0, 4095, 0, 100);
        pitchValue = map(rawPitch, 0, 4095, 0, 100);

        bool changed =
            abs(bassValue - previousBass) >= 2 ||
            abs(pitchValue - previousPitch) >= 2 ||
            abs(volumeValue - previousVolume) >= 3;

        if (changed) {
            previousBass = bassValue;
            previousPitch = pitchValue;
            previousVolume = volumeValue;

            sendControls();
        }
    }

    delay(2);
}
