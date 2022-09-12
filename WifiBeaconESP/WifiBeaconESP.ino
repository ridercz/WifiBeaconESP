#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "WifiBeaconEspConfig.h"

ESP8266WebServer server(HTTP_PORT);
DNSServer dnsServer;
int lastClientCount = -1;
char currentProfile[64];
char adminPrefix[64];
char ssid[32];
int channelNumber;
unsigned long restartMillis = 0;
unsigned long nextBlinkMillis = 0;
unsigned long nextBlinkState = true;

void setup() {
  // Finish initialization of ESP
  delay(1000);

  // Print banner
  Serial.begin(9600);
  Serial.println();
  Serial.println("WifiBeaconESP version " VERSION);
  Serial.println("https://github.com/ridercz/WifiBeaconESP");
  Serial.println("Copyright (c) Michal Altair Valasek, 2022");
  Serial.println("              www.rider.cz | www.altair.blog");
  Serial.println();

  // Switch to AP mode
  Serial.print("Switching to AP mode...");
  WiFi.mode(WIFI_AP);
  Serial.println("OK");

  // Initialize SPIFFS
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);
#endif

  Serial.print("Initializing SPIFFS...");
  if (SPIFFS.begin()) {
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    halt_system();
  }

  // Load system configuration from file
  StaticJsonDocument<512> cfgJson;
  Serial.printf("Loading system configuration from '%s'...", FILENAME_SYSTEM_CFG);
  File cfgFile = SPIFFS.open(FILENAME_SYSTEM_CFG, FILE_READ);
  DeserializationError error = deserializeJson(cfgJson, cfgFile);
  if (error) {
    Serial.println("Failed!");
    if (!SPIFFS.exists(FILENAME_SYSTEM_CFG)) Serial.println("File not found!");
    Serial.println("Loading default configuration");
  }
  strlcpy(currentProfile, cfgJson["currentProfile"] | DEFAULT_PROFILE_NAME, sizeof(currentProfile));
  strlcpy(adminPrefix, cfgJson["adminPrefix"] | DEFAULT_ADMIN_PREFIX, sizeof(adminPrefix));
  cfgFile.close();
  Serial.println("OK");
  Serial.printf("  Profile:          %s\n", currentProfile);
  Serial.printf("  Admin prefix:     %s\n", adminPrefix);

  // Load configuration from profile
  String profileConfigurationFileName = String(String("/") + String(currentProfile) + String(FILENAME_PROFILE_CFG));
  if (!SPIFFS.exists(profileConfigurationFileName)) {
    Serial.printf("Requested configuration file '%s' was not found, using profile '%s' instead.\n", profileConfigurationFileName.c_str(), DEFAULT_PROFILE_NAME);
    strlcpy(currentProfile, DEFAULT_PROFILE_NAME, sizeof(currentProfile));
    profileConfigurationFileName = String(String("/") + DEFAULT_PROFILE_NAME + String(FILENAME_PROFILE_CFG));
  }

  Serial.printf("Loading profile configuration from '%s'...", profileConfigurationFileName.c_str());
  cfgFile = SPIFFS.open(profileConfigurationFileName.c_str(), FILE_READ);
  error = deserializeJson(cfgJson, cfgFile);
  if (error) {
    Serial.println("Failed!");
    if (!SPIFFS.exists(profileConfigurationFileName.c_str())) Serial.println("File not found!");
    halt_system();
  }
  channelNumber = cfgJson["channel"];
  strlcpy(ssid, cfgJson["ssid"], sizeof(ssid));
  cfgFile.close();
  Serial.println("OK");
  Serial.printf("  SSID:             %s\n", ssid);
  Serial.printf("  Channel:          %i\n", channelNumber);
  Serial.printf("  MAC:              %s\n", WiFi.softAPmacAddress().c_str());
  Serial.printf("  IP address:       %s\n", AP_ADDRESS);

  // Parse IP address and netmask
  IPAddress ip, nm;
  ip.fromString(AP_ADDRESS);
  nm.fromString(AP_NETMASK);

  // Configure AP
  Serial.print("Configuring access point...");
  WiFi.softAPConfig(ip, ip, nm);
  WiFi.softAP(ssid, "", channelNumber, false, AP_MAX_CLIENTS);
  Serial.println("OK");

  // Setup DNS
  Serial.print("Starting DNS server...");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", ip);
  Serial.println("OK");

  // Setup mDNS
  Serial.print("Starting mDNS server...");
  if (!MDNS.begin("beacon")) {
    Serial.println("Failed!");
    halt_system();
  }
  Serial.print("Configuring...");
  MDNS.addService("http", "tcp", 80);
  Serial.println("OK");

  // Configure and enable HTTP server
  String adminSave = String(String(adminPrefix) + URL_ADMIN_SAVE);
  String adminReset = String(String(adminPrefix) + URL_ADMIN_RESET);
  String adminCss = String(String(adminPrefix) + URL_ADMIN_CSS);

  Serial.print("Starting HTTP server...");
  server.serveStatic(adminCss.c_str(), SPIFFS, FILENAME_ADMIN_CSS);
  server.on(adminPrefix, handleAdminIndex);   // Administration homepage
  server.on(adminSave, handleAdminSave);      // Administration save settings
  server.on(adminReset, handleAdminReset);    // Administration reset system
  server.onNotFound(handleRequest);           // All other pages
  server.begin();
  Serial.println("OK");
  Serial.println();

#ifdef LED_BUILTIN
  digitalWrite(LED_BUILTIN, true);
#endif
}

void loop() {
  // Handle network transactions
  dnsServer.processNextRequest();
  server.handleClient();

  // Perform reset if requested
  if (restartMillis != 0 && millis() >= restartMillis) ESP.restart();

  // Track number of connected clients
  int currentClientCount = WiFi.softAPgetStationNum();
  if (lastClientCount != currentClientCount) {
    lastClientCount = currentClientCount;
    Serial.printf("Connected clients: %i\n", currentClientCount);
  }

  // Blink LED
#ifdef LED_BUILTIN
  if (millis() >= nextBlinkMillis) {
    digitalWrite(LED_BUILTIN, nextBlinkState);
    nextBlinkMillis = millis() + NORMAL_BLINK_INTERVAL;
    nextBlinkState = !nextBlinkState;
  }
#endif
}

void handleAdminIndex() {
  // Prepare first part of admin homepage
  String html = ADMIN_HTML_HEADER;
  html += "<p>WifiBeaconESP version <b>" VERSION "</b> is running.</p><p>\n";
  html += "<p><b>" + String(lastClientCount) + "</b> clients connected.</p>\n";
  html += "<form action=\"" URL_ADMIN_RESET "\" method=\"GET\"><p><input type=\"submit\" value=\"Reset device\" /></p></form>\n";
  html += "<form action=\"" URL_ADMIN_SAVE "\" method=\"POST\">\n<p><select name=\"currentProfile\" style=\"width:345px\">";

  // List all profiles
  Dir root = SPIFFS.openDir("/");
  while (root.next()) {
    String fileName = String(root.fileName());
    if (fileName.endsWith(FILENAME_PROFILE_CFG)) {
      String profileName = fileName.substring(1, fileName.indexOf("/", 1));
      if (profileName.equalsIgnoreCase(currentProfile)) {
        html += "<option selected=\"selected\" value=\"" + profileName + "\">" + profileName + " (current)</option>\n";
      } else {
        html += "<option value=\"" + profileName + "\">" + profileName + "</option>\n";
      }
    }
  }

  // Prepare second part of admin homepage
  html += "</select></p>\n<p><input type=\"submit\" value=\"Change Profile\" style=\"width: 150px\" /></p>\n</form>";
  html += ADMIN_HTML_FOOTER;

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Server", "WifiBeaconESP/" VERSION);
  server.send(200, "text/html", html);
}

void handleAdminSave() {
  // Load system configuration from file
  StaticJsonDocument<512> cfgJson;
  Serial.printf("Loading system configuration from '%s'...", FILENAME_SYSTEM_CFG);
  File cfgFile = SPIFFS.open(FILENAME_SYSTEM_CFG, FILE_READ);
  DeserializationError error = deserializeJson(cfgJson, cfgFile);
  if (error) {
    Serial.println("Failed!");
    if (!SPIFFS.exists(FILENAME_SYSTEM_CFG)) Serial.println("File not found!");
    halt_system();
  }
  cfgFile.close();
  Serial.println("OK");

  // Modify configuration
  cfgJson["currentProfile"] = server.arg("currentProfile");

  // Save configuration to file
  Serial.print("Saving system configuration...");
  cfgFile = SPIFFS.open(FILENAME_SYSTEM_CFG, FILE_WRITE);
  serializeJsonPretty(cfgJson, cfgFile);
  cfgFile.close();
  Serial.println("OK");

  String html = ADMIN_HTML_HEADER;
  html += "<h2>Profile Change</h2>";
  html += "<p>Active profile was changed to <b>" + server.arg("currentProfile") + "</b>. To apply the changes, reset device.</p>";
  html += "<form action=\"" URL_ADMIN_RESET "\" method=\"GET\"><p><input type=\"submit\" value=\"Reset device\"/></p></form>";
  html += ADMIN_HTML_FOOTER;

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Server", "WifiBeaconESP/" VERSION);
  server.send(200, "text/html", html);
}

void handleAdminReset() {
  String html = ADMIN_HTML_HEADER;
  html += "<h2>Restart Device</h2>";
  html += "<p>The device is restarting in " + String(RESTART_DELAY) + " seconds.</p><p>Please wait.</p>";
  html += ADMIN_HTML_FOOTER;

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Server", "WifiBeaconESP/" VERSION);
  server.send(200, "text/html", html);

  // Request restart
  Serial.printf("Restarting MCU in %i seconds...\n", RESTART_DELAY);
  restartMillis = millis() + RESTART_DELAY * 1000;
}

void handleRequest() {
  // Send files from portal directory
  if (sendFileFromProfile(server.uri())) return;

  // Fallback - file not found
  String message = "<html><head><title>302 Found</title></head><body><a href=\"http://" AP_ADDRESS "/\">Continue here</a></body></html>";
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Server", "WifiBeaconESP/" VERSION);
  server.sendHeader("Location", "http://" AP_ADDRESS "/");
  server.send(302, "text/html", message);
}

bool sendFileFromProfile(String path) {
  // Append index.htm to directory requests
  if (path.endsWith("/")) path += "index.htm";

  // Determine MIME type
  String dataType = "application/octet-stream";
  if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".txt")) {
    dataType = "text/plain";
  }

  // Find appropriate file in SPIFFS
  File dataFile;
  String fsPath = String(String("/") + String(currentProfile) + path);
  if (SPIFFS.exists(fsPath.c_str())) {
    // File exists
    dataFile = SPIFFS.open(fsPath.c_str(), FILE_READ);
  } else {
    // File does not exist, but maybe it's directory with index.htm
    fsPath += "/index.htm";
    if (SPIFFS.exists(fsPath.c_str())) {
      // Yes, it is - use index.htm
      dataType = "text/html";
      dataFile = SPIFFS.open(fsPath.c_str(), FILE_READ);
    } else {
      // No, it is not
      return false;
    }
  }

  // Stream the resulting file
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Server", "WifiBeaconESP/" VERSION);
  server.streamFile(dataFile, dataType);
  dataFile.close();
  return true;
}

void halt_system() {
  Serial.println("--- System halted! ---");
  while (true) {
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, true);
    delay(ERROR_BLINK_INTERVAL);
    digitalWrite(LED_BUILTIN, false);
    delay(ERROR_BLINK_INTERVAL);
#endif
  }
}
