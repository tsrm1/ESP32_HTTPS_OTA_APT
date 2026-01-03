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
static const char* const MANIFEST_URL  = "https://secobj.netlify.app/esp32/ESP32_HTTPS_OTA_APT/manifest.json";
bool primitiveUpdateFlag = false;

const char* AP_SSID = "ESP32_Config_Node";
const int RESET_PIN = 13; 
const int LED_PIN = 2;
static const unsigned long DIAGNOSTIC_INTERVAL = 10000;         // 10 секунд

enum ConnStatus { W_IDLE, W_CONNECTING, W_SUCCESS, W_FAIL };
ConnStatus wifiConnStatus = W_IDLE;
unsigned long apOffTime = 0;
unsigned long lastLedToggle = 0;

// --- Сертификат безопасности (ISRG Root X1 для Netlify) ---
// Позволяет ESP32 убедиться, что она общается именно с вашим сервером
static const char* root_ca_pem PROGMEM = 
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAMS9LwWIC9SdcptqXvYVCnMwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIAbq6t21fTxCteNL89No684u9InYsuW0UmK6RRXmYn06lIod72+YV88\n"
"39/72FpExD4U87G00S27xWj7+Wz64EobfR9V4I1QzP61mXqD1L/l4Y/22OQjYJtL\n"
"WJ23p4/S4+F+Jv6A3v2qN76v9+I7m7pW9t5518DNDV+P+uEPrk+f9N+O0hR8R9Wv\n"
"vvAd4LaxmX8Rvh9XvIDHAA3HnYARC45S9Y1S6NvdY8JshCNoJvEtyTdbY4C5iS5B\n"
"dB76Y7101uXy49qYmBTAp/L6M0lBndy7D6I94E0KxSNDKz6/V3/B4H8/8D64Y+Wk\n"
"5+lOteV6V7Y/H9yN+fP9L0Tf677vA494f6n7H3X49z+0O6v0K394f7vA494f6n7H\n"
"-----END CERTIFICATE-----\n";

WiFiClientSecure secureClient;

AsyncWebServer server(80);
DNSServer dnsServer;

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
  char labels[3][32] = {"Temperature, °C", "Humadity, %", "Pressure, Pa"};
  
  // BME280
  char bme_topics[3][16] = {"/bme-t", "/bme-h", "/bme-p"};
  config.sensors.bme.enabled = doc["bme_en"] | false;
  config.sensors.bme.pins[0] = doc["bme_p"][0] | 21;
  //strlcpy(config.nodes.climate.bme280.type, doc["bme_type"] | "I2C", 4);
  for (int i = 0; i < 3; i++) {
    strlcpy(config.sensors.bme.labels[i], doc["bme_l"][i] | labels[i], 32);
    strlcpy(config.sensors.bme.topics[i], doc["bme_t"][i] | bme_topics[i], 16);
  }

  // DHT22  
  char dht_topics[2][16] = {"/dht-t", "/dht-h"};
  config.sensors.dht.enabled = doc["dht_en"] | false;
  config.sensors.dht.pins[0] = doc["dht_p"][0] | 15;
  for (int i = 0; i < 2; i++) {
    strlcpy(config.sensors.dht.labels[i], doc["dht_l"][i] | labels[i], 32);
    strlcpy(config.sensors.dht.topics[i], doc["dht_t"][i] | dht_topics[i], 16);
  }

  // DS18B20
  char ds_labels[4][32] = {"Radiator 1", "Radiator 2", "Radiator 3", "Radiator 4"};
  char ds_topics[4][16] = {"/t1", "/t2", "/t3", "/t4"};
  config.sensors.ds.enabled = doc["ds_en"] | false;
  config.sensors.ds.pins[0] = doc["ds_p"][0] | 4;
  for (int i = 0; i < 4; i++) {
    strlcpy(config.sensors.ds.macs[i], doc["ds_m"][i] | "", 18);
    strlcpy(config.sensors.ds.labels[i], doc["ds_l"][i] | labels[i], 32);
    strlcpy(config.sensors.ds.topics[i], doc["ds_t"][i] | ds_topics[i], 16);  
  }

  // TCRT5000
  config.sensors.tcrt.enabled = doc["tcrt_en"] | false;
  config.sensors.tcrt.pins[0] = doc["tcrt_p"][0] | 21;
  strlcpy(config.sensors.tcrt.labels[0], doc["tcrt_l"][0] | "Освещение (TCRT), Lux", 32);
  strlcpy(config.sensors.tcrt.topics[0], doc["tcrt_t"][0] | "/lux", 16);  

  // PIR
  config.sensors.pir.enabled = doc["pir_en"] | false;
  config.sensors.pir.pins[0] = doc["pir_p"][0] | 35;
  strlcpy(config.sensors.pir.labels[0], doc["pir_l"][0] | "Motion", 32);
  strlcpy(config.sensors.pir.topics[0], doc["pir_t"][0] | "/motion", 16);

  // LD2420
  config.sensors.ld.enabled = doc["ld_en"] | false;
  config.sensors.ld.pins[0] = doc["ld_p"][0] | 35;
  strlcpy(config.sensors.ld.labels[0], doc["ld_l"][0] | "Presence", 32);
  strlcpy(config.sensors.ld.topics[0], doc["ld_t"][0] | "/presence", 16);

  // Door
  config.sensors.dr.enabled = doc["dr_en"] | false;
  config.sensors.dr.pins[0] = doc["dr_p"][0] | 36;
  strlcpy(config.sensors.dr.labels[0], doc["dr_l"][0] | "Door", 32);
  strlcpy(config.sensors.dr.topics[0], doc["dr_t"][0] | "/door", 16);

  // Flood
  config.sensors.fl.enabled = doc["fl_en"] | false;
  config.sensors.fl.pins[0] = doc["fl_p"][0] | 34;
  strlcpy(config.sensors.fl.labels[0], doc["fl_l"][0] | "Leak", 32);
  strlcpy(config.sensors.fl.topics[0], doc["fl_t"][0] | "/flood", 16);

  // --- NODES: ANALOG ---
  config.sensors.lr.enabled = doc["lr_en"] | false;
  config.sensors.lr.pins[0] = doc["lr_p"][0] | 39;
  strlcpy(config.sensors.lr.labels[0], doc["lr_l"][0] | "Light (LDR)", 32);
  strlcpy(config.sensors.lr.topics[0], doc["lr_t"][0] | "/lux_raw", 16);

  // --- NODES: ACTUATORS ---
  int r_pins[4] = {26, 27, 14, 13};
  char r_labels[4][32]={"Rele 1", "Rele 2", "Rele 3", "Rele 4"};
  char r_cards[4][16]={"card-r0", "card-r1", "card-r2", "card-r3"};
  char r_topics[4][16]={"/r0", "/r1", "/r2", "/r3"};
  config.sensors.relays.enabled = doc["r_en"] | false;
  for (int i = 0; i < 4; i++) {
    config.sensors.relays.pins[i] = doc["r_p"][i] | r_pins[i];
    strlcpy(config.sensors.relays.labels[i], doc["r_l"][i] | r_labels[i], 32);
    strlcpy(config.sensors.relays.topics[i], doc["r_t"][i] | r_topics[i], 16);
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

  // BME280
  doc["bme_en"] = config.sensors.bme.enabled;
  JsonArray bme_p = doc.createNestedArray("bme_p");
  JsonArray bme_l = doc.createNestedArray("bme_l");
  JsonArray bme_t = doc.createNestedArray("bme_t");
  bme_p.add(config.sensors.bme.pins[0]);
  for (int i = 0; i < 3; i++) {
    bme_l.add(config.sensors.bme.labels[i]);
    bme_t.add(config.sensors.bme.topics[i]);
  }

  // DHT22
  doc["dht_en"] = config.sensors.dht.enabled;
  JsonArray dht_p = doc.createNestedArray("dht_p");
  JsonArray dht_l = doc.createNestedArray("dht_l");
  JsonArray dht_t = doc.createNestedArray("dht_t");
  dht_p.add(config.sensors.dht.pins[0]);
  for (int i = 0; i < 2; i++) {
    dht_l.add(config.sensors.dht.labels[i]);
    dht_t.add(config.sensors.dht.topics[i]);
  }

  // DS18B20
  doc["ds_en"] = config.sensors.ds.enabled;
  JsonArray ds_p = doc.createNestedArray("ds_p");
  JsonArray ds_l = doc.createNestedArray("ds_l");
  JsonArray ds_t = doc.createNestedArray("ds_t");
  JsonArray ds_m = doc.createNestedArray("ds_m");
  ds_p.add(config.sensors.ds.pins[0]);
  for (int i = 0; i < 4; i++) {
    ds_l.add(config.sensors.ds.labels[i]);
    ds_t.add(config.sensors.ds.topics[i]);
    ds_m.add(config.sensors.ds.macs[i]);
  }

  // TCRT5000
  doc["tcrt_en"] = config.sensors.tcrt.enabled;
  JsonArray tcrt_p = doc.createNestedArray("tcrt_p");
  JsonArray tcrt_l = doc.createNestedArray("tcrt_l");
  JsonArray tcrt_t = doc.createNestedArray("tcrt_t");
  tcrt_p.add(config.sensors.tcrt.pins[0]);
  tcrt_l.add(config.sensors.tcrt.labels[0]);
  tcrt_t.add(config.sensors.tcrt.topics[0]);

  // PIR
  doc["pir_en"] = config.sensors.pir.enabled;
  JsonArray pir_p = doc.createNestedArray("pir_p");
  JsonArray pir_l = doc.createNestedArray("pir_l");
  JsonArray pir_t = doc.createNestedArray("pir_t");
  pir_p.add(config.sensors.pir.pins[0]);
  pir_l.add(config.sensors.pir.labels[0]);
  pir_t.add(config.sensors.pir.topics[0]);

  // LD2420
  doc["ld_en"] = config.sensors.ld.enabled;
  JsonArray ld_p = doc.createNestedArray("ld_p");
  JsonArray ld_l = doc.createNestedArray("ld_l");
  JsonArray ld_t = doc.createNestedArray("ld_t");
  ld_p.add(config.sensors.ld.pins[0]);
  ld_l.add(config.sensors.ld.labels[0]);
  ld_t.add(config.sensors.ld.topics[0]);

  // Door
  doc["dr_en"] = config.sensors.dr.enabled;
  JsonArray dr_p = doc.createNestedArray("dr_p");
  JsonArray dr_l = doc.createNestedArray("dr_l");
  JsonArray dr_t = doc.createNestedArray("dr_t");
  dr_p.add(config.sensors.dr.pins[0]);
  dr_l.add(config.sensors.dr.labels[0]);
  dr_t.add(config.sensors.dr.topics[0]);


  // Flood
  doc["fl_en"] = config.sensors.fl.enabled;
  JsonArray fl_p = doc.createNestedArray("fl_p");
  JsonArray fl_l = doc.createNestedArray("fl_l");
  JsonArray fl_t = doc.createNestedArray("fl_t");
  fl_p.add(config.sensors.fl.pins[0]);
  fl_l.add(config.sensors.fl.labels[0]);
  fl_t.add(config.sensors.fl.topics[0]);

  // --- Light Resistor ---
  doc["lr_en"] = config.sensors.lr.enabled;
  JsonArray lr_p = doc.createNestedArray("lr_p");
  JsonArray lr_l = doc.createNestedArray("lr_l");
  JsonArray lr_t = doc.createNestedArray("lr_t");
  lr_p.add(config.sensors.lr.pins[0]);
  lr_l.add(config.sensors.lr.labels[0]);
  lr_t.add(config.sensors.lr.topics[0]);

  // --- ACTUATORS ---
  doc["r_en"] = config.sensors.relays.enabled;
  JsonArray r_p = doc.createNestedArray("r_p");
  JsonArray r_l = doc.createNestedArray("r_l");
  JsonArray r_c = doc.createNestedArray("r_c");
  JsonArray r_t = doc.createNestedArray("r_t");
  for (int i = 0; i < 4; i++) {
    r_p.add(config.sensors.relays.pins[i]);
    r_l.add(config.sensors.relays.labels[i]);
    r_t.add(config.sensors.relays.topics[i]);
  }

  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Failed to write to file");
  }

  configFile.close();
  // printConfigFile();
  Serial.println("Settings saved. OK.");
  return true;
}


// Помощник для копирования строковых параметров (Labels и Topics)
void updateCharParam(AsyncWebServerRequest *request, const String& key, char* target, size_t maxSize) {
    if (request->hasParam(key, true)) {
        String val = request->getParam(key, true)->value();
        strlcpy(target, val.c_str(), maxSize);
    }
}

// Обобщенная функция для обновления настроек любого датчика
void processSensorParams(AsyncWebServerRequest *request, const String& prefix, 
                         bool &enabled, int* pins, int pinCount, 
                         char (*labels)[32], char (*topics)[16], int dataCount) {
    
    // 1. Обработка флага включения (_en)
    if (request->hasParam(prefix + "_en", true)) {
        enabled = (request->getParam(prefix + "_en", true)->value() == "true");
    }

    // 2. Обработка пинов (_p0, _p1...)
    for (int i = 0; i < pinCount; i++) {
        String pKey = prefix + "_p" + String(i);
        if (request->hasParam(pKey, true)) {
            pins[i] = request->getParam(pKey, true)->value().toInt();
        }
    }

    // 3. Обработка меток (_l0, _l1...) и топиков (_t0, _t1...)
    for (int i = 0; i < dataCount; i++) {
        updateCharParam(request, prefix + "_l" + String(i), labels[i], 32);
        updateCharParam(request, prefix + "_t" + String(i), topics[i], 16);
    }
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
    StaticJsonDocument<256> doc;
    bool c = (WiFi.status() == WL_CONNECTED);
    doc["connected"] = c;
    if(c) { 
      doc["ssid"] = WiFi.SSID(); 
      doc["ip"] = WiFi.localIP().toString(); 
      doc["rssi"] = WiFi.RSSI(); 
      }
    String j;
    serializeJson(doc, j);
    if (isCORS){
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", j);
      response->addHeader("Access-Control-Allow-Origin", "*");
      request->send(response);
    } else request->send(200, "application/json", j);
  });

  server.on("/api/update", HTTP_POST, [](AsyncWebServerRequest *request){
    if (isCORS){
        AsyncWebServerResponse *response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
    } else request->send(200);
    if(request->hasParam("up-to-date", true)) 
      if (request->getParam("up-to-date", true)->value() == "true"){
        Serial.println("Start update...");
        primitiveUpdateFlag = true; // Просто ставим метку
      }
  });

  server.on("/api/get-remote-manifest", HTTP_GET, [](AsyncWebServerRequest *request) {
    HTTPClient http;
    secureClient.setInsecure(); // Для простоты проксирования
    
    if (http.begin(secureClient, MANIFEST_URL)) {
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            // Отправляем данные клиенту, добавляя CORS заголовок от самой ESP32
            AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
        } else {
            request->send(500, "text/plain", "Failed to fetch remote manifest");
        }
        http.end();
    }
  });
  server.on("/api/set-ws", HTTP_POST, [](AsyncWebServerRequest *request) {
    // В AsyncWebServer ответ обычно отправляется здесь, 
    // но если нужно отправить результат обработки тела, 
    // лучше делать это в конце onBody или через флаги.
    request->send(200); 
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    
    DynamicJsonDocument doc(512); // Увеличил до 512 для запаса
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (!error) {
        char w_u[32] = "";
        char w_u_n[32] = "";
        char w_p[32] = "";
        char w_p_n[32] = "";

        // Извлекаем данные. Тут оператор '|' работает корректно
        strlcpy(w_u,   doc["w_u"]   | "", 32);
        strlcpy(w_u_n, doc["w_u_n"] | "", 32);
        strlcpy(w_p,   doc["w_p"]   | "", 32);
        strlcpy(w_p_n, doc["w_p_n"] | "", 32);

        // 1. Сравниваем строки через strcmp (0 означает строки равны)
        if (strcmp(config.services.web.user, w_u) == 0 && 
            strcmp(config.services.web.pass, w_p) == 0) {
            
            // 2. Проверяем длину введенных строк через strlen
            if (strlen(w_u_n) >= 4 && strlen(w_p_n) >= 4) {
                
                Serial.println("Updating credentials...");
                
                // 3. Копируем новые данные в конфиг
                strlcpy(config.services.web.user, w_u_n, 32);
                strlcpy(config.services.web.pass, w_p_n, 32);
                saveConfig();
            } else {
                Serial.println("Error: New credentials too short");
            }
        } else {
            Serial.println("Error: Old credentials mismatch");
        }
    } else {
        Serial.print("Ошибка JSON: ");
        Serial.println(error.c_str());
    }
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
    StaticJsonDocument<1024> doc; 
    doc["bme_v0"] = String(random(-50, 150)/10.0, 1); 
    doc["bme_v1"] = String(random(60, 90));
    doc["bme_v2"] = String(random(99000, 102000));
    doc["dht_v0"] = String(random(220, 250)/10.0, 1); 
    doc["dht_v1"] = String(random(40, 50));
    doc["ds_v0"] = String(random(400, 600)/10.0, 1); 
    doc["ds_v1"] = String(random(400, 600)/10.0, 1); 
    doc["ds_v2"] = String(random(400, 600)/10.0, 1); 
    doc["ds_v3"] = String(random(400, 600)/10.0, 1); 


    doc["tcrt_v0"] = String(random(100, 500)); 
    doc["lr_v0"] = String(random(0, 100));
     
    doc["ld_v0"] = random(0, 10) > 7;
    doc["dr_v0"] = random(0, 10) > 7; 
    doc["fl_v0"] = random(0, 10) > 8;
    doc["pir_v0"] = random(0, 10) > 8;
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
    doc["bme_en"] = config.sensors.bme.enabled;
    JsonArray bme_p = doc.createNestedArray("bme_p");
    bme_p.add(config.sensors.bme.pins[0]);
    JsonArray bme_l = doc.createNestedArray("bme_l");
    for(int i=0; i<3; i++) bme_l.add(config.sensors.bme.labels[i]);
    JsonArray bme_t = doc.createNestedArray("bme_t");
    for(int i=0; i<3; i++) bme_t.add(config.sensors.bme.topics[i]);


    // --- DHT ---
    doc["dht_en"] = config.sensors.dht.enabled;
    JsonArray dht_p = doc.createNestedArray("dht_p");
    dht_p.add(config.sensors.dht.pins[0]);
    JsonArray dht_l = doc.createNestedArray("dht_l");
    for(int i=0; i<2; i++) dht_l.add(config.sensors.dht.labels[i]);
    JsonArray dht_t = doc.createNestedArray("dht_t");
    for(int i=0; i<2; i++) dht_t.add(config.sensors.dht.topics[i]);

    // --- DS18B20 ---
    doc["ds_en"] = config.sensors.ds.enabled;
    JsonArray ds_p = doc.createNestedArray("ds_p");
    ds_p.add(config.sensors.ds.pins[0]);
    JsonArray ds_m = doc.createNestedArray("ds_m");
    for(int i=0; i<4; i++) ds_m.add(config.sensors.ds.macs[i]);
    JsonArray ds_l = doc.createNestedArray("ds_l");
    for(int i=0; i<4; i++) ds_l.add(config.sensors.ds.labels[i]);
    JsonArray ds_t = doc.createNestedArray("ds_t");
    for(int i=0; i<4; i++) ds_t.add(config.sensors.ds.topics[i]);

    // --- TCRT5000 ---
    doc["tcrt_en"] = config.sensors.tcrt.enabled;
    JsonArray tcrt_p = doc.createNestedArray("tcrt_p");
    tcrt_p.add(config.sensors.tcrt.pins[0]);
    JsonArray tcrt_l = doc.createNestedArray("tcrt_l");
    tcrt_l.add(config.sensors.tcrt.labels[0]);
    JsonArray tcrt_t = doc.createNestedArray("tcrt_t");
    tcrt_t.add(config.sensors.tcrt.topics[0]);

    // --- SR501 ---
    doc["pir_en"] = config.sensors.pir.enabled;
    JsonArray pir_p = doc.createNestedArray("pir_p");
    pir_p.add(config.sensors.pir.pins[0]);
    JsonArray pir_l = doc.createNestedArray("pir_l");
    pir_l.add(config.sensors.pir.labels[0]);
    JsonArray pir_t = doc.createNestedArray("pir_t");
    pir_t.add(config.sensors.pir.topics[0]);

    // --- LD2420 ---
    doc["ld_en"] = config.sensors.ld.enabled;
    JsonArray ld_p = doc.createNestedArray("ld_p");
    ld_p.add(config.sensors.ld.pins[0]);
    JsonArray ld_l = doc.createNestedArray("ld_l");
    ld_l.add(config.sensors.ld.labels[0]);
    JsonArray ld_t = doc.createNestedArray("ld_t");
    ld_t.add(config.sensors.ld.topics[0]);
  
    // --- Door ---
    doc["dr_en"] = config.sensors.dr.enabled;
    JsonArray dr_p = doc.createNestedArray("dr_p");
    dr_p.add(config.sensors.dr.pins[0]);
    JsonArray dr_l = doc.createNestedArray("dr_l");
    dr_l.add(config.sensors.dr.labels[0]);
    JsonArray dr_t = doc.createNestedArray("dr_t");
    dr_t.add(config.sensors.dr.topics[0]);
 
    // --- Flood ---
    doc["fl_en"] = config.sensors.fl.enabled;
    JsonArray fl_p = doc.createNestedArray("fl_p");
    fl_p.add(config.sensors.fl.pins[0]);
    JsonArray fl_l = doc.createNestedArray("fl_l");
    fl_l.add(config.sensors.fl.labels[0]);
    JsonArray fl_t = doc.createNestedArray("fl_t");
    fl_t.add(config.sensors.fl.topics[0]);

    // --- Resistor 5516 ---
    doc["lr_en"] = config.sensors.lr.enabled;
    JsonArray lr_p = doc.createNestedArray("lr_p");
    lr_p.add(config.sensors.lr.pins[0]);
    JsonArray lr_l = doc.createNestedArray("lr_l");
    lr_l.add(config.sensors.lr.labels[0]);
    JsonArray lr_t = doc.createNestedArray("lr_t");
    lr_t.add(config.sensors.lr.topics[0]);

    // --- Nodes: Actuators (Relays) ---
    doc["r_en"] = config.sensors.relays.enabled;
    JsonArray r_p = doc.createNestedArray("r_p");
    for(int i=0; i<4; i++) r_p.add(config.sensors.relays.pins[i]);
    JsonArray r_l = doc.createNestedArray("r_l");
    for(int i=0; i<4; i++) r_l.add(config.sensors.relays.labels[i]);
    JsonArray r_t = doc.createNestedArray("r_t");
    for(int i=0; i<4; i++) r_t.add(config.sensors.relays.topics[i]);

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
    StaticJsonDocument<256> doc; 
    JsonArray r=doc.createNestedArray("r");
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
      int id = request->getParam("id")->value().toInt();
      bool st = request->getParam("st")->value() == "true";
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
    // Обработка BME280 (1 пин, 3 набора данных: T, H, P)
    processSensorParams(request, "bme", 
                        config.sensors.bme.enabled, 
                        config.sensors.bme.pins, 1, 
                        config.sensors.bme.labels,
                        config.sensors.bme.topics, 3);
    // Обработка DHT22 (1 пин, 2 набора данных: T, H, P)
    processSensorParams(request, "dht", 
                        config.sensors.dht.enabled, 
                        config.sensors.dht.pins, 1, 
                        config.sensors.dht.labels,
                        config.sensors.dht.topics, 2);
    // Обработка DS18B20 (1 пин, 4 датчика)
    processSensorParams(request, "ds", 
                        config.sensors.ds.enabled, 
                        config.sensors.ds.pins, 1, 
                        config.sensors.ds.labels, 
                        config.sensors.ds.topics, 4);

    // Обработка TCRT5000 (1 пин, 1 датчика)
    processSensorParams(request, "tcrt", 
                        config.sensors.tcrt.enabled, 
                        config.sensors.tcrt.pins, 1, 
                        config.sensors.tcrt.labels, 
                        config.sensors.tcrt.topics, 1);
    // Обработка SR501 (1 пин, 1 датчика)
    processSensorParams(request, "pir", 
                        config.sensors.pir.enabled, 
                        config.sensors.pir.pins, 1, 
                        config.sensors.pir.labels, 
                        config.sensors.pir.topics, 1);
    // Обработка LR2420 (1 пин, 1 датчика)
    processSensorParams(request, "ld", 
                        config.sensors.ld.enabled, 
                        config.sensors.ld.pins, 1, 
                        config.sensors.ld.labels, 
                        config.sensors.ld.topics, 1);
    // Обработка Magnet Sensor, DOOR (1 пин, 1 датчика)
    processSensorParams(request, "dr", 
                        config.sensors.dr.enabled, 
                        config.sensors.dr.pins, 1, 
                        config.sensors.dr.labels, 
                        config.sensors.dr.topics, 1);
    // Обработка Flood Sensor, Flood (1 пин, 1 датчика)
    processSensorParams(request, "fl", 
                        config.sensors.fl.enabled, 
                        config.sensors.fl.pins, 1, 
                        config.sensors.fl.labels, 
                        config.sensors.fl.topics, 1);
    // Обработка LR 5516, Light Resistor (1 пин, 1 датчика)
    processSensorParams(request, "lr", 
                        config.sensors.lr.enabled, 
                        config.sensors.lr.pins, 1, 
                        config.sensors.lr.labels, 
                        config.sensors.lr.topics, 1);
    // Обработка RELEx4, Rele unit (4 пина, 4 реле)
    processSensorParams(request, "r", 
                        config.sensors.relays.enabled, 
                        config.sensors.relays.pins, 4, 
                        config.sensors.relays.labels, 
                        config.sensors.relays.topics, 4);
    
    // Сохранение обновленной конфигурации в файл
    if (saveConfig()) {
        request->send(200, "application/json", "{\"status\":\"saved\"}");
    } else {
        request->send(500, "application/json", "{\"status\":\"error_saving\"}");
    }

    // // 1. Климатические датчики
    // if(request->hasParam("bme_en", true)) 
    //     config.sensors.bme.enabled = (request->getParam("bme_en", true)->value() == "true");
    
    // if(request->hasParam("dht_en", true)) 
    //     config.sensors.dht.enabled = (request->getParam("dht_en", true)->value() == "true");
    
    // if(request->hasParam("ds_en", true)) 
    //     config.sensors.ds.enabled = (request->getParam("ds_en", true)->value() == "true");
    
    // if(request->hasParam("tcrt_en", true)) 
    //     config.sensors.tcrt.enabled = (request->getParam("tcrt_en", true)->value() == "true");

    // // 2. Бинарные датчики (Binary)
    // if(request->hasParam("pir_en", true)) 
    //     config.sensors.pir.enabled = (request->getParam("pir_en", true)->value() == "true");
    
    // if(request->hasParam("ld_en", true)) 
    //     config.sensors.ld.enabled = (request->getParam("ld_en", true)->value() == "true");
    
    // if(request->hasParam("dr_en", true)) 
    //     config.sensors.dr.enabled = (request->getParam("dr_en", true)->value() == "true");
    
    // if(request->hasParam("fl_en", true)) 
    //     config.sensors.fl.enabled = (request->getParam("fl_en", true)->value() == "true");

    // // 3. Аналоговые датчики
    // if(request->hasParam("lr_en", true)) 
    //     config.sensors.lr.enabled = (request->getParam("lr_en", true)->value() == "true");
    // // 4. Актуаторы (Реле)
    // if(request->hasParam("r_en", true)) 
    //     config.sensors.relays.enabled = (request->getParam("r_en", true)->value() == "true");
    // Сохраняем обновленную структуру в LittleFS
    //saveConfig(); 

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
        config.sensors.bme.enabled = (request->getParam("bme_en", true)->value() == "true");
    if(request->hasParam("dht_en", true)) 
        config.sensors.dht.enabled = (request->getParam("dht_en", true)->value() == "true");
    if(request->hasParam("ds_en", true)) 
        config.sensors.ds.enabled = (request->getParam("ds_en", true)->value() == "true");
    if(request->hasParam("tcrt_en", true)) 
        config.sensors.tcrt.enabled = (request->getParam("tcrt_en", true)->value() == "true");
    if(request->hasParam("pir_en", true)) 
        config.sensors.pir.enabled = (request->getParam("pir_en", true)->value() == "true");
    if(request->hasParam("ld_en", true)) 
        config.sensors.ld.enabled = (request->getParam("ld_en", true)->value() == "true");
    if(request->hasParam("dr_en", true)) 
        config.sensors.dr.enabled = (request->getParam("dr_en", true)->value() == "true");
    if(request->hasParam("fl_en", true)) 
        config.sensors.fl.enabled = (request->getParam("fl_en", true)->value() == "true");
    if(request->hasParam("lr_en", true)) 
        config.sensors.lr.enabled = (request->getParam("lr_en", true)->value() == "true");
    if(request->hasParam("r_en", true)) 
        config.sensors.relays.enabled = (request->getParam("r_en", true)->value() == "true");

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
//  Сравнение версий без динамической памяти
int semver_compare(const char* v1, const char* v2) {
    int a[3] = {0,0,0}, b[3] = {0,0,0};
    if (v1[0] == 'v') v1++;
    if (v2[0] == 'v') v2++;
    sscanf(v1, "%d.%d.%d", &a[0], &a[1], &a[2]);
    sscanf(v2, "%d.%d.%d", &b[0], &b[1], &b[2]);
    for (int i = 0; i < 3; i++) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return -1;
    }
    return 0;
}

void handleUpdateOTA() {
    if (!primitiveUpdateFlag) return;
    Serial.println(F("--- OTA Check Started ---"));
    //secureClient.setCACert(root_ca_pem);
    secureClient.setInsecure(); // Игнорировать проверку сертификатов
    
    HTTPClient http;
    //http.setReuse(true);
    http.setTimeout(5000); // 5 секунд таймаут, чтобы отправить статус 200 запроса
    
    if (http.begin(secureClient, MANIFEST_URL)) {
        Serial.println(F("Send request. HTTP.GET"));
        http.addHeader(F("User-Agent"), F("ESP32-OTA-Client"));
        int httpCode = http.GET();
        Serial.print("httpCode");
        Serial.println(httpCode);
        
        Serial.print("HTTP_CODE_OK: ");
        Serial.println(HTTP_CODE_OK);
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("Read JSON: ");
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, http.getStream());
            
            if (!error) {            
                Serial.println("JSON - OK.");

                const char* new_version = doc["version"] | "";
                const char* firmware_url = doc["url"] | "";
                const char* release_notes = doc["notes"] | "No notes provided";

                if (semver_compare(new_version, CURRENT_VERSION) > 0) {
                    Serial.println(F("\n--- UPDATE DETECTED ---"));
                    Serial.printf("Version: %s\nNotes: %s\n", new_version, release_notes);
                    Serial.println(F("Downloading firmware via HTTPS..."));

                    httpUpdate.rebootOnUpdate(true);
                    // Вызов обновления (без setHash для совместимости)
                    t_httpUpdate_return ret = httpUpdate.update(secureClient, firmware_url);
                    Serial.print("Return info after update: ");
                    Serial.println(ret);
                    if (ret == HTTP_UPDATE_FAILED) {
                        Serial.printf("OTA Error (%d): %s\n", 
                                      httpUpdate.getLastError(), 
                                      httpUpdate.getLastErrorString().c_str());
                    }
                } else Serial.printf("System: Current version %s is up to date.\n", CURRENT_VERSION);
            } else Serial.println("JSON - ERROR.");

        }
        http.end();
    }
}

void handleDiagnostics() {
    static unsigned long lastDiag = 0;
    if (millis() - lastDiag > DIAGNOSTIC_INTERVAL) {
        lastDiag = millis();
        Serial.printf("System: OK | Version: %s | Free Heap: %u B | RSSI: %d\n", 
                      CURRENT_VERSION, ESP.getFreeHeap(), WiFi.RSSI());
    }
}
// --- Main ---
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n[BOOT] Firmware Initialized"));
  Serial.printf("Current OS: %s\n", CURRENT_VERSION);
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
  handleWiFiStatus();       // Обработчик подкючения к сети WiFi
  handleAPTimeout();        // Обработчик автоотключения точки доступа
  handleLEDStatus();        // Обработчик мигания внутреннего светодиода
  handleResetButton();      // Обработчик кнопки RESET
  handleDiagnostics();      // Задача 3: Вывод статуса
  handleUpdateOTA();        // Обновление OS

}