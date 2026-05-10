// ============================================
// mqtt_handler.cpp – MQTT Client Implementation
// PubSubClient wrapper cho Smart Home ESP32-CAM
// Smart Home VKU v2.0 – Phase 3
// ============================================

#include "mqtt_handler.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ===== Private: MQTT Client =====
static WiFiClient       wifiClient;
static PubSubClient     mqttClient(wifiClient);
static mqtt_cmd_callback_t cmdCallback = NULL;
static unsigned long    lastReconnectAttempt = 0;
static const unsigned long RECONNECT_INTERVAL = 5000;  // 5 giây giữa mỗi lần reconnect

// ===== Private: Parse simple JSON command =====
// Hỗ trợ format: {"cmd":"capture"} hoặc {"cmd":"enroll","name":"Nghia"}
static void parse_mqtt_message(const char* topic, const char* msg) {
    if (!cmdCallback) return;
    
    // Tìm "cmd":"xxx"
    const char* cmd_start = strstr(msg, "\"cmd\":\"");
    if (!cmd_start) return;
    cmd_start += 7;  // Bỏ qua "cmd":"
    
    const char* cmd_end = strchr(cmd_start, '"');
    if (!cmd_end) return;
    
    char cmd[32] = {0};
    int cmd_len = min((int)(cmd_end - cmd_start), 31);
    strncpy(cmd, cmd_start, cmd_len);
    
    // Tìm "name":"xxx" (tùy chọn, dùng khi enroll)
    char param[FACE_NAME_MAX] = {0};
    const char* name_start = strstr(msg, "\"name\":\"");
    if (name_start) {
        name_start += 8;  // Bỏ qua "name":"
        const char* name_end = strchr(name_start, '"');
        if (name_end) {
            int name_len = min((int)(name_end - name_start), FACE_NAME_MAX - 1);
            strncpy(param, name_start, name_len);
        }
    }
    
    Serial.printf("[MQTT] CMD: %s, PARAM: %s\n", cmd, param);
    cmdCallback(cmd, param);
}

// ===== Private: MQTT Callback (nhận message) =====
static void mqtt_callback_internal(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    char msg[256];
    int len = min((unsigned int)255, length);
    memcpy(msg, payload, len);
    msg[len] = '\0';
    
    Serial.printf("[MQTT] Received [%s]: %s\n", topic, msg);
    
    // Xử lý message
    if (strcmp(topic, TOPIC_CAM_CMD) == 0) {
        parse_mqtt_message(topic, msg);
    }
}

// ===== Private: Attempt MQTT connection =====
static bool mqtt_reconnect() {
    Serial.printf("[MQTT] Connecting to %s:%d...\n", MQTT_SERVER, MQTT_PORT);
    
    bool connected;
    if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS_WORD);
    } else {
        connected = mqttClient.connect(MQTT_CLIENT);
    }
    
    if (connected) {
        Serial.println("[MQTT] Connected!");
        
        // Subscribe topic nhận lệnh từ ESP32 chính
        mqttClient.subscribe(TOPIC_CAM_CMD);
        Serial.printf("[MQTT] Subscribed: %s\n", TOPIC_CAM_CMD);
        
        // Publish trạng thái online
        mqtt_publish_status("online");
        
        return true;
    } else {
        Serial.printf("[MQTT] Failed, rc=%d. Retry in %ds\n", 
                      mqttClient.state(), RECONNECT_INTERVAL / 1000);
        return false;
    }
}

// ============================================
// PUBLIC API
// ============================================

void mqtt_init() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback_internal);
    mqttClient.setBufferSize(512);
    mqttClient.setKeepAlive(60);    // 60s keepalive – tránh timeout khi stream+face đang chạy
    mqttClient.setSocketTimeout(10);
    
    Serial.printf("[MQTT] Initialized: %s:%d (client: %s)\n", 
                  MQTT_SERVER, MQTT_PORT, MQTT_CLIENT);
    
    // Thử kết nối lần đầu
    mqtt_reconnect();
}

void mqtt_loop() {
    if (!mqttClient.connected()) {
        // Auto-reconnect mỗi RECONNECT_INTERVAL
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            mqtt_reconnect();
        }
    } else {
        // Process incoming messages
        mqttClient.loop();
    }
}

bool mqtt_connected() {
    return mqttClient.connected();
}

void mqtt_publish_face_result(const char* status, const char* name, float score) {
    if (!mqttClient.connected()) return;
    
    char payload[192];
    if (name && strlen(name) > 0) {
        snprintf(payload, sizeof(payload),
                 "{\"status\":\"%s\",\"name\":\"%s\",\"score\":%.2f}",
                 status, name, score);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"status\":\"%s\",\"score\":%.2f}",
                 status, score);
    }
    
    mqttClient.publish(TOPIC_FACE_RESULT, payload);
    Serial.printf("[MQTT] Published [%s]: %s\n", TOPIC_FACE_RESULT, payload);
}

void mqtt_publish_status(const char* status_msg) {
    if (!mqttClient.connected()) return;
    
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"status\":\"%s\",\"ip\":\"%s\",\"faces\":%d,\"heap\":%d}",
             status_msg,
             WiFi.localIP().toString().c_str(),
             0,
             ESP.getFreeHeap());
    
    mqttClient.publish(TOPIC_CAM_STATUS, payload);
}

void mqtt_set_cmd_callback(mqtt_cmd_callback_t cb) {
    cmdCallback = cb;
}

void mqtt_door_unlock() {
    if (!mqttClient.connected()) return;
    mqttClient.publish(TOPIC_DOOR_CMD, "UNLOCK", true);
    Serial.println("[MQTT] → home/door/command: UNLOCK (face match)");
}
