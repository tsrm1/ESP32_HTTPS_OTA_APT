#ifndef SENSORS_CONFIG_H
#define SENSORS_CONFIG_H

struct Config {
  struct System {
    char device_name[32] = "Smart-Controller-01";
    int reboot_interval = 0;
  } system;

  struct Services {
    struct {
      bool enabled = false;
      char ssid[32] = "";
      char pass[64] = "";
      bool ap_mode = true;
    } wifi;
    struct {
      char user[32] = "admin";
      char pass[32] = "admin";
    } web;
    struct {
      bool enabled = false;
      char token[64] = "";
      int ids_count = 0;
      long ids[6]; 
    } telegram;
    struct {
      bool enabled = false;
      char host[32] = "192.168.1.2";
      int port = 1883;
      char user[32] = "user";
      char pass[32] = "password";
      char base_topic[64] = "home/sensor1";
      int interval = 5;
    } mqtt;
  } services;

  struct Nodes {
    struct Climate {
      struct {
        bool enabled = false; 
        int pin = 21;
        char labels[3][32]={"Temperature", "Humadity", "Peassure"};
        char units[3][8]={"°C", "%", "Pa"};
        char ui_cards[3][16]={"card-bme-t", "card-bme-h", "card-bme-p"};
        char topics[3][16]={"/bme-t", "/bme-h", "/bme-p"};
      } bme280;
      struct {
        bool enabled = false; 
        int pin=15;
        char labels[2][32]={"Temperature", "Humadity"};
        char units[2][8]={"°C", "%"};
        char ui_cards[2][16]={"card-dht-t", "card-dht-h"};
        char topics[3][16]={"/dht-t", "/dht-h"};
      } dht22;
      struct {
        bool enabled = false; 
        int pin=4;
        char macs[4][18]; // MAC-addres DS18B20
        char labels[4][32]={"Radiator 1", "Radiator 2", "Radiator 3", "Radiator 4"};
        char units[4][8]={"°C","°C","°C","°C"};
        char ui_cards[4][16]={"card-t1", "card-t2", "card-t3", "card-t4"};
        char topics[4][16]={"/t1", "/t2", "/t3", "/t4"};
      } ds18b20;
      struct {
        bool enabled = false; 
        int pin = 21;
        char label[32]="Light (TCRT)";
        char unit[8]="Lux";
        char ui_card[16]="card-tcrt";
        char topic[16]="/lux";
      } tcrt5000;
    } climate;

    struct Binary {
      struct {
        bool enabled = false; 
        int pin=35;
        char label[32]="Motion";
        char ui_card[16]="card-pir";
        char topic[16]="/motion";
      } pir;
      struct {
        bool enabled = false; 
        int pin=35;
        char label[32]="Presence";
        char ui_card[16]="card-pres";
        char topic[16]="/presence";
      } ld2420;
      struct {
        bool enabled = false; 
        int pin=36;
        char label[32]="Door";
        char ui_card[16]="card-door";
        char topic[16]="/door";
      } door;
      struct {
        bool enabled = false; 
        int pin=34;
        char label[32]="Leak";
        char ui_card[16]="card-flood";
        char topic[16]="/flood";
      } flood;
    } binary;

    struct Analog {          
      struct {
        bool enabled = false; 
        int pin=39;
        char label[32]="Light (LDR)";
        char ui_card[16]="card-lux-5516";
        char topic[16]="/lux_raw";
      } light_resistor;
    } analog;
    
    struct Actuators {
      struct {
        bool enabled = false;
        int pins[4] = {26, 27, 14, 13};
        char labels[4][32]={"Rele 1", "Rele 2", "Rele 3", "Rele 4"};
        char ui_cards[4][16]={"card-r0", "card-r1", "card-r2", "card-r3"};
        char topics[4][16]={"/r0", "/r1", "/r2", "/r3"};
      } relays;
    } actuators;
  } nodes;
};
#endif