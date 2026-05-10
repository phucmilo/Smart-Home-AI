#pragma once
// ============================================================
// config.h – ESP32 Main (Servo only – Home Assistant)
// Smart Home VKU v2.0
// ============================================================

// ===== WiFi =====
#define WIFI_SSID "Tro Bao Tran T4"
#define WIFI_PASS "thy0905542257"

// ===== MQTT Broker =====
// Chạy lệnh này trên Mac để lấy IP: ipconfig getifaddr en0
#define MQTT_SERVER "192.168.88.83" // IP máy Mac chạy Docker
#define MQTT_PORT 1883
#define MQTT_CLIENT "ESP32_DOOR"
#define MQTT_USER "mqttuser"      // ← Phải khớp mosquitto/passwd
#define MQTT_PASS_WORD "mqttpass" // ← Phải khớp mosquitto/passwd

// ===== MQTT Topics =====
#define TOPIC_CMD "home/door/command" // HA → ESP32
#define TOPIC_STATE "home/door/state" // ESP32 → HA

// ===== MQTT Payloads =====
#define CMD_UNLOCK "UNLOCK"
#define CMD_LOCK "LOCK"
#define STATE_UNLOCKED "unlocked"
#define STATE_LOCKED "locked"

// ===== MQTT Retry =====
#define MQTT_RETRY_MS 5000

// ===== Touch TTP223 =====
#define TOUCH_PIN 4             // GPIO4 – TTP223 output

// ===== Keypad 4x4 =====
#define KP_ROWS 4
#define KP_COLS 4
#define KP_R1 13    // Hàng 1
#define KP_R2 15    // Hàng 2
#define KP_R3 14    // Hàng 3
#define KP_R4 26    // Hàng 4
#define KP_C1 25    // Cột 1
#define KP_C2 33    // Cột 2
#define KP_C3 32    // Cột 3
#define KP_C4 21    // Cột 4
#define KP_DEFAULT_PASS "1006"
#define KP_MAX_LEN 8

// ===== Buzzer =====
#define BUZZER_PIN 12   // GPIO12 – Active buzzer

// ===== MQ Gas Sensor =====
#define GAS_PIN          34    // GPIO34 – MQ analog out (AO)
#define GAS_WARN_LEVEL   1500  // Cảnh báo (raw ADC 0-4095)
#define GAS_DANGER_LEVEL 2500  // Nguy hiểm
#define TOPIC_GAS        "home/gas"  // ESP32 → Web (retained)

// ===== LCD I2C 16x2 =====
#define LCD_ADDR  0x27   // Thử 0x3F nếu không lên
#define LCD_COLS  16
#define LCD_ROWS  2
#define LCD_SDA   0      // GPIO0  – SDA (pull-up HIGH = boot bình thường ✓)
#define LCD_SCL   22     // GPIO22 – SCL

// ===== MQTT Keypad Topics =====
#define TOPIC_KP_CMD    "home/keypad/cmd"    // Web → ESP32
#define TOPIC_KP_STATUS "home/keypad/status" // ESP32 → Web

// ===== RFID RC522 (SPI) =====
#define RFID_SS_PIN   5         // GPIO5  → RC522 SDA/SS
#define RFID_RST_PIN  27        // GPIO27 → RC522 RST
// SCK=18, MOSI=23, MISO=19 (SPI mặc định của ESP32)

// ===== MQTT RFID Topics =====
#define TOPIC_RFID_SCAN "home/rfid/scan"  // ESP32 → Web
#define TOPIC_RFID_CMD  "home/rfid/cmd"   // Web → ESP32

// ===== Servo =====
#define SERVO_PIN 2             // GPIO2 (PWM)
#define SERVO_LOCKED_ANGLE 0    // 0°  = cửa khóa
#define SERVO_UNLOCKED_ANGLE 90 // 90° = cửa mở
#define SERVO_MOVE_DELAY_MS 600 // Thời gian chờ servo xoay xong

// ===== Tự động khóa lại =====
#define AUTO_LOCK_MS 0 // Tự khóa sau 5 giây (0 = tắt tự khóa)

// ===== Fingerprint DY50 =====
#define FP_RX_PIN         16    // ESP32 RX2 ← DY50 TX
#define FP_TX_PIN         17    // ESP32 TX2 → DY50 RX
#define FP_BAUD           57600
#define FP_ENROLL_TIMEOUT 15000 // Timeout mỗi bước đăng ký (ms)

// ===== MQTT Fingerprint Topics =====
#define TOPIC_FP_ENROLL  "home/finger/enroll"  // Web → ESP32: ID (1-127)
#define TOPIC_FP_DELETE  "home/finger/delete"  // Web → ESP32: ID
#define TOPIC_FP_STATUS  "home/finger/status"  // ESP32 → Web: trạng thái
