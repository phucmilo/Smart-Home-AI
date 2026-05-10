#pragma once
#ifndef _CONFIG_H_
#define _CONFIG_H_
// ============================================
// config.h – Cấu hình chung ESP32-CAM
// Smart Home VKU v2.0 – Phase 3
// ============================================

// ===== WiFi =====
#define WIFI_SSID       "Tro Bao Tran T4"          // ← ĐỔI: SSID WiFi của bạn
#define WIFI_PASS       "thy0905542257"      // ← ĐỔI: Password WiFi

// ===== MQTT Broker =====
#define MQTT_SERVER     "192.168.88.83"     // IP máy Mac chạy Docker
#define MQTT_PORT       1883
#define MQTT_CLIENT     "ESP32_CAM"
#define MQTT_USER       "mqttuser"
#define MQTT_PASS_WORD  "mqttpass"

// ===== MQTT Topics =====
#define TOPIC_FACE_RESULT   "home/door/face_result"   // CAM → All: kết quả nhận diện
#define TOPIC_CAM_CMD       "home/cam/cmd"            // Server → CAM: lệnh điều khiển
#define TOPIC_CAM_STATUS    "home/status/cam"         // CAM → All: trạng thái camera
#define TOPIC_DOOR_CMD      "home/door/command"       // CAM → ESP32 Main: UNLOCK/LOCK


// ===== Face Recognition =====
#define FACE_NAME_MAX           32      // Độ dài tối đa tên khuôn mặt
#define FACE_DETECT_MIN_SCORE   0.7f    // Ngưỡng confidence detection
#define FACE_MATCH_THRESHOLD    0.40f   // Ngưỡng cosine similarity (crop-only, không landmark)
#define FACE_ENROLL_CONFIRM     5       // Số ảnh xác nhận khi enroll

// ===== MQTT Publish Cooldown (tránh spam message) =====
#define MATCH_COOLDOWN_MS       5000    // 5s giữa 2 lần publish MATCH cùng người
#define NOMATCH_COOLDOWN_MS     10000   // 10s giữa 2 lần publish NO_MATCH

// ===== LED Pins (AI Thinker ESP32-CAM) =====
#define LED_FLASH_PIN   4       // Flash LED trắng sáng – Active HIGH
#define LED_STATUS_PIN  33      // LED đỏ mặt sau – Active LOW (LOW=ON)

// ===== Camera =====
#define CAM_FRAME_SIZE      FRAMESIZE_QVGA  // 320×240 – tối ưu cho face detect
#define CAM_JPEG_QUALITY    10              // Chất lượng JPEG (1-63, nhỏ = tốt)

#endif // _CONFIG_H_
