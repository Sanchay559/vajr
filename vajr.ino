#include "WiFi.h"

#define SOS 19  // GPIO 19 for the SOS button
#define SLEEP_PIN 18  // GPIO 18 for sleep mode control

// Communication and control flags
boolean stringComplete = false;
String inputString = "";
String fromGSM = "";
String SOS_NUM = "+917982860425";  // Your SOS number
int SOS_Time = 5000;  // SOS button press duration in milliseconds
bool CALL_END = true;  // State flag for call end
String res = "";  // Buffer for storing response from GSM module
char response[50];  // Declare 'response' with a fixed-size char array
unsigned long buttonPressStart = 0;  // Time when button was first pressed
bool locationSent = false;  // Flag to avoid sending multiple times

void setup() {
  WiFi.mode(WIFI_OFF);  // Disable WiFi for power saving
  btStop();  // Disable Bluetooth

  pinMode(SOS, INPUT_PULLUP);  // SOS button with pull-up
  pinMode(SLEEP_PIN, OUTPUT);  // Sleep control pin

  Serial.begin(115200);  // For debugging
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // UART for GSM (ESP32 RX: 16, TX: 17)

  delay(20000);  // Time for GSM module initialization
  digitalWrite(SLEEP_PIN, HIGH);  // Activate sleep mode

  // Check basic communication
  Serial2.println("AT");
  delay(1000);
  Serial2.println("AT+GPS=1");  // Enable GPS
  delay(1000);
  Serial2.println("AT+GPSLP=2");  // Set GPS to low power mode
  delay(1000);
  Serial2.println("AT+SLEEP=1");  // Configure sleep mode
  delay(1000);
}

void loop() {
  // Read from GSM module
  if (Serial2.available()) {
    char inChar = Serial2.read();

    if (inChar == '\n') {
      if (fromGSM == "OK\r") {
        Serial.println("---------IT WORKS-------");
      } else if (fromGSM == "RING\r") {
        digitalWrite(SLEEP_PIN, LOW);  // Wake from sleep mode
        Serial.println("---------IT'S RINGING-------");
        Serial2.println("ATA");  // Answer call
      } else if (fromGSM == "ERROR\r") {
        Serial.println("---------ERROR-------");
      } else if (fromGSM == "NO CARRIER\r") {
        Serial.println("---------CALL ENDED-------");
        CALL_END = true;
        digitalWrite(SLEEP_PIN, HIGH);  // Re-enter sleep mode
      }

      Serial.println(fromGSM);
      fromGSM = "";  // Clear the buffer
    } else {
      fromGSM += inChar;  // Build the response
    }
  }

  // Echo input from Serial Monitor to GSM module
  if (Serial.available()) {
    int inByte = Serial.read();
    Serial2.write(inByte);
  }

  // Check SOS button press duration
  if (digitalRead(SOS) == LOW) {
    if (buttonPressStart == 0) {
      // Start counting when button is pressed
      buttonPressStart = millis();
    }

    if (millis() - buttonPressStart >= SOS_Time && !locationSent) {
      // If the button is held for at least 5 seconds, send location
      digitalWrite(SLEEP_PIN, LOW);  // Wake up
      delay(1000);
      Serial2.println("AT+LOCATION=2");  // Request GPS location
      delay(1000);

      res = "";
      while (Serial2.available()) {
        char add = Serial2.read();
        res += add;
      }

      res = res.substring(17, 38);  // Extract GPS data
      strcpy(response, res.c_str());  // Convert to char array

      Serial.print("Received Data: ");
      Serial.println(response);

      if (!strstr(response, "GPS NOT")) {
        int i = 0;
        while (response[i] != ',') {
          i++;
        }

        String location = String(response);
        String lat = location.substring(2, i);  // Latitude
        String longi = location.substring(i + 1);  // Longitude

        String Gmaps_link = "http://maps.google.com/maps?q=" + lat + "+" + longi;  // Google Maps link

        // Send SMS with location
        Serial2.println("AT+CMGF=1");  // Text mode
        delay(1000);
        Serial2.println("AT+CMGS=\"" + SOS_NUM + "\"\r");  // Prepare to send SMS
        delay(1000);
        Serial2.println("I'm here: " + Gmaps_link);  // Send SMS content
        delay(1000);
        Serial2.println((char)26);  // End of SMS
        delay(1000);

        locationSent = true;  // Prevent multiple sends
      }

      res = "";  // Clear response buffer
    }
  } else {
    // Reset if the button is released
    buttonPressStart = 0;
    locationSent = false;
  }

  if (stringComplete) {
    Serial2.print(inputString);  // Send full message to GSM module
    inputString = "";  // Reset input string
    stringComplete = false;
  }
}
