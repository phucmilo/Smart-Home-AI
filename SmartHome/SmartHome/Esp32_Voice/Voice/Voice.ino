/*
 * ============================================================
 *  Voice.ino – ESP32 AI Voice Control
 *  Smart Home VKU v2.0 – Phase 4
 * ============================================================
 *  Bật/Tắt đèn LED bằng giọng nói chạy trên ESP32 edge
 *
 *  3 CHẾ ĐỘ HOẠT ĐỘNG (chọn trong config.h):
 *
 *  1. CHẾ ĐỘ TEST (mặc định):
 *     - Không cần thư viện ML nào
 *     - Voice Activity Detection bằng RMS energy
 *     - Phát hiện giọng nói → toggle LED
 *     - Dùng để TEST PHẦN CỨNG (INMP441 + LED + TTP223)
 *
 *  2. CHẾ ĐỘ TFLITE (USE_TFLITE_MODEL):
 *     - Cần: TensorFlowLite_ESP32 + model_data.h
 *     - Pre-trained "Yes"/"No" → bật/tắt đèn
 *
 *  3. CHẾ ĐỘ EDGE IMPULSE (USE_EDGE_IMPULSE):
 *     - Cần: Edge Impulse Arduino Library export
 *     - Custom keywords: "bật đèn"/"tắt đèn"
 *
 *  PHẦN CỨNG:
 *    - ESP32 DevKit V1 (WROOM-32)
 *    - INMP441 I2S MEMS Microphone
 *    - MAX98357A I2S Amplifier + Loa (optional)
 *    - LED trên GPIO5
 *    - TTP223 Touch Sensor trên GPIO4 (backup control)
 *
 *  THƯ VIỆN CẦN CÀI (Arduino Library Manager):
 *    - ESP32 Board Core 2.0.x (QUAN TRỌNG: không dùng 3.x)
 *    - (Chế độ TFLite) TensorFlowLite_ESP32
 *    - (Chế độ Edge Impulse) Library export từ Edge Impulse
 * ============================================================
 */

#include "config.h"
#include <driver/i2s.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_system.h"

#ifdef USE_MQTT
  #include <WiFi.h>
  #include <PubSubClient.h>
#endif

// In nguyên nhân reset từ lần chạy trước
void printResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.print("[RESET] Nguyên nhân reset: ");
    switch (reason) {
        case ESP_RST_POWERON:   Serial.println("POWER ON"); break;
        case ESP_RST_SW:        Serial.println("SOFTWARE RESET"); break;
        case ESP_RST_PANIC:     Serial.println("PANIC/EXCEPTION ← LỖI CODE"); break;
        case ESP_RST_INT_WDT:   Serial.println("INTERRUPT WATCHDOG"); break;
        case ESP_RST_TASK_WDT:  Serial.println("TASK WATCHDOG"); break;
        case ESP_RST_WDT:       Serial.println("OTHER WATCHDOG"); break;
        case ESP_RST_BROWNOUT:  Serial.println("BROWNOUT"); break;
        default: Serial.printf("UNKNOWN (%d)\n", reason); break;
    }
}

// ==================== Conditional ML Includes ====================
#ifdef USE_TFLITE_MODEL
  #include "model_data.h"
  #include <TensorFlowLite_ESP32.h>
  #include "tensorflow/lite/micro/all_ops_resolver.h"
  #include "tensorflow/lite/micro/micro_error_reporter.h"
  #include "tensorflow/lite/micro/micro_interpreter.h"
  #include "tensorflow/lite/schema/schema_generated.h"
#endif

#ifdef USE_EDGE_IMPULSE
  // Đổi tên library theo project Edge Impulse của bạn
  #include <SmartHome_Voice_inferencing.h>
#endif

// ==================== Global State ====================
bool lightState = false;
bool lastTouchState = false;

// ==================== MQTT State ====================
#ifdef USE_MQTT
  WiFiClient   wifiClient;
  PubSubClient mqttClient(wifiClient);

  bool          mqttConnected     = false;
  bool          lastPublishedLight = !false;  // opposite → force first publish
  unsigned long lastMqttRetry      = 0;

  void wifiConnect();
  void mqttConnect();
  void mqttCallback(char* topic, byte* payload, unsigned int length);
  void publishLightState();
#endif

// Audio ring buffer (circular)
int16_t audioBuffer[AUDIO_BUFFER_LEN];
volatile int audioWriteIdx = 0;
volatile bool audioBufferReady = false;

// Voice Activity Detection state
bool isSpeaking = false;
bool waitingForSilence = false;  // Sau khi toggle, phải im lặng trước khi nhận lệnh mới
unsigned long speechStartTime = 0;
unsigned long silenceStartTime = 0;
unsigned long lastCommandTime = 0;

// Audio level (để hiển thị trên Serial Monitor)
float currentRMS = 0;

// ==================== TFLite State ====================
#ifdef USE_TFLITE_MODEL
  // TFLite Micro objects
  tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* input_tensor = nullptr;
  TfLiteTensor* output_tensor = nullptr;

  // Tensor arena – bộ nhớ cho interpreter
  constexpr int kTensorArenaSize = 10 * 1024;  // 10 KB
  uint8_t tensor_arena[kTensorArenaSize];
#endif

// ==================== I2S Setup ====================
/*
 * Khởi tạo I2S0 cho INMP441 Microphone
 * - Master mode, RX only
 * - 16kHz sample rate
 * - 32-bit per sample (INMP441 output 24-bit trong 32-bit frame)
 * - Mono Left channel (L/R pin → GND)
 */
void setupI2S() {
    Serial.println("[I2S] Đang khởi tạo INMP441...");

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        // L/R=GND → LEFT channel. Nếu RMS vẫn=0, thử đổi thành I2S_CHANNEL_FMT_ONLY_RIGHT
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_MIC_BCLK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_DIN
    };

    esp_err_t err;

    err = i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2S] LỖI driver_install: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_MIC_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[I2S] LỖI set_pin: %d\n", err);
        return;
    }

    // Xóa DMA buffer cũ
    i2s_zero_dma_buffer(I2S_MIC_PORT);

    Serial.println("[I2S] INMP441 khởi tạo OK");
    Serial.printf("[I2S] Sample rate: %d Hz, Pins: BCLK=%d, WS=%d, DIN=%d\n",
                  SAMPLE_RATE, I2S_MIC_BCLK, I2S_MIC_WS, I2S_MIC_DIN);
}

// ==================== Audio Capture ====================
/*
 * Đọc một chunk audio (~100ms) từ I2S
 * Ghi vào audioBuffer dạng ring buffer
 * Return: số samples đã đọc
 */
int readAudioChunk() {
    static int32_t raw_samples[CHUNK_SAMPLES];
    size_t bytes_read = 0;

    // Đọc raw 32-bit samples từ I2S (timeout 100ms)
    esp_err_t err = i2s_read(I2S_MIC_PORT, raw_samples,
                              CHUNK_SAMPLES * sizeof(int32_t),
                              &bytes_read, pdMS_TO_TICKS(100));

    // Debug: log lỗi I2S (chỉ in mỗi 2 giây để không spam)
    static unsigned long lastErrLog = 0;
    if (err != ESP_OK) {
        if (millis() - lastErrLog > 2000) {
            Serial.printf("[I2S] LỖI i2s_read: err=%d\n", err);
            lastErrLog = millis();
        }
        return 0;
    }
    if (bytes_read == 0) {
        if (millis() - lastErrLog > 2000) {
            Serial.println("[I2S] bytes_read=0 (timeout hoặc không có data)");
            lastErrLog = millis();
        }
        return 0;
    }

    int samples_read = bytes_read / sizeof(int32_t);

    // Convert 32-bit → 16-bit và ghi vào ring buffer
    // INMP441 output: 24-bit left-aligned trong 32-bit word
    // Shift right 16 để lấy 16 bits cao nhất (MSB)
    for (int i = 0; i < samples_read; i++) {
        audioBuffer[audioWriteIdx] = (int16_t)(raw_samples[i] >> 16);
        audioWriteIdx = (audioWriteIdx + 1) % AUDIO_BUFFER_LEN;
    }

    return samples_read;
}

/*
 * Copy audio buffer (circular) thành linear buffer 1 giây
 * linearBuf phải có kích thước >= AUDIO_BUFFER_LEN
 */
void getLinearAudioBuffer(int16_t* linearBuf) {
    // Copy từ vị trí writeIdx đến cuối, rồi từ đầu đến writeIdx
    int startIdx = audioWriteIdx;
    for (int i = 0; i < AUDIO_BUFFER_LEN; i++) {
        linearBuf[i] = audioBuffer[(startIdx + i) % AUDIO_BUFFER_LEN];
    }
}

// ==================== Audio Analysis ====================
/*
 * Tính RMS (Root Mean Square) energy của audio buffer
 * RMS cao = có âm thanh/giọng nói
 * RMS thấp = im lặng
 */
float computeRMS(int16_t* buffer, int length) {
    float sum = 0;
    for (int i = 0; i < length; i++) {
        float sample = (float)buffer[i];
        sum += sample * sample;
    }
    return sqrt(sum / length);
}

/*
 * Tính Zero-Crossing Rate (ZCR) – tần suất tín hiệu đổi dấu
 * ZCR cao = tiếng ồn/noise (tần số cao)
 * ZCR trung bình = giọng nói
 * ZCR thấp = âm trầm/im lặng
 */
float computeZCR(int16_t* buffer, int length) {
    int crossings = 0;
    for (int i = 1; i < length; i++) {
        if ((buffer[i] > 0 && buffer[i-1] < 0) ||
            (buffer[i] < 0 && buffer[i-1] > 0)) {
            crossings++;
        }
    }
    return (float)crossings / (length - 1);
}

/*
 * Tính spectral energy đơn giản (năng lượng ở dải tần thấp vs cao)
 * Giọng nói con người: tập trung ở 100-4000 Hz
 * Noise: phân bố đều hoặc tập trung ở tần số cao
 */
float computeLowHighRatio(int16_t* buffer, int length) {
    // Chia buffer thành 2 nửa thời gian
    // Dùng autocorrelation đơn giản tại lag ~5ms (80 samples at 16kHz)
    // Nếu có tương quan cao = có tính tuần hoàn = giọng nói
    int lag = 80; // ~5ms, tương ứng F0 ~200Hz (giọng nam)
    float correlation = 0;
    float energy = 0;

    int limit = length - lag;
    if (limit <= 0) return 0;

    for (int i = 0; i < limit; i++) {
        correlation += (float)buffer[i] * (float)buffer[i + lag];
        energy += (float)buffer[i] * (float)buffer[i];
    }

    if (energy < 1.0f) return 0;
    return correlation / energy;  // -1 to 1, cao = có tính tuần hoàn
}

// ==================== LED Control ====================
void setLight(bool state) {
    lightState = state;
    digitalWrite(LIGHT_PIN, state ? HIGH : LOW);
    Serial.printf("💡 Đèn: %s\n", state ? "BẬT" : "TẮT");
    #ifdef USE_MQTT
        publishLightState();
    #endif
}

void toggleLight() {
    setLight(!lightState);
}

// ==================== TTP223 Touch Handler ====================
/*
 * Xử lý cảm biến chạm TTP223 (backup control)
 * Chạm 1 lần = toggle đèn
 * Edge detection: chỉ trigger khi LOW → HIGH
 */
void handleTouch() {
    bool currentTouch = digitalRead(TOUCH_PIN);

    if (currentTouch == HIGH && lastTouchState == LOW) {
        toggleLight();
        Serial.println("👆 Touch detected → Toggle");
        delay(50); // Debounce
    }

    lastTouchState = currentTouch;
}

// ==================== Voice Activity Detection (chế độ test) ====================
/*
 * Phát hiện giọng nói dựa trên RMS energy
 * Khi phát hiện giọng nói (RMS > threshold kéo dài > 200ms):
 *   → Toggle đèn
 *   → Cooldown 1 giây trước khi nhận lệnh tiếp
 *
 * Đây KHÔNG phải keyword spotting - chỉ detect CÓ hay KHÔNG giọng nói
 * Dùng để test phần cứng trước khi tích hợp AI model
 */
void handleVoiceDetection_Simple() {
    // Tính RMS của chunk vừa đọc (100ms gần nhất)
    int startIdx = (audioWriteIdx - CHUNK_SAMPLES + AUDIO_BUFFER_LEN) % AUDIO_BUFFER_LEN;
    static int16_t chunk[CHUNK_SAMPLES];
    for (int i = 0; i < CHUNK_SAMPLES; i++) {
        chunk[i] = audioBuffer[(startIdx + i) % AUDIO_BUFFER_LEN];
    }
    currentRMS = computeRMS(chunk, CHUNK_SAMPLES);

    unsigned long now = millis();

    // Cooldown – chờ sau khi đã toggle
    if (now - lastCommandTime < VAD_COOLDOWN_MS) {
        // Trong cooldown: nếu RMS đã về thấp → reset waitingForSilence
        if (waitingForSilence && currentRMS < VAD_RMS_THRESHOLD) {
            waitingForSilence = false;
            isSpeaking = false;
        }
        return;
    }

    // Sau cooldown: phải đợi im lặng trước khi nhận lệnh mới
    if (waitingForSilence) {
        if (currentRMS < VAD_RMS_THRESHOLD) {
            waitingForSilence = false;
            isSpeaking = false;
        }
        return;
    }

    if (currentRMS > VAD_RMS_THRESHOLD) {
        // Có âm thanh
        if (!isSpeaking) {
            isSpeaking = true;
            speechStartTime = now;
        }

        // Nếu nói đủ lâu → xác nhận là speech
        if (now - speechStartTime >= VAD_SPEECH_MIN_MS) {
            float zcr = computeZCR(chunk, CHUNK_SAMPLES);
            float autocorr = computeLowHighRatio(chunk, CHUNK_SAMPLES);

            // Giọng nói: autocorrelation cao (> 0.6) VÀ ZCR không quá cao (< 0.25)
            bool likelySpeech = (autocorr > 0.6f) && (zcr < 0.25f);

            if (likelySpeech) {
                Serial.printf("🎤 VOICE DETECTED! RMS=%.0f ZCR=%.3f AutoCorr=%.3f → TOGGLE\n",
                              currentRMS, zcr, autocorr);
                toggleLight();
                lastCommandTime = now;
                isSpeaking = false;
                waitingForSilence = true;  // Phải im lặng trước khi nhận lệnh tiếp
            }
        }

        silenceStartTime = now;
    } else {
        // Im lặng
        if (isSpeaking && (now - silenceStartTime >= VAD_SILENCE_MIN_MS)) {
            isSpeaking = false;
        }
    }
}

// ==================== TFLite Inference ====================
#ifdef USE_TFLITE_MODEL

/*
 * Khởi tạo TFLite Micro Interpreter
 * Load model pre-trained micro_speech (Yes/No)
 */
void setupTFLite() {
    Serial.println("[TFLite] Đang khởi tạo...");

    // Load model
    model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[TFLite] LỖI: Model version %d != Schema %d\n",
                      model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    // Resolver – đăng ký tất cả operators
    static tflite::AllOpsResolver resolver;

    // Tạo interpreter
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    // Allocate tensors
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("[TFLite] LỖI: AllocateTensors() failed!");
        return;
    }

    // Get input/output tensors
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    Serial.printf("[TFLite] OK! Input: %d elements, Output: %d elements\n",
                  input_tensor->bytes, output_tensor->bytes);
    Serial.printf("[TFLite] Arena used: %d / %d bytes\n",
                  interpreter->arena_used_bytes(), kTensorArenaSize);
}

/*
 * Chạy TFLite inference trên audio buffer 1 giây
 * Return: label index (0-3) hoặc -1 nếu lỗi
 * confidence: output confidence score
 *
 * QUAN TRỌNG: Phần feature extraction (MFCC/log-mel spectrogram)
 * cần match chính xác với format mà model đã được train.
 * Model micro_speech expect: 49 frames × 40 MFCC bins = 1960 int8 values
 *
 * TODO: Implement proper MFCC feature extraction
 * Hiện tại dùng simplified spectral features
 */
int runTFLiteInference(float* confidence) {
    if (interpreter == nullptr || input_tensor == nullptr) {
        return -1;
    }

    // Copy audio data lên linear buffer
    int16_t linearBuf[AUDIO_BUFFER_LEN];
    getLinearAudioBuffer(linearBuf);

    // ========================================
    // Feature Extraction: Simplified Spectrogram
    // ========================================
    // Model micro_speech expects 1960 int8 values (49 × 40)
    // Đây là simplified version – cho demo/test
    // Để accuracy cao, dùng Edge Impulse (Phase B) hoặc
    // implement full MFCC pipeline

    int input_size = input_tensor->bytes;
    int8_t* input_data = input_tensor->data.int8;

    // Simple energy-based features (49 frames of 320 samples each)
    int frame_step = AUDIO_BUFFER_LEN / 49;  // ~326 samples
    for (int frame = 0; frame < 49; frame++) {
        int frame_start = frame * frame_step;
        int frame_end = frame_start + frame_step;
        if (frame_end > AUDIO_BUFFER_LEN) frame_end = AUDIO_BUFFER_LEN;

        // Tính energy phổ cho mỗi frame (simplified 40 bins)
        for (int bin = 0; bin < 40; bin++) {
            float sum = 0;
            int bin_start = frame_start + (bin * frame_step / 40);
            int bin_end = frame_start + ((bin + 1) * frame_step / 40);
            for (int i = bin_start; i < bin_end && i < AUDIO_BUFFER_LEN; i++) {
                sum += abs(linearBuf[i]);
            }
            float energy = sum / (bin_end - bin_start + 1);

            // Quantize to int8 (-128 to 127)
            int idx = frame * 40 + bin;
            if (idx < input_size) {
                input_data[idx] = (int8_t)constrain(
                    (int)(energy / 128.0f) - 128, -128, 127);
            }
        }
    }

    // ========================================
    // Run Inference
    // ========================================
    unsigned long t_start = micros();
    TfLiteStatus invoke_status = interpreter->Invoke();
    unsigned long t_end = micros();

    if (invoke_status != kTfLiteOk) {
        Serial.println("[TFLite] LỖI: Invoke() failed!");
        return -1;
    }

    Serial.printf("[TFLite] Inference time: %lu µs\n", t_end - t_start);

    // ========================================
    // Parse Output
    // ========================================
    int8_t* output_data = output_tensor->data.int8;
    int best_label = 0;
    int8_t best_score = output_data[0];

    for (int i = 1; i < NUM_LABELS; i++) {
        if (output_data[i] > best_score) {
            best_score = output_data[i];
            best_label = i;
        }
    }

    // Dequantize score to float (0-1)
    float scale = output_tensor->params.scale;
    int zero_point = output_tensor->params.zero_point;
    *confidence = (best_score - zero_point) * scale;

    // Log kết quả
    Serial.printf("[TFLite] Result: %s (conf=%.2f) | scores: ",
                  LABEL_NAMES[best_label], *confidence);
    for (int i = 0; i < NUM_LABELS; i++) {
        float s = (output_data[i] - zero_point) * scale;
        Serial.printf("%s=%.2f ", LABEL_NAMES[i], s);
    }
    Serial.println();

    return best_label;
}

/*
 * Xử lý kết quả inference TFLite
 * "yes" → bật đèn
 * "no"  → tắt đèn
 */
void handleTFLiteResult() {
    float confidence;
    int label = runTFLiteInference(&confidence);

    if (label < 0 || confidence < CONFIDENCE_THRESHOLD) {
        return;  // Bỏ qua nếu lỗi hoặc confidence thấp
    }

    switch (label) {
        case LABEL_YES:
            Serial.printf("✅ 'YES' detected (%.0f%%) → BẬT ĐÈN\n", confidence * 100);
            setLight(true);
            break;
        case LABEL_NO:
            Serial.printf("❌ 'NO' detected (%.0f%%) → TẮT ĐÈN\n", confidence * 100);
            setLight(false);
            break;
        case LABEL_SILENCE:
        case LABEL_UNKNOWN:
        default:
            // Không làm gì
            break;
    }
}

#endif // USE_TFLITE_MODEL

// ==================== Edge Impulse Inference ====================
#ifdef USE_EDGE_IMPULSE

/*
 * Chạy Edge Impulse inference
 * Sử dụng thư viện export từ Edge Impulse Studio
 *
 * Workflow để có thư viện:
 * 1. Thu âm "bật đèn", "tắt đèn" (100+ mẫu/lệnh)
 * 2. Upload lên edgeimpulse.com
 * 3. Create Impulse: Audio → MFCC → Classifier
 * 4. Train → Deploy → Arduino Library
 * 5. Cài library vào Arduino IDE
 * 6. Bỏ comment #define USE_EDGE_IMPULSE trong config.h
 */
void handleEdgeImpulseResult() {
    // Copy audio lên linear buffer
    int16_t linearBuf[AUDIO_BUFFER_LEN];
    getLinearAudioBuffer(linearBuf);

    // Tạo signal từ audio buffer
    signal_t signal;
    signal.total_length = AUDIO_BUFFER_LEN;
    signal.get_data = [](size_t offset, size_t length, float *out_ptr) -> int {
        // Cannot capture linearBuf in static lambda - sử dụng global
        // Cần refactor nếu dùng Edge Impulse thực tế
        return EIDSP_OK;
    };

    // Tạo buffer cho kết quả
    ei_impulse_result_t result = {0};

    // Chạy classifier
    // EI_CLASSIFIER_RAW_SAMPLE_COUNT, EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME
    // được define bởi Edge Impulse library

    Serial.println("[EI] Running inference...");

    // TODO: Implement proper Edge Impulse inference call
    // run_classifier(&signal, &result, false);

    // Parse kết quả
    Serial.println("[EI] Predictions:");
    // for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    //     Serial.printf("  %s: %.5f\n", result.classification[ix].label,
    //                   result.classification[ix].value);
    //     // Map labels: "bat_den" → bật, "tat_den" → tắt
    //     if (strcmp(result.classification[ix].label, "bat_den") == 0 &&
    //         result.classification[ix].value > CONFIDENCE_THRESHOLD) {
    //         setLight(true);
    //     }
    //     if (strcmp(result.classification[ix].label, "tat_den") == 0 &&
    //         result.classification[ix].value > CONFIDENCE_THRESHOLD) {
    //         setLight(false);
    //     }
    // }
}

#endif // USE_EDGE_IMPULSE

// ==================== Serial Status ====================
/*
 * In thông tin trạng thái lên Serial Monitor
 * Format: Audio level meter + trạng thái đèn
 */
void printStatus() {
    // Audio level bar
    int bars = (int)(currentRMS / 100);
    if (bars > 30) bars = 30;

    Serial.print("[");
    for (int i = 0; i < 30; i++) {
        if (i < bars) Serial.print("█");
        else Serial.print("░");
    }
    Serial.printf("] RMS=%-6.0f | 💡%s",
                  currentRMS,
                  lightState ? "ON " : "OFF");

    #ifdef USE_TFLITE_MODEL
        Serial.print(" | Mode: TFLite");
    #elif defined(USE_EDGE_IMPULSE)
        Serial.print(" | Mode: Edge Impulse");
    #else
        Serial.print(" | Mode: VAD Test");
    #endif

    Serial.println();
}

// ==================== WiFi / MQTT ====================
#ifdef USE_MQTT

void wifiConnect() {
    Serial.print(F("[WiFi] Kết nối "));
    Serial.print(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 30) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print(F("\n[WiFi] OK – IP: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("\n[WiFi] Thất bại – kiểm tra SSID/Pass"));
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg(reinterpret_cast<char*>(payload), length);
    Serial.print(F("[MQTT] ← "));
    Serial.println(msg);
    if (msg == "ON")  setLight(true);
    else if (msg == "OFF") setLight(false);
}

void publishLightState() {
    if (!mqttClient.connected()) return;
    if (lightState == lastPublishedLight) return;
    lastPublishedLight = lightState;
    const char* state = lightState ? "ON" : "OFF";
    mqttClient.publish(MQTT_TOPIC_LIGHT_STATE, state, true);
    Serial.print(F("[MQTT] → light: "));
    Serial.println(state);
}

void mqttConnect() {
    if (mqttClient.connected()) return;
    Serial.print(F("[MQTT] Kết nối broker..."));
    bool ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS_WORD);
    if (ok) {
        mqttConnected = true;
        Serial.println(F(" OK"));
        mqttClient.subscribe(MQTT_TOPIC_LIGHT_CMD);
        lastPublishedLight = !lightState;   // reset guard → republish sau reconnect
        publishLightState();
    } else {
        mqttConnected = false;
        Serial.printf(" thất bại (rc=%d) – thử lại sau 5s\n", mqttClient.state());
    }
}

#endif // USE_MQTT

// ==================== SETUP ====================
void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Tắt brownout detector
    Serial.begin(115200);
    delay(1000);  // Chờ Serial ổn định
    printResetReason();  // In nguyên nhân reset

    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║   ESP32 AI Voice – Smart Home v2.0      ║");
    Serial.println("║   Bật/Tắt Đèn Bằng Giọng Nói           ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();

    // ---- GPIO Setup ----
    pinMode(LIGHT_PIN, OUTPUT);
    pinMode(TOUCH_PIN, INPUT);
    digitalWrite(LIGHT_PIN, LOW);
    Serial.printf("[GPIO] LED: GPIO%d, Touch: GPIO%d\n", LIGHT_PIN, TOUCH_PIN);

    // ---- WiFi + MQTT (nếu enabled) ----
    #ifdef USE_MQTT
        wifiConnect();
        mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
        mqttClient.setCallback(mqttCallback);
        mqttConnect();
    #endif

    // ---- I2S Microphone ----
    setupI2S();

    // ---- TFLite (nếu enabled) ----
    #ifdef USE_TFLITE_MODEL
        setupTFLite();
    #endif

    // ---- Mode Info ----
    #ifdef USE_TFLITE_MODEL
        Serial.println("[MODE] TFLite Micro Speech – Yes=BẬT, No=TẮT");
    #elif defined(USE_EDGE_IMPULSE)
        Serial.println("[MODE] Edge Impulse – Keyword Spotting Tiếng Việt");
    #else
        Serial.println("[MODE] Test VAD – Phát hiện giọng nói → Toggle đèn");
        Serial.println("[MODE] Chạm TTP223 để toggle (backup control)");
        Serial.printf("[MODE] VAD Threshold: RMS > %d\n", VAD_RMS_THRESHOLD);
    #endif

    Serial.println();
    Serial.println("🎤 Sẵn sàng! Hãy nói vào micro hoặc chạm cảm biến...");
    Serial.println("─────────────────────────────────────────────");
    Serial.println();

    // Khởi tạo audio buffer
    memset(audioBuffer, 0, sizeof(audioBuffer));
    audioWriteIdx = 0;
}

// ==================== MAIN LOOP ====================
/*
 * Vòng lặp chính:
 * 1. Đọc TTP223 touch sensor (backup, luôn hoạt động)
 * 2. Đọc chunk audio từ INMP441 (100ms, non-blocking-ish)
 * 3. Xử lý audio:
 *    - Test mode: VAD → toggle khi detect giọng nói
 *    - TFLite: inference mỗi 500ms → yes/no
 *    - Edge Impulse: inference → bật_đèn/tắt_đèn
 * 4. In status lên Serial
 */

unsigned long lastInferenceTime = 0;
unsigned long lastStatusPrint = 0;
int totalSamplesRead = 0;

void loop() {
    // ---- 0. MQTT keep-alive ----
    #ifdef USE_MQTT
    {
        unsigned long now = millis();
        if (!mqttClient.connected()) {
            if (now - lastMqttRetry >= MQTT_RETRY_MS) {
                lastMqttRetry = now;
                mqttConnect();
            }
        } else {
            mqttClient.loop();
        }
    }
    #endif

    // ---- 1. Touch Sensor (backup – luôn active) ----
    handleTouch();

    // ---- 2. Đọc audio chunk ----
    int samplesRead = readAudioChunk();
    totalSamplesRead += samplesRead;

    // ---- 3. Xử lý audio ----
    unsigned long now = millis();

    #if defined(USE_TFLITE_MODEL)
        // TFLite mode: chạy inference mỗi INFERENCE_INTERVAL_MS
        if (now - lastInferenceTime >= INFERENCE_INTERVAL_MS &&
            totalSamplesRead >= AUDIO_BUFFER_LEN) {
            handleTFLiteResult();
            lastInferenceTime = now;
        }

    #elif defined(USE_EDGE_IMPULSE)
        // Edge Impulse mode
        if (now - lastInferenceTime >= INFERENCE_INTERVAL_MS &&
            totalSamplesRead >= AUDIO_BUFFER_LEN) {
            handleEdgeImpulseResult();
            lastInferenceTime = now;
        }

    #else
        // Test mode: Voice Activity Detection
        if (samplesRead > 0) {
            handleVoiceDetection_Simple();
        }
    #endif

    // ---- 4. Print status (mỗi 500ms) ----
    if (now - lastStatusPrint >= 500) {
        // Tính RMS cho status display (nếu chưa tính)
        if (currentRMS == 0 && samplesRead > 0) {
            int startIdx = (audioWriteIdx - CHUNK_SAMPLES + AUDIO_BUFFER_LEN) % AUDIO_BUFFER_LEN;
            static int16_t chunk[CHUNK_SAMPLES];
            for (int i = 0; i < CHUNK_SAMPLES; i++) {
                chunk[i] = audioBuffer[(startIdx + i) % AUDIO_BUFFER_LEN];
            }
            currentRMS = computeRMS(chunk, CHUNK_SAMPLES);
        }
        printStatus();
        lastStatusPrint = now;
        currentRMS = 0;  // Reset cho lần đo tiếp
    }

    // Small delay để tránh watchdog timeout
    delay(1);
}