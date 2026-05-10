// ============================================
// Web_server.ino – Main Sketch
// ESP32-CAM AI Thinker – Smart Home VKU v2.0
//
// Chức năng:
//   • Camera streaming (MJPEG) qua Web UI
//   • Face Detection + Recognition (ESP-face library)
//   • MQTT publish kết quả nhận diện → ESP32 chính
//   • SPIFFS lưu khuôn mặt persistent (không mất khi tắt nguồn)
//   • Web UI: enrollment, quản lý faces, camera settings
//
// Yêu cầu:
//   • Board: AI Thinker ESP32-CAM
//   • Arduino ESP32 Board Core: 2.0.0 ~ 2.0.4
//   • Partition: "Huge APP (3MB No OTA / 1MB SPIFFS)"
//   • PSRAM: Enabled
//   • Thư viện: PubSubClient (MQTT)
//   • Nguồn: 5V / 2A riêng (KHÔNG dùng USB PC)
//
// MQTT Topics:
//   • Publish:   home/door/face_result  → {"status":"MATCH","name":"Nghia","score":0.89}
//   • Subscribe: home/cam/cmd           → {"cmd":"capture"} / {"cmd":"enroll","name":"X"}
//
// Tài liệu: Docs/task.md – Phase 3
// ============================================

#include "esp_camera.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include "config.h"
#include "camera_pins.h"
#include "mqtt_handler.h"
#include "face_recognition_112_v1_s8.hpp"

// ===== Khai báo hàm từ app_httpd.cpp =====
void startCameraServer();
extern FaceRecognition112V1S8 *s_recognizer;

// Cross-task face result (từ app_httpd.cpp stream task)
extern volatile bool   g_face_result_pending;
extern volatile float  g_face_result_score;
extern char            g_face_result_name[];
extern char            g_face_result_status[];

// Enrollment control (từ app_httpd.cpp)
extern bool g_is_enrolling;
extern char g_enroll_name[];
extern bool g_face_detect_on;
extern bool g_face_recognize_on;

// ===== WiFi reconnect timer =====
static unsigned long lastWifiCheck = 0;
static const unsigned long WIFI_CHECK_INTERVAL = 30000;  // 30 giây

// ===== LED blink timer =====
static unsigned long lastBlink = 0;
static bool blinkState = false;

// ===== MQTT command callback =====
// Xử lý lệnh nhận từ ESP32 chính qua MQTT topic: home/cam/cmd
void onMqttCommand(const char* cmd, const char* param) {
    Serial.printf("[Main] MQTT CMD: %s | PARAM: %s\n", cmd, param);
    
    if (strcmp(cmd, "capture") == 0) {
        // Chụp ảnh – TODO: có thể lưu SPIFFS hoặc gửi qua MQTT
        Serial.println("[Main] Capture request received");
    }
    else if (strcmp(cmd, "enroll") == 0 && strlen(param) > 0) {
        // Bắt đầu enrollment qua MQTT
        strncpy(g_enroll_name, param, FACE_NAME_MAX - 1);
        g_is_enrolling = true;
        g_face_detect_on = true;  // Tự động bật detection
        Serial.printf("[Main] MQTT Enroll: %s\n", param);
    }
    else if (strcmp(cmd, "standby") == 0) {
        // Tạm dừng face detection (tiết kiệm điện)
        g_face_detect_on = false;
        g_face_recognize_on = false;
        Serial.println("[Main] Standby mode");
    }
    else if (strcmp(cmd, "resume") == 0) {
        // Khôi phục face detection
        g_face_detect_on = true;
        g_face_recognize_on = true;
        Serial.println("[Main] Resume mode");
    }
    else {
        Serial.printf("[Main] Unknown command: %s\n", cmd);
    }
}

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  ESP32-CAM Smart Home v2.0 – Phase 3  ");
    Serial.println("  AI Thinker | Face Recognition + MQTT  ");
    Serial.println("========================================");
    
    // ---- LED PIN SETUP ----
    pinMode(LED_FLASH_PIN, OUTPUT);
    pinMode(LED_STATUS_PIN, OUTPUT);
    digitalWrite(LED_FLASH_PIN, LOW);    // Flash LED tắt
    digitalWrite(LED_STATUS_PIN, HIGH);  // Status LED tắt (active LOW)
    
    // ---- SPIFFS INIT (cho face recognizer flash storage) ----
    Serial.println("\n[1/5] Initializing SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("  ⚠️ SPIFFS mount failed");
    } else {
        Serial.printf("  → SPIFFS: Total=%dKB, Used=%dKB\n",
                      SPIFFS.totalBytes() / 1024, SPIFFS.usedBytes() / 1024);
    }
    
    // ---- PSRAM CHECK ----
    Serial.println("\n[2/5] Checking PSRAM...");
    if (psramFound()) {
        Serial.printf("  → PSRAM: %d KB total, %d KB free\n",
                      ESP.getPsramSize() / 1024,
                      ESP.getFreePsram() / 1024);
    } else {
        Serial.println("  ⚠️ PSRAM NOT FOUND! Face Detection/Recognition will NOT work.");
        Serial.println("  → Check: Tools > PSRAM > Enabled in Arduino IDE");
    }
    
    // ---- CAMERA INIT ----
    Serial.println("\n[3/5] Initializing Camera (OV2640)...");
    
    camera_config_t cam_config;
    cam_config.ledc_channel = LEDC_CHANNEL_0;
    cam_config.ledc_timer   = LEDC_TIMER_0;
    cam_config.pin_d0       = Y2_GPIO_NUM;
    cam_config.pin_d1       = Y3_GPIO_NUM;
    cam_config.pin_d2       = Y4_GPIO_NUM;
    cam_config.pin_d3       = Y5_GPIO_NUM;
    cam_config.pin_d4       = Y6_GPIO_NUM;
    cam_config.pin_d5       = Y7_GPIO_NUM;
    cam_config.pin_d6       = Y8_GPIO_NUM;
    cam_config.pin_d7       = Y9_GPIO_NUM;
    cam_config.pin_xclk     = XCLK_GPIO_NUM;
    cam_config.pin_pclk     = PCLK_GPIO_NUM;
    cam_config.pin_vsync    = VSYNC_GPIO_NUM;
    cam_config.pin_href     = HREF_GPIO_NUM;
    cam_config.pin_sscb_sda = SIOD_GPIO_NUM;
    cam_config.pin_sscb_scl = SIOC_GPIO_NUM;
    cam_config.pin_pwdn     = PWDN_GPIO_NUM;
    cam_config.pin_reset    = RESET_GPIO_NUM;
    cam_config.xclk_freq_hz = 20000000;  // 20 MHz XCLK
    cam_config.pixel_format = PIXFORMAT_JPEG;
    cam_config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    cam_config.fb_location  = CAMERA_FB_IN_PSRAM;
    cam_config.jpeg_quality = CAM_JPEG_QUALITY;
    cam_config.fb_count     = 1;
    
    // PSRAM → higher resolution + double buffer
    if (psramFound()) {
        cam_config.frame_size   = CAM_FRAME_SIZE;     // QVGA 320×240
        cam_config.jpeg_quality = CAM_JPEG_QUALITY;    // High quality
        cam_config.fb_count     = 2;                   // Double buffer
        cam_config.grab_mode    = CAMERA_GRAB_LATEST;  // Lấy frame mới nhất
    } else {
        cam_config.frame_size   = FRAMESIZE_SVGA;
        cam_config.fb_location  = CAMERA_FB_IN_DRAM;
    }
    
    esp_err_t err = esp_camera_init(&cam_config);
    if (err != ESP_OK) {
        Serial.printf("  ❌ Camera init FAILED! Error: 0x%x\n", err);
        Serial.println("  → Check camera ribbon cable connection");
        Serial.println("  → Check 5V power supply (need 2A)");
        // Blink LED đỏ nhanh để báo lỗi
        while (true) {
            digitalWrite(LED_STATUS_PIN, LOW);   // ON
            delay(100);
            digitalWrite(LED_STATUS_PIN, HIGH);  // OFF
            delay(100);
        }
        return;
    }
    
    // Camera sensor settings
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        // Drop frame size for initial higher FPS
        s->set_framesize(s, FRAMESIZE_QVGA);
        Serial.println("  → Camera: OV2640, QVGA 320×240, JPEG");
    }
    Serial.println("  ✅ Camera initialized OK");
    
    // ---- WiFi CONNECT ----
    Serial.println("\n[4/5] Connecting to WiFi...");
    Serial.printf("  → SSID: %s\n", WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    WiFi.setSleep(false);  // Tắt WiFi sleep để giảm latency
    
    int wifi_timeout = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_timeout < 40) {
        delay(500);
        Serial.print(".");
        wifi_timeout++;
        
        // Blink LED đỏ chậm khi đang kết nối
        blinkState = !blinkState;
        digitalWrite(LED_STATUS_PIN, blinkState ? LOW : HIGH);
    }
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("  ❌ WiFi connection FAILED!");
        Serial.println("  → Check SSID and Password in config.h");
        // LED đỏ sáng liên tục báo lỗi WiFi
        digitalWrite(LED_STATUS_PIN, LOW);  // ON
    } else {
        Serial.println("  ✅ WiFi connected!");
        Serial.printf("  → IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  → RSSI: %d dBm\n", WiFi.RSSI());
    }
    
    // ---- MQTT INIT ----
    Serial.println("\n[5/5] Initializing MQTT...");
    Serial.printf("  → Broker: %s:%d\n", MQTT_SERVER, MQTT_PORT);
    mqtt_init();
    mqtt_set_cmd_callback(onMqttCommand);
    
    // ---- START WEB SERVER ----
    Serial.println("\n[OK] Starting Camera Web Server...");
    startCameraServer();
    
    // ---- READY! ----
    Serial.println("\n========================================");
    Serial.println("  ✅ ESP32-CAM READY!");
    Serial.printf("  📡 Web UI:  http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  📹 Stream:  http://%s:81/stream\n", WiFi.localIP().toString().c_str());
    Serial.printf("  📸 Capture: http://%s/capture\n", WiFi.localIP().toString().c_str());
    Serial.printf("  📊 Status:  http://%s/status\n", WiFi.localIP().toString().c_str());
    Serial.println("========================================");
    Serial.printf("  Faces: %d | Heap: %dKB | PSRAM: %dKB\n",
                  s_recognizer ? s_recognizer->get_enrolled_id_num() : 0,
                  ESP.getFreeHeap() / 1024,
                  ESP.getFreePsram() / 1024);
    Serial.println("========================================\n");
    
    // Status LED solid ON = ready
    digitalWrite(LED_STATUS_PIN, LOW);  // ON (active LOW)
}

// ============================================
// LOOP
// ============================================
void loop() {
    // ---- 1. MQTT Loop (duy trì kết nối + xử lý messages) ----
    mqtt_loop();
    
    // ---- 2. Publish face result (thread-safe: từ stream task → main loop) ----
    if (g_face_result_pending) {
        mqtt_publish_face_result(g_face_result_status, g_face_result_name, g_face_result_score);
        g_face_result_pending = false;
    }
    
    // ---- 3. WiFi reconnect (mỗi 30 giây kiểm tra) ----
    unsigned long now = millis();
    if (now - lastWifiCheck > WIFI_CHECK_INTERVAL) {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Disconnected! Reconnecting...");
            WiFi.reconnect();
            
            // Blink LED đỏ khi mất WiFi
            digitalWrite(LED_STATUS_PIN, HIGH);  // OFF
            delay(200);
            digitalWrite(LED_STATUS_PIN, LOW);   // ON
        }
    }
    
    // ---- 4. Periodic status publish (mỗi 60 giây) ----
    static unsigned long lastStatusPublish = 0;
    if (now - lastStatusPublish > 60000) {
        lastStatusPublish = now;
        if (mqtt_connected()) {
            mqtt_publish_status("running");
        }
    }
    
    // ---- 5. Heap monitoring ----
    static unsigned long lastHeapCheck = 0;
    if (now - lastHeapCheck > 30000) {
        lastHeapCheck = now;
        Serial.printf("[Heap] Free: %dKB | PSRAM Free: %dKB | Min: %dKB\n",
                      ESP.getFreeHeap() / 1024,
                      ESP.getFreePsram() / 1024,
                      ESP.getMinFreeHeap() / 1024);
    }
    
    delay(10);  // Yield CPU, tránh watchdog timeout
}
