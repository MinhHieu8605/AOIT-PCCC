#pragma once

// WiFi
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""
#define WIFI_CHANNEL 6

// MQTT
#define MQTT_HOST "broker.hivemq.com"
#define MQTT_PORT 1883
#define TOPIC_DATA "aiot-pccc/demo-vn-2026/telemetry"
#define TOPIC_CMD  "aiot-pccc/demo-vn-2026/command"
#define TOPIC_STATUS "aiot-pccc/demo-vn-2026/status"

// Pins
#define DHT_PIN    15
#define TRIG_PIN   5
#define ECHO_PIN   18
#define GAS_PIN    34
#define LED_GREEN  25
#define LED_YELLOW 26
#define LED_RED    13
#define BUZZER_PIN 4
#define BUTTON_PIN 27

// Threshold
#define TEMP_WARN   45
#define TEMP_DANGER 60
#define GAS_WARN    60
#define GAS_DANGER  80
#define WATER_LOW   25
#define TANK_DEPTH  200

#define SENSOR_TIME 2000
#define TEST_TIME   5000
#define MQTT_RETRY_TIME 5000
#define BUZZER_ON_TIME 250
#define BUZZER_OFF_TIME 500
