/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <AudioTools.h>
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

// WIFI----------------
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";

//Variables to save values from HTML form
String ssid = "";
String pass = "";

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";

// Timer variables
unsigned long previousMillis = 0;
const long interval = 20000;  // interval to wait for Wi-Fi connection (milliseconds)

// WiFi End----------------------------

// SD Card----------------------------
#define SD_CS      5
#define SPI_MOSI  23
#define SPI_MISO  19
#define SPI_SCK   18
// SD Card End-----------------------

// Open Ai---------------------------
// API endpoint
const char* serverUrl = "http://your_local_server/process-audio";
// Open AI End-----------------------

// Audio Start-----------------------------
// I2S amplifier / DAC
#define I2S_DOUT  22
#define I2S_BCLK  26
#define I2S_LRC   25


// Audio End-------------------------------

// Recording Start----------------------------
I2SStream i2sStream;  // Access I2S as stream

File audioFile;  // final output stream
WAVEncoder encoder;

EncodedAudioStream out_stream(&audioFile, &encoder);  // encode as wav file
StreamCopy copier(out_stream, i2sStream);        

const unsigned long recordDuration = 5000; 

I2SStream i2s;
AudioInfo info(22050, 1, 16);
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy OutputCopier; 
File playFile;
bool playbackInitialized = false;  // Ensure playback is only initialized once
// Recording End---------------------------

// State Machine --------------------------
#define STATE_RESET 0
#define STATE_IDLE 1
#define STATE_RECORD 2
#define STATE_AI 3
#define STATE_PLAY 4
#define STATE_PLAYING 5

int state = 0;
// State Machine End -----------------------

// Wifi----------------------------------
// Initialize WiFi
bool initWiFi() {
  if(ssid==""){
    Serial.println("Undefined SSID");
    return false;
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  // Attempt to connect to WiFi network:
  unsigned long startMillis = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startMillis > 10000) { // 10-second timeout
      Serial.println("WiFi connection failed!");
      return false;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected to WiFi!");
  Serial.println(WiFi.localIP());
  return true;
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  return String();
}
// Wifi End---------------------------------

// SD----------------------------------------------
bool initSD() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  Serial.println("Initializing SD card...");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD initialization failed!");
    return false;
  }
  Serial.println("SD initialization done.");
  return true;
}

// Read File from SD Card
String readFile(const char* path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = SD.open(path);
  if (!file) {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

// Write File to SD Card
void writeFile(const char* path, const char* message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}
// SD End---------------------------------------------------

// open ai -------------------------------------------------
bool processAIRequest() {
  // Open the audio file from SD card
  File audioFile = SD.open("/recording.wav", FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open recording file");
    return false;
  }

  // Prepare HTTP client
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "audio/wav");

  // Create a buffer to hold file data
  const size_t bufferSize = 2048;
  uint8_t buffer[bufferSize];

  // Read file data into buffer and send in chunks
  int bytesRead;
  WiFiClient* stream = http.getStreamPtr();
  while ((bytesRead = audioFile.read(buffer, bufferSize)) > 0) {
    stream->write(buffer, bytesRead);
  }
  audioFile.close();

  // Get the response from the server
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);

    // Open file to save the response
    File responseFile = SD.open("/speech.mp3", FILE_WRITE);
    if (!responseFile) {
      Serial.println("Failed to open file for writing");
      http.end();
      return false;
    }

    // Read the response stream and write to file
    WiFiClient* responseStream = http.getStreamPtr();
    while (responseStream->connected() || responseStream->available()) {
      bytesRead = responseStream->readBytes(buffer, bufferSize);
      if (bytesRead > 0) {
        responseFile.write(buffer, bytesRead);
      }
    }
    responseFile.close();
    Serial.println("Response saved as /speech.mp3");
  } else {
    Serial.printf("Error code: %d\n", httpResponseCode);
    http.end();
    return false;
  }

  http.end();
  return true;
}
// Open ai End ------------------------------------

// Recording Start ------------------------------
void recordingSetup() {
  Serial.println("Initialize i2s");

  I2SConfig config = i2sStream.defaultConfig(RX_MODE);
  config.i2s_format = I2S_STD_FORMAT;
  config.sample_rate = 16000;
  config.channels = 1;
  config.bits_per_sample = 16;
  config.is_master = true;
  config.use_apll = false;
  // i2s pins used on ESP32
  config.pin_bck = 14;
  config.pin_ws = 15;
  config.pin_data_rx = 21;  // input
  config.port_no = I2S_NUM_1;

  bool i2s_result = i2sStream.begin(config);

  if (!i2s_result) {
    Serial.println("i2s_result failed to start!");
  }
  bool out_stream_result = out_stream.begin(config);
  if (!out_stream_result) {
    Serial.println("out_stream_result failed to start!");
  }
}

void processRecordingRequest() {
  Serial.println("Recording started...");

  recordingSetup();
  
  String filename = "/recording.wav";

  if (SD.exists(filename)) {
    SD.remove(filename);
  } 

  audioFile = SD.open(filename, FILE_WRITE);
  if (!audioFile) {
    Serial.println("Recording Error -- Failed to open file for reading");
  }

  unsigned long startTime = millis();

  while (millis() - startTime < recordDuration) {
    copier.copy();
    audioFile.flush();  // Force writing of data
  }

  Serial.println("Recording complete.");
  out_stream.end();
  i2sStream.end();
  audioFile.close();
}
// Recording End ----------------------------------

// Audio Start--------------------------------
void processPlayRequest() {
  Serial.println("Play speech.mp3");

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  Serial.println("Initializing SD card...");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD initialization failed!");
  }
  Serial.println("SD initialization done.");

  playFile = SD.open("/speech.mp3");
  Serial.println(playFile.size());

  auto config = i2s.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.i2s_format = I2S_LSB_FORMAT;
  config.pin_bck = I2S_BCLK;
  config.pin_ws = I2S_LRC;
  config.pin_data = I2S_DOUT;  // input
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  decoder.begin();

  // begin copy
  OutputCopier.begin(decoder, playFile);

  Serial.println("Play speech.mp3 End...");
}

void stopPlayback() {
  Serial.println("Stopping playback.");
  i2s.end();
  decoder.end();
  if (playFile) {
    playFile.close();
  }
}
// Audio End-----------------------------------

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  
  
  if (!initSD()) {
    ESP.restart();
    return;
  }

  ssid = readFile(ssidPath);
  pass = readFile(passPath);
  Serial.print("SSID: "); Serial.print(ssid); Serial.print(" -- Password: "); Serial.println(pass);

  if(initWiFi()) {
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SD, "/index.html", "text/html", false, processor);
    });
    server.serveStatic("/", SD, "/");
    
    // Recording
    server.on("/record", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SD, "/index.html", "text/html", false, processor);
      state = STATE_RECORD;
    });
    
    server.on("/ai", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SD, "/index.html", "text/html", false, processor);
    });

    // Playing Audio
    server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SD, "/index.html", "text/html", false, processor);
      state = STATE_PLAY;
      playbackInitialized = false;
    });
    server.begin();
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SD, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", SD, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(passPath, pass.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router");
      delay(3000);
      ESP.restart();
    });
    server.begin();
  }
}

void loop() {
  switch (state) {
    case STATE_RESET: {
      Serial.println("Reset State");
      state = STATE_IDLE;
      }
      break;

    case STATE_IDLE: {
      }
      break;
    
    case STATE_RECORD: {
      processRecordingRequest();
      state = STATE_AI;
      }
      break;

    case STATE_AI: {
      processAIRequest();
      state = STATE_PLAY;
      playbackInitialized = false;
      }
      break;

    case STATE_PLAY: {  
      // Initialize playback only once when entering STATE_PLAY
      if (!playbackInitialized) {
        Serial.println("Start Playing Response");
        processPlayRequest();
        playbackInitialized = true;
      }
    }
      break;
  }

  if (state == STATE_PLAY && playbackInitialized) {
    if (!OutputCopier.copy()) {
      stopPlayback();
      playbackInitialized = false;
      state = STATE_IDLE;
    }
  }
  
  delay(1);
}