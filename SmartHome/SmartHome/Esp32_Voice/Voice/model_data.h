/*
 * ============================================================
 *  model_data.h – TF Micro Speech Pre-trained Model
 *  Smart Home VKU v2.0 – Phase 4
 * ============================================================
 *  Pre-trained model nhận diện: "yes", "no", "silence", "unknown"
 *  Classes: [silence=0, unknown=1, yes=2, no=3]
 *
 *  Input:  1960 int8 values (49 MFCC frames × 40 coefficients)
 *  Output: 4 int8 scores
 *
 * ============================================================
 *  ⚠️  FILE NÀY LÀ PLACEHOLDER – CẦN THAY MODEL THỰC
 * ============================================================
 *
 *  CÁCH LẤY MODEL THỰC (chọn 1 trong 2 cách):
 *
 *  Cách 1 – Tải từ TFLite examples (Arduino):
 *    1. Cài library "Arduino_TensorFlowLite" hoặc "TensorFlowLite_ESP32"
 *       trong Arduino IDE (Library Manager)
 *    2. File → Examples → Harvard_TinyMLx → magic_wand
 *       (hoặc tìm example "micro_speech")
 *    3. Copy nội dung file model_data.h/model.h từ example đó vào đây
 *
 *  Cách 2 – Download trực tiếp (script tự động):
 *    # Chạy trong terminal:
 *    chmod +x tools/download_model.sh
 *    ./tools/download_model.sh
 *
 *  Cách 3 – Download thủ công:
 *    URL model (Google Drive / TF Hub):
 *    https://storage.googleapis.com/download.tensorflow.org/models/tflite/micro/micro_speech_nov_2021.zip
 *    → Giải nén → lấy file model_data.cc → copy array vào đây
 *
 *  Cách 4 – Dùng TF Lite Micro Arduino library có sẵn:
 *    Sau khi cài TensorFlowLite_ESP32, tìm file tại:
 *    ~/Arduino/libraries/TensorFlowLite_ESP32/src/tensorflow/lite/micro/examples/
 *    hoặc tìm kiếm "micro_speech_model_data" trên máy
 *
 * ============================================================
 *  Khi chưa có model thực, BẬT CHẾ ĐỘ TEST thay thế:
 *    Trong config.h: giữ nguyên (KHÔNG define USE_TFLITE_MODEL)
 *    → Chương trình chạy ở chế độ VAD (Voice Activity Detection)
 *    → Phát hiện TIẾNG NÓI → toggle đèn (không phân biệt yes/no)
 *    → Đủ để TEST PHẦN CỨNG INMP441 + I2S ngay lập tức
 * ============================================================
 */

#ifndef MODEL_DATA_H
#define MODEL_DATA_H

// ==================== PLACEHOLDER MODEL DATA ====================
// ⚠️ Đây là placeholder – inference sẽ KHÔNG chính xác
// Thay thế bằng model thực theo hướng dẫn ở trên

// Model size thực: ~18,000-20,000 bytes
// Đây chỉ là minimal stub để code compile được

alignas(8) const unsigned char g_model[] = {
    // TFLite Flatbuffer magic bytes (4 bytes)
    0x1C, 0x00, 0x00, 0x00,
    // Placeholder – model không hoạt động
    // THAY TOÀN BỘ NỘI DUNG NÀY bằng model thực
    0x54, 0x46, 0x4C, 0x33,  // "TFL3"
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

const int g_model_len = sizeof(g_model);

// ==================== Model Info (để tham khảo) ====================
// Model thực sau khi thay sẽ có các đặc điểm sau:
//   - Kiến trúc: DS-CNN (Depthwise Separable CNN)
//   - Input: [1, 49, 40, 1] int8 (49 frames × 40 MFCC coefficients)
//   - Output: [1, 4] int8 (scores cho 4 classes)
//   - Classes: silence(0), unknown(1), yes(2), no(3)
//   - Quantization: INT8, scale và zero_point trong tensor metadata
//   - Kích thước: ~18-20 KB
//   - Inference time: ~80-150 ms trên ESP32 @ 240MHz

#endif // MODEL_DATA_H
