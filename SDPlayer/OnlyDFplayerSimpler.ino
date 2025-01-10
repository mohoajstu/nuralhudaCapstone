#include "Arduino.h" // Include the core Arduino library
#include "DFRobotDFPlayerMini.h" // Include the DFRobot DFPlayer Mini library

#ifdef ESP32
  #define FPSerial Serial1  // For ESP32, use hardware serial port 1
#else
  #include <SoftwareSerial.h> // Include SoftwareSerial library for non-ESP32 boards
  SoftwareSerial FPSerial(16, 17); // Define SoftwareSerial on pins 16 (RX) and 17 (TX)
#endif

DFRobotDFPlayerMini myDFPlayer; // Create an instance of the DFRobotDFPlayerMini class

void setup() {
  #ifdef ESP32
  FPSerial.begin(9600, SERIAL_8N1, 16, 17); // Start serial communication for ESP32 with 9600 baud rate, 8 data bits, no parity, and 1 stop bit
#else
  FPSerial.begin(9600); // Start serial communication for other boards with 9600 baud rate
#endif

  Serial.begin(115200); // Start the Serial monitor communication with 115200 baud rate

  Serial.println(F("DFRobot DFPlayer Mini Demo")); // Print a demo start message
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)")); // Print initialization message
  
  if (!myDFPlayer.begin(FPSerial)) { // Initialize the DFPlayer Mini with the defined serial interface
    Serial.println(F("Unable to begin:")); // If initialization fails, print an error message
    Serial.println(F("1.Please recheck the connection!")); // Suggest rechecking the connection
    Serial.println(F("2.Please insert the SD card!")); // Suggest checking for an inserted SD card
    while(true); // Stay in an infinite loop if initialization fails
  }
  Serial.println(F("DFPlayer Mini online.")); // Print a success message if initialization succeeds
  
  myDFPlayer.volume(30);  // Set the DFPlayer Mini volume to 30 (max is 30)
  myDFPlayer.play(1);  // Start playing the first track on the SD card
}

void loop() {
  static unsigned long timer = millis(); // Initialize a timer variable to track time elapsed
  
  if (millis() - timer > 9000) { // Check if 9 seconds have passed
    timer = millis(); // Reset the timer
    myDFPlayer.next();  // Play the next MP3 track
  }
  
  if (myDFPlayer.available()) { // Check if the DFPlayer Mini has any new data available
    uint8_t type = myDFPlayer.readType(); // Read the type of the message
    int value = myDFPlayer.read(); // Read the message value
    if (type == DFPlayerPlayFinished) { // Check if the message type indicates the end of a track
      Serial.print(F("Finished playing track ")); // Print a message to the Serial monitor
      Serial.println(value); // Print the track number that just finished playing
    }
  }
}
