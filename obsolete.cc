
// -------------------------------------------------------------------------------------------

// #include <Arduino.h>
// #include <WiFi.h>
// #include "secrets.h"

// void loop() {
//     float temp_celsius = temperatureRead();

//     Serial.print("Temp onBoard ");
//     Serial.print(temp_celsius);
//     Serial.println("°C");

//     delay(1000);
// }

// String generateDeviceIdFromEfuse() {
//     uint64_t chipId = ESP.getEfuseMac();
//     char id[32];
//     snprintf(id, sizeof(id), "esp32-%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
//     return String(id);
// }

// void connectWiFi() {

//     Serial.print("Laczenie z Wi-Fi: ");
//     Serial.println(WIFI_SSID);
//     WiFi.mode(WIFI_STA);
//     WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

//     while (WiFi.status() != WL_CONNECTED) {
//        delay(500);
//         Serial.print(".");
//     }
    
//     Serial.println();
//     Serial.println("Polaczono z Wi-Fi");
//     Serial.print("Adres IP: ");
//     Serial.println(WiFi.localIP());
// }

// void setup() {
//     Serial.begin(115200);
// }
// -------------------------------------------------------------------------------------------- 

// #include <WiFi.h>
// #include <PubSubClient.h>

// // WiFi
// const char *ssid = ""; // Enter your Wi-Fi name
// const char *password = "";  // Enter Wi-Fi password

// // MQTT Broker
// const char *mqtt_broker = "broker.emqx.io";
// const char *topic = "emqx/esp32";
// const char *mqtt_username = "emqx";
// const char *mqtt_password = "public";
// const int mqtt_port = 1883;

// WiFiClient espClient;
// PubSubClient client(espClient);

// std::string uniqDeviceId(){
//     std::uint64_t chipId = ESP.getEfuseMac();
//     char id[32];
//     sprintf(id, "esp32-%04X%08X", (std::uint16_t)(chipId >> 32), (std::uint32_t)chipId);
//     return id;
// }

// void callback(char *topic, byte *payload, unsigned int length) {
//     Serial.print("Message arrived in topic: ");
//     Serial.println(topic);
//     Serial.print("Message:");
//     for (int i = 0; i < length; i++) {
//         Serial.print((char) payload[i]);
//     }
//     Serial.println();
//     Serial.println("-----------------------");
// }

// void setup() {
//     // Set software serial baud to 115200;
//     Serial.begin(115200);
//     // Connecting to a WiFi network
//     WiFi.begin(ssid, password);
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.println("Connecting to WiFi..");
//     }
//     Serial.println("Connected to the Wi-Fi network");
//     //connecting to a mqtt broker
//     client.setServer(mqtt_broker, mqtt_port);
//     client.setCallback(callback);
//     while (!client.connected()) {
//         String client_id = "esp32-client-";
//         client_id += String(WiFi.macAddress());
//         Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
//         if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
//             Serial.println("Public EMQX MQTT broker connected");
//         } else {
//             Serial.print("failed with state ");
//             Serial.print(client.state());
//             delay(2000);
//         }
//     }
//     // Publish and subscribe
//     client.publish(topic, "Hi, I'm ESP32 ^^");
//     client.subscribe(topic);
// }


// void loop() {
//     client.loop();
// }

// #include <Arduino.h>
// #include <WiFi.h>
// #include <PubSubClient.h>

// #define LED 2

// void setup() {
//   Serial.begin(115200);
//   pinMode(LED, OUTPUT);
// }

// void loop() {
//   digitalWrite(LED, HIGH);
//   Serial.println("LED is on");
//   delay(1000);
//   digitalWrite(LED, LOW);
//   Serial.println("LED is off");
//   delay(1000);
// }