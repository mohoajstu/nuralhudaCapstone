
#include <WiFiClientSecure.h>  // For HTTPS
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "password";
const char* password = "password";

// API endpoint
const char* server = "api.openai.com";
const char* apiKey = "key";

const char* openai_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCA1WgAwIBAgIRAPfUmUZWV6agDp7LeOggkOEwCgYIKoZIzj0EAwIwOzEL
MAkGA1UEBhMCVVMxHjAcBgNVBAoTFUdvb2dsZSBUcnVzdCBTZXJ2aWNlczEMMAoG
A1UEAxMDV0UxMB4XDTI1MDEyMjIzMTc1M1oXDTI1MDQyMzAwMTc1MFowGTEXMBUG
A1UEAxMOYXBpLm9wZW5haS5jb20wWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATn
YyuOPt3869aCZUsdJ42c7YzWRr36SfQs5+ZNunqupMP6wVD6+YpR6WN/2l4ZV0Ku
Z7ZNPL9Cz/K4tVjy/qBRo4ICWjCCAlYwDgYDVR0PAQH/BAQDAgeAMBMGA1UdJQQM
MAoGCCsGAQUFBwMBMAwGA1UdEwEB/wQCMAAwHQYDVR0OBBYEFGx88Ra+ZYLp5ziA
BrM03bjN7N/GMB8GA1UdIwQYMBaAFJB3kjVnxP+ozKnme9mAeXvMk/k4MF4GCCsG
AQUFBwEBBFIwUDAnBggrBgEFBQcwAYYbaHR0cDovL28ucGtpLmdvb2cvcy93ZTEv
OTlRMCUGCCsGAQUFBzAChhlodHRwOi8vaS5wa2kuZ29vZy93ZTEuY3J0MCsGA1Ud
EQQkMCKCDmFwaS5vcGVuYWkuY29tghAqLmFwaS5vcGVuYWkuY29tMBMGA1UdIAQM
MAowCAYGZ4EMAQIBMDYGA1UdHwQvMC0wK6ApoCeGJWh0dHA6Ly9jLnBraS5nb29n
L3dlMS9iZDJWNkFKWVVYZy5jcmwwggEFBgorBgEEAdZ5AgQCBIH2BIHzAPEAdwBO
daMnXJoQwzhbbNTfP1LrHfDgjhuNacCx+mSxYpo53wAAAZSQhcdaAAAEAwBIMEYC
IQCS4t+uceXjTc4mYdlcYDKT3q18OjKQs4Lc3hB4YMPEjwIhAJ0cS+zu6rRPyQej
wsq0Fb8QkULr6gCKDI62pcjy8cbpAHYAfVkeEuF4KnscYWd8Xv340IdcFKBOlZ65
Ay/ZDowuebgAAAGUkIXHeQAABAMARzBFAiBzJv065lsu6Pj2BbXSYMtcF+sUlYRV
pGtDl/+obu5LzAIhANLhYooYMNnJ2f7A5VhMBU8o0QxTF8OdVL52CRrLwfjvMAoG
CCqGSM49BAMCA0gAMEUCIB3namQlUJ5uGwuJIJkk57COKvN9IPzBpLbqFRT9Ww6Y
AiEA+LuUVCxR7CGbi2TYU2YtcBrQaQN+YCpM3phcnqUCQAg=
-----END CERTIFICATE-----
)EOF";

const int port = 443;  // HTTPS port

WiFiClientSecure client;

String transcription = "What is your name?";
String completionText = "Hi, I am Nur Huda AI Assistant!";

void setup() {
  // Initialize serial and wait for port to open:
  Serial.begin(115200);
  delay(100);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  // Attempt to connect to Wi-Fi
  connectToWiFi();

  // Connect to the OpenAI server
  if (!connectToServer()) {
    Serial.println("Failed to connect to OpenAI server.");
    return;
  }
  bool transcribeResult = transcribe();
  if (transcribeResult) {
    Serial.print("Transcription: ");
    Serial.println(transcription);
  }
  else {
    Serial.println("Transcription Failed!");
  }
  client.stop();  // Close the connection

   // Connect to the OpenAI server
  if (!connectToServer()) {
    Serial.println("Failed to connect to OpenAI server.");
    return;
  }
  bool chatCompletionResult = chatCompletion();
  if (chatCompletionResult) {
    Serial.print("Chat Completion: ");
    Serial.println(completionText);
  }
  else {
    Serial.println("Chat Completion Failed!");
  }
  client.stop();  // Close the connection

  if (!connectToServer()) {
    Serial.println("Failed to connect to OpenAI server.");
    return;
  }
  bool ttsResult = tts(transcription);
  if (ttsResult) {
    Serial.println("Audio response generated successfully.");
  } else {
    Serial.println("Audio response generation failed.");
  }
  client.stop(); 
  
  Serial.println("Finish");
}

void connectToWiFi() {
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Attempt to connect to WiFi network:
  unsigned long startMillis = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startMillis > 10000) { // 10-second timeout
      Serial.println("WiFi connection failed!");
      return;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected to WiFi!");
}

bool connectToServer() {
  // Load the certificate (optional, depending on your OpenAI setup)
  // client.setCACert(openai_cert);  // Uncomment and load certificate if needed
  
  client.setInsecure();  // Disable certificate validation (use only for testing or non-production)
  if (!client.connect(server, port)) {
    Serial.println("Connection to OpenAI failed!");
    return false;
  }

  Serial.println("Connected to OpenAI server!");
  return true;
}

bool transcribe() {
  // Open the WAV file from LittleFS
  File audioFile = LittleFS.open("/audio.wav", "r");
  if (!audioFile) {
    Serial.println("Failed to open audio file");
    return false;
  }

  // Prepare HTTP request
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  bodyStart += "Content-Type: audio/wav\r\n\r\n";

  String bodyModel = "\r\n--" + boundary + "\r\n";
  bodyModel += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
  bodyModel += "whisper-1\r\n";
  String bodyEnd = "--" + boundary + "--\r\n";

  size_t bodyLength = bodyStart.length() + audioFile.size() + bodyModel.length() + bodyEnd.length();

  // Send POST request
  client.println("POST /v1/audio/transcriptions HTTP/1.1");
  client.println("Host: api.openai.com");
  client.println("Authorization: Bearer " + String(apiKey));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(bodyLength));
  client.println("Connection: close");
  client.println();

  // Send body start
  client.print(bodyStart);

  // Send the WAV file content
  uint8_t buffer[512];
  size_t bytesRead;
  while ((bytesRead = audioFile.read(buffer, sizeof(buffer))) > 0) {
    client.write(buffer, bytesRead);
  }

  client.print(bodyModel);
  client.print(bodyEnd);
  audioFile.close();  // Close the file

  // Wait for the response from the server
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }

  // Read and print the JSON response
  String jsonResponse = "";
  while (client.available()) {
    char c = client.read();
    jsonResponse += c;
  }

  // Print the JSON response to Serial Monitor
  Serial.print("Response: ");
  Serial.println(jsonResponse);

  // Extract the "text" field from the JSON response
  int textStart = jsonResponse.indexOf("\"text\":\"") + 8;
  int textEnd = jsonResponse.indexOf("\"", textStart);
  if (textStart > 0 && textEnd > textStart) {
    transcription = jsonResponse.substring(textStart, textEnd);
    return true;
  } else {
    Serial.println("Failed to parse transcription.");
    return false;
  }
}

bool chatCompletion() {
  // Prepare JSON body
  StaticJsonDocument<512> jsonDoc;
  JsonArray messages = jsonDoc.createNestedArray("messages");
  
  JsonObject developerMessage = messages.createNestedObject();
  developerMessage["role"] = "developer";
  developerMessage["content"] = "You are a helpful assistant.";
  
  JsonObject userMessage = messages.createNestedObject();
  userMessage["role"] = "user";
  userMessage["content"] = transcription;
  
  jsonDoc["model"] = "gpt-4o";  // Correct model name
  
  String requestBody;
  serializeJson(jsonDoc, requestBody);

  // Prepare HTTP request headers
  client.println("POST /v1/chat/completions HTTP/1.1");
  client.println("Host: api.openai.com");
  client.println("Authorization: Bearer " + String(apiKey));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(requestBody.length()));
  client.println("Connection: close");
  client.println();
  
  // Send the HTTP request
  client.print(requestBody);

  // Wait for the response from the server
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }

    // Wait for the response
  while (!client.available()) {
    delay(1000);  // Wait for the response from the server
    Serial.println("Waiting...");
  }

  // Read and print the JSON response
  String jsonResponse = "";
  while (client.available()) {
    char c = client.read();
    jsonResponse += c;
  }

  // Print the JSON response to Serial Monitor
  Serial.println("Response: ");
  Serial.println(jsonResponse);

  String cleanedJsonResponse = cleanJsonResponse(jsonResponse);

  // Parse the JSON response to extract the content
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, cleanedJsonResponse);

  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  // Extract the content
  const char* content = doc["choices"][0]["message"]["content"];

  // Convert const char* to String
  completionText = String(content);

  return true;
}

bool tts(String text) {
  Serial.println("TTS Start...");
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["model"] = "tts-1";
  jsonDoc["input"] = text;  // Use the provided text parameter
  jsonDoc["voice"] = "alloy";
  jsonDoc["response_format"] = "wav";

  String requestBody;
  serializeJson(jsonDoc, requestBody);  // Serialize JSON to requestBody

  client.println("POST /v1/audio/speech HTTP/1.1");
  client.println("Host: api.openai.com");
  client.println("Authorization: Bearer " + String(apiKey));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(requestBody.length()));
  client.println("Connection: close");
  client.println();

  client.print(requestBody);

   // Wait for the response
  while (!client.available()) {
    delay(1000);  // Wait for the response from the server
    Serial.println("Waiting...");
  }

  // Check if the server returns a successful response
  String response = client.readStringUntil('\r');
  if (response.indexOf("HTTP/1.1 200 OK") != -1) {
    Serial.println("Request succeeded");

    // Open the file in LittleFS to save the response (audio data)
    File audioFile = LittleFS.open("/speech.wav", "w");
    if (!audioFile) {
      Serial.println("Failed to open file for writing.");
      return false;
    }

    // Save the audio response to the file
    while (client.available()) {
      audioFile.write(client.read());
    }
    audioFile.close();

    Serial.println("Audio saved as speech.wav");
    return true;
  } else {
    Serial.println("HTTP request failed");
    return false;
  }
}

// Function to clean up the raw response
String cleanJsonResponse(String rawResponse) {
  // Remove the leading number (if present)
  int firstBracePos = rawResponse.indexOf('{');
  if (firstBracePos != -1) {
    rawResponse = rawResponse.substring(firstBracePos);  // Keep everything from '{'
  }

  // Remove the trailing number (if present)
  int lastBracePos = rawResponse.lastIndexOf('}');
  if (lastBracePos != -1) {
    rawResponse = rawResponse.substring(0, lastBracePos + 1);  // Keep everything up to '}'
  }

  return rawResponse;
}

void loop() {
  // Do nothing in the loop
}

