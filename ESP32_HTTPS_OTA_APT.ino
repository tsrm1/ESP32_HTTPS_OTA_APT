#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>


#include "WebInterface.h" // WEB-Interface for Homepage
#include "SensorsConfig.h" // структура настроек датчиков и служб
Config config;// инициализация структуры настроек

// --- Константы ---
static const char* const CURRENT_VERSION = "0.2.7";
const char* AP_SSID = "ESP32_Config_Node";
const int RESET_PIN = 13; 
const int LED_PIN = 2;

enum ConnStatus { W_IDLE, W_CONNECTING, W_SUCCESS, W_FAIL };
ConnStatus wifiConnStatus = W_IDLE;
unsigned long apOffTime = 0;
unsigned long lastLedToggle = 0;

AsyncWebServer server(80);
DNSServer dnsServer;

// // Добавлена структура для хранения данных DS18B20
// struct DSConfig {
//   char mac[17];
//   int userIndex;
// };

void initFS(){
  if (!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed"); return; } // If filesystem - LittleFS
  Serial.println(F("Filesystem mounted successfully."));
}
bool relay_states[4] = {false, false, false, false};

void loadRelays() {
  if (LittleFS.exists("/relays.json")) {
    File f = LittleFS.open("/relays.json", "r");
    StaticJsonDocument<256> doc;
    deserializeJson(doc, f);
    for(int i=0; i<4; i++) relay_states[i] = doc["r"][i] | false;
    f.close();
  }
}

void saveRelays() {
  StaticJsonDocument<256> doc;
  JsonArray r = doc.createNestedArray("r");
  for(int i=0; i<4; i++) r.add(relay_states[i]);
  File f = LittleFS.open("/relays.json", "w");
  serializeJson(doc, f);
  f.close();
}

// --- Файловая система ---
bool loadConfig() {
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) { Serial.println("File config.json not found!"); return false; }
  DynamicJsonDocument doc(6000);
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) { Serial.println(F("Error parsing JSON")); return false; }
  configFile.close();

  // --- SYSTEM ---
  strlcpy(config.system.device_name, doc["s_dn"] | "Smart-Controller-01", 32);
  config.system.reboot_interval = doc["s_rbi"] | 0;

  // --- SERVICES ---
  // WiFi
  config.services.wifi.enabled = doc["w_en"] | false;
  strlcpy(config.services.wifi.ssid, doc["w_ssid"] | "", 32);
  strlcpy(config.services.wifi.pass, doc["w_pass"] | "", 64);
  config.services.wifi.ap_mode = doc["w_ap"] | true;

  // Web
  strlcpy(config.services.web.user, doc["u_l"] | "admin", 32);
  strlcpy(config.services.web.pass, doc["u_p"] | "admin", 32);

  // Telegram
  config.services.telegram.enabled = doc["tg_en"] | false;
  strlcpy(config.services.telegram.token, doc["tg_t"] | "", 64);
  config.services.telegram.ids_count = doc["tg_c"] | 0;
  for (int i = 0; i < 6; i++) {
    config.services.telegram.ids[i] = doc["tg_ids"][i] | 0;
  }

  // MQTT
  config.services.mqtt.enabled = doc["m_en"] | false;
  strlcpy(config.services.mqtt.host, doc["m_ip"] | "192.168.1.2", 32);
  config.services.mqtt.port = doc["m_port"] | 1883;
  strlcpy(config.services.mqtt.user, doc["m_u"] | "user", 32);
  strlcpy(config.services.mqtt.pass, doc["m_p"] | "password", 32);
  strlcpy(config.services.mqtt.base_topic, doc["m_bt"] | "home/sensor1", 64);
  config.services.mqtt.interval = doc["m_i"] | 5;

  // --- NODES: CLIMATE ---
  char labels[3][32] = {"Temperature", "Humadity", "Peassure"};
  char units[3][8] = {"°C", "%", "Pa"};
  
  // BME280
  char bme_cards[3][16] = {"card-bme-t", "card-bme-h", "card-bme-p"};
  char bme_topics[3][16] = {"/bme-t", "/bme-h", "/bme-p"};
  config.nodes.climate.bme280.enabled = doc["bme_en"] | false;
  config.nodes.climate.bme280.pin = doc["bme_p"] | 21;
  //strlcpy(config.nodes.climate.bme280.type, doc["bme_type"] | "I2C", 4);
  for (int i = 0; i < 3; i++) {
    strlcpy(config.nodes.climate.bme280.labels[i], doc["bme_l"][i] | labels[i], 32);
    strlcpy(config.nodes.climate.bme280.units[i], doc["bme_u"][i] | units[i], 8);
    strlcpy(config.nodes.climate.bme280.ui_cards[i], doc["bme_c"][i] | bme_cards[i], 16);
    strlcpy(config.nodes.climate.bme280.topics[i], doc["bme_t"][i] | bme_topics[i], 16);
  }

  // DHT22  
  char dht_cards[2][16] = {"card-dht-t", "card-dht-h"};
  char dht_topics[2][16] = {"/dht-t", "/dht-h"};
  config.nodes.climate.dht22.enabled = doc["dht_en"] | false;
  config.nodes.climate.dht22.pin = doc["dht_p"] | 15;
  for (int i = 0; i < 2; i++) {
    strlcpy(config.nodes.climate.dht22.labels[i], doc["dht_l"][i] | labels[i], 32);
    strlcpy(config.nodes.climate.dht22.units[i], doc["dht_u"][i] | units[i], 8);
    strlcpy(config.nodes.climate.dht22.ui_cards[i], doc["dht_c"][i] | dht_cards[i], 16);
    strlcpy(config.nodes.climate.dht22.topics[i], doc["dht_t"][i] | dht_topics[i], 16);
  }

  // DS18B20
  char ds_labels[4][32] = {"Radiator 1", "Radiator 2", "Radiator 3", "Radiator 4"};
  char ds_cards[4][16] = {"card-t1", "card-t2", "card-t3", "card-t4"};
  char ds_topics[4][16] = {"/t1", "/t2", "/t3", "/t4"};
  char ds_units[4][8] = {"°C","°C","°C","°C"};
  config.nodes.climate.ds18b20.enabled = doc["ds_en"] | false;
  config.nodes.climate.ds18b20.pin = doc["ds_p"] | 4;
  for (int i = 0; i < 4; i++) {
    strlcpy(config.nodes.climate.ds18b20.macs[i], doc["ds_m"][i] | "", 18);
    strlcpy(config.nodes.climate.ds18b20.labels[i], doc["ds_l"][i] | labels[i], 32);
    strlcpy(config.nodes.climate.ds18b20.ui_cards[i], doc["ds_c"][i] | ds_cards[i], 16);
    strlcpy(config.nodes.climate.ds18b20.topics[i], doc["ds_t"][i] | ds_topics[i], 16);  
    strlcpy(config.nodes.climate.ds18b20.units[i], doc["ds_u"][i] | ds_units[i], 8);
  }

  // TCRT5000
  config.nodes.climate.tcrt5000.enabled = doc["tcrt_en"] | false;
  config.nodes.climate.tcrt5000.pin = doc["tcrt_pin"] | 21;
  strlcpy(config.nodes.climate.tcrt5000.label, doc["tcrt_l"] | "Освещение (TCRT)", 32);
  strlcpy(config.nodes.climate.tcrt5000.unit, doc["tcrt_u"] | "Lux", 8);
  strlcpy(config.nodes.climate.tcrt5000.ui_card, doc["tcrt_c"] | "card-tcrt", 16);
  strlcpy(config.nodes.climate.tcrt5000.topic, doc["tcrt_t"] | "/lux", 16);  
  // --- NODES: BINARY ---
  // PIR
  config.nodes.binary.pir.enabled = doc["pir_en"] | false;
  config.nodes.binary.pir.pin = doc["pir_p"] | 35;
  strlcpy(config.nodes.binary.pir.label, doc["pir_l"] | "Motion", 32);
  strlcpy(config.nodes.binary.pir.ui_card, doc["pir_c"] | "card-pir", 16);
  strlcpy(config.nodes.binary.pir.topic, doc["pir_t"] | "/motion", 16);

  // LD2420
  config.nodes.binary.ld2420.enabled = doc["ld_en"] | false;
  config.nodes.binary.ld2420.pin = doc["ld_p"] | 35;
  strlcpy(config.nodes.binary.ld2420.label, doc["ld_l"] | "Presence", 32);
  strlcpy(config.nodes.binary.ld2420.ui_card, doc["ld_c"] | "card-pres", 16);
  strlcpy(config.nodes.binary.ld2420.topic, doc["ld_t"] | "/presence", 16);

  // Door
  config.nodes.binary.door.enabled = doc["door_en"] | false;
  config.nodes.binary.door.pin = doc["door_p"] | 36;
  strlcpy(config.nodes.binary.door.label, doc["door_l"] | "Door", 32);
  strlcpy(config.nodes.binary.door.ui_card, doc["door_c"] | "card-door", 16);
  strlcpy(config.nodes.binary.door.topic, doc["door_t"] | "/door", 16);

  // Flood
  config.nodes.binary.flood.enabled = doc["fl_en"] | false;
  config.nodes.binary.flood.pin = doc["fl_p"] | 34;
  strlcpy(config.nodes.binary.flood.label, doc["fl_l"] | "Leak", 32);
  strlcpy(config.nodes.binary.flood.ui_card, doc["fl_c"] | "card-flood", 16);
  strlcpy(config.nodes.binary.flood.topic, doc["fl_t"] | "/flood", 16);

  // --- NODES: ANALOG ---
  config.nodes.analog.light_resistor.enabled = doc["5516_en"] | false;
  config.nodes.analog.light_resistor.pin = doc["5516_p"] | 39;
  strlcpy(config.nodes.analog.light_resistor.label, doc["5516_l"] | "Light (LDR)", 32);
  strlcpy(config.nodes.analog.light_resistor.ui_card, doc["5516_c"] | "card-lux-5516", 16);
  strlcpy(config.nodes.analog.light_resistor.topic, doc["5516_t"] | "/lux_raw", 16);

  // --- NODES: ACTUATORS ---
  int r_pins[4] = {26, 27, 14, 13};
  char r_labels[4][32]={"Rele 1", "Rele 2", "Rele 3", "Rele 4"};
  char r_cards[4][16]={"card-r0", "card-r1", "card-r2", "card-r3"};
  char r_topics[4][16]={"/r0", "/r1", "/r2", "/r3"};
  config.nodes.actuators.relays.enabled = doc["r_en"] | false;
  for (int i = 0; i < 4; i++) {
    config.nodes.actuators.relays.pins[i] = doc["r_p"][i] | r_pins[i];
    strlcpy(config.nodes.actuators.relays.labels[i], doc["r_l"][i] | r_labels[i], 32);
    strlcpy(config.nodes.actuators.relays.ui_cards[i], doc["r_c"][i] | r_cards[i], 16);
    strlcpy(config.nodes.actuators.relays.topics[i], doc["r_t"][i] | r_topics[i], 16);
  }

  Serial.println("Settings loaded. OK!");
  return true;
}

bool saveConfig() {
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {Serial.println("Failed to open config.json for writing"); return false; }
  DynamicJsonDocument doc(6000); 

  // --- SYSTEM ---
  doc["s_dn"] = config.system.device_name;
  doc["s_rbi"] = config.system.reboot_interval;

  // --- SERVICES ---
  doc["w_en"] = config.services.wifi.enabled;
  doc["w_ssid"] = config.services.wifi.ssid;
  doc["w_pass"] = config.services.wifi.pass;
  doc["w_ap"] = config.services.wifi.ap_mode;

  doc["u_l"] = config.services.web.user;
  doc["u_p"] = config.services.web.pass;

  doc["tg_en"] = config.services.telegram.enabled;
  doc["tg_t"] = config.services.telegram.token;
  doc["tg_c"] = config.services.telegram.ids_count;
  JsonArray tg_ids = doc.createNestedArray("tg_ids");
  for (int i = 0; i < 6; i++) tg_ids.add(config.services.telegram.ids[i]);

  doc["m_en"] = config.services.mqtt.enabled;
  doc["m_ip"] = config.services.mqtt.host;
  doc["m_port"] = config.services.mqtt.port;
  doc["m_u"] = config.services.mqtt.user;
  doc["m_p"] = config.services.mqtt.pass;
  doc["m_bt"] = config.services.mqtt.base_topic;
  doc["m_i"] = config.services.mqtt.interval;

  // --- NODES: CLIMATE ---
  // BME280
  doc["bme_en"] = config.nodes.climate.bme280.enabled;
  doc["bme_p"] = config.nodes.climate.bme280.pin;
  JsonArray bme_l = doc.createNestedArray("bme_l");
  JsonArray bme_u = doc.createNestedArray("bme_u");
  JsonArray bme_c = doc.createNestedArray("bme_c");
  JsonArray bme_t = doc.createNestedArray("bme_t");
  for (int i = 0; i < 3; i++) {
    bme_l.add(config.nodes.climate.bme280.labels[i]);
    bme_u.add(config.nodes.climate.bme280.units[i]);
    bme_c.add(config.nodes.climate.bme280.ui_cards[i]);
    bme_t.add(config.nodes.climate.bme280.topics[i]);
  }

  // DHT22
  doc["dht_en"] = config.nodes.climate.dht22.enabled;
  doc["dht_p"] = config.nodes.climate.dht22.pin;
  JsonArray dht_l = doc.createNestedArray("dht_l");
  JsonArray dht_u = doc.createNestedArray("dht_u");
  JsonArray dht_c = doc.createNestedArray("dht_c");
  JsonArray dht_t = doc.createNestedArray("dht_t");
  for (int i = 0; i < 2; i++) {
    dht_l.add(config.nodes.climate.dht22.labels[i]);
    dht_u.add(config.nodes.climate.dht22.units[i]);
    dht_c.add(config.nodes.climate.dht22.ui_cards[i]);
    dht_t.add(config.nodes.climate.dht22.topics[i]);
  }

  // DS18B20
  doc["ds_en"] = config.nodes.climate.ds18b20.enabled;
  doc["ds_p"] = config.nodes.climate.ds18b20.pin;
  JsonArray ds_u = doc.createNestedArray("ds_u");
  JsonArray ds_m = doc.createNestedArray("ds_m");
  JsonArray ds_l = doc.createNestedArray("ds_l");
  JsonArray ds_c = doc.createNestedArray("ds_c");
  JsonArray ds_t = doc.createNestedArray("ds_t");
  for (int i = 0; i < 4; i++) {
    ds_u.add(config.nodes.climate.ds18b20.units[i]);
    ds_m.add(config.nodes.climate.ds18b20.macs[i]);
    ds_l.add(config.nodes.climate.ds18b20.labels[i]);
    ds_c.add(config.nodes.climate.ds18b20.ui_cards[i]);
    ds_t.add(config.nodes.climate.ds18b20.topics[i]);
  }

  // TCRT5000
  doc["tcrt_en"] = config.nodes.climate.tcrt5000.enabled;
  doc["tcrt_p"] = config.nodes.climate.tcrt5000.pin;
  doc["tcrt_l"] = config.nodes.climate.tcrt5000.label;
  doc["tcrt_u"] = config.nodes.climate.tcrt5000.unit;
  doc["tcrt_c"] = config.nodes.climate.tcrt5000.ui_card;
  doc["tcrt_t"] = config.nodes.climate.tcrt5000.topic;

  // --- NODES: BINARY ---
  // PIR
  doc["pir_en"] = config.nodes.binary.pir.enabled;
  doc["pir_p"] = config.nodes.binary.pir.pin;
  doc["pir_l"] = config.nodes.binary.pir.label;
  doc["pir_c"] = config.nodes.binary.pir.ui_card;
  doc["pir_t"] = config.nodes.binary.pir.topic;

  // LD2420
  doc["ld_en"] = config.nodes.binary.ld2420.enabled;
  doc["ld_p"] = config.nodes.binary.ld2420.pin;
  doc["ld_l"] = config.nodes.binary.ld2420.label;
  doc["ld_c"] = config.nodes.binary.ld2420.ui_card;
  doc["ld_t"] = config.nodes.binary.ld2420.topic;

  // Door
  doc["door_en"] = config.nodes.binary.door.enabled;
  doc["door_p"] = config.nodes.binary.door.pin;
  doc["door_l"] = config.nodes.binary.door.label;
  doc["door_c"] = config.nodes.binary.door.ui_card;
  doc["door_t"] = config.nodes.binary.door.topic;

  // Flood
  doc["fl_en"] = config.nodes.binary.flood.enabled;
  doc["fl_p"] = config.nodes.binary.flood.pin;
  doc["fl_l"] = config.nodes.binary.flood.label;
  doc["fl_c"] = config.nodes.binary.flood.ui_card;
  doc["fl_t"] = config.nodes.binary.flood.topic;

  // --- NODES: ANALOG ---
  doc["5516_en"] = config.nodes.analog.light_resistor.enabled;
  doc["5516_p"] = config.nodes.analog.light_resistor.pin;
  doc["5516_l"] = config.nodes.analog.light_resistor.label;
  doc["5516_c"] = config.nodes.analog.light_resistor.ui_card;
  doc["5516_t"] = config.nodes.analog.light_resistor.topic;

  // --- NODES: ACTUATORS ---
  doc["r_en"] = config.nodes.actuators.relays.enabled;
  JsonArray r_p = doc.createNestedArray("r_p");
  JsonArray r_l = doc.createNestedArray("r_l");
  JsonArray r_c = doc.createNestedArray("r_c");
  JsonArray r_t = doc.createNestedArray("r_t");
  for (int i = 0; i < 4; i++) {
    r_p.add(config.nodes.actuators.relays.pins[i]);
    r_l.add(config.nodes.actuators.relays.labels[i]);
    r_c.add(config.nodes.actuators.relays.ui_cards[i]);
    r_t.add(config.nodes.actuators.relays.topics[i]);
  }

  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Failed to write to file");
  }

  configFile.close();
  // printConfigFile();
  Serial.println("Settings saved. OK.");
  return true;
}



// --- LED Helpers ---
void handleLEDStatus() {
  if (wifiConnStatus == W_CONNECTING) {
    if (millis() - lastLedToggle > 100) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); lastLedToggle = millis(); }
  } else if (wifiConnStatus == W_IDLE) {
    if (millis() - lastLedToggle > 1000) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); lastLedToggle = millis(); }
  }
}

// --- API Routing ---
bool isCORS=true;
void setupAPI() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(config.services.web.user, config.services.web.pass)) return request->requestAuthentication();
    request->send_P(200, "text/html", index_html);
  });
  server.on("/api/wifi-status", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc; bool c = (WiFi.status() == WL_CONNECTED); doc["connected"] = c;
    if(c) { doc["ssid"] = WiFi.SSID(); doc["ip"] = WiFi.localIP().toString(); doc["rssi"] = WiFi.RSSI(); }
    String j; serializeJson(doc, j); 
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", j);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    } else request->send(200, "application/json", j);
  });
  server.on("/api/ap-disable", HTTP_GET, [](AsyncWebServerRequest *request){
    if (WiFi.status() == WL_CONNECTED) apOffTime = millis() + 5000; 
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    } else request->send(200);
  });
  server.on("/api/values", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<1024> doc; doc["in_t"] = String(random(220, 250)/10.0, 1); doc["in_h"] = String(random(40, 50));
    doc["lux"] = String(random(100, 500)); 
    doc["lux_out"] = String(random(0, 5000));
    doc["pir"] = random(0, 10) > 8; 
    doc["pres"] = random(0, 10) > 7;
    doc["out_t"] = String(random(-50, 150)/10.0, 1); 
    doc["out_h"] = String(random(60, 90));
    doc["t1"] = String(random(400, 600)/10.0, 1); 
    doc["t2"] = String(random(400, 600)/10.0, 1);
    doc["door"] = random(0, 10) > 7; 
    doc["flood"] = random(0, 10) > 8;
    String j; 
    serializeJson(doc, j); 
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", j);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    } else request->send(200, "application/json", j);
  });
  server.on("/api/get-all", HTTP_GET, [](AsyncWebServerRequest *request){
    // Используем DynamicJsonDocument для работы с большой вложенной структурой
    DynamicJsonDocument doc(5000); 

    // --- System ---
    doc["s_dn"] = config.system.device_name;
    doc["s_rbi"] = config.system.reboot_interval;

    // --- Services: WiFi & Web ---
    doc["w_en"] = config.services.wifi.enabled;
    doc["w_ssid"] = config.services.wifi.ssid;
    //doc["w_pass"] = config.services.wifi.pass;
    doc["w_ap"]   = config.services.wifi.ap_mode;
    //doc["u_l"]    = config.services.web.user;
    //doc["u_p"]    = config.services.web.pass;

    // --- Services: Telegram ---
    doc["tg_en"] = config.services.telegram.enabled;
    //doc["tg_t"]  = config.services.telegram.token;
    doc["tg_c"]  = config.services.telegram.ids_count;
    JsonArray tg_ids = doc.createNestedArray("tg_ids");
    for(int i=0; i<config.services.telegram.ids_count; i++) tg_ids.add(config.services.telegram.ids[i]);

    // --- Services: MQTT ---
    doc["m_en"]   = config.services.mqtt.enabled;
    doc["m_ip"]   = config.services.mqtt.host;
    doc["m_port"] = config.services.mqtt.port;
    //doc["m_u"]    = config.services.mqtt.user;
    //doc["m_p"]    = config.services.mqtt.pass;
    doc["m_bt"]   = config.services.mqtt.base_topic;
    doc["m_i"]    = config.services.mqtt.interval;

    // --- Nodes: Climate ---
    // --- BME ---
    doc["bme_en"] = config.nodes.climate.bme280.enabled;
    doc["bme_p"] = config.nodes.climate.bme280.pin;
    JsonArray bme_l = doc.createNestedArray("bme_l");
    for(int i=0; i<3; i++) bme_l.add(config.nodes.climate.bme280.labels[i]);
    JsonArray bme_u = doc.createNestedArray("bme_u");
    for(int i=0; i<3; i++) bme_u.add(config.nodes.climate.bme280.units[i]);
    JsonArray bme_t = doc.createNestedArray("bme_t");
    for(int i=0; i<3; i++) bme_t.add(config.nodes.climate.bme280.topics[i]);


    // --- DHT ---
    doc["dht_en"] = config.nodes.climate.dht22.enabled;
    doc["dht_p"] = config.nodes.climate.dht22.pin;
    JsonArray dht_l = doc.createNestedArray("dht_l");
    for(int i=0; i<2; i++) dht_l.add(config.nodes.climate.dht22.labels[i]);
    JsonArray dht_u = doc.createNestedArray("dht_u");
    for(int i=0; i<2; i++) dht_u.add(config.nodes.climate.dht22.units[i]);
    JsonArray dht_t = doc.createNestedArray("dht_t");
    for(int i=0; i<2; i++) dht_t.add(config.nodes.climate.dht22.topics[i]);

    // --- DS18B20 ---
    doc["ds_en"] = config.nodes.climate.ds18b20.enabled;
    doc["ds_p"] = config.nodes.climate.ds18b20.pin;
    JsonArray ds_m = doc.createNestedArray("ds_m");
    for(int i=0; i<4; i++) ds_m.add(config.nodes.climate.ds18b20.macs[i]);
    JsonArray ds_l = doc.createNestedArray("ds_l");
    for(int i=0; i<4; i++) ds_l.add(config.nodes.climate.ds18b20.labels[i]);
    JsonArray ds_u = doc.createNestedArray("ds_u");
    for(int i=0; i<4; i++) ds_u.add(config.nodes.climate.ds18b20.units[i]);
    JsonArray ds_t = doc.createNestedArray("ds_t");
    for(int i=0; i<4; i++) ds_t.add(config.nodes.climate.ds18b20.topics[i]);




    // --- TCRT5000 ---
    doc["tcrt_en"] = config.nodes.climate.tcrt5000.enabled;
    doc["tcrt_p"] = config.nodes.climate.tcrt5000.pin;
    doc["tcrt_l"] = config.nodes.climate.tcrt5000.label;
    doc["tcrt_u"] = config.nodes.climate.tcrt5000.unit;
    doc["tcrt_t"] = config.nodes.climate.tcrt5000.topic;

    // --- Nodes: Binary ---
    // --- SR501 ---
    doc["pir_en"]  = config.nodes.binary.pir.enabled;
    doc["pir_p"]   = config.nodes.binary.pir.pin;
    doc["pir_l"]   = config.nodes.binary.pir.label;
    doc["pir_t"]   = config.nodes.binary.pir.topic;

    // --- LD2420 ---
    doc["ld_en"] = config.nodes.binary.ld2420.enabled;
    doc["ld_p"]  = config.nodes.binary.ld2420.pin;
    doc["ld_l"] = config.nodes.binary.ld2420.label;
    doc["ld_t"]  = config.nodes.binary.ld2420.topic;

    // --- Door ---
    doc["door_en"] = config.nodes.binary.door.enabled;
    doc["door_p"]  = config.nodes.binary.door.pin;
    doc["door_l"] = config.nodes.binary.door.label;
    doc["door_t"]  = config.nodes.binary.door.topic;

    // --- Flood ---
    doc["fl_en"]   = config.nodes.binary.flood.enabled;
    doc["fl_p"]    = config.nodes.binary.flood.pin;
    doc["fl_l"]   = config.nodes.binary.flood.label;
    doc["fl_t"]    = config.nodes.binary.flood.topic;

    // --- Nodes: Analog ---
    // --- Resistor 5516 ---
    doc["5516_en"] = config.nodes.analog.light_resistor.enabled;
    doc["5516_p"]  = config.nodes.analog.light_resistor.pin;
    doc["5516_l"] = config.nodes.analog.light_resistor.label;
    doc["5516_t"]  = config.nodes.analog.light_resistor.topic;

    // --- Nodes: Actuators (Relays) ---
    doc["r_en"] = config.nodes.actuators.relays.enabled;
    JsonArray r_p = doc.createNestedArray("r_p");
    for(int i=0; i<4; i++) r_p.add(config.nodes.actuators.relays.pins[i]);
    JsonArray r_l = doc.createNestedArray("r_l");
    for(int i=0; i<4; i++) r_l.add(config.nodes.actuators.relays.labels[i]);
    JsonArray r_t = doc.createNestedArray("r_t");
    for(int i=0; i<4; i++) r_t.add(config.nodes.actuators.relays.topics[i]);

    // Сериализация
    String responseData;
    serializeJson(doc, responseData);

    // Отправка ответа с учетом CORS
    if (isCORS) {
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", responseData);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
    } else {
        request->send(200, "application/json", responseData);
    }
});
  server.on("/api/get-relays", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc; JsonArray r=doc.createNestedArray("r");
    for(int i=0; i<4; i++) r.add(relay_states[i]);
    String j; 
    serializeJson(doc, j); 
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", j);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    } else request->send(200, "application/json", j);
  });
  server.on("/api/set-relay", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("id") && request->hasParam("st")) {
      int id = request->getParam("id")->value().toInt(); bool st = request->getParam("st")->value() == "true";
      if(id >= 0 && id < 4) { relay_states[id] = st; saveRelays(); }
    }
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    } else request->send(200);
  });

  server.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks(false, false, false, 150);
    DynamicJsonDocument doc(3072); JsonArray arr = doc.to<JsonArray>();
    int limit = (n > 25) ? 25 : n;
    for(int i=0; i<limit; i++) {
      JsonObject obj = arr.createNestedObject();
      obj["ssid"] = WiFi.SSID(i); obj["rssi"] = WiFi.RSSI(i);
      obj["enc"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    String j; 
    serializeJson(doc, j); 
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", j);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    } else request->send(200, "application/json", j);
    WiFi.scanDelete();
  });
  server.on("/api/wifi-connect", HTTP_POST, [](AsyncWebServerRequest *request){
    strlcpy(config.services.wifi.ssid, request->getParam("s", true)->value().c_str(), 32);
    strlcpy(config.services.wifi.pass, request->getParam("p", true)->value().c_str(), 64);
    saveConfig(); 
    wifiConnStatus = W_CONNECTING; 
    WiFi.disconnect(true); // Сброс драйвера перед новой попыткой
    delay(100);
    WiFi.begin(config.services.wifi.ssid, config.services.wifi.pass); 
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    } else request->send(200);
  });
  server.on("/api/set-sens", HTTP_POST, [](AsyncWebServerRequest *request){
    // 1. Климатические датчики
    if(request->hasParam("bme_en", true)) 
        config.nodes.climate.bme280.enabled = (request->getParam("bme_en", true)->value() == "true");
    
    if(request->hasParam("dht_en", true)) 
        config.nodes.climate.dht22.enabled = (request->getParam("dht_en", true)->value() == "true");
    
    if(request->hasParam("ds_en", true)) 
        config.nodes.climate.ds18b20.enabled = (request->getParam("ds_en", true)->value() == "true");
    
    if(request->hasParam("tcrt_en", true)) 
        config.nodes.climate.tcrt5000.enabled = (request->getParam("tcrt_en", true)->value() == "true");

    // 2. Бинарные датчики (Binary)
    if(request->hasParam("pir_en", true)) 
        config.nodes.binary.pir.enabled = (request->getParam("pir_en", true)->value() == "true");
    
    if(request->hasParam("ld_en", true)) 
        config.nodes.binary.ld2420.enabled = (request->getParam("ld_en", true)->value() == "true");
    
    if(request->hasParam("door_en", true)) 
        config.nodes.binary.door.enabled = (request->getParam("door_en", true)->value() == "true");
    
    if(request->hasParam("fl_en", true)) 
        config.nodes.binary.flood.enabled = (request->getParam("fl_en", true)->value() == "true");

    // 3. Аналоговые датчики
    if(request->hasParam("5516_en", true)) 
        config.nodes.analog.light_resistor.enabled = (request->getParam("5516_en", true)->value() == "true");
    // 4. Актуаторы (Реле)
    if(request->hasParam("r_en", true)) 
        config.nodes.actuators.relays.enabled = (request->getParam("r_en", true)->value() == "true");
    // Сохраняем обновленную структуру в LittleFS
    saveConfig(); 

    // Ответ сервера с учетом CORS
    if (isCORS){
        AsyncWebServerResponse *response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
    } else {
        request->send(200);
    }
  });
  server.on("/api/set-srv", HTTP_POST, [](AsyncWebServerRequest *request){
    // Активация и деактивация сенсоров, переключатели
    if(request->hasParam("bme_en", true)) 
        config.nodes.climate.bme280.enabled = (request->getParam("bme_en", true)->value() == "true");
    if(request->hasParam("dht_en", true)) 
        config.nodes.climate.dht22.enabled = (request->getParam("dht_en", true)->value() == "true");
    if(request->hasParam("ds_en", true)) 
        config.nodes.climate.ds18b20.enabled = (request->getParam("ds_en", true)->value() == "true");
    if(request->hasParam("tcrt_en", true)) 
        config.nodes.climate.tcrt5000.enabled = (request->getParam("tcrt_en", true)->value() == "true");
    if(request->hasParam("pir_en", true)) 
        config.nodes.binary.pir.enabled = (request->getParam("pir_en", true)->value() == "true");
    if(request->hasParam("ld_en", true)) 
        config.nodes.binary.ld2420.enabled = (request->getParam("ld_en", true)->value() == "true");
    if(request->hasParam("door_en", true)) 
        config.nodes.binary.door.enabled = (request->getParam("door_en", true)->value() == "true");
    if(request->hasParam("fl_en", true)) 
        config.nodes.binary.flood.enabled = (request->getParam("fl_en", true)->value() == "true");
    if(request->hasParam("5516_en", true)) 
        config.nodes.analog.light_resistor.enabled = (request->getParam("5516_en", true)->value() == "true");
    if(request->hasParam("r_en", true)) 
        config.nodes.actuators.relays.enabled = (request->getParam("r_en", true)->value() == "true");

    // --- 1. Системные настройки (System) ---
    if(request->hasParam("s_dn", true)) 
        strlcpy(config.system.device_name, request->getParam("s_dn", true)->value().c_str(), 32);
    if(request->hasParam("s_rbi", true)) 
        config.system.reboot_interval = request->getParam("s_rbi", true)->value().toInt();

    // --- 2. WiFi и Web-авторизация (Services) ---
    if(request->hasParam("w_en", true)) 
        config.services.wifi.enabled = (request->getParam("w_en", true)->value() == "true");
    if(request->hasParam("w_ssid", true)) 
        strlcpy(config.services.wifi.ssid, request->getParam("w_ssid", true)->value().c_str(), 32);
    if(request->hasParam("w_pass", true)) 
        strlcpy(config.services.wifi.pass, request->getParam("w_pass", true)->value().c_str(), 64);
    if(request->hasParam("w_ap", true)) 
        config.services.wifi.ap_mode = (request->getParam("w_ap", true)->value() == "true");
    
    if(request->hasParam("u_l", true)) 
        strlcpy(config.services.web.user, request->getParam("u_l", true)->value().c_str(), 32);
    if(request->hasParam("u_p", true)) 
        strlcpy(config.services.web.pass, request->getParam("u_p", true)->value().c_str(), 32);

    // --- 3. Telegram (Services) ---
    if(request->hasParam("tg_en", true)) 
        config.services.telegram.enabled = (request->getParam("tg_en", true)->value() == "true");
    if(request->hasParam("tg_t", true)) 
        strlcpy(config.services.telegram.token, request->getParam("tg_t", true)->value().c_str(), 64);
    if(request->hasParam("tg_c", true)) 
        config.services.telegram.ids_count = request->getParam("tg_c", true)->value().toInt();
    
    // Цикл для сбора ID пользователей Telegram (параметры id0, id1...id5)
    for(int i = 0; i < 6; i++) {
        String pName = "id" + String(i);
        if(request->hasParam(pName, true)) {
            config.services.telegram.ids[i] = request->getParam(pName, true)->value().toInt();
        }
    }

    // --- 4. MQTT (Services) ---
    if(request->hasParam("m_en", true)) 
        config.services.mqtt.enabled = (request->getParam("m_en", true)->value() == "true");
    if(request->hasParam("m_ip", true)) 
        strlcpy(config.services.mqtt.host, request->getParam("m_ip", true)->value().c_str(), 32);
    if(request->hasParam("m_port", true)) 
        config.services.mqtt.port = request->getParam("m_port", true)->value().toInt();
    if(request->hasParam("m_u", true)) 
        strlcpy(config.services.mqtt.user, request->getParam("m_u", true)->value().c_str(), 32);
    if(request->hasParam("m_p", true)) 
        strlcpy(config.services.mqtt.pass, request->getParam("m_p", true)->value().c_str(), 32);
    if(request->hasParam("m_bt", true)) 
        strlcpy(config.services.mqtt.base_topic, request->getParam("m_bt", true)->value().c_str(), 64);
    if(request->hasParam("m_i", true)) 
        config.services.mqtt.interval = request->getParam("m_i", true)->value().toInt();

    // Сохранение в LittleFS
    saveConfig(); 

    // Ответ сервера
    if (isCORS) {
        AsyncWebServerResponse *response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
    } else {
        request->send(200);
    }
  });
  server.on("/api/ds_scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int pin = request->getParam("pin")->value().toInt();

    OneWire ow(pin);
    DallasTemperature ds(&ow);
    ds.begin();
    ds.requestTemperatures();
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();
    DeviceAddress addr;
    for (int i = 0; i < ds.getDeviceCount(); i++) {
      ds.getAddress(addr, i);
      JsonObject o = arr.createNestedObject();
      char rom[24];
      sprintf(rom,"%02:%02:%02:%02:%02:%02:%02:%02",
        addr[0],addr[1],addr[2],addr[3],
        addr[4],addr[5],addr[6],addr[7]);
      o["rom"] = rom;
      o["temp"] = ds.getTempC(addr);
    }
    String j;
    serializeJson(doc, j);
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", j);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    } else request->send(200, "application/json", j);
  });
  server.begin();
}

// --- Loop Handlers ---
void handleWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED && wifiConnStatus != W_SUCCESS) {
    wifiConnStatus = W_SUCCESS; digitalWrite(LED_PIN, HIGH);
    Serial.printf("\n>>> Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    if (WiFi.getMode() & WIFI_AP) { apOffTime = millis() + 60000; } 
  }
}

void handleAPTimeout() {
  if (apOffTime > 0 && millis() > apOffTime) { 
    Serial.println("Rebooting to STA only..."); apOffTime = 0;
    WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
    delay(1000); ESP.restart(); 
  }
}

void handleResetButton() {
  if (digitalRead(RESET_PIN) == LOW) {
    bool aborted = false;
    for(int i=0; i<50; i++) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); delay(100); if(digitalRead(RESET_PIN) == HIGH) { aborted = true; break; } }
    if(!aborted) { LittleFS.format(); delay(500); ESP.restart(); }
    else { digitalWrite(LED_PIN, (wifiConnStatus == W_SUCCESS) ? HIGH : LOW); }
  }
}

// --- Main ---
void setup() {
  Serial.begin(115200);
  initFS();
  pinMode(LED_PIN, OUTPUT); pinMode(RESET_PIN, INPUT_PULLUP);
  loadConfig(); loadRelays();

  WiFi.persistent(false); // Запрещаем SDK мешать нашему коду

  if(strlen(config.services.wifi.ssid) > 2 && strlen(config.services.wifi.pass) > 4 ) {
    Serial.println("Silent STA attempt...");
    WiFi.mode(WIFI_STA); 
    WiFi.begin(config.services.wifi.ssid, config.services.wifi.pass);
    wifiConnStatus = W_CONNECTING;
    unsigned long startT = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startT < 15000) { handleLEDStatus(); delay(100); }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Starting Config AP...");
    WiFi.mode(WIFI_AP_STA); WiFi.softAP(AP_SSID);
    wifiConnStatus = (strlen(config.services.wifi.ssid) > 2) ? W_CONNECTING : W_IDLE;
  } else {
    wifiConnStatus = W_SUCCESS; digitalWrite(LED_PIN, HIGH);
    Serial.print("Success IP: "); Serial.println(WiFi.localIP());
  }
  setupAPI();
}

void loop() {
  handleWiFiStatus();
  handleAPTimeout();
  handleLEDStatus();
  handleResetButton();
}