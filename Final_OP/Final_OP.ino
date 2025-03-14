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

// SD Card----------------------------
#define SD_CS      5
#define SPI_MOSI  23
#define SPI_MISO  19
#define SPI_SCK   18

// microphone ---------------------
#define BUTTON_PIN 33
#define MICROPHONE_BCK 14
#define MICROPHONE_WS 15
#define MICROPHONE_RX 21

// I2S amplifier / DAC
#define I2S_DOUT  22
#define I2S_BCLK  26
#define I2S_LRC   25

// State Machine 
#define STATE_RESET 0
#define STATE_IDLE 1
#define STATE_RECORD 2
#define STATE_AI 3
#define STATE_PLAY 4
#define STATE_PLAYING 5

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
// WiFi End----------------------------

// Open Ai---------------------------
WiFiClient client;
// API endpoint
// #define serverUrl "http://165.227.41.139:3001/process-audio"
// #define SERVER_IP "165.227.41.139"
// #define SERVER_PORT 3001
#define serverUrl "http://192.168.137.1:3000/process-audio"
#define SERVER_IP "192.168.137.1"
#define SERVER_PORT 3000
// Open AI End-----------------------

// Recording Start----------------------------
I2SStream i2sStream;  // Access I2S as stream
File audioFile;  // final output stream
WAVEncoder encoder;
EncodedAudioStream out_stream(&audioFile, &encoder);  // encode as wav file
StreamCopy copier(out_stream, i2sStream);        
bool isRecording = false;
// Recording End---------------------------

// Amplifier
I2SStream i2s;
AudioInfo info(22050, 1, 16);
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy OutputCopier; 
File playFile;
bool playbackInitialized = false;  // Ensure playback is only initialized once

// Global flags for AI request task
volatile bool aiRequestComplete = false;
volatile bool aiRequestSuccess = false;
bool waitingPlaybackInitialized = false;

// Waiting playback objects (plays "/waiting.mp3")
File waitingFile;
I2SStream waitingI2S;
AudioInfo waitingInfo(22050, 1, 16);
EncodedAudioStream waitingDecoder(&waitingI2S, new MP3DecoderHelix());
StreamCopy waitingOutputCopier;

// --- State Machine variable ---
int state = STATE_IDLE;
// Amplifier End
// State Machine End -----------------------

// Wifi----------------------------------
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
// String processor(const String& var) {
//   return String();
// }
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
  Serial.println("Start Process AI Request");

  // Open audio file
  File recordingFile = SD.open("/recording.wav", FILE_READ);
  if (!recordingFile) {
    Serial.println("Failed to open recording file");
    return false;
  }
  Serial.println("Recording file opened");

  // Connect to the server
  if (!client.connect(SERVER_IP, SERVER_PORT)) {
    Serial.println("Connection to server failed");
    return false;
  }
  Serial.println("Connected to server");

  // Send HTTP POST request headers
  client.println("POST /process-audio HTTP/1.1");
  client.println("Host: 192.168.137.1");
  client.println("Content-Type: audio/wav");
  client.println("Transfer-Encoding: chunked");
  client.println("Connection: close");
  client.println();

  // Send file data in chunks
  const size_t bufferSize = 512;
  uint8_t buffer[bufferSize];
  size_t bytesRead;
  while ((bytesRead = recordingFile.read(buffer, bufferSize)) > 0) {
    client.print(String(bytesRead, HEX));
    client.print("\r\n");
    client.write(buffer, bytesRead);
    client.print("\r\n");
  }
  recordingFile.close();

  // Send final zero-length chunk
  client.print("0\r\n\r\n");

  // Receive response
  Serial.println("Reading server response...");

  // Read status line
  unsigned long timeout = millis();
  while (!client.available() && millis() - timeout < 30000);
  if (!client.available()) {
    Serial.println("Response timeout");
    client.stop();
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  if (statusLine.indexOf("200 OK") == -1) {
    Serial.print("Server error: ");
    Serial.println(statusLine);
    client.stop();
    return false;
  }

  // Parse headers
  int contentLength = 0;
  while (client.connected()) {
    String headerLine = client.readStringUntil('\n');
    headerLine.trim();
    if (headerLine.startsWith("Content-Length: ")) {
      contentLength = headerLine.substring(16).toInt();
    }
    if (headerLine.isEmpty()) break;
  }

  if (contentLength <= 0) {
    Serial.println("Invalid Content-Length");
    client.stop();
    return false;
  }

  // Create output file
  File mp3File = SD.open("/speech.mp3", FILE_WRITE);
  if (!mp3File) {
    Serial.println("Failed to create MP3 file");
    client.stop();
    return false;
  }

  // Stream response to file
  uint8_t recvBuffer[512];
  size_t receivedTotal = 0;
  unsigned long downloadStart = millis();

  // Loop until we receive the full content or timeout occurs
  while ((client.connected() || client.available()) && (receivedTotal < contentLength)) {
    if (client.available()) {
      size_t bytesToRead = min(sizeof(recvBuffer), contentLength - receivedTotal);
      int bytesRead = client.read(recvBuffer, bytesToRead);
      if (bytesRead > 0) {
        mp3File.write(recvBuffer, bytesRead);
        receivedTotal += bytesRead;
        Serial.printf("Received %d bytes (Total: %d/%d)\n", bytesRead, receivedTotal, contentLength);
      }
    }
    // Increase timeout to 60 seconds if necessary
    if (millis() - downloadStart > 60000) {  
      Serial.println("Download timeout");
      break;
    }
  }

  mp3File.close();
  client.stop();

  if (receivedTotal != contentLength) {
    Serial.println("Incomplete download");
    return false;
  }

  Serial.println("MP3 saved successfully");
  return true;
}

// Run processAIRequest in a separate FreeRTOS task
void taskProcessAIRequest(void * parameter) {
  aiRequestSuccess = processAIRequest();
  aiRequestComplete = true;
  vTaskDelete(NULL);
}
// Open ai End ------------------------------------

// Recording Start ------------------------------
void recordingSetup() {
  Serial.println("Initialize i2s");

  disableAmplifier();

  I2SConfig config = i2sStream.defaultConfig(RX_MODE);
  config.i2s_format = I2S_STD_FORMAT;
  config.sample_rate = 16000;
  config.channels = 1;
  config.bits_per_sample = 16;
  config.is_master = true;
  config.use_apll = false;
  // i2s pins used on ESP32
  config.pin_bck = MICROPHONE_BCK;
  config.pin_ws = MICROPHONE_WS;
  config.pin_data_rx = MICROPHONE_RX;  // input
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

// Disable amplifier outputs to reduce interference during recording
void disableAmplifier() {
  pinMode(I2S_DOUT, OUTPUT);
  digitalWrite(I2S_DOUT, LOW);
  
  pinMode(I2S_BCLK, OUTPUT);
  digitalWrite(I2S_BCLK, LOW);
  
  pinMode(I2S_LRC, OUTPUT);
  digitalWrite(I2S_LRC, LOW);
}
// Recording End ----------------------------------

// Audio Start--------------------------------
void processPlayRequest() {
  Serial.println("Play speech.mp3");

  playFile = SD.open("/speech.mp3");
  Serial.println(playFile.size());

  auto config = i2s.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.i2s_format = I2S_STD_FORMAT;
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
  // Allow the output copier to flush any remaining data
  while (OutputCopier.copy()) {
    delay(1); // yield time for internal buffers to flush
  }
  
  // End the decoder and I2S stream after the last frame is processed.
  decoder.end();
  i2s.end();
  if (playFile) {
    playFile.close();
  }
}

void startWaitPlayback() {
  waitingFile = SD.open("/waiting.mp3");
  if (!waitingFile) {
    Serial.println("Failed to open waiting.mp3");
    return;
  }
  auto config = i2s.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.i2s_format = I2S_STD_FORMAT;
  config.pin_bck = I2S_BCLK;
  config.pin_ws = I2S_LRC;
  config.pin_data = I2S_DOUT;
  // Use the same I2S port as speech playback
  config.port_no = I2S_NUM_0;
  waitingI2S.begin(config);
  waitingDecoder.begin();
  waitingOutputCopier.begin(waitingDecoder, waitingFile);
  waitingPlaybackInitialized = true;
  Serial.println("Started waiting playback");
}

void stopWaitPlayback() {
  waitingOutputCopier.end();
  waitingDecoder.end();
  waitingI2S.end();
  if (waitingFile) {
    waitingFile.close();
  }
  waitingPlaybackInitialized = false;
  Serial.println("Stopped waiting playback");
}
// Audio End-----------------------------------

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  // AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  
  
  if (!initSD()) {
    ESP.restart();
    return;
  }

  ssid = readFile(ssidPath);
  pass = readFile(passPath);
  Serial.print("SSID: "); Serial.print(ssid); Serial.print(" -- Password: "); Serial.println(pass);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if(initWiFi()) {
    // // Route for root / web page
    // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //   request->send(SD, "/index.html", "text/html", false, processor);
    // });
    // server.serveStatic("/", SD, "/");
    
    // // Recording
    // server.on("/record", HTTP_GET, [](AsyncWebServerRequest *request) {
    //   request->send(SD, "/index.html", "text/html", false, processor);
    //   state = STATE_RECORD;
    // });
    
    // server.on("/ai", HTTP_GET, [](AsyncWebServerRequest *request) {
    //   request->send(SD, "/index.html", "text/html", false, processor);
    //   state = STATE_AI;
    // });

    // // Playing Audio
    // server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request) {
    //   request->send(SD, "/index.html", "text/html", false, processor);
    //   state = STATE_PLAY;
    //   playbackInitialized = false;
    // });
    // server.begin();
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
  static unsigned long lastDebounceTime = 0;
  static bool buttonActive = false;
  
  // Button pressed to start/stop recording
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonActive && (millis() - lastDebounceTime > 50)) {
      recordingSetup();
      buttonActive = true;
      lastDebounceTime = millis();
      if (state == STATE_IDLE) {
        state = STATE_RECORD;
        isRecording = true;
        Serial.println("Starting recording...");
      }
    }
  } else {
    if (buttonActive) {
      buttonActive = false;
      if (isRecording) {
        isRecording = false;
        Serial.println("Stopping recording...");
        out_stream.end();
        i2sStream.end();
        audioFile.close();
        // Transition to AI state and start asynchronous AI request task
        state = STATE_AI;
        aiRequestComplete = false;
        xTaskCreatePinnedToCore(taskProcessAIRequest, "AI_Task", 8192, NULL, 1, NULL, 1);
      }
    }
  }
  
  // State machine handling
  switch (state) {
    case STATE_RESET:
      Serial.println("Reset State");
      state = STATE_IDLE;
      break;
      
    case STATE_IDLE:
      // Do nothing
      break;
      
    case STATE_RECORD:
      if (!audioFile) {
        String filename = "/recording.wav";
        if (SD.exists(filename)) SD.remove(filename);
        audioFile = SD.open(filename, FILE_WRITE);
      }
      if (isRecording) {
        copier.copy();
        audioFile.flush();
      }
      break;
      
    case STATE_AI:
      // In STATE_AI, run waiting sound playback while the AI task is running.
      if (!waitingPlaybackInitialized) {
        startWaitPlayback();
      }
      // Drive waiting playback copy – if the file finishes, loop it.
      if (!waitingOutputCopier.copy()) {
        waitingFile.close();
        waitingFile = SD.open("/waiting.mp3");
        waitingOutputCopier.begin(waitingDecoder, waitingFile);
      }
      // When the asynchronous AI request finishes, stop waiting playback and transition to STATE_PLAY.
      if (aiRequestComplete) {
        stopWaitPlayback();
        state = STATE_PLAY;
      }
      break;
      
    case STATE_PLAY:
      if (!playbackInitialized) {
        Serial.println("Start Playing Response");
        processPlayRequest();
        playbackInitialized = true;
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