#define BLYNK_TEMPLATE_ID "TMPL6qzgyOInR"
#define BLYNK_TEMPLATE_NAME "Smart Plant Monitoring System"
#define BLYNK_AUTH_TOKEN "19V-e_JkWFzQlcZD_TK50LCfjdW-7f92"

#include <SPI.h>
#include <WiFi.h>
#include <BlynkSimpleWifi.h>
#include <Servo.h>
#include <TimeLib.h>
#include <EEPROM.h>

// WiFi credentials
char ssid[] = "HUAWEI-2.4G-6Mg3"; 
char pass[] = "gZGGB44Y";

// Servo setup
Servo myservo1;  // First servo motor
Servo myservo2;  // Second servo motor

// LDR setup
int ldrPin = A0;  // Pin where the photoresistor is connected
int lightThreshold = 800;  // Threshold value for detecting sunlight

// Moisture sensor
// Moisture sensor
int moisturePin = A1;  // Analog input pin for the moisture sensor
int moistureValue = 0;  // Store the moisture level
int moistureThreshold = 800;  // Threshold value for dry soil
int relayPin = 3;  // Relay pin to control the water pump

// Timing logic
unsigned long lastOpenTime = 0;  // Tracks when the servos were last opened
unsigned long sunlightAccumulatedMillis = 0;  // Accumulated sunlight exposure time
const unsigned long sunlightLimitMillis = 15 * 60 * 1000;  // 15 seconds for testing
bool exposureLimitReached = false;  // Flag to track if the daily limit is reached
int lastDay = 0;  // Tracks the last day for daily reset

// EEPROM Addresses
#define EXPOSURE_LIMIT_REACHED_ADDR 0
#define ACCUMULATED_MILLIS_ADDR 1
#define LAST_DAY_ADDR 5

// Blynk Virtual Pins
#define VIRTUAL_PIN_MOISTURE V1  // Moisture status
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

  // Reset EEPROM (you can remove this line after testing)
  resetEEPROM();

  // Initialize EEPROM
  exposureLimitReached = EEPROM.read(EXPOSURE_LIMIT_REACHED_ADDR);
  sunlightAccumulatedMillis = EEPROM.read(ACCUMULATED_MILLIS_ADDR) * 1000UL; // Stored in seconds
  lastDay = EEPROM.read(LAST_DAY_ADDR);

  Serial.println("Restored values from EEPROM:");
  Serial.print("Exposure Limit Reached: ");
  Serial.println(exposureLimitReached);
  Serial.print("Accumulated Millis: ");
  Serial.println(sunlightAccumulatedMillis);
  Serial.print("Last Day: ");
  Serial.println(lastDay);

  // Servo setup
  myservo1.attach(9);  // First servo on pin 9
  myservo2.attach(10); // Second servo on pin 10

  // Initialize the CLOSE roof
  myservo1.write(30);
  myservo2.write(30);

  // Initialize the clock
  setTime(0, 0, 0, 1, 1, 2023);  // (hour, minute, second, day, month, year)

  // Moisture and relay setup
  pinMode(moisturePin, INPUT);  // Set moisture sensor pin as input
  pinMode(relayPin, OUTPUT);   // Set relay pin as output
  digitalWrite(relayPin, LOW); // Ensure pump is initially OFF
}

void loop() {

    if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();  // Only run Blynk if Wi-Fi is connected
  } else {
    Serial.println("Blynk is offline. Continuing offline functionality.");
  }

  time_t currentTime = now();
  int currentDay = day(currentTime);

  // Check for a new day to reset exposure tracking
  if (currentDay != lastDay) {
    resetDailyExposure();
    lastDay = currentDay;

    // Save the new day in EEPROM
    EEPROM.update(LAST_DAY_ADDR, lastDay);
  }

  handleSunlightExposure();
  handleMoisture();
}

void handleSunlightExposure() {
  int ldrValue = analogRead(ldrPin);  // Read the value from the photoresistor
  Serial.print("LDR Value: ");
  Serial.println(ldrValue);
Blynk.virtualWrite(VIRTUAL_PIN_LDR, ldrValue);
  unsigned long currentMillis = millis();

  // If sunlight is detected and the daily limit is not yet reached
  if (ldrValue > lightThreshold && !exposureLimitReached) {
    Serial.println("Sunlight detected, attempting to open roof...");
    if (lastOpenTime == 0) {
    myservo1.write(140);
    myservo2.write(140);
      lastOpenTime = currentMillis;
      Serial.println("Roof opened.");
    if (!roofOpenTrigger) {
      if (WiFi.status() == WL_CONNECTED) {
        Blynk.logEvent("roof_status", "Roof is OPEN: sunlight detected");
      }
      roofOpenTrigger = true;
      roofCloseTrigger = false;
    }
    }

    if (lastOpenTime > 0) {
      unsigned long elapsedTime = currentMillis - lastOpenTime;
      Serial.print("Elapsed Time: ");
      Serial.println(elapsedTime);

      sunlightAccumulatedMillis += elapsedTime;
      lastOpenTime = currentMillis;

      if (sunlightAccumulatedMillis >= sunlightLimitMillis) {
        exposureLimitReached = true;
        closeRoof();

        EEPROM.update(EXPOSURE_LIMIT_REACHED_ADDR, exposureLimitReached);
        EEPROM.update(ACCUMULATED_MILLIS_ADDR, sunlightAccumulatedMillis / 1000UL);

        Serial.println("Daily sunlight limit reached. Roof closed.");

      if (!roofCloseTrigger) {
        if (WiFi.status() == WL_CONNECTED) {
          Blynk.logEvent("roof_status", "Roof is CLOSED: sunlight requirement met");
        }
        roofCloseTrigger = true;
        roofOpenTrigger = false;
      }
      }
    }
  } else {
    // If no sunlight is detected, close the roof if it was open
    if (lastOpenTime > 0) {
      closeRoof();
      lastOpenTime = 0;
      Serial.println("No sunlight detected. Roof closed.");
    }
   if (!roofCloseTrigger) {
      if (WiFi.status() == WL_CONNECTED) {
        Blynk.logEvent("roof_status", "Roof is CLOSED: no sunlight detected");
      }
      roofCloseTrigger = true;
      roofOpenTrigger = false;
    }
  }

  delay(100);
}

void closeRoof() {
  myservo1.write(30);
  myservo2.write(30);
  Serial.println("Roof closed.");
}

void resetDailyExposure() {
  sunlightAccumulatedMillis = 0;
  exposureLimitReached = false;

  // Reset values in EEPROM
  EEPROM.update(EXPOSURE_LIMIT_REACHED_ADDR, exposureLimitReached);
  EEPROM.update(ACCUMULATED_MILLIS_ADDR, sunlightAccumulatedMillis / 1000UL);

  Serial.println("New day detected. Exposure limit reset.");
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
    Serial.println("Soil is moist, pump OFF. Status sent to Blynk.");
  } else {  // If the soil is moist (above or equal to threshold)
    digitalWrite(relayPin, LOW);  // Turn off the pump
    if (WiFi.status() == WL_CONNECTED) {
      Blynk.virtualWrite(VIRTUAL_PIN_MOISTURE, "THE PLANT IS DRY. PUMP IS ON");
    }
    Serial.println("Soil is dry, pump ON. Status sent to Blynk.");
  }

  delay(1000);
}
// Function to reset EEPROM
void resetEEPROM() {
  // Reset the EEPROM to default values
  EEPROM.write(EXPOSURE_LIMIT_REACHED_ADDR, 0);
  EEPROM.write(ACCUMULATED_MILLIS_ADDR, 0);
  EEPROM.write(LAST_DAY_ADDR, 0);
  Serial.println("EEPROM has been reset.");
}
