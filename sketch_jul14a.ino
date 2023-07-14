#include <WiFi.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

// Wi-Fi credentials
const char* ssid = "leck_mic";
const char* password = "12345687";
const char* apSSID = "ESP32-Setup";
const char* apPassword = "YourPassword";
const char* admin_username = "admin";
const char* admin_pw = "1234";

// Server settings
const char* serverName = "localhost"; // Replace with your server address
const int port = 80;

// Pin configuration
const int ledPin = 2;  // Pin number for the internal LED
const int tube1Pin = 18;  // Replace with the correct pin number for Tube 1
const int tube2Pin = 20;  // Replace with the correct pin number for Tube 2

// Debounce time in milliseconds
const unsigned long debounceTime = 100;

// Time in milliseconds to wait for Wi-Fi connection
const unsigned long wifiTimeout = 30000;

// Volatile variables for particle detection
volatile bool tube1Detected = false;
volatile bool tube2Detected = false;
volatile unsigned long tube1DetectionTime = 0;
volatile unsigned long tube2DetectionTime = 0;
volatile bool newParticleDetected = false;

// AsyncWebServer instance
AsyncWebServer asyncServer(80);

// Flag to indicate if Wi-Fi setup mode is active
bool wifiSetupMode = false;

// Timer to track Wi-Fi connection timeout
unsigned long wifiTimeoutTimer;

void tube1ISR() {
  // Code to handle particle detection on Tube 1
  unsigned long currentTime = millis();
  if (debounceTime == 0 || currentTime - tube1DetectionTime >= debounceTime) {
    tube1Detected = true;
    tube1DetectionTime = currentTime;
    newParticleDetected = true;
    Serial.println("Particle detected on Tube 1");
  }
}

void tube2ISR() {
  // Code to handle particle detection on Tube 2
  unsigned long currentTime = millis();
  if (debounceTime == 0 || currentTime - tube2DetectionTime >= debounceTime) {
    tube2Detected = true;
    tube2DetectionTime = currentTime;
    newParticleDetected = true;
    Serial.println("Particle detected on Tube 2");
  }
}

void sendPOSTRequest(const String& url) {
  WiFiClient client;
  if (client.connect(serverName, port)) {
    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST("");

    if (httpResponseCode == HTTP_CODE_OK) {
      Serial.println("POST request sent successfully");
    } else {
      Serial.print("Error sending POST request. Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("Failed to connect to server");
  }
  client.stop();
}

void handleTube1Detection() {
  digitalWrite(ledPin, HIGH);  // Turn on the LED
  delay(200);  // Delay for 200ms
  digitalWrite(ledPin, LOW);  // Turn off the LED

  // Send POST request for Tube 1
  String url = "http://" + String(serverName) + "/data/1";
  sendPOSTRequest(url);
}

void handleTube2Detection() {
  digitalWrite(ledPin, HIGH);  // Turn on the LED
  delay(100);  // Delay for 100ms
  digitalWrite(ledPin, LOW);  // Turn off the LED
  delay(100);  // Delay for another 100ms
  digitalWrite(ledPin, HIGH);  // Turn on the LED
  delay(100);  // Delay for 100ms
  digitalWrite(ledPin, LOW);  // Turn off the LED

  // Send POST request for Tube 2
  String url = "http://" + String(serverName) + "/data/2";
  sendPOSTRequest(url);
}

void setup() {
  // Initialize Serial communication for debugging
  Serial.begin(115200);

  // Set LED pin to HIGH
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  // Attempt to connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.println("Attemptting to connect to WIFI: "+String(ssid));
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < wifiTimeout) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Wi-Fi connection successful
    Serial.println("Connected to Wi-Fi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(ledPin, LOW);  // Turn off the LED
  } else {
    // Wi-Fi connection failed
    Serial.println("Failed to connect to Wi-Fi");
    wifiSetupMode = true;
  }
  

  // If Wi-Fi setup mode is active, start an access point
  if (wifiSetupMode) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    Serial.println("Wi-Fi Setup Mode. Connect to SSID: " + String(apSSID));
    Serial.print("Password: ");
    Serial.println(apPassword);
    digitalWrite(ledPin, HIGH);  // Turn on the LED
  }

  // Configure interrupt pins and attach ISRs
  pinMode(tube1Pin, INPUT_PULLUP);
  pinMode(tube2Pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tube1Pin), tube1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(tube2Pin), tube2ISR, FALLING);

  // Start AsyncElegantOTA
  AsyncElegantOTA.begin(&asyncServer, admin_username, admin_pw); // Replace with your desired username and password

  // Handle root URL request
  asyncServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });

  // Handle settings URL request
  asyncServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check if authentication is provided
    if (!request->authenticate(admin_username, admin_pw)) {
      return request->requestAuthentication();
    }
    
    String settingsPage = "<html><body>";
    settingsPage += "<h2>Wi-Fi Settings</h2>";
    settingsPage += "<form action=\"/save-settings\" method=\"POST\">";
    settingsPage += "<label for=\"ssid\">SSID:</label><br>";
    settingsPage += "<input type=\"text\" id=\"ssid\" name=\"ssid\"><br>";
    settingsPage += "<label for=\"password\">Password:</label><br>";
    settingsPage += "<input type=\"password\" id=\"password\" name=\"password\"><br>";
    settingsPage += "<h2>Server Settings</h2>";
    settingsPage += "<label for=\"server\">Server Address:</label><br>";
    settingsPage += "<input type=\"text\" id=\"server\" name=\"server\"><br>";
    settingsPage += "<input type=\"submit\" value=\"Save\">";
    settingsPage += "</form>";
    settingsPage += "</body></html>";

    request->send(200, "text/html", settingsPage);
  });

  // Handle save settings URL request
  asyncServer.on("/save-settings", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Check if authentication is provided
    //if (!request->authenticate("YourUsername", "YourPassword")) {
    //  return request->requestAuthentication();
    Serial.println("Updated settings");

    // Get the new Wi-Fi credentials and server address from the request
    String newSSID = request->arg("ssid");
    String newPassword = request->arg("password");
    String newServer = request->arg("server");
    Serial.println("New SSID: " + newSSID);
    Serial.println("New password: " + newPassword);
    Serial.println("New server: " + newServer);

    // Save the new Wi-Fi credentials and server address to memory
    WiFi.mode(WIFI_STA);
    Serial.println("Attempting to connect to WIFI: "+newSSID);
    WiFi.begin(newSSID.c_str(), newPassword.c_str());
    serverName = newServer.c_str(); // Update the server name variable
    Serial.print("Trying...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
    }
    Serial.println("Success!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(ledPin, LOW);  // Turn off the LED
  });

  asyncServer.begin();
  Serial.println("HTTP server started");
}

void loop() {
  if (newParticleDetected) {
    if (tube1Detected) {
      handleTube1Detection();
      tube1Detected = false;
    }

    if (tube2Detected) {
      handleTube2Detection();
      tube2Detected = false;
    }

    newParticleDetected = false;
  }

  // Allow AsyncElegantOTA to handle OTA updates
  AsyncElegantOTA.loop();

  // Other tasks or functionality can be added here
}
