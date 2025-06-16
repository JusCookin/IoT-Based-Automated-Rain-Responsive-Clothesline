// ESP32 code for automatic cloth protection system with Google Sheets integration
// This system monitors rain and moves clothes to a covered area using a servo
// It also displays the system status on an OLED display and sends data to Google Sheets

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <DHT.h> // Include DHT library for DHT11 sensor

// WiFi credentials
const char* ssid = "";//ur ssid
const char* password = "";//ur password

// Google Script ID
String GOOGLE_SCRIPT_ID = ""; // Store in EEPROM

// EEPROM settings
#define EEPROM_SIZE 512
#define EEPROM_SCRIPT_ID_ADDR 0
#define MAX_SCRIPT_ID_LENGTH 100

// Pin definitions
const int rainSensorPin = 34;   // Analog pin for MH-RD rain sensor
const int rainDigitalPin = 35;  // Digital output of MH-RD sensor (optional)
const int servoPin = 13;        // PWM pin for SG90 servo motor
const int DHTPIN = 32;          // DHT11 data pin (replacing the temperature sensor)
#define DHTTYPE DHT11           // DHT11 sensor type

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Threshold values
const int rainThreshold = 2000; // Threshold for rain detection (adjust as needed)

// Position values for servo (in degrees) - REVERSED FROM ORIGINAL
const int outsidePosition = 180;  // Position when clothes are outside (was 0)
const int coveredPosition = 0;    // Position when clothes are in covered area (was 180)

// Variables
int rainValue = 0;
bool isRaining = false;
bool clothesProtected = false;
float temperature = 0.0;
float humidity = 0.0;
unsigned long lastDataSend = 0;
const unsigned long dataSendInterval = 30000; // Reduced to 30 seconds to ensure more frequent updates
unsigned long lastSuccessfulSend = 0;         // Track when the last successful data send occurred
const unsigned long resendTimeout = 15000;    // Retry sending data after 15 seconds if unsuccessful

// Create servo object
Servo clothesServo;

// Flag to track if data needs to be sent due to status change
bool forceDataSend = false;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  EEPROM.begin(EEPROM_SIZE);
  
  // Load Google Script ID from EEPROM
  loadScriptIDFromEEPROM();
  
  // Initialize pins
  pinMode(rainSensorPin, INPUT);
  if (rainDigitalPin >= 0) {
    pinMode(rainDigitalPin, INPUT);
  }
  
  // Initialize DHT11 sensor
  dht.begin();
  
  // Attach servo to pin
  clothesServo.setPeriodHertz(50);   // Standard 50Hz servo
  clothesServo.attach(servoPin, 500, 2400); // Adjust min/max pulse width if needed
  
  // Set initial position (clothes outside)
  clothesServo.write(outsidePosition);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  // Initial setup for display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Rain Protection System"));
  display.println(F("Initializing..."));
  display.display();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Initial delay to stabilize
  delay(2000);
  
  Serial.println("Rain sensor cloth protection system initialized");
  updateDisplay("System ready", "Monitoring for rain", false);
  
  // Send initial data to confirm connection
  forceDataSend = true;
  sendDataToGoogleSheets();
}

void loop() {
  // Read rain sensor value (analog)
  rainValue = analogRead(rainSensorPin);
  
  // Read temperature and humidity from DHT11
  readDHTSensor();
  
  // Print the current readings
  Serial.print("Rain sensor value: ");
  Serial.println(rainValue);
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print("°C, Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
  
  // Store previous state to detect changes
  bool wasRaining = isRaining;
  bool wasProtected = clothesProtected;
  
  // Check if it's raining (lower value means more rain for MH-RD)
  if (rainValue < rainThreshold) {
    isRaining = true;
    
    // Move clothes to covered area if not already protected
    if (!clothesProtected) {
      Serial.println("Rain detected! Moving clothes to covered area...");
      updateDisplay("RAIN DETECTED!", "Moving clothes...", true);
      moveClothesToCoveredArea();
      clothesProtected = true;
      updateDisplay("RAIN DETECTED!", "Clothes protected", true);
      forceDataSend = true;  // Force data send when status changes
    }
  } else {
    isRaining = false;
    
    // Move clothes back outside if currently protected
    if (clothesProtected) {
      Serial.println("No rain detected. Moving clothes back outside...");
      updateDisplay("No rain detected", "Moving clothes out...", false);
      moveClothesToOutside();
      clothesProtected = false;
      updateDisplay("No rain detected", "Clothes outside", false);
      forceDataSend = true;  // Force data send when status changes
    } else {
      // Regularly update the display with current status
      updateDisplay("No rain detected", "Clothes outside", false);
    }
  }
  
  // If there was a state change, log it
  if (wasRaining != isRaining || wasProtected != clothesProtected) {
    Serial.println("Status changed - Rain: " + String(isRaining ? "Yes" : "No") + 
                  ", Clothes: " + String(clothesProtected ? "Protected" : "Outside"));
  }
  
  // Send data to Google Sheets periodically or if forced
  unsigned long currentMillis = millis();
  if (forceDataSend || (currentMillis - lastDataSend >= dataSendInterval)) {
    sendDataToGoogleSheets();
    lastDataSend = currentMillis;
    forceDataSend = false;
  }
  
  // If the last send attempt wasn't successful, try again after resendTimeout
  if (currentMillis - lastSuccessfulSend > resendTimeout && 
      currentMillis - lastDataSend > resendTimeout) {
    Serial.println("Retrying data send after timeout...");
    sendDataToGoogleSheets();
    lastDataSend = currentMillis;
  }
  
  // Check for new script ID from serial
  checkForNewScriptID();
  
  // Check every 5 seconds
  delay(5000);
}

void readDHTSensor() {
  // Read temperature and humidity from DHT11
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();
  
  // Check if reading was successful
  if (isnan(newHumidity) || isnan(newTemperature)) {
    Serial.println("Failed to read from DHT sensor!");
    // Keep previous values if reading failed
  } else {
    // Update only if reading was successful
    humidity = newHumidity;
    temperature = newTemperature;
  }
}

void moveClothesToCoveredArea() {
  // Gradually move servo to covered position
  for (int pos = outsidePosition; pos >= coveredPosition; pos -= 5) {
    clothesServo.write(pos);
    delay(100); // Move slowly
  }
  
  Serial.println("Clothes are now protected!");
}

void moveClothesToOutside() {
  // Gradually move servo to outside position
  for (int pos = coveredPosition; pos <= outsidePosition; pos += 5) {
    clothesServo.write(pos);
    delay(100); // Move slowly
  }
  
  Serial.println("Clothes moved back outside.");
}

void updateDisplay(String line1, String line2, bool rainStatus) {
  display.clearDisplay();
  
  // Draw header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Rain Protection System"));
  display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
  
  // Draw status
  display.setCursor(0, 15);
  display.setTextSize(1);
  display.println(line1);
  display.setCursor(0, 25);
  display.println(line2);
  
  // Draw rain status icon
  display.setCursor(0, 40);
  display.print(F("Rain: "));
  if (rainStatus) {
    display.println(F("YES"));
    // Draw raindrop icon
    for (int i = 0; i < 3; i++) {
      display.drawLine(90 + (i*10), 40, 95 + (i*10), 45, SSD1306_WHITE);
      display.drawLine(90 + (i*10), 40, 85 + (i*10), 45, SSD1306_WHITE);
      display.drawLine(85 + (i*10), 45, 95 + (i*10), 45, SSD1306_WHITE);
    }
  } else {
    display.println(F("NO"));
    // Draw sun icon
    display.drawCircle(90, 40, 8, SSD1306_WHITE);
  }
  
  // Draw sensor readings
  display.setCursor(0, 50);
  display.print(F("Sensor: "));
  display.println(rainValue);
  
  // Add both temperature and humidity
  display.setCursor(0, 58);
  display.print(F("T:"));
  display.print(temperature);
  display.print(F("C"));
  
  display.setCursor(70, 58);
  display.print(F("H:"));
  display.print(humidity);
  display.print(F("%"));
  
  display.display();
}

void connectToWiFi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Connecting to WiFi..."));
  display.println(ssid);
  display.display();
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to WiFi with IP: ");
    Serial.println(WiFi.localIP());
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("WiFi Connected!"));
    display.println(WiFi.localIP().toString());
    display.display();
    delay(2000);
  } else {
    Serial.println("Failed to connect to WiFi!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("WiFi Connection Failed!"));
    display.println(F("Continuing in offline mode"));
    display.display();
    delay(2000);
  }
}

void sendDataToGoogleSheets() {
  // Check if we're connected to WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send data.");
    reconnectWiFi();
    return;
  }
  
  // Check if we have a script ID
  if (GOOGLE_SCRIPT_ID.length() == 0) {
    Serial.println("No Google Script ID set. Cannot send data.");
    return;
  }
  
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate verification
  
  // Make sure this is based on clothesProtected status instead of isRaining
  String clothes_status = clothesProtected ? "In Cover" : "Drying";
  
  // URL encode spaces in the status
  clothes_status.replace(" ", "%20");
  
  // Use a direct HTTP request instead of HTTPClient for more control
  String url = "/macros/s/" + GOOGLE_SCRIPT_ID + "/exec";
  String data = "?rain_value=" + String(rainValue) + 
                "&is_raining=" + String(isRaining ? "Yes" : "No") + 
                "&clothes_status=" + clothes_status +
                "&temperature=" + String(temperature) +
                "&humidity=" + String(humidity);
  
  Serial.print("Connecting to script.google.com...");
  Serial.println("Status being sent: " + clothes_status);
  
  bool success = false;
  
  if (client.connect("script.google.com", 443)) {
    Serial.println("Connected!");
    
    // Send HTTP request
    client.print(String("GET ") + url + data + " HTTP/1.1\r\n" +
                "Host: script.google.com\r\n" +
                "User-Agent: ESP32\r\n" +
                "Connection: close\r\n\r\n");
    
    Serial.println("Data sent to Google Sheets!");
    
    // Wait for response with longer timeout
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.println(line);
        if (line.indexOf("success") >= 0) {
          Serial.println("Data successfully saved to Google Sheets!");
          success = true;
          lastSuccessfulSend = millis();
        }
      }
      delay(10);
    }
    
    client.stop();
    Serial.println("Connection closed");
  } else {
    Serial.println("Connection to Google failed!");
  }
  
  if (!success) {
    Serial.println("Data send unsuccessful, will retry later");
    // Next retry will happen based on resendTimeout logic
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi!");
    } else {
      Serial.println("\nFailed to reconnect to WiFi.");
    }
  }
}

void loadScriptIDFromEEPROM() {
  char scriptIdBuffer[MAX_SCRIPT_ID_LENGTH];
  for (int i = 0; i < MAX_SCRIPT_ID_LENGTH; i++) {
    scriptIdBuffer[i] = EEPROM.read(EEPROM_SCRIPT_ID_ADDR + i);
    if (scriptIdBuffer[i] == 0) break;
  }
  scriptIdBuffer[MAX_SCRIPT_ID_LENGTH - 1] = 0; // Ensure null termination
  
  if (scriptIdBuffer[0] != 0 && scriptIdBuffer[0] != 255) {
    GOOGLE_SCRIPT_ID = String(scriptIdBuffer);
    Serial.print("Loaded Google Script ID from EEPROM: ");
    Serial.println(GOOGLE_SCRIPT_ID); 
  } else {
    Serial.println("No Google Script ID found in EEPROM.");
  }
}

void saveScriptIDToEEPROM(String scriptId) {
  // Clear existing data
  for (int i = 0; i < MAX_SCRIPT_ID_LENGTH; i++) {
    EEPROM.write(EEPROM_SCRIPT_ID_ADDR + i, 0);
  }
  
  // Write new data
  for (int i = 0; i < scriptId.length() && i < MAX_SCRIPT_ID_LENGTH - 1; i++) {
    EEPROM.write(EEPROM_SCRIPT_ID_ADDR + i, scriptId.charAt(i));
  }
  
  EEPROM.commit();
  GOOGLE_SCRIPT_ID = scriptId;
  
  Serial.print("Saved new Google Script ID to EEPROM: ");
  Serial.println(scriptId);
}

void checkForNewScriptID() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    
    // Check if input starts with "SET_SCRIPT_ID:"
    if (input.startsWith("SET_SCRIPT_ID:")) {
      String newScriptId = input.substring(14); // Length of "SET_SCRIPT_ID:"
      newScriptId.trim();
      
      if (newScriptId.length() > 0) {
        saveScriptIDToEEPROM(newScriptId);
      }
    }
  }
}
