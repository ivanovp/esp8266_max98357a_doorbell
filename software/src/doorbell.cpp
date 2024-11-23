/**
 * @file        doorbell.cpp
 * @brief       Doorbell
 * @author      Copyright (C) Peter Ivanov, 2024
 *
 * Created      2024-01-11 11:48:53
 * Last modify: 2024-11-23 12:49:15 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#include <Arduino.h>

#include "AudioGeneratorWAV.h"
#include "AudioGeneratorAAC.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorMOD.h"
#include "AudioOutputI2SNoDAC.h"
#include "AudioFileSourceLittleFS.h"

#include "main.h"
#include "common.h"
#include "config.h"
#include "doorbell.h"
#include "http_server.h"
#include "fileutils.h"

#define WAV                             1
#define AAC                             2
#define MP3                             3
#define MOD                             4

#define EVENT_DOORBELL                  0
#define EVENT_COURTYARD_LAMP            1
#define EVENT_DOORBELL_WEB              2
#define EVENT_DOORBELL_MQTT             3

#define DOORBELL_FILE_TYPE                  WAV
#define DOORBELL_SOFTWARE_DEBOUNCE_TIME_MS  100
#define DOORBELL_LONG_PRESS_TIME_MS         5000
#define DOORBELL_MQTT_FOLLOW_TOPIC_FILENAME "doorbell_mqtt_follow.txt"
#define DOORBELL_CONFIG_FILENAME            "doorbell.txt"
#define DOORBELL_MAX_MQTT_FOLLOW_TOPICS     8
#define DOORBELL_HISTORY_FILENAME           "doorbell_history.txt"

#if ENABLE_DOORBELL
static AudioFileSourceLittleFS *in = NULL;
static AudioGenerator *audio_gen = NULL;
static AudioOutputI2S *out;
static uint8_t replay_cntr = 0;
static uint32_t replay_timestamp_ms = 0;
static uint32_t switch_press_timestamp_ms = 0;
static String audioFileName = DOORBELL_AUDIO_FILE_NAME;
static uint8_t audioPlayCount = DOORBELL_AUDIO_PLAY_COUNT;
static uint32_t audioPlayDelay_ms = DOORBELL_AUDIO_PLAY_DELAY_MS;
static float audioGain = DOORBELL_AUDIO_GAIN;
#if DOORBELL_HISTORY_LENGTH > 0
static String doorbell_history[DOORBELL_HISTORY_LENGTH];
#endif
#if ENABLE_MQTT_CLIENT
static String mqttTopicPlayAudio;
static String mqttTopicPress;
static String mqttTopicLongPress;
static const char *mqttMsg = "1";

typedef struct
{
    String topic;
    String value;
} followedMqttTopics_t;
static followedMqttTopics_t followedMqttTopics[DOORBELL_MAX_MQTT_FOLLOW_TOPICS];
static bool subscribedToMqttTopics = false;
#endif /* ENABLE_MQTT_CLIENT */
#endif /* ENABLE_DOORBELL */

#if ENABLE_DOORBELL
static void prepare_audio()
{
    delete in;
    delete audio_gen;

#if DOORBELL_FILE_TYPE == WAV
    in = new AudioFileSourceLittleFS(audioFileName.c_str());
    audio_gen = new AudioGeneratorWAV();
#elif DOORBELL_FILE_TYPE == AAC
    in = new AudioFileSourceLittleFS(audioFileName.c_str());
    audio_gen = new AudioGeneratorAAC();
#elif DOORBELL_FILE_TYPE == MP3
    in = new AudioFileSourceLittleFS(audioFileName.c_str());
    audio_gen = new AudioGeneratorMP3();
#elif DOORBELL_FILE_TYPE == MOD
    in = new AudioFileSourceLittleFS(audioFileName.c_str());
    audio_gen = new AudioGeneratorMOD();
#else
#error Unsupported file type!
#endif
}

bool doorbell_is_playing()
{
    bool is_playing = false;

    if (audio_gen->isRunning())
    {
        is_playing = true;
    }
    else if (replay_timestamp_ms)
    {
        is_playing = true;
    }
    else if (replay_cntr)
    {
        is_playing = true;
    }

    return is_playing;
}


bool doorbell_mqtt_init()
{
    bool ret = true;
#if ENABLE_MQTT_CLIENT
    String lines[DOORBELL_MAX_MQTT_FOLLOW_TOPICS];
    String mqttTopic;
    uint16_t lineCnt;
    lineCnt = readStringsFromFile(DOORBELL_MQTT_FOLLOW_TOPIC_FILENAME, 0,
                                  lines, DOORBELL_MAX_MQTT_FOLLOW_TOPICS);
    TRACE("Number of lines in file: %i\n", lineCnt);
    for (int i = 0; i < lineCnt && i < DOORBELL_MAX_MQTT_FOLLOW_TOPICS; i++)
    {
        /* Two line formats accepted: with comma or without comma */
        int commaIdx = lines[i].indexOf(',');
        if (commaIdx != -1)
        {
            /* With comma (MQTT topic, value to follow), example:
             * /switches/mansardlamp/switch,1
             * Only switch on command is followed */
            followedMqttTopics[i].topic = lines[i].substring(0, commaIdx);
            followedMqttTopics[i].value = lines[i].substring(commaIdx + 1);
        }
        else
        {
            /* Without comma (MQTT topic only), example:
             * switches/workshoplamp/switch
             * Either switch on (1) or switch off (1) are followed.
             */
            followedMqttTopics[i].topic = lines[i];
            followedMqttTopics[i].value.clear(); /* Any value is accepted */
        }
        mqttTopic = followedMqttTopics[i].topic;
        if (mqttTopic.length())
        {
            TRACE("Following topic '%s'... ", mqttTopic.c_str());
            if (mqttClient.subscribe(mqttTopic.c_str()))
            {
                TRACE("Successfully subscribed.\n");
            }
            else
            {
                ret = false;
                ERROR("Cannot subscribe to topic '%s'!\n", mqttTopic.c_str());
            }
        }
        else
        {
            break;
        }
    }
#endif

    return ret;
}

#if DOORBELL_HISTORY_LENGTH > 0
static String doorbell_get_timestamp()
{
    String timestamp;
    char buffer[32];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    timestamp = buffer;

    return timestamp;
}
#endif


void doorbell_update_history(uint8_t eventType)
{
#if DOORBELL_HISTORY_LENGTH > 0
    uint16_t lineNum = 0;
    size_t writtenBytes = 0;

    lineNum = readStringsFromFile(DOORBELL_HISTORY_FILENAME, 0, doorbell_history, DOORBELL_HISTORY_LENGTH, false);

    File file = LittleFS.open(DOORBELL_HISTORY_FILENAME, "w");
    if (file)
    {
        writtenBytes = file.print(doorbell_get_timestamp().c_str());
        if (eventType == EVENT_COURTYARD_LAMP)
        {
            writtenBytes += file.print(" courtyard lamp");
        }
        else if (eventType == EVENT_DOORBELL)
        {
            writtenBytes += file.print(" doorbell switch");
        }
        else if (eventType == EVENT_DOORBELL_WEB)
        {
            writtenBytes += file.print(" doorbell through web");
        }
        else if (eventType == EVENT_DOORBELL_MQTT)
        {
            writtenBytes += file.print(" doorbell through MQTT");
        }
        else
        {
            writtenBytes += file.print(" unknown event!");
        }
        file.print("\n");

        if (writtenBytes < 28)
        {
            ERROR("Cannot write data to %s!\n", DOORBELL_HISTORY_FILENAME);
        }

        for (uint16_t idx = 0; idx < lineNum; idx++)
        {
            writtenBytes = file.print(doorbell_history[idx]);
            writtenBytes += file.print("\n");
            if (writtenBytes < doorbell_history[idx].length() + 1)
            {
                ERROR("Cannot write data to %s!\n", DOORBELL_HISTORY_FILENAME);
                break;
            }
        }

        file.close();
    }
    else
    {
        ERROR("Cannot create %s!\n", DOORBELL_HISTORY_FILENAME);
    }
#endif
}

/*
 * It should be called in the loop function.
 */
void doorbell_task(uint8_t mqtt_flags)
{
#if ENABLE_MQTT_CLIENT
    if ((mqttClient.connected() && !subscribedToMqttTopics) || (mqtt_flags & MQTT_FLAG_CONNECTED))
    {
        subscribedToMqttTopics = doorbell_mqtt_init();
    }
    if ((!mqttClient.connected() && subscribedToMqttTopics) || (mqtt_flags & MQTT_FLAG_DISCONNECTED))
    {
        subscribedToMqttTopics = false;
    }
#endif
#if DOORBELL_SWITCH_PIN != -1
    static int prevSwitchStatus = HIGH; /* GPIO pin is pulled high, inverted logic! */
    int switchStatus = digitalRead(DOORBELL_SWITCH_PIN);
    //setLed(switchStatus == LOW); /* inverted logic */
    int risingEdge = (prevSwitchStatus ^ switchStatus) & switchStatus;
    int fallingEdge = (prevSwitchStatus ^ switchStatus) & prevSwitchStatus;
    if (fallingEdge) /* inverted logic */
    {
        /* The switch has just pressed */
        if (!switch_press_timestamp_ms)
        {
            switch_press_timestamp_ms = millis();
        }
    }
    if (risingEdge) /* inverted logic */
    {
        /* The switch has just released */
        if (switch_press_timestamp_ms
            && switch_press_timestamp_ms + DOORBELL_SOFTWARE_DEBOUNCE_TIME_MS < millis())
        {
            /* Someone pressed the button, ring the bell! */
#if ENABLE_MQTT_CLIENT
            boolean ok;

            if (mqttClient.connected())
            {
                ok = mqttClient.publish(mqttTopicPress.c_str(), mqttMsg);
                if (ok)
                {
                    TRACE("Publish %s, %s\n", mqttTopicPress.c_str(), mqttMsg);
                }
                else
                {
                    ERROR("Cannot publish %s, %s\n", mqttTopicPress.c_str(), mqttMsg);
                }
            }
#endif
            if (!doorbell_is_playing())
            {
                doorbell_play();
                doorbell_update_history(EVENT_DOORBELL);
            }
        }
        switch_press_timestamp_ms = 0;
    }
    if (switch_press_timestamp_ms && switch_press_timestamp_ms + DOORBELL_LONG_PRESS_TIME_MS < millis())
    {
        /* The button was pressed for long time */
#if ENABLE_MQTT_CLIENT
        boolean ok;

        if (mqttClient.connected())
        {
            ok = mqttClient.publish(mqttTopicLongPress.c_str(), mqttMsg);
            if (ok)
            {
                TRACE("Publish %s, %s\n", mqttTopicLongPress.c_str(), mqttMsg);
            }
            else
            {
                ERROR("Cannot publish %s, %s\n", mqttTopicLongPress.c_str(), mqttMsg);
            }
        }
#endif
        switch_press_timestamp_ms = 0;
        doorbell_update_history(EVENT_COURTYARD_LAMP);
    }
    prevSwitchStatus = switchStatus;
#endif

    if (audio_gen->isRunning())
    {
        audio_gen->loop();
    }
    else
    {
        if (replay_timestamp_ms)
        {
            if (replay_timestamp_ms < millis())
            {
                TRACE("Re-playing audio... ");
                prepare_audio();
                if (audio_gen->begin(in, out))
                {
                    TRACE("Done.\n");
                }
                else
                {
                    ERROR("Cannot replay audio!\n");
                }
                replay_timestamp_ms = 0;
            }
        }
        else if (replay_cntr)
        {
            TRACE("Audio playing done\n");
            TRACE("Stopping audio... ");
            if (audio_gen->stop())
            {
                TRACE("Done.\n");
            }
            else
            {
                ERROR("Cannot stop audio!\n");
            }
            replay_cntr--;
            if (replay_cntr)
            {
                TRACE("Re-playing audio in %i ms...\n", audioPlayDelay_ms);
                replay_timestamp_ms = millis() + audioPlayDelay_ms;
            }
            else
            {
                TRACE("No more audio playing...\n");
                replay_timestamp_ms = 0;
            }
        }
    }
}

void doorbell_init()
{
#if DOORBELL_SWITCH_PIN != -1
    pinMode(DOORBELL_SWITCH_PIN, INPUT_PULLUP);
#endif
    audioLogger = &Serial;
    audioFileName = readStringFromFile(DOORBELL_CONFIG_FILENAME, 0);
    String str = readStringFromFile(DOORBELL_CONFIG_FILENAME, 1);
    if (str.isEmpty())
    {
        audioFileName = DOORBELL_AUDIO_FILE_NAME;
    }
    else
    {
        audioPlayCount = str.toInt();
    }
    str = readStringFromFile(DOORBELL_CONFIG_FILENAME, 2);
    if (!str.isEmpty())
    {
        audioPlayDelay_ms = str.toInt();
    }
    str = readStringFromFile(DOORBELL_CONFIG_FILENAME, 3);
    if (!str.isEmpty())
    {
        audioGain = str.toFloat();
    }
    prepare_audio();
#if ENABLE_DOORBELL_I2S_DAC
    out = new AudioOutputI2S();
#else
    out = new AudioOutputI2SNoDAC();
#endif
    out->SetGain(audioGain);
#if ENABLE_MQTT_CLIENT
    mqttTopicPlayAudio = mqttSwitchesTopicPrefix + "playAudio";
    mqttTopicPress = mqttSwitchesTopicPrefix + "press";
    mqttTopicLongPress = mqttSwitchesTopicPrefix + "longPress";
#endif
}

void doorbell_play()
{
#if ENABLE_MQTT_CLIENT
    boolean ok;

#endif

    if (!doorbell_is_playing())
    {
        TRACE("Start playing audio... ");
        prepare_audio();
        if (audio_gen->begin(in, out))
        {
            TRACE("done.\n");
            replay_cntr = audioPlayCount;
        }
        else
        {
            ERROR("Cannot play audio!\n");
        }
    }
    else
    {
        ERROR("Audio playing has already started!\n");
    }

#if ENABLE_MQTT_CLIENT
    if (mqttClient.connected())
    {
        ok = mqttClient.publish(mqttTopicPlayAudio.c_str(), mqttMsg);
        if (ok)
        {
            TRACE("Publish %s, %s\n", mqttTopicPlayAudio.c_str(), mqttMsg);
        }
        else
        {
            ERROR("Cannot publish %s, %s\n", mqttTopicPlayAudio.c_str(), mqttMsg);
        }
    }
#endif
}

#if ENABLE_HTTP_SERVER
/*
 * It handles functions of doorbell
 */
void doorbell_handle_doorbell_htm(ESP8266WebServer &httpServer, String requestUri)
{
    String buf;
    String bell;

#if ENABLE_HTTP_AUTH
    if (!http_is_authenticated(DOORBELL_HTM))
    {
        request_http_auth();
        return;
    }
#endif

    if (httpServer.hasArg("bell"))
    {
        bell = httpServer.arg("bell");
    }

    buf += html_begin(false, homepageTitleStr, "Ringing the bell", 1, INDEX_HTM);
    if (bell == "RING")
    {
        doorbell_play();
        doorbell_update_history(EVENT_DOORBELL_WEB);
    }
    buf += "<p>" + html_link_to_index() + "</p>";
    buf += html_footer();
    buf += html_end();

    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(200, "text/html; charset=utf-8", buf);
}

String doorbell_generate_index_htm()
{
    String buf;
    String disabledStr;

    buf += "<form action=\"" DOORBELL_HTM "\">Doorbell: ";
    if (doorbell_is_playing())
    {
        disabledStr = " disabled=\"true\"";
    }
    buf += "<input type=\"submit\" name=\"bell\" value=\"RING\"" + disabledStr + ">";
    buf += "</form>";

#if DOORBELL_HISTORY_LENGTH > 0
    uint16_t lines = 0;

    buf += "<p>Last " TOSTR(DOORBELL_HISTORY_LENGTH) " events:<br>";

    lines = readStringsFromFile(DOORBELL_HISTORY_FILENAME, 0, doorbell_history, DOORBELL_HISTORY_LENGTH, false);

    for (uint16_t idx = 0; idx < lines; idx++)
    {
        buf += doorbell_history[idx] + "<br>";
    }

    buf += "</p>";
#endif

    return buf;
}
#endif

#if ENABLE_MQTT_CLIENT
void doorbell_mqtt_callback(String& topicStr, String& payloadStr, unsigned int length)
{
    uint16_t i;

    for (i = 0; i < DOORBELL_MAX_MQTT_FOLLOW_TOPICS; i++)
    {
        if (topicStr == followedMqttTopics[i].topic
            && (followedMqttTopics[i].value.isEmpty()
              || (payloadStr == followedMqttTopics[i].value)))
        {
            TRACE("Followed topic '%s' match, payload: '%s'\n", topicStr.c_str(), payloadStr.c_str());
            doorbell_play();
            doorbell_update_history(EVENT_DOORBELL_MQTT);
        }
    }
}
#endif /* MQTT_CLIENT */


#endif
