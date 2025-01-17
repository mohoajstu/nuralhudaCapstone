#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SD.h>
#include <SPI.h>

// Wi-Fi credentials
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";

// HTTPS server details
const char* server = "example.com"; // Replace with your server domain
const int port = 443;              // HTTPS port
const char* endpoint = "/upload";  // Endpoint to upload the file

// Root CA certificate (PEM format)
const char* root_ca = R"EOF(
-----BEGIN CERTIFICATE-----
YOUR_SERVER_ROOT_CA_CERTIFICATE_HERE
-----END CERTIFICATE-----
)EOF";

// SD card settings
#define SD_CS 5 // Chip select pin for SD card (adjust based on your ESP32)

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized.");

  // Configure WiFiClientSecure
  client.setCACert(root_ca);

  // Send the `.wav` file
  sendWavFile("/input.wav");
}

void sendWavFile(const char* filePath) {
  // Open the .wav file on the SD card
  File wavFile = SD.open(filePath, FILE_READ);
  if (!wavFile) {
    Serial.println("Failed to open input file.");
    return;
  }

  // Connect to the HTTPS server
  if (!client.connect(server, port)) {
    Serial.println("Failed to connect to server.");
    wavFile.close();
    return;
  }
  Serial.println("Connected to the server.");

  // Prepare HTTP POST headers
  String boundary = "----ESP32Boundary";
  String contentType = "audio/wav";
  String contentDisposition = "Content-Disposition: form-data; name=\"file\"; filename=\"input.wav\"";
  size_t contentLength = wavFile.size();

  client.println(String("POST ") + endpoint + " HTTP/1.1");
  client.println(String("Host: ") + server);
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(contentLength + boundary.length() + 44)); // Approximate calculation
  client.println("Connection: close");
  client.println();

  // Write POST body
  client.println("--" + boundary);
  client.println(contentDisposition);
  client.println("Content-Type: " + contentType);
  client.println();

  // Stream file contents
  while (wavFile.available()) {
    client.write(wavFile.read());
  }
  client.println();
  client.println("--" + boundary + "--");

  // Close the input file
  wavFile.close();

  // Read the server response
  Serial.println("Server response:");
  while (client.connected() || client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  client.stop();
}

void loop() {
  // Nothing to do here
}
