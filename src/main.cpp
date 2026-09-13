#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

#include "config.h"

DHT dht(DHT_PIN, DHT22);
LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

float temperature = 0.0;
float humidity = 0.0;
float gas = 0.0;
float waterLevel = 0.0;

bool dhtOk = false;
bool waterOk = false;
bool testMode = false;
bool buzzerOn = false;
bool dangerWasActive = false;
bool wifiWasConnected = false;
bool mqttAttempted = false;

unsigned long lastSensorRead = 0;
unsigned long lastTelemetry = 0;
unsigned long lastBuzzerChange = 0;
unsigned long lastMqttAttempt = 0;
unsigned long testStartedAt = 0;

String getStatus() {
    if (testMode || gas >= GAS_DANGER ||
        (dhtOk && temperature >= TEMP_DANGER) ||
        (waterOk && waterLevel <= WATER_LOW)) {
        return "DANGER";
    }

    if (!dhtOk || !waterOk) {
        return "FAULT";
    }

    if (temperature >= TEMP_WARN || gas >= GAS_WARN) {
        return "WARNING";
    }

    return "SAFE";
}

float readDistanceCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    const unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
        return -1.0;
    }

    return duration * 0.0343 / 2.0;
}

void readSensors() {
    const float newTemperature = dht.readTemperature();
    const float newHumidity = dht.readHumidity();
    dhtOk = !isnan(newTemperature) && !isnan(newHumidity);

    if (dhtOk) {
        temperature = newTemperature;
        humidity = newHumidity;
    }

    gas = analogRead(GAS_PIN) * 100.0 / 4095.0;

    const float distance = readDistanceCm();
    waterOk = distance >= 2.0 && distance <= 400.0;
    if (waterOk) {
        waterLevel = (TANK_DEPTH - distance) * 100.0 / TANK_DEPTH;
        waterLevel = constrain(waterLevel, 0.0, 100.0);
    }

    Serial.printf(
        "T: %.1f | H: %.0f | Gas: %.0f%% | Water: %.0f%% | %s\r\n",
        temperature,
        humidity,
        gas,
        waterLevel,
        getStatus().c_str());
}

void updateOutputs(unsigned long now) {
    const String status = getStatus();

    digitalWrite(LED_GREEN, status == "SAFE");
    digitalWrite(LED_YELLOW, status == "WARNING" || status == "FAULT");
    digitalWrite(LED_RED, status == "DANGER");

    if (status != "DANGER") {
        noTone(BUZZER_PIN);
        buzzerOn = false;
        dangerWasActive = false;
        return;
    }

    if (!dangerWasActive) {
        dangerWasActive = true;
        buzzerOn = true;
        lastBuzzerChange = now;
        tone(BUZZER_PIN, 1200);
        return;
    }

    const unsigned long interval = buzzerOn ? BUZZER_ON_TIME : BUZZER_OFF_TIME;
    if (now - lastBuzzerChange < interval) {
        return;
    }

    lastBuzzerChange = now;
    buzzerOn = !buzzerOn;
    if (buzzerOn) {
        tone(BUZZER_PIN, 1200);
    } else {
        noTone(BUZZER_PIN);
    }
}

void updateLcd() {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print(" H:");
    lcd.print(humidity, 0);

    lcd.setCursor(0, 1);
    lcd.print("G:");
    lcd.print(gas, 0);
    lcd.print(" W:");
    lcd.print(waterLevel, 0);
    lcd.print(" ");
    lcd.print(getStatus());
}

void handleMqttCommand(char* topic, byte* payload, unsigned int length) {
    if (String(topic) != TOPIC_CMD) {
        return;
    }

    String command;
    for (unsigned int i = 0; i < length; ++i) {
        command += static_cast<char>(payload[i]);
    }
    command.trim();

    if (command == "TEST") {
        testMode = true;
        testStartedAt = millis();
        Serial.println("MQTT command: TEST");
    } else if (command == "RESET") {
        testMode = false;
        Serial.println("MQTT command: RESET");
    }
}

void reportWifiStatus() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected == wifiWasConnected) {
        return;
    }

    wifiWasConnected = connected;
    if (connected) {
        Serial.printf(
            "Wi-Fi connected | IP=%s | RSSI=%d dBm\r\n",
            WiFi.localIP().toString().c_str(),
            WiFi.RSSI());
    } else {
        Serial.println("Wi-Fi disconnected");
    }
}

void connectMqttIfNeeded(unsigned long now) {
    if (WiFi.status() != WL_CONNECTED || mqtt.connected()) {
        return;
    }

    if (mqttAttempted && now - lastMqttAttempt < MQTT_RETRY_TIME) {
        return;
    }
    lastMqttAttempt = now;
    mqttAttempted = true;

    char clientId[32];
    const uint64_t chipId = ESP.getEfuseMac();
    snprintf(clientId, sizeof(clientId), "ESP32-PCCC-%04X", static_cast<uint16_t>(chipId));

    Serial.printf("Connecting MQTT as %s...\r\n", clientId);
    if (mqtt.connect(clientId, TOPIC_STATUS, 0, true, "offline")) {
        mqtt.subscribe(TOPIC_CMD);
        mqtt.publish(TOPIC_STATUS, "online", true);
        Serial.println("MQTT connected");
    } else {
        Serial.printf("MQTT connect failed, state=%d\r\n", mqtt.state());
    }
}

void publishTelemetry() {
    if (!mqtt.connected()) {
        return;
    }

    const String payload =
        "{\"temperature\":" + String(temperature, 1) +
        ",\"humidity\":" + String(humidity, 1) +
        ",\"gas\":" + String(gas, 1) +
        ",\"water_level\":" + String(waterLevel, 1) +
        ",\"status\":\"" + getStatus() + "\"}";

    if (mqtt.publish(TOPIC_DATA, payload.c_str())) {
        Serial.printf("MQTT publish: %s\r\n", payload.c_str());
    } else {
        Serial.println("MQTT publish failed");
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    analogReadResolution(12);
    dht.begin();

    lcd.init();
    lcd.backlight();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
    Serial.println("Connecting to Wokwi-GUEST...");

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(handleMqttCommand);
}

void loop() {
    const unsigned long now = millis();

    static bool previousButtonState = HIGH;
    const bool buttonState = digitalRead(BUTTON_PIN);
    if (previousButtonState == HIGH && buttonState == LOW) {
        testMode = true;
        testStartedAt = now;
        Serial.println("Button: TEST");
    }
    previousButtonState = buttonState;

    if (testMode && now - testStartedAt >= TEST_TIME) {
        testMode = false;
    }

    if (now - lastSensorRead >= SENSOR_TIME) {
        lastSensorRead = now;
        readSensors();
        updateLcd();
    }

    updateOutputs(now);
    reportWifiStatus();
    connectMqttIfNeeded(now);
    mqtt.loop();

    if (now - lastTelemetry >= SENSOR_TIME) {
        lastTelemetry = now;
        publishTelemetry();
    }

    delay(10);
}
