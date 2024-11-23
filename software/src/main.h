/**
 * @file        main.h
 * @brief       Definitions of main.cpp
 * @author      Copyright (C) Peter Ivanov, 2011, 2012, 2013, 2014, 2021, 2023, 2024
 *
 * Created      2011-01-19 11:48:53
 * Last modify: 2024-11-23 12:50:09 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#ifndef INCLUDE_MAIN_H
#define INCLUDE_MAIN_H

#include <stdint.h>

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "PubSubClient.h"       /* MQTT */

#include "common.h"
#include "config.h"
#include "secrets.h"  // add WLAN Credentials in here.
#include "trace.h"

#include <FS.h>        // File System for Web Server Files
#include <LittleFS.h>  // This file system is used.

#define MQTT_FLAG_DISCONNECTED  1
#define MQTT_FLAG_CONNECTED     2

extern String hostname;
extern WiFiClient wifiClient;

#if ENABLE_RESET
extern bool board_reset;
extern uint32_t board_reset_timestamp_ms;
#endif

#if ENABLE_MQTT_CLIENT
extern PubSubClient mqttClient;

extern uint32_t mqtt_connect_start_time;
extern uint32_t mqtt_publish_start_time;
extern uint32_t mqtt_publish_interval_sec;
extern String mqttSensorTopicPrefix;
extern String mqttSwitchesTopicPrefix;
#endif

extern void setLed(bool on=true);
extern void setLed2(bool on=true);
extern String sec2str(uint32_t a_seconds);
extern String sec2str_short(uint32_t a_seconds);

#endif /* INCLUDE_MAIN_H */

