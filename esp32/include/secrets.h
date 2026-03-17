#pragma once
#define WIFI_SSID "esp_lab"
#define WIFI_PASSWORD "12345678"
#define MQTT_HOST "156.17.45.68"
#define MQTT_PORT 1883
#define MQTT_GROUP "g05"

// Opcjonalnie: uwierzytelnianie MQTT
// #define MQTT_USERNAME "twoj_login"
// #define MQTT_PASSWORD "twoje_haslo"

// Opcjonalnie: TLS dla brokera MQTT (np. port 8883)
// Domyslnie zostaw 0, bo laboratorium pokazuje zwykly broker na porcie 1883.
// Zmien na 1 tylko wtedy, gdy broker obsluguje TLS.
// #define MQTT_USE_TLS 1

// Bezpieczniejsza opcja: pinning/CA cert brokera
// #define MQTT_CA_CERT R"EOF(
// -----BEGIN CERTIFICATE-----
// ...
// -----END CERTIFICATE-----
// )EOF