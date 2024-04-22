#include "WiFi.h"

#define SOS 19  // GPIO 19 for the SOS button
#define SLEEP_PIN 18  // GPIO 18 for sleep mode control

// Communication and control flags
boolean stringComplete = false;
String inputString = "";
String fromGSM = "";
int c = 0;
String SOS_NUM = "+917982860425";  // Your SOS number (Update as needed)

int SOS_Time = 5;  // SOS button press duration in seconds
bool CALL_END = true;  // State flag for call end
String res = "";  // Buffer for storing response from GSM module
char response[50];  // Declare 'response' with a fixed-size char array

void setup() {
  // Set WiFi and Bluetooth modes to off for power-saving
  WiFi.mode(WIFI_OFF);  // Disable WiFi
  btStop();  // Disable Bluetooth

  // Set up pin modes
  pinMode(SOS, INPUT_PULLUP);  // SOS button with pull-up
  pinMode(SLEEP_PIN, OUTPUT);  // Sleep control pin

  // Set up serial communication
  Serial.begin(115200);  // For debugging via Serial Monitor
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // UART interface for GSM communication (ESP32 RX: 16, TX: 17)

  // Allow time for GSM module initialization
  delay(20000);

  // Activate sleep mode
  digitalWrite(SLEEP_PIN, HIGH);

  // Basic AT commands to verify communication
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
      // Check GSM module response
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

      // Print GSM module response
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

  // SOS button handling
  if (digitalRead(SOS) == LOW && CALL_END) {
    Serial.print("Calling in..");
    for (c = 0; c < SOS_Time; c++) {
      Serial.println(SOS_Time - c);  // Countdown for SOS button press
      delay(1000);  // 1-second delay
      if (digitalRead(SOS) == HIGH) {  // If button is released, break
        break;
      }
    }

    if (c == SOS_Time) {  // If SOS button held for 5 seconds
      // Wake up and request GPS location
      digitalWrite(SLEEP_PIN, LOW);  // Wake from sleep mode
      delay(1000);
      Serial2.println("AT+LOCATION=2");  // Request GPS location
      Serial.println("AT+LOCATION=2");

      // Wait for response from GSM module
      while (!Serial2.available()) {
        delay(10);  // Small delay to avoid busy-waiting
      }

      res = "";
      while (Serial2.available()) {
        char add = Serial2.read();
        res += add;  // Collect response
      }

      res = res.substring(17, 38);  // Extract location data
      strcpy(response, res.c_str());  // Convert to char array

      Serial.print("Received Data: ");
      Serial.println(response);

      if (strstr(response, "GPS NOT")) {
        Serial.println("No GPS data available");
      } else {
        // Parse GPS coordinates
        int i = 0;
        while (response[i] != ',') {
          i++;
        }

        String location = String(response);
        String lat = location.substring(2, i);  // Latitude
        String longi = location.substring(i + 1);  // Longitude

        // Create Google Maps link
        String Gmaps_link = "http://maps.google.com/maps?q=" + lat + "+" + longi;

        // Send SMS with Google Maps link
        Serial2.println("AT+CMGF=1");  // Text mode
        delay(1000);
        Serial2.println("AT+CMGS=\"" + SOS_NUM + "\"\r");  // Prepare to send SMS
        delay(1000);

        Serial2.println("I'm here: " + Gmaps_link);  // SMS content
        delay(1000);
        Serial2.println((char)26);  // End of SMS (Ctrl-Z)
        delay(1000);

        // Make a call after sending the SMS
        Serial2.println("ATD" + SOS_NUM);  // Dial SOS number
        CALL_END = false;
      }

      res = "";  // Clear response buffer
    }
  }

  if (stringComplete) {
    Serial2.print(inputString);  // Send full message to GSM module
    inputString = "" ;  // Reset input string
    stringComplete = false;
  }
}
