#pragma once
// ============================================
// mqtt_handler.h – MQTT Client Interface
// PubSubClient wrapper cho Smart Home MQTT
// ============================================

#include <Arduino.h>
#include "config.h"

// ===== Public API =====

// Khởi tạo MQTT client và kết nối broker
void mqtt_init();

// Gọi trong loop() – duy trì kết nối + xử lý messages
void mqtt_loop();

// Kiểm tra trạng thái kết nối
bool mqtt_connected();

// Publish kết quả nhận diện khuôn mặt
// status: "MATCH" / "NO_MATCH" / "ENROLLED"
// name: tên người (hoặc "" nếu NO_MATCH)
// score: cosine similarity (0.0 – 1.0)
// JSON format: {"status":"MATCH","name":"Nghia","score":0.89}
void mqtt_publish_face_result(const char* status, const char* name, float score);

// Publish trạng thái camera
void mqtt_publish_status(const char* status_msg);

// ===== Command Callback =====
// Callback type cho lệnh MQTT nhận từ ESP32 chính
// cmd: "capture", "enroll", "standby", "resume"
// param: tham số bổ sung (name khi enroll, hoặc "")
typedef void (*mqtt_cmd_callback_t)(const char* cmd, const char* param);

// Đặt callback xử lý lệnh MQTT
void mqtt_set_cmd_callback(mqtt_cmd_callback_t cb);

// Mở cửa trực tiếp qua MQTT (gọi khi face MATCH)
void mqtt_door_unlock();
