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

  struct Sensors {
    struct {
      bool enabled = false; 
      int pins[1] = {21};
      char labels[3][32]={"Temperature, °C", "Humidity, %", "Pressure, Pa"};
      char topics[3][16]={"/bme-t", "/bme-h", "/bme-p"};
    } bme;
    struct {
      bool enabled = false; 
      int pins[1] = {15};
      char labels[2][32]={"Temperature, °C", "Humidity, %"};
      char topics[3][16]={"/dht-t", "/dht-h"};
    } dht;
    struct {
      bool enabled = false; 
      int pins[1] = {4};
      char macs[4][18]; // MAC-addres DS18B20
      char labels[4][32]={"Radiator 1, °C", "Radiator 2, °C", "Radiator 3, °C", "Radiator 4, °C"};
      char topics[4][16]={"/t1", "/t2", "/t3", "/t4"};
    } ds;
    struct {
      bool enabled = false; 
      int pins[1] = {21};
      char labels[1][32]={"Light (TCRT), Lux"};
      char topics[1][16]={"/lux"};
    } tcrt;
    struct {
      bool enabled = false; 
      int pins[1] = {35};
      char labels[1][32]={"Motion"};
      char topics[1][16]={"/motion"};
    } pir;
    struct {
      bool enabled = false; 
      int pins[1] = {35};
      char labels[1][32]={"Presence"};
      char topics[1][16]={"/presence"};
    } ld;
    struct {
      bool enabled = false; 
      int pins[1] = {36};
      char labels[1][32]={"Door"};
      char topics[1][16]={"/door"};
    } dr;
    struct {
      bool enabled = false; 
      int pins[1] = {34};
      char labels[1][32]={"Leak"};
      char topics[1][16]={"/flood"};
    } fl;
    struct {
      bool enabled = false; 
      int pins[1] = {39};
      char labels[1][32]={"Light (LDR)"};
      char topics[1][16]={"/lux_raw"};
    } lr;
    struct {
      bool enabled = false;
      int pins[4] = {26, 27, 14, 13};
      char labels[4][32]={"Rele 1", "Rele 2", "Rele 3", "Rele 4"};
      char topics[4][16]={"/r0", "/r1", "/r2", "/r3"};
    } relays;
  } sensors;
};
#endif