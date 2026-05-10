# Task: ESP32 Voice AI – Bật/Tắt Đèn Bằng Giọng Nói

## Phase A – Test Phần Cứng + AI Cơ Bản
- [x] Tạo `config.h` – pin definitions, audio params
- [x] Tạo `Voice.ino` – I2S capture + AI inference + LED + TTP223 backup
- [x] Tạo `model_data.h` – Pre-trained TF Micro Speech model (placeholder, cần model thực)
- [x] Tạo `tools/download_model.sh` – Script tải model từ Google Storage
- [x] Test compilation trong Arduino IDE (300KB/22% flash, 53KB/16% RAM – OK)
- [ ] **[NEXT]** Verify I2S audio capture (Serial Monitor)
- [ ] Verify AI inference (Yes → bật, No → tắt)
- [ ] Verify TTP223 backup control

### Hướng dẫn chạy thử ngay (không cần model):
1. Mở `Esp32_Voice/Voice.ino` trong Arduino IDE
2. Đảm bảo `config.h` KHÔNG define `USE_TFLITE_MODEL` (dòng đó đang comment)
3. Chọn Board: ESP32 Dev Module, Core **2.0.x**
4. Upload → mở Serial Monitor 115200 baud
5. Nói vào mic → LED toggle (chế độ VAD test)
6. Chạm TTP223 → LED toggle (backup)

### Để bật chế độ TFLite (sau khi có model thực):
1. Chạy: `./tools/download_model.sh` → tạo `model_data.h` đầy đủ
2. Cài thư viện `TensorFlowLite_ESP32` (Arduino Library Manager)
3. Trong `config.h`: bỏ comment `#define USE_TFLITE_MODEL`
4. Upload lại → nói "Yes"/"No"

## Phase B – Edge Impulse Tiếng Việt (sau Phase A)
- [ ] Tạo tài khoản Edge Impulse
- [ ] Thu âm "bật đèn", "tắt đèn" (100+ mẫu/lệnh)
- [ ] Train DS-CNN model trên Edge Impulse
- [ ] Export Arduino Library → cài vào project
- [ ] Update Voice.ino labels
- [ ] Test accuracy ≥ 80%

## Phase C – MQTT Integration (sau Phase A hoặc B)
- [ ] Thêm WiFi connection
- [ ] Thêm MQTT client
- [ ] Publish `home/voice/command`
- [ ] Test end-to-end với ESP32 Main
