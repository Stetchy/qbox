#include <ESP8266HTTPClient.h>

#include <EEPROM.h>
#include "qbox.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>


ESP8266WebServer server(REC_PORT);
WiFiManager wifiManager;

unsigned long lastMillis;
unsigned long currMillis;
unsigned long last;
unsigned long period = 1000;
String qVersion = "0.01";
String qboxName = "QBox-" + String(ESP.getChipId());
const char* id = qboxName.c_str();
bool waitingOnAck = false;
const uint16_t locPort = 23352;
const uint32_t stepDur = 2000;

void setup() {

  Serial.begin(115200);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  digitalWrite(LED_RED, LED_OFF);
  digitalWrite(LED_GREEN, LED_OFF);

  pinMode(PIN_ACKNOWLEDGE, INPUT_PULLUP);
  pinMode(PIN_GO, INPUT_PULLUP);
  
  readApi(api);
  launchWiFi();

  WiFiClient client;
  HTTPClient http;

  String ip = WiFi.localIP().toString();

  if (http.begin(client, api + "/qbox/create")) {
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(toJSONQBox());
  }
  http.end();
  
  server.on("/go", HTTP_GET, handleGo);
  server.on("/ack", HTTP_GET, handleAck);
  server.on("/util/qbox/info", HTTP_GET, handleBoardInfo);
  server.on("/util/qbox/ver", HTTP_GET, handleVer);
  server.on("/util/qbox/assigned", HTTP_GET, []() {
    server.send(200, "text/html", "QBox assignment acknowledged.");
  });
  server.begin();
}

void readApi(String &api) {
  char idArr[32];
  unsigned char k;
  int i = 0;
  EEPROM.begin(512);
  k = EEPROM.read(API_ADDR);
  for (i; i<33; i++) {
    if (k != '\0') {
      k = EEPROM.read(API_ADDR + i);
      idArr[i] = k;
    }
  }
  EEPROM.end();
  yield();
  idArr[i] = '\0';
  api = String(idArr);
}

void storeApi(String id) {
  int i;
  int size = id.length();
  char idArr[size+1];
  strcpy(idArr, id.c_str());
  EEPROM.begin(512);
  for (i=0; i < size; i++) {
    EEPROM.write(API_ADDR+i, idArr[i]);
  }
  EEPROM.write(API_ADDR+size, '\0');
  EEPROM.commit();
  EEPROM.end();
}

void loop() {
  server.handleClient();
}

void configModeCallback(WiFiManager *wifiManager) {
  bool redOn = digitalRead(LED_RED);
  if (millis() - last > 500) {
    last = millis();
    redOn = !redOn;
    digitalWrite(LED_GREEN, !redOn);
    digitalWrite(LED_RED, redOn);
  }
}

void apiCallback(WiFiManager *wifiManager) {
  String uri = wifiManager->api_static_name;
  storeApi(uri);
}

void launchWiFi() {
  wifiManager.setAPISaveCallback(apiCallback);
  wifiManager.setLoopCallback(configModeCallback);
  wifiManager.autoConnect(id);
  wifiManager.setLoopCallback(NULL);
  digitalWrite(LED_GREEN, LED_OFF);
  digitalWrite(LED_RED, LED_OFF);
}

const char* wl_status_to_string(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
  }
}

void handleVer() {
  server.send(200, "text/html", qVersion);
}

void handleBoardInfo() {
  String info = "";
  info += "Chip ID: " + String(ESP.getChipId()) + "\n";
  info += "Flash Chip Size: " + String(ESP.getFlashChipSize()) + "\n";
  info += "Flash Chip Speed: " + String(ESP.getFlashChipSpeed()) + "\n";
  info += "Flash Chip Mode: " + String(ESP.getFlashChipMode()) + "\n";
  info += "Sketch Size: " + String(ESP.getSketchSize()) + "\n";
  info += "Sketch MD5: " + String(ESP.getSketchMD5()) + "\n";
  server.send(200, "text/html", info);
}

String toJSON(String cue, String message) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& qbox = root.createNestedObject("qbox");
  root["text"] = message;
  root["cue"] = String(cue);
  qbox["id"] = String(id);
  qbox["ip"] = WiFi.localIP().toString();
  String json;
  root.prettyPrintTo(json);
  return json;
}

String toJSONQBox() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["isAssigned"] = true;
  root["name"] = String(ESP.getChipId());
  root["id"] = String(ESP.getChipId());
  root["ipAddress"] = WiFi.localIP().toString() + ":1000";
  String json;
  root.prettyPrintTo(json);
  return json;
}

void handleGo() {
  digitalWrite(LED_GREEN, HIGH);
  String cue;
  if (server.arg("cue") == "") {
    cue = "?";
  } else {
    cue = server.arg("cue");
  }
  server.send(200, "text/json", toJSON(cue, "Go command received."));
  delay(5000);
  digitalWrite(LED_GREEN, LOW);
}

void handleAck() {
  String cue;
  if (server.arg("cue") == "") {
    cue = "?";
  } else {
    cue = server.arg("cue");
  }
  while (digitalRead(PIN_ACKNOWLEDGE) != SWITCH_IS_DOWN) {
    bool ledOn = digitalRead(LED_RED);
    currMillis = millis();
    if (currMillis - lastMillis > period) {
      ledOn = !ledOn;
      digitalWrite(LED_RED, ledOn);
      lastMillis = currMillis;
    }
    yield();
  }
  currMillis = 0;
  digitalWrite(LED_RED, LED_OFF);
  server.send(200, "text/json", toJSON(cue, "Request acknowledged."));
}
