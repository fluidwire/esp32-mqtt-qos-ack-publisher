/*
 * ESP32 MQTT Publisher with Delivery Acknowledgement
 * ---------------------------------------------------
 * Publishes sequence-numbered readings and waits for an application-level
 * ack on a separate topic. Unacked messages are retried, and delivery
 * statistics are printed so you can measure an unreliable network.
 *
 * Flow:
 *   device  -> <base>/data  {"seq":N,"uptime_s":...,"rssi":...}
 *   backend -> <base>/ack   N          (plain sequence number)
 *
 * Test without a backend: see the acker one-liner in README.md.
 *
 * Required library (install via Library Manager):
 *   - "PubSubClient" by Nick O'Leary
 *
 * Setup:
 *   1. Copy config.h.example to config.h, fill in WiFi + broker.
 *   2. Flash. Open Serial Monitor at 115200.
 *
 * (c) 2026 Fluidwire. Released under the MIT License.
 * https://github.com/fluidwire
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

const uint32_t PUBLISH_INTERVAL_MS = 10 * 1000UL;
const uint32_t ACK_TIMEOUT_MS      = 3 * 1000UL;
const uint8_t  MAX_RETRIES         = 3;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

String dataTopic = String(MQTT_BASE_TOPIC) + "/data";
String ackTopic  = String(MQTT_BASE_TOPIC) + "/ack";

uint32_t seq          = 0;      // sequence of the message in flight
bool     awaitingAck  = false;
uint8_t  retries      = 0;
uint32_t sentAt       = 0;
uint32_t lastNewMsgAt = 0;
char     lastPayload[96];

// Delivery statistics
uint32_t statSent = 0, statAcked = 0, statRetried = 0, statDropped = 0;

void printStats() {
  Serial.printf("[stats] sent=%lu acked=%lu retried=%lu dropped=%lu\n",
                (unsigned long)statSent, (unsigned long)statAcked,
                (unsigned long)statRetried, (unsigned long)statDropped);
}

void onMessage(char* topic, byte* payload, unsigned int length) {
  char buf[16];
  unsigned int n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';
  uint32_t acked = strtoul(buf, nullptr, 10);
  if (awaitingAck && acked == seq) {
    awaitingAck = false;
    statAcked++;
    Serial.printf("[ack] seq=%lu confirmed after %lu ms\n",
                  (unsigned long)acked, (unsigned long)(millis() - sentAt));
    printStats();
  }
}

void connectWiFi() {
  Serial.print(F("[wifi] connecting to "));
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.println();
  Serial.print(F("[wifi] IP: "));
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  while (!mqtt.connected()) {
    Serial.print(F("[mqtt] connecting... "));
    bool ok = mqtt.connect(MQTT_CLIENT_ID,
                           strlen(MQTT_USER) ? MQTT_USER : nullptr,
                           strlen(MQTT_PASSWORD) ? MQTT_PASSWORD : nullptr);
    if (ok) {
      Serial.println(F("ok"));
      mqtt.subscribe(ackTopic.c_str(), 1);
    } else {
      Serial.print(F("failed state="));
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

void sendCurrent() {
  mqtt.publish(dataTopic.c_str(), lastPayload);
  sentAt = millis();
  Serial.printf("[tx] %s (attempt %u)\n", lastPayload, retries + 1);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  uint32_t now = millis();

  if (awaitingAck) {
    if (now - sentAt >= ACK_TIMEOUT_MS) {
      if (retries < MAX_RETRIES) {
        retries++;
        statRetried++;
        Serial.printf("[retry] no ack for seq=%lu\n", (unsigned long)seq);
        sendCurrent();
      } else {
        awaitingAck = false;
        statDropped++;
        Serial.printf("[drop] seq=%lu gave up after %u retries\n",
                      (unsigned long)seq, MAX_RETRIES);
        printStats();
      }
    }
  } else if (now - lastNewMsgAt >= PUBLISH_INTERVAL_MS || lastNewMsgAt == 0) {
    seq++;
    retries = 0;
    snprintf(lastPayload, sizeof(lastPayload),
             "{\"seq\":%lu,\"uptime_s\":%lu,\"rssi\":%d}",
             (unsigned long)seq, (unsigned long)(now / 1000UL), WiFi.RSSI());
    statSent++;
    awaitingAck  = true;
    lastNewMsgAt = now;
    sendCurrent();
  }

  delay(20);
}
