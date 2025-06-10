#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <Sensors.h>
#include <time.h>

#define LED_PIN 2

//Server object and wifi/time information
AsyncWebServer server(80);
const char* AP_SSID = "Hot Water Monitor";
const char* AP_PASSWORD = "solarpower";
const char* STORED_SSID = "Peachy5.0";
const char* STORED_PASSWORD = "Friendly";

//Time information
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = -28800;
const int daylightOffset_sec = 3600;

//Variables used to update wifi credentials
String inputSSID;
String inputPass;
bool updateWIFI = false;

//Static IP configuration for access point mode
IPAddress ap_IP(192, 168, 1, 1);
IPAddress ap_subnet(255, 255, 255, 0);
IPAddress ap_gateway(192, 168, 1, 1);

//Stores timedata and updates automatically when time is first fetched
struct tm timeData;

//Function declaration
bool connectWIFI(const char* ssid, const char* pass);

//Runs once at startup
void setup() {
  //Initialize serial communication for laptop terminal
  Serial.begin(115200);

  //Turn LED off to begin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  //Mount the SPIFFS file system
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  //Configure the WiFi
  WiFi.mode(WIFI_STA);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  connectWIFI(STORED_SSID, STORED_PASSWORD);

  //Callback functions of the web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });
  server.serveStatic("/", SPIFFS, "/");
  server.on("/temperature-rt", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", tempProbe::getRealTimeTemp().c_str());
  });
  server.on("/temperature-range", HTTP_GET, [](AsyncWebServerRequest *request) {
  // Check for query parameters
  if (!request->hasParam("start") || !request->hasParam("end")) {
    request->send(400, "application/json", "{\"error\":\"Missing start or end param\"}");
    return;
  }

  uint64_t startTime = strtoull(request->getParam("start")->value().c_str(), NULL, 10);
  uint64_t endTime = strtoull(request->getParam("end")->value().c_str(), NULL, 10);

  // Prepare arrays to collect data
  String glycol = "[";
  String preheat = "[";
  String ambient = "[";
  String source = "[";
  String hot = "[";

  // Open the CSV
  File file = SPIFFS.open("/historical_data.csv", "r");
  if (!file) {
    request->send(500, "application/json", "{\"error\":\"File read error\"}");
    return;
  }

  // Skip header
  file.readStringUntil('\n');

  // Read and parse each line
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // Split line into parts
    std::vector<String> parts;
    int idx;
    while ((idx = line.indexOf(',')) != -1) {
      parts.push_back(line.substring(0, idx));
      line = line.substring(idx + 1);
    }
    parts.push_back(line); // Last item

    uint64_t timestamp = strtoull(parts[0].c_str(), NULL, 10);
    if (timestamp < startTime || timestamp > endTime) continue;

    // Note that timestamp is in Unix seconds, but Highcharts uses milliseconds, so we do the conversion here
    glycol += "[" + String((uint64_t)timestamp * 1000) + "," + String(parts[1].toFloat()) + "],";
    preheat += "[" + String((uint64_t)timestamp * 1000) + "," + String(parts[2].toFloat()) + "],";
    ambient += "[" + String((uint64_t)timestamp * 1000) + "," + String(parts[3].toFloat()) + "],";
    source += "[" + String((uint64_t)timestamp * 1000) + "," + String(parts[4].toFloat()) + "],";
    hot += "[" + String((uint64_t)timestamp * 1000) + "," + String(parts[5].toFloat()) + "],";
  }

  file.close();

  // Remove trailing commas
  if (glycol.endsWith(",")) glycol.remove(glycol.length() - 1);
  if (preheat.endsWith(",")) preheat.remove(preheat.length() - 1);
  if (ambient.endsWith(",")) ambient.remove(ambient.length() - 1);
  if (source.endsWith(",")) source.remove(source.length() - 1);
  if (hot.endsWith(",")) hot.remove(hot.length() - 1);

  // Close arrays
  glycol += "]";
  preheat += "]";
  ambient += "]";
  source += "]";
  hot += "]";

  // Build response
  String json = "{";
  json += "\"glycol\":" + glycol + ",";
  json += "\"preheat\":" + preheat + ",";
  json += "\"ambient\":" + ambient + ",";
  json += "\"source\":" + source + ",";
  json += "\"hot\":" + hot;
  json += "}";

  // Send response
  request->send(200, "application/json", json);
});
  server.on("/flow-rt", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", flowMeter::instance.getRealTimeFlow().c_str());
  });
  server.on("/flow-hr", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", flowMeter::instance.getHourlyFlow().c_str());
  });
  server.on("/energy-rt", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", tempProbe::getRealTimePower().c_str());
  });
  server.on("/energy-hr", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", tempProbe::getHourlyEnergy().c_str());
  });
  server.on("/historical-data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/historical_data.csv");
  });
  server.on("/wifi-creds", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("ssid")) {
      inputSSID = request->getParam("ssid")->value();
      inputPass = request->getParam("pass")->value();
      updateWIFI = true;
    }
    request->send_P(200, "text/plain", "Connecting, please wait 15 seconds and then enter 192.168.1.1 in the address bar");
  });
  server.on("/wifi-stat", HTTP_GET, [](AsyncWebServerRequest *request){
    String details;
    if(WiFi.status() == WL_CONNECTED)
    {
      details += "Connected to ";
      details += WiFi.SSID();
      details += " with IP Address: ";
      details += WiFi.localIP().toString();
    }
    else
    {
      details += "Not connected to a network";
    }
    request->send_P(200, "text/plain", details.c_str());
  });

  //Start the web server
  server.begin();

  //Initialize the temperature probes and flow meter
  pinMode(FLOW_METER_PIN, INPUT_PULLUP);
  tempProbe::sensors.begin();
  printf("Found %d sensors\n", tempProbe::sensors.getDeviceCount());
}

void loop() {
  tempProbe::readAllProbes();
  delay(5000); // Occurs every 6 seconds because getFlowRate takes 1 second
  if(updateWIFI)
  {
    updateWIFI = false;
    connectWIFI(inputSSID.c_str(), inputPass.c_str());
  }
}

bool connectWIFI(const char* ssid, const char* pass) {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);
  for(auto i = 0; i<5; i++){
    if(WiFi.status() == WL_CONNECTED){
      Serial.println();
      Serial.printf("Connected to the WiFi network with IP Address: %s\n", WiFi.localIP().toString().c_str());
      digitalWrite(LED_PIN, HIGH);
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      delay(500);
    if(!getLocalTime(&timeData)){
      Serial.println("Failed to obtain time");
    } else {
      Serial.printf("Current time: %s\n", asctime(&timeData));
    }
      return true;
    }
    delay(3000);
    Serial.print(".");
  }
  if(WiFi.status() != WL_CONNECTED)
    Serial.println("Failed to connect to the WiFi network, operating in AP mode");
  return false;
}