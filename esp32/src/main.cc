#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <esp_system.h>
#include "secrets.h"

#ifndef MQTT_USE_TLS
#define MQTT_USE_TLS 0
#endif

#if MQTT_USE_TLS
#include <WiFiClientSecure.h>
WiFiClientSecure netClient;
#else
WiFiClient netClient;
#endif

namespace {
constexpr uint32_t SCHEMA_VERSION = 1;
constexpr char TOPIC_ROOT[] = "lab";
constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.nist.gov";
constexpr long GMT_OFFSET_SEC = 0;
constexpr int DAYLIGHT_OFFSET_SEC = 0;
constexpr time_t MIN_VALID_UNIX_TIME = 1700000000;
constexpr unsigned long WIFI_RETRY_DELAY_MS = 500;
constexpr unsigned long MQTT_RETRY_DELAY_MS = 2000;
constexpr unsigned long NTP_RETRY_DELAY_MS = 1000;
constexpr uint8_t NTP_MAX_ATTEMPTS = 40;
constexpr unsigned long MEASUREMENT_INTERVAL_MS = 5000;
constexpr unsigned long STATUS_INTERVAL_MS = 30000;
constexpr size_t JSON_DOC_SIZE = 256;
constexpr size_t PAYLOAD_BUFFER_SIZE = 256;
constexpr uint16_t MQTT_BUFFER_SIZE = 512;

struct SensorConfig {
    const char* name;
    const char* unit;
    float baseValue;
    float amplitude;
    float periodSeconds;
    float phaseRad;
    float minValue;
    float maxValue;
};

constexpr SensorConfig SENSORS[] = {
    {"temperature", "C",   24.0f,  3.5f,  60.0f, 0.00f,  15.0f,  40.0f},
    {"humidity",    "%",   50.0f, 18.0f,  75.0f, 0.90f,  10.0f, 100.0f},
    {"pressure",    "hPa",1008.0f, 7.0f,  95.0f, 1.60f, 980.0f, 1045.0f},
    {"light",       "lx", 420.0f,260.0f,  45.0f, 2.20f,   0.0f, 1500.0f}
};
}

PubSubClient mqttClient(netClient);

String deviceId;
String groupId;
String baseTopic;
String statusTopic;

uint32_t sequenceNumber = 0;
unsigned long lastMeasurementAt = 0;
unsigned long lastStatusAt = 0;
bool timeSynchronized = false;

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

bool isHexLower(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

bool isValidUuidV4(const String& uuid) {
    if (uuid.length() != 36) {
        return false;
    }

    for (size_t i = 0; i < uuid.length(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (uuid[i] != '-') {
                return false;
            }
            continue;
        }

        if (!isHexLower(uuid[i])) {
            return false;
        }
    }

    const char version = uuid[14];
    const char variant = uuid[19];
    const bool validVariant = (variant == '8' || variant == '9' || variant == 'a' || variant == 'b');

    return version == '4' && validVariant;
}

String generateUuidV4() {
    uint8_t bytes[16];
    for (uint8_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = static_cast<uint8_t>(esp_random() & 0xFF);
    }

    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

    char uuid[37];
    snprintf(
        uuid,
        sizeof(uuid),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
    );

    return String(uuid);
}

String loadOrCreateDeviceId() {
    Preferences prefs;
    prefs.begin("device", false);

    String storedUuid = prefs.getString("uuid", "");
    if (isValidUuidV4(storedUuid)) {
        prefs.end();
        return storedUuid;
    }

    const String newUuid = generateUuidV4();
    prefs.putString("uuid", newUuid);
    prefs.end();
    return newUuid;
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
    for (uint8_t attempt = 0; attempt < NTP_MAX_ATTEMPTS; ++attempt) {
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
    for (const auto& cfg : SENSORS) {
        if (sensor == cfg.name) {
            return unit == cfg.unit;
        }
    }
    return false;
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

float roundToTwoDecimals(float value) {
    return roundf(value * 100.0f) / 100.0f;
}

float generateSineMockValue(const SensorConfig& cfg) {
    const double t = static_cast<double>(millis()) / 1000.0;
    const double angle = ((2.0 * PI * t) / cfg.periodSeconds) + cfg.phaseRad;
    double value = static_cast<double>(cfg.baseValue) +
                   static_cast<double>(cfg.amplitude) * sin(angle);

    if (value < cfg.minValue) {
        value = cfg.minValue;
    }
    if (value > cfg.maxValue) {
        value = cfg.maxValue;
    }

    return roundToTwoDecimals(static_cast<float>(value));
}

String buildMeasurementTopic(const char* sensorName) {
    return baseTopic + "/" + String(sensorName);
}

bool publishRawJson(const String& topic, JsonDocument& doc, bool retained = false) {
    char payload[PAYLOAD_BUFFER_SIZE];
    const size_t payloadLength = serializeJson(doc, payload, sizeof(payload));

    if (payloadLength == 0 || payloadLength >= sizeof(payload)) {
        Serial.println("Nie udalo sie zserializowac payloadu JSON.");
        return false;
    }

    const bool publishOk = mqttClient.publish(topic.c_str(), payload, retained);
    if (!publishOk) {
        Serial.println("Publikacja MQTT nie powiodla sie.");
        return false;
    }

    Serial.print("Publikacja na topic: ");
    Serial.println(topic);
    Serial.println(payload);
    return true;
}

bool publishMeasurement(const SensorConfig& sensorCfg) {
    if (!syncTimeWithNTP()) {
        Serial.println("Pominieto publikacje: brak poprawnej synchronizacji czasu.");
        return false;
    }

    const float value = generateSineMockValue(sensorCfg);
    const uint64_t tsMs = getTimestampMs();
    const uint32_t seq = sequenceNumber;

    if (!validateMeasurement(deviceId, sensorCfg.name, value, sensorCfg.unit, tsMs, seq)) {
        Serial.println("Pominieto publikacje: payload nie przeszedl walidacji.");
        return false;
    }

    StaticJsonDocument<JSON_DOC_SIZE> doc;
    doc["schema_version"] = SCHEMA_VERSION;
    doc["group_id"] = groupId;
    doc["device_id"] = deviceId;
    doc["sensor"] = sensorCfg.name;
    doc["value"] = value;
    doc["unit"] = sensorCfg.unit;
    doc["ts_ms"] = tsMs;
    doc["seq"] = seq;

    const bool ok = publishRawJson(buildMeasurementTopic(sensorCfg.name), doc, false);
    if (ok) {
        ++sequenceNumber;
    }
    return ok;
}

bool publishStatus(const char* statusValue, bool retained = true) {
    if (!syncTimeWithNTP()) {
        Serial.println("Pominieto publikacje statusu: brak poprawnej synchronizacji czasu.");
        return false;
    }

    StaticJsonDocument<JSON_DOC_SIZE> doc;
    doc["schema_version"] = SCHEMA_VERSION;
    doc["group_id"] = groupId;
    doc["device_id"] = deviceId;
    doc["status"] = statusValue;
    doc["ts_ms"] = getTimestampMs();

    return publishRawJson(statusTopic, doc, retained);
}

#if MQTT_USE_TLS
void configureMqttTransport() {
    Serial.println("MQTT TLS: wlaczone.");

    #ifdef MQTT_CA_CERT
    netClient.setCACert(MQTT_CA_CERT);
    Serial.println("Wczytano certyfikat CA dla MQTT TLS.");
    #else
    netClient.setInsecure();
    Serial.println("UWAGA: TLS bez weryfikacji certyfikatu (setInsecure).");
    #endif
}
#else
void configureMqttTransport() {
    Serial.println("MQTT TLS: wylaczone.");
}
#endif

bool mqttConnectWithOptionalAuth() {
    #if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    return mqttClient.connect(deviceId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
    #else
    return mqttClient.connect(deviceId.c_str());
    #endif
}

void connectMQTT() {
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient.setKeepAlive(30);

    while (!mqttClient.connected()) {
        Serial.print("Laczenie z MQTT...");

        if (mqttConnectWithOptionalAuth()) {
            Serial.println("OK");
            publishStatus("online", true);
        } else {
            Serial.print("blad, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" - ponowna proba za 2 s");
            delay(MQTT_RETRY_DELAY_MS);
        }
    }
}

void publishAllMeasurements() {
    for (const auto& sensorCfg : SENSORS) {
        publishMeasurement(sensorCfg);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    deviceId = loadOrCreateDeviceId();
    groupId = String(MQTT_GROUP);
    groupId.toLowerCase();

    if (!isValidUuidV4(deviceId)) {
        haltWithError("device_id w NVS nie jest poprawnym UUIDv4.");
    }

    if (!isValidTopicSegment(groupId)) {
        haltWithError("MQTT_GROUP musi skladac sie z malych liter, cyfr lub '-'.");
    }

    baseTopic = String(TOPIC_ROOT) + "/" + groupId + "/" + deviceId;
    statusTopic = baseTopic + "/status";

    Serial.print("Device ID (UUIDv4): ");
    Serial.println(deviceId);
    Serial.print("Topic statusowy: ");
    Serial.println(statusTopic);
    Serial.println("Tematy pomiarowe:");
    for (const auto& sensorCfg : SENSORS) {
        Serial.println(buildMeasurementTopic(sensorCfg.name));
    }

    connectWiFi();
    configureMqttTransport();
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
    if (now - lastMeasurementAt >= MEASUREMENT_INTERVAL_MS) {
        publishAllMeasurements();
        lastMeasurementAt = now;
    }

    if (now - lastStatusAt >= STATUS_INTERVAL_MS) {
        publishStatus("online", true);
        lastStatusAt = now;
    }
}
