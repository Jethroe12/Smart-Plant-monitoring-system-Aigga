#define BLYNK_TEMPLATE_ID "TMPL6nwHdQPte"
#define BLYNK_TEMPLATE_NAME "SMART PLANT MONITORING"
#define BLYNK_AUTH_TOKEN "B4J7MbIH39GTAEGycp6kqxpALmt2kkDT"

#define BLYNK_PRINT Serial

#include <SPI.h>
#include <WiFi.h>
#include <BlynkSimpleWifi.h>
#include <Servo.h>

// WiFi credentials
char ssid[] = "Redmi 12C66"; 
char pass[] = "123456789066";

// Servo motor declarations
Servo myservo1;  // First servo motor
Servo myservo2;  // Second servo motor

// LDR sensor
int ldrPin = A0;         // Pin for the photoresistor
int lightThreshold = 800;  // Threshold value for detecting sunlight
unsigned long lastMoveTime = 0;  // Track the last time the servos moved (30-second timer)
unsigned long last60SecMoveTime = 0;  // Track the last time for the 60-second timer
unsigned long accumulatedTime30Sec = 0;  // Accumulated time for the 30-second timer
unsigned long accumulatedTime60Sec = 0;  // Accumulated time for the 60-second timer
unsigned long timerDuration30Sec = 15000;  // 15 seconds in milliseconds
unsigned long timerDuration60Sec = 30000;  // 30 seconds in milliseconds
bool at130Degrees = false;  // Tracks if servos are at 130 degrees
bool counting30Sec = false;  // Tracks if we are counting the 30-second timer
bool pauseCounting30Sec = false;  // Flag to pause the 30-second timer when sunlight is lost
unsigned long pausedTime30Sec = 0;  // Tracks paused time for the 30-second timer

// Moisture sensor
int moisturePin = A1;  // Analog input pin for the moisture sensor
int moistureValue = 0;  // Store the moisture level
int moistureThreshold = 700;  // Threshold value for dry soil
int relayPin = 3;  // Relay pin to control the water pump

// Blynk Virtual Pins
#define VIRTUAL_PIN_MOISTURE V1  // Moisture status
#define VIRTUAL_PIN_ROOF V2      // Roof status
#define VIRTUAL_PIN_LDR V4   //Chart

// For sending Blynk notifications
bool roofOpenTrigger = false;
bool roofCloseTrigger = false;

void setup() {
  Serial.begin(9600);

  // Attempt Wi-Fi connection with a timeout
  WiFi.begin(ssid, pass);
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiTimeout < 10000) {  // 10-second timeout
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    // Blynk initialization
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  } else {
    Serial.println("WiFi connection failed. Running offline.");
  }

  // Servo setup
  myservo1.attach(9);  // First servo on pin 9
  myservo2.attach(10); // Second servo on pin 10

  // Close servo
  myservo1.write(30);
  myservo2.write(30);

  // Moisture sensor setup
  pinMode(relayPin, OUTPUT);  // Set relay pin as output

  // Initialize timers
  lastMoveTime = millis();
  last60SecMoveTime = millis();
  Serial.println("System started.");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();  // Only run Blynk if Wi-Fi is connected
  } else {
    Serial.println("Blynk is offline. Continuing offline functionality.");
  }

  handleLDR();
  handleMoisture();
  handleTimers();
  updateRoofStatus();
}

void handleLDR() {
  int ldrValue = analogRead(ldrPin);
  Serial.print("LDR Value: ");
  Serial.println(ldrValue);
Blynk.virtualWrite(VIRTUAL_PIN_LDR, ldrValue);

  if (ldrValue > lightThreshold && !at130Degrees && !counting30Sec) {
    myservo1.write(130);
    myservo2.write(130);
    lastMoveTime = millis();
    at130Degrees = true;
    counting30Sec = true;
    pausedTime30Sec = 0;
    Serial.println("Sunlight detected. Moving to 130 degrees, starting 30-second count.");
    if (!roofOpenTrigger) {
      if (WiFi.status() == WL_CONNECTED) {
        Blynk.logEvent("roof_status", "Roof is OPEN: sunlight detected");
      }
      roofOpenTrigger = true;
      roofCloseTrigger = false;
    }
  }

  if (at130Degrees && counting30Sec && !pauseCounting30Sec) {
    unsigned long currentMillis = millis();
    accumulatedTime30Sec = currentMillis - lastMoveTime;
    Serial.print("Accumulated time for 30 seconds: ");
    Serial.println(accumulatedTime30Sec / 1000);

    if (accumulatedTime30Sec >= timerDuration30Sec) {
      myservo1.write(30);
      myservo2.write(30);
      Serial.println("30 seconds passed, moving to 30 degrees (closed).");
      counting30Sec = false;
      pauseCounting30Sec = false;
      if (!roofCloseTrigger) {
        if (WiFi.status() == WL_CONNECTED) {
          Blynk.logEvent("roof_status", "Roof is CLOSED: sunlight requirement met");
        }
        roofCloseTrigger = true;
        roofOpenTrigger = false;
      }
    }
  }

  if (ldrValue <= lightThreshold && at130Degrees) {
    myservo1.write(30);
    myservo2.write(30);
    if (counting30Sec) {
      pauseCounting30Sec = true;
      pausedTime30Sec = millis() - lastMoveTime;
      counting30Sec = false;
      Serial.println("No sunlight detected, moving to 30 degrees and pausing 30-second count.");
    }
    if (!roofCloseTrigger) {
      if (WiFi.status() == WL_CONNECTED) {
        Blynk.logEvent("roof_status", "Roof is CLOSED: no sunlight detected");
      }
      roofCloseTrigger = true;
      roofOpenTrigger = false;
    }
  }

  if (ldrValue > lightThreshold && pauseCounting30Sec) {
    lastMoveTime = millis() - pausedTime30Sec;
    counting30Sec = true;
    pauseCounting30Sec = false;
    myservo1.write(130);
    myservo2.write(130);
    at130Degrees = true;
    Serial.println("Sunlight detected again. Resuming 30-second count and moving to 130 degrees.");
    if (!roofOpenTrigger) {
      if (WiFi.status() == WL_CONNECTED) {
        Blynk.logEvent("roof_status", "Roof is OPEN: sunlight detected");
      }
      roofOpenTrigger = true;
      roofCloseTrigger = false;
    }
  }
}

void handleTimers() {
  unsigned long currentMillis = millis();
  accumulatedTime60Sec = currentMillis - last60SecMoveTime;
  Serial.print("Accumulated time for 60 seconds: ");
  Serial.println(accumulatedTime60Sec / 1000);

  if (accumulatedTime60Sec >= timerDuration60Sec) {
    accumulatedTime60Sec = 0;
    last60SecMoveTime = millis();
    at130Degrees = false;
    counting30Sec = false;
    Serial.println("60 seconds completed. System reset, starting new cycle.");
  }
}

void updateRoofStatus() {
  if (at130Degrees) {
    if (WiFi.status() == WL_CONNECTED) {
      Blynk.virtualWrite(VIRTUAL_PIN_ROOF, "THE ROOF IS OPEN");
    }
    Serial.println("The roof is open. Status sent to Blynk.");
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      Blynk.virtualWrite(VIRTUAL_PIN_ROOF, "THE ROOF IS CLOSED");
    }
    Serial.println("The roof is closed. Status sent to Blynk.");
  }
}

void handleMoisture() {
  moistureValue = analogRead(moisturePin);  // Read the analog moisture sensor value
  Serial.print("Moisture Value: ");
  Serial.println(moistureValue);

  if (moistureValue < moistureThreshold) {  // If the soil is dry (below threshold)
    digitalWrite(relayPin, HIGH);  // Turn on the pump
    if (WiFi.status() == WL_CONNECTED) {
      Blynk.virtualWrite(VIRTUAL_PIN_MOISTURE, "THE PLANT HAS ENOUGH WATER. PUMP IS OFF");
    }
    Serial.println("Soil is dry, pump ON. Status sent to Blynk.");
  } else {  // If the soil is moist (above or equal to threshold)
    digitalWrite(relayPin, LOW);  // Turn off the pump
    if (WiFi.status() == WL_CONNECTED) {
      Blynk.virtualWrite(VIRTUAL_PIN_MOISTURE, "THE PLANT IS DRY. PUMP IS ON");
    }
    Serial.println("Soil is moist, pump OFF. Status sent to Blynk.");
  }

  delay(1000);
}