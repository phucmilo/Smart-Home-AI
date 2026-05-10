/*
 * ============================================================
 *  config.h – Cấu hình ESP32 AI Voice
 *  Smart Home VKU v2.0 – Phase 4
 * ============================================================
 *  ESP32 DevKit V1 (WROOM-32) + INMP441 + MAX98357A + LED
 * ============================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== CHỌN CHẾ ĐỘ HOẠT ĐỘNG ====================
// Uncomment MỘT trong các dòng dưới để chọn mode AI:
// - Không uncomment gì = CHẾ ĐỘ TEST (VAD đơn giản, không cần thư viện ML)
// - USE_TFLITE_MODEL  = TF Micro Speech (cần thư viện TFLite + model_data.h)
// - USE_EDGE_IMPULSE  = Edge Impulse custom (cần thư viện Edge Impulse export)

// #define USE_TFLITE_MODEL
// #define USE_EDGE_IMPULSE

// ==================== I2S0 – INMP441 MICROPHONE INPUT ====================
#define I2S_MIC_PORT      I2S_NUM_0
#define I2S_MIC_BCLK      14    // INMP441 SCK  → GPIO14
#define I2S_MIC_WS        15    // INMP441 WS   → GPIO15
#define I2S_MIC_DIN       32    // INMP441 SD   → GPIO32

// ==================== I2S1 – MAX98357A SPEAKER OUTPUT ====================
#define I2S_SPK_PORT      I2S_NUM_1
#define I2S_SPK_BCLK      26    // MAX98357A BCLK  → GPIO26
#define I2S_SPK_WS        25    // MAX98357A LRC   → GPIO25
#define I2S_SPK_DOUT      27    // MAX98357A DIN   → GPIO27

// ==================== LED & TOUCH ====================
#define LIGHT_PIN         5     // LED output (giữ từ test TTP223)
#define TOUCH_PIN         4     // TTP223 touch sensor (backup control)

// ==================== AUDIO PARAMETERS ====================
#define SAMPLE_RATE       16000   // 16 kHz – chuẩn cho keyword spotting
#define AUDIO_LENGTH_MS   1000    // 1 giây mỗi lần capture
#define AUDIO_BUFFER_LEN  (SAMPLE_RATE * AUDIO_LENGTH_MS / 1000)  // 16000 samples

// I2S DMA buffer
#define DMA_BUF_COUNT     4
#define DMA_BUF_LEN       1024

// Chunk size mỗi lần đọc trong loop (100ms = responsive touch)
#define CHUNK_SAMPLES     1600    // 100ms at 16kHz

// ==================== VOICE ACTIVITY DETECTION (chế độ test) ====================
// Ngưỡng RMS để phát hiện có giọng nói (tuỳ chỉnh theo môi trường)
#define VAD_RMS_THRESHOLD       400     // RMS > threshold = có tiếng nói
#define VAD_SPEECH_MIN_MS       80      // Nói ít nhất 80ms mới tính
#define VAD_SILENCE_MIN_MS      200     // Im lặng 200ms = kết thúc lệnh
#define VAD_COOLDOWN_MS         3000    // Chờ 3s giữa 2 lệnh liên tiếp

// ==================== INFERENCE (chế độ AI) ====================
#define CONFIDENCE_THRESHOLD    0.75f   // Confidence tối thiểu để chấp nhận
#define INFERENCE_INTERVAL_MS   500     // Chạy inference mỗi 500ms

// Labels cho TF Micro Speech model
#define LABEL_SILENCE     0
#define LABEL_UNKNOWN     1
#define LABEL_YES         2
#define LABEL_NO          3

// Số labels cho micro_speech
#define NUM_LABELS        4
static const char* LABEL_NAMES[NUM_LABELS] = {
    "silence", "unknown", "yes", "no"
};

// ==================== MQTT CONFIG ====================
#define USE_MQTT
#define WIFI_SSID            "MerryHouse"
#define WIFI_PASS            "0906943799"
#define MQTT_SERVER          "192.168.88.83"
#define MQTT_PORT            1883
#define MQTT_CLIENT_ID       "ESP32_VOICE"
#define MQTT_USER            "mqttuser"
#define MQTT_PASS_WORD       "mqttpass"
#define MQTT_TOPIC_LIGHT_CMD   "home/light/command"   // Web → ESP32
#define MQTT_TOPIC_LIGHT_STATE "home/light/state"     // ESP32 → Web
#define MQTT_RETRY_MS        5000

#endif // CONFIG_H
