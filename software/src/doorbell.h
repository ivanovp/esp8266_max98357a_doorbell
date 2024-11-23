/**
 * @file        doorbell.h
 * @brief       Definitions of doorbell
 * @author      Copyright (C) Peter Ivanov, 2024
 *
 * Created      2024-01-11 11:48:53
 * Last modify: 2024-11-23 12:49:17 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#ifndef INCLUDE_DOORBELL
#define INCLUDE_DOORBELL

#include <stdint.h>

#include <Arduino.h>

#include "common.h"
#include "config.h"

#if ENABLE_DOORBELL
extern void doorbell_task(uint8_t mqtt_flags);
extern void doorbell_init();
extern void doorbell_play();
#if ENABLE_HTTP_SERVER
extern void doorbell_handle_doorbell_htm(ESP8266WebServer &httpServer, String requestUri);
extern String doorbell_generate_index_htm();
#endif
#if ENABLE_MQTT_CLIENT
extern void doorbell_mqtt_callback(String& topicStr, String& payloadStr, unsigned int length);
#endif
#endif

#endif /* INCLUDE_DOORBELL */

