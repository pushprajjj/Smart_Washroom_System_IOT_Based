#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Replace Firebase credentials section
#define API_KEY "AIzaSyAujt_zf6fCCLEPICef4_VAd7W4rSQshJE"
#define DATABASE_URL "byte4genodemcu-default-rtdb.firebaseio.com"

// WiFi credentials
#define WIFI_SSID "Mr.MorningStar"
#define WIFI_PASSWORD "Byte4ge@7874"

// LED pin
#define LED_PIN 2  // Built-in LED on NodeMCU (GPIO2)

// Ultrasonic Sensor 1 (Object Detection) pins
#define TRIG_PIN_1 D1
#define ECHO_PIN_1 D2
#define OBJECT_PIN D5
#define MAX_DISTANCE_1 30 // Maximum distance to detect object (in cm)

// Ultrasonic Sensor 2 (Water Level) pins
#define TRIG_PIN_2 D3
#define ECHO_PIN_2 D4
#define WATER_MOTOR_PIN D8
#define TANK_HEIGHT 10   // Total height of tank in cm
#define MIN_WATER_LEVEL 30 // Minimum water level percentage

// Add MQ2 sensor and exhaust fan definitions
#define MQ2_PIN A0        // Analog pin for MQ2 sensor
#define EXHAUST_RELAY_PIN D7
#define GAS_THRESHOLD 24  // Adjust this threshold based on your needs




// Add auto flush relay pin and timing constants
#define FLUSH_RELAY_PIN D6
#define FLUSH_DURATION 10000  // 10 seconds in milliseconds
unsigned long flushStartTime = 0;
bool flushInProgress = false;
bool FlushOverride = false;

// Add these constants and variables near the top with other definitions
#define DISTANCE_THRESHOLD 10  // Distance threshold for object detection in cm
unsigned long currentMillis = 0;
unsigned long lastObjectTime = 0;
bool objectDetected = false;

// Replace Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  // Configure LED pin
  pinMode(LED_PIN, OUTPUT);

  // Configure ultrasonic sensor pins
  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);


  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);

  pinMode(OBJECT_PIN, OUTPUT);
  pinMode(WATER_MOTOR_PIN, OUTPUT);
  pinMode(MQ2_PIN, INPUT);
  pinMode(EXHAUST_RELAY_PIN, OUTPUT);
   pinMode(FLUSH_RELAY_PIN, OUTPUT);
   
  // Initialize relays to OFF
  digitalWrite(OBJECT_PIN, HIGH);
  digitalWrite(WATER_MOTOR_PIN, HIGH);
  digitalWrite(EXHAUST_RELAY_PIN, HIGH);
  digitalWrite(FLUSH_RELAY_PIN, HIGH);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Connected to WiFi");

  // Replace Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase authentication OK");
  } else {
    Serial.printf("Firebase authentication failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);


}

void loop() {
  // Add this at the start of loop()
  currentMillis = millis();







  // Object Detection Sensor
  long duration1, distance1;
  digitalWrite(TRIG_PIN_1, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN_1, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN_1, LOW);
  duration1 = pulseIn(ECHO_PIN_1, HIGH);
  distance1 = duration1 * 0.034 / 2;

  // Water Level Sensor
  long duration2, distance2;
  digitalWrite(TRIG_PIN_2, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN_2, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN_2, LOW);
  duration2 = pulseIn(ECHO_PIN_2, HIGH);
  distance2 = duration2 * 0.034 / 2;

  // Calculate water level percentage
  int waterLevelPercent = 100 - ((distance2 * 100) / TANK_HEIGHT);


  // Modified Water Level Logic

  if (waterLevelPercent <= MIN_WATER_LEVEL) {
    digitalWrite(WATER_MOTOR_PIN, LOW);
    Firebase.RTDB.setBool(&fbdo, "test/relays/WATER_MOTOR_PIN", true);
    Serial.println("Low Water Level - Water motor ON");
  } 

    if (waterLevelPercent >= 80) {
    digitalWrite(WATER_MOTOR_PIN, LOW);
    Firebase.RTDB.setBool(&fbdo, "test/relays/WATER_MOTOR_PIN", true);
    Serial.println("Low Water Level - Water motor ON");
  } 



  // Add MQ2 sensor reading and control logic
  int gasLevel = analogRead(MQ2_PIN);

  if (gasLevel > GAS_THRESHOLD) {
    digitalWrite(EXHAUST_RELAY_PIN, LOW);
    Firebase.RTDB.setBool(&fbdo, "test/relays/EXHAUST_RELAY_PIN", true);
    Serial.println("High Gas Level - Exhaust Fan ON");
  } else {
    digitalWrite(EXHAUST_RELAY_PIN, HIGH);
    Firebase.RTDB.setBool(&fbdo, "test/relays/EXHAUST_RELAY_PIN", false);
    Serial.println("Gas Level Normal - Exhaust Fan OFF");
  }


  // Modify your object detection logic to include flush control
  if (distance1 < DISTANCE_THRESHOLD) {
    if (!objectDetected) {
      objectDetected = true;
      lastObjectTime = currentMillis;
      Serial.println("Object detected!");
      digitalWrite(OBJECT_PIN, LOW);
      Firebase.RTDB.setBool(&fbdo, "test/relays/ObjectStatus", true);
    }
  } else {
    if (objectDetected) {
      objectDetected = false;
      if (!flushInProgress) {
        flushInProgress = true;
        flushStartTime = currentMillis;
        digitalWrite(FLUSH_RELAY_PIN, LOW);
        Firebase.RTDB.setBool(&fbdo, "test/relays/flushStatus", true);
        Serial.println("Flushing");
      }

      Serial.println("Object removed");
      digitalWrite(OBJECT_PIN, HIGH);
      Firebase.RTDB.setBool(&fbdo, "test/relays/ObjectStatus", false);
    }
  }




  // flush web control
  if (Firebase.RTDB.getBool(&fbdo, "/test/relays/FlushOverride")) {
    FlushOverride = fbdo.boolData();
    if (FlushOverride && !flushInProgress) {
      flushInProgress = true;
      flushStartTime = currentMillis;
      digitalWrite(FLUSH_RELAY_PIN, LOW);
      Serial.println("Flushing by web");
      Firebase.RTDB.setBool(&fbdo, "test/relays/flushStatus", true);
    }

  }


  // Add auto flush timer logic
  if (flushInProgress) {
    if (currentMillis - flushStartTime >= FLUSH_DURATION) {

      Firebase.RTDB.setBool(&fbdo, "test/relays/flushStatus", false);
      Firebase.RTDB.setBool(&fbdo, "test/relays/FlushOverride", false);
      digitalWrite(FLUSH_RELAY_PIN, HIGH);
      flushInProgress = false;
      Serial.println("Flush cycle completed");
    }
  }


  // Replace Firebase write operations
  Firebase.RTDB.setInt(&fbdo, "test/sensors/objectDistance", distance1);
  Firebase.RTDB.setInt(&fbdo, "test/sensors/waterLevel", waterLevelPercent);
  Firebase.RTDB.setInt(&fbdo, "test/sensors/gasLevel", gasLevel);





  // Add flush relay status to Firebase

  // Print sensor readings
  Serial.print("Object Distance: ");
  Serial.print(distance1);
  Serial.println(" cm");

  Serial.print("Water Level: ");
  Serial.print(waterLevelPercent);
  Serial.println("%");

  Serial.print("Gas Level: ");
  Serial.println(gasLevel);


}
