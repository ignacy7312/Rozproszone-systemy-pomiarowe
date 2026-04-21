#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "secrets.h"

namespace {
constexpr uint32_t SCHEMA_VERSION = 1;
constexpr char SENSOR_NAME[] = "temperature";
constexpr char SENSOR_UNIT[] = "C";
constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.nist.gov";
constexpr long GMT_OFFSET_SEC = 0;
constexpr int DAYLIGHT_OFFSET_SEC = 0;
constexpr time_t MIN_VALID_UNIX_TIME = 1700000000;
constexpr unsigned long WIFI_RETRY_DELAY_MS = 500;
constexpr unsigned long MQTT_RETRY_DELAY_MS = 2000;
constexpr unsigned long PUBLISH_INTERVAL_MS = 5000;
constexpr unsigned long NTP_RETRY_DELAY_MS = 500;
constexpr uint8_t NTP_MAX_ATTEMPTS = 40;
constexpr size_t JSON_DOC_SIZE = 256;
constexpr size_t PAYLOAD_BUFFER_SIZE = 256;
}

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String deviceId;
String groupId;
String measurementTopic;

uint32_t sequenceNumber = 0;
unsigned long lastPublishAt = 0;
bool timeSynchronized = false;

String generateDeviceIdFromEfuse() {
    uint64_t chipId = ESP.getEfuseMac();
    char id[32];
    snprintf(id, sizeof(id), "esp32-%04x%08x", (uint16_t)(chipId >> 32), (uint32_t)chipId);
    return String(id);
}

bool isValidTopicSegment(const String& segment) {
    if (segment.isEmpty()) {
        return false;
    }

    for (size_t i = 0; i < segment.length(); ++i) {
        const char c = segment[i];
        const bool isLowerLetter = (c >= 'a' && c <= 'z');
        const bool isDigit = (c >= '0' && c <= '9');
        const bool isAllowedSymbol = (c == '-');

        if (!(isLowerLetter || isDigit || isAllowedSymbol)) {
            return false;
        }
    }

    return true;
}

void haltWithError(const String& message) {
    Serial.println();
    Serial.println("BLAD KRYTYCZNY:");
    Serial.println(message);
    while (true) {
        delay(1000);
    }
}

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    Serial.print("Laczenie z Wi-Fi: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(WIFI_RETRY_DELAY_MS);
        Serial.print('.');
    }

    Serial.println();
    Serial.println("Polaczono z Wi-Fi");
    Serial.print("Adres IP: ");
    Serial.println(WiFi.localIP());

    timeSynchronized = false;
}

bool syncTimeWithNTP() {
    if (timeSynchronized) {
        return true;
    }

    Serial.println("Synchronizacja czasu przez NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

    time_t now = 0;
    uint8_t attempts = 0;

    while (attempts < NTP_MAX_ATTEMPTS) {
        time(&now);
        if (now >= MIN_VALID_UNIX_TIME) {
            timeSynchronized = true;
            Serial.println("Czas zsynchronizowany.");
            Serial.print("Unix time [s]: ");
            Serial.println(static_cast<unsigned long>(now));
            return true;
        }

        Serial.println("Oczekiwanie na synchronizacje czasu...");
        delay(NTP_RETRY_DELAY_MS);
        ++attempts;
    }

    Serial.println("Nie udalo sie zsynchronizowac czasu przez NTP.");
    return false;
}

uint64_t getTimestampMs() {
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    return (static_cast<uint64_t>(tv.tv_sec) * 1000ULL) +
           (static_cast<uint64_t>(tv.tv_usec) / 1000ULL);
}

bool isSupportedUnitForSensor(const String& sensor, const String& unit) {
    if (sensor == "temperature") {
        return unit == "C";
    }

    return true;
}

bool validateMeasurement(const String& currentDeviceId,
                         const String& sensor,
                         float value,
                         const String& unit,
                         uint64_t tsMs,
                         uint32_t seq) {
    if (currentDeviceId.isEmpty()) {
        Serial.println("Walidacja nie powiodla sie: device_id jest pusty.");
        return false;
    }

    if (sensor.isEmpty()) {
        Serial.println("Walidacja nie powiodla sie: sensor jest pusty.");
        return false;
    }

    if (!isfinite(value)) {
        Serial.println("Walidacja nie powiodla sie: value nie jest liczba skonczona.");
        return false;
    }

    if (tsMs == 0) {
        Serial.println("Walidacja nie powiodla sie: ts_ms musi byc dodatnia liczba calkowita.");
        return false;
    }

    if (!isSupportedUnitForSensor(sensor, unit)) {
        Serial.println("Walidacja nie powiodla sie: unit nie odpowiada typowi sensora.");
        return false;
    }

    (void)seq;
    return true;
}

void connectMQTT() {
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);

    while (!mqttClient.connected()) {
        Serial.print("Laczenie z MQTT...");

        if (mqttClient.connect(deviceId.c_str())) {
            Serial.println("OK");
        } else {
            Serial.print("blad, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" - ponowna proba za 2 s");
            delay(MQTT_RETRY_DELAY_MS);
        }
    }
}

bool publishMeasurement() {
    if (!syncTimeWithNTP()) {
        Serial.println("Pominieto publikacje: brak poprawnej synchronizacji czasu.");
        return false;
    }

    const float temperatureC = temperatureRead();
    const uint64_t tsMs = getTimestampMs();
    const uint32_t seq = sequenceNumber;

    if (!validateMeasurement(deviceId, SENSOR_NAME, temperatureC, SENSOR_UNIT, tsMs, seq)) {
        Serial.println("Pominieto publikacje: payload nie przeszedl walidacji.");
        return false;
    }

    StaticJsonDocument<JSON_DOC_SIZE> doc;
    doc["schema_version"] = SCHEMA_VERSION;
    doc["group_id"] = groupId;
    doc["device_id"] = deviceId;
    doc["sensor"] = SENSOR_NAME;
    doc["value"] = temperatureC;
    doc["unit"] = SENSOR_UNIT;
    doc["ts_ms"] = tsMs;
    doc["seq"] = seq;

    char payload[PAYLOAD_BUFFER_SIZE];
    const size_t payloadLength = serializeJson(doc, payload, sizeof(payload));
    if (payloadLength == 0 || payloadLength >= sizeof(payload)) {
        Serial.println("Nie udalo sie zserializowac payloadu JSON.");
        return false;
    }

    const bool publishOk = mqttClient.publish(measurementTopic.c_str(), payload);
    if (!publishOk) {
        Serial.println("Publikacja MQTT nie powiodla sie.");
        return false;
    }

    Serial.print("Publikacja na topic: ");
    Serial.println(measurementTopic);
    Serial.println(payload);

    ++sequenceNumber;
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    deviceId = generateDeviceIdFromEfuse();
    groupId = String(MQTT_GROUP);
    groupId.toLowerCase();

    if (!isValidTopicSegment(groupId)) {
        haltWithError("MQTT_GROUP musi skladac sie z malych liter, cyfr lub '-'.");
    }

    if (!isValidTopicSegment(deviceId)) {
        haltWithError("Wygenerowany device_id nie spelnia zasad nazewnictwa topicow.");
    }

    measurementTopic = "lab/" + groupId + "/" + deviceId + "/" + SENSOR_NAME;

    Serial.print("Device ID: ");
    Serial.println(deviceId);
    Serial.print("Topic pomiarowy: ");
    Serial.println(measurementTopic);

    connectWiFi();
    syncTimeWithNTP();
    connectMQTT();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
        syncTimeWithNTP();
    }

    if (!mqttClient.connected()) {
        connectMQTT();
    }

    mqttClient.loop();

    const unsigned long now = millis();
    if (now - lastPublishAt >= PUBLISH_INTERVAL_MS) {
        if (publishMeasurement()) {
            lastPublishAt = now;
        }
    }
}
