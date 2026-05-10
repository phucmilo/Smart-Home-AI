# 🎙️ ESP32 Voice AI – Bật/Tắt Đèn Bằng Giọng Nói On-Device

**Ngày:** 11/04/2026 | **Dự án:** Smart Home VKU v2.0 – Phase 4  
**Phần cứng:** ESP32 DevKit V1 (WROOM-32) + INMP441 + MAX98357A + LED

---

## Bối cảnh

- ✅ Đã test thành công TTP223 + LED toggle (file `Voice.ino` hiện tại)
- Bước tiếp theo: **Tích hợp AI nhận diện giọng nói on-device** để bật/tắt đèn LED bằng lệnh tiếng nói
- Phần cứng: ESP32 WROOM-32 (không phải S3) → giới hạn lựa chọn model

---

## Nghiên cứu Model – So Sánh Các Phương Án

| Tiêu chí | TF Micro Speech | Edge Impulse (DS-CNN) | ESP-SR (WakeNet+MultiNet) | Voice Module ngoài |
|---|---|---|---|---|
| **Độ khó** | ⭐⭐ Trung bình | ⭐⭐ Trung bình | ⭐⭐⭐ Khó | ⭐ Dễ |
| **Cần train?** | ❌ Không (pre-trained) | ⚠️ Nhẹ (upload WAV + nhấn nút) | ❌ Không | ❌ Không |
| **Tiếng Việt?** | ❌ Chỉ "Yes"/"No" | ✅ **TÙY CHỈNH THOẢI MÁI** | ❌ Không hỗ trợ | ⚠️ Tùy module |
| **ESP32 WROOM?** | ✅ Chạy được | ✅ Chạy được | ❌ **Cần ESP32-S3** | ✅ Qua UART |
| **Model size** | ~20 KB | ~80-200 KB (INT8) | N/A | N/A |
| **Inference time** | ~100ms | ~80-150ms | N/A | N/A |
| **RAM cần** | ~60 KB | ~60-100 KB | N/A | N/A |
| **Phù hợp dự án** | 🟡 Test phần cứng | ✅ **SẢN PHẨM CUỐI** | ❌ Không tương thích | ❌ Không đúng mục tiêu AI |

### 🏆 Kết luận chọn Model

> [!IMPORTANT]
> **Chiến lược 2 giai đoạn:**
> 1. **Phase A – Test phần cứng:** Dùng **TF Micro Speech** (model "Yes"/"No" có sẵn, KHÔNG cần train) → xác nhận INMP441 + ESP32 I2S hoạt động → nói "Yes" = bật đèn, "No" = tắt đèn
> 2. **Phase B – Sản phẩm cuối:** Dùng **Edge Impulse DS-CNN** train lệnh tiếng Việt ("bật đèn", "tắt đèn") → deploy Arduino Library → thay thế model
>
> **Lý do:** Phase A cho phép test+demo NGAY mà không mất thời gian thu âm. Phase B bổ sung sau khi phần cứng đã ổn định.

---

## GPIO Mapping (theo WiringGuide đã có)

| GPIO | Chức năng | Module |
|---|---|---|
| GPIO14 | I2S0 BCLK (Mic) | INMP441 SCK |
| GPIO15 | I2S0 WS (Mic) | INMP441 WS |
| GPIO32 | I2S0 DIN (Mic) | INMP441 SD |
| GPIO26 | I2S1 BCLK (Loa) | MAX98357A BCLK |
| GPIO25 | I2S1 WS (Loa) | MAX98357A LRC |
| GPIO27 | I2S1 DOUT (Loa) | MAX98357A DIN |
| GPIO5 | LED Output | Đèn (giữ pinout từ test TTP223) |
| GPIO4 | TTP223 Input | Cảm biến chạm (backup control) |

---

## Proposed Changes

### Phase A – Test Phần Cứng với TF Micro Speech ("Yes"/"No" → Toggle LED)

> Mục tiêu: Xác nhận INMP441 thu âm + ESP32 inference + LED toggle bằng giọng nói hoạt động.

---

#### [MODIFY] [Voice.ino](file:///Users/vuhieunghia/Documents/SmartHome/Esp32_Voice/Voice.ino)

Thay đổi hoàn toàn file hiện tại từ chương trình TTP223 thành chương trình AI Voice.

**Cấu trúc code mới:**

```
Esp32_Voice/
├── Voice.ino            ← Main sketch: I2S capture + TFLite inference + LED control
├── model_data.h         ← TF Micro Speech model (pre-trained Yes/No, C array)
├── audio_provider.h/cpp ← I2S driver cho INMP441 (custom cho GPIO mapping)
├── feature_provider.h   ← MFCC feature extraction
└── config.h             ← Pin definitions, thresholds, WiFi/MQTT settings
```

**Flow chính:**

```
┌─────────────────────────────────────────────────────────────┐
│  1. I2S0 liên tục thu âm từ INMP441 (16kHz, 16-bit, mono) │
│  2. Buffer 1 giây audio → tính MFCC (40 coefficients)      │
│  3. Feed MFCC vào TFLite Interpreter → DS-CNN Inference     │
│  4. Output: "yes" / "no" / "silence" / "unknown"            │
│  5. Nếu "yes" (confidence > 0.75) → BẬT đèn GPIO5         │
│  6. Nếu "no"  (confidence > 0.75) → TẮT đèn GPIO5         │
│  7. Serial print kết quả để debug                           │
│  8. Giữ TTP223 GPIO4 backup → vẫn toggle bằng chạm         │
└─────────────────────────────────────────────────────────────┘
```

#### [NEW] [config.h](file:///Users/vuhieunghia/Documents/SmartHome/Esp32_Voice/config.h)

Cấu hình tập trung:
- Pin definitions (I2S, LED, Touch)
- Audio parameters (sample rate, buffer size)
- Inference threshold
- WiFi/MQTT credentials (chuẩn bị cho Phase C)

#### [NEW] [model_data.h](file:///Users/vuhieunghia/Documents/SmartHome/Esp32_Voice/model_data.h)

Pre-trained model TF Micro Speech dạng C array (`const unsigned char g_model[]`).
Model ~20KB, nhận diện 4 class: "yes", "no", "silence", "unknown".

#### [NEW] [audio_provider.cpp](file:///Users/vuhieunghia/Documents/SmartHome/Esp32_Voice/audio_provider.cpp)

Custom I2S driver cho INMP441 trên ESP32:
- I2S0 Master mode, RX only
- GPIO14 (BCLK), GPIO15 (WS), GPIO32 (DIN)
- Sample rate: 16000 Hz, 32-bit/sample (INMP441 output), convert to 16-bit
- DMA buffer: 2 × 1024 samples
- Channel: Mono Left (L/R pin → GND)

---

### Phase B – Edge Impulse DS-CNN Tiếng Việt (Sau khi Phase A thành công)

> [!NOTE]
> Phase B chỉ cần **thay file model** và **sửa label mapping** trong `Voice.ino`. Kiến trúc code giữ nguyên.

**Quy trình:**
1. Thu âm 100+ mẫu/lệnh: "bật đèn", "tắt đèn", noise, unknown
2. Upload lên Edge Impulse Studio → MFCC (40 coeff) → DS-CNN classifier
3. Train → Export Arduino Library (.zip) INT8
4. Thay `model_data.h` bằng model mới
5. Sửa label: `"bat_den"` → BẬT, `"tat_den"` → TẮT

#### [MODIFY] [model_data.h](file:///Users/vuhieunghia/Documents/SmartHome/Esp32_Voice/model_data.h)

Thay model TF Micro Speech bằng model Edge Impulse custom.

#### [MODIFY] [Voice.ino](file:///Users/vuhieunghia/Documents/SmartHome/Esp32_Voice/Voice.ino)

Sửa label mapping từ "yes"/"no" thành "bat_den"/"tat_den".

---

### Phase C – Tích hợp MQTT (Sau Phase A hoặc B)

#### [MODIFY] [Voice.ino](file:///Users/vuhieunghia/Documents/SmartHome/Esp32_Voice/Voice.ino)

Thêm:
- WiFi connection + reconnect
- MQTT client (PubSubClient)
- Publish lệnh: `home/voice/command` → `{"cmd":"bat_den","confidence":0.91}`
- Subscribe cảnh báo: `home/alert`

---

## Thư Viện Arduino Cần Cài

| Thư viện | Phiên bản | Mục đích |
|---|---|---|
| `TensorFlowLite_ESP32` | Latest | TFLite Micro interpreter |
| `ESP32 Board Core` | **2.0.x** (QUAN TRỌNG) | ESP32 Arduino core |
| `PubSubClient` | 2.8+ | MQTT client (Phase C) |
| `ArduinoJson` | 6.x | JSON payload MQTT (Phase C) |
| `WiFi` | Built-in | WiFi connection (Phase C) |

> [!WARNING]
> **PHẢI dùng ESP32 Board Core version 2.0.x** (ví dụ 2.0.17). Version 3.x đã thay đổi I2S API cũ (`driver/i2s.h` → `driver/i2s_std.h`) và có thể gây lỗi compilation với TFLite library. Kiểm tra trong Arduino IDE: `Tools → Board → Board Manager → esp32 by Espressif → chọn 2.0.17`.

---

## User Review Required

> [!IMPORTANT]
> **Câu hỏi cần xác nhận trước khi bắt đầu:**
>
> 1. **Phần cứng đã nối xong chưa?** Bạn đã có INMP441 + MAX98357A nối theo WiringGuide chưa? Hay hiện tại chỉ có ESP32 + TTP223 + LED?
>
> 2. **Bắt đầu từ Phase A (Yes/No tiếng Anh)?** Hay muốn nhảy thẳng sang Phase B (Edge Impulse tiếng Việt)?
>    - Phase A: Code ngay, test ngay (chỉ cần INMP441 + ESP32)
>    - Phase B: Cần thu âm trước (~3-4 giờ), train trên Edge Impulse (~30 phút)
>
> 3. **MAX98357A có sẵn không?** Nếu chưa có loa, có thể bỏ qua phần phản hồi âm thanh, chỉ dùng Serial + LED feedback.
>
> 4. **LED output giữ GPIO5?** Hay muốn đổi sang pin khác? (Hiện tại Voice.ino đang dùng GPIO5 cho LED)

---

## Verification Plan

### Phase A – Test TF Micro Speech

#### Automated Tests
1. **Compilation test:** `arduino-cli compile --fqbn esp32:esp32:esp32 Esp32_Voice/` → build thành công
2. **Upload test:** Nạp firmware qua USB → Serial Monitor hiển thị "Voice AI Ready"

#### Manual Verification
1. **I2S test:** Nói vào mic → Serial Monitor hiển thị amplitude (không phải 0 hoặc giá trị cố định)
2. **Inference test:** Nói "Yes" → Serial hiển thị `Detected: YES (conf=0.XX)` + LED bật
3. **Inference test:** Nói "No" → Serial hiển thị `Detected: NO (conf=0.XX)` + LED tắt
4. **Noise rejection:** Im lặng / tiếng ồn → Serial hiển thị `silence` hoặc `unknown`, LED không thay đổi
5. **TTP223 backup:** Chạm cảm biến → LED toggle (chức năng cũ vẫn hoạt động)

### Phase B – Test Edge Impulse Model

1. Nói "bật đèn" → LED bật + Serial log
2. Nói "tắt đèn" → LED tắt + Serial log
3. Accuracy test: 20 lần nói mỗi lệnh → đếm nhận đúng → mục tiêu ≥ 80%

### Phase C – Test MQTT Integration

1. Nói lệnh → MQTT message xuất hiện trên MQTT Explorer
2. ESP32 Main nhận message → relay đèn hoạt động

---

## Timeline Ước Tính

| Giai đoạn | Thời gian | Deliverable |
|---|---|---|
| Phase A: Code + Test | 2-3 giờ | ESP32 nhận "Yes"/"No" → toggle LED |
| Phase B: Thu âm + Train | 4-6 giờ | Model tiếng Việt "bật đèn"/"tắt đèn" |
| Phase C: MQTT | 1-2 giờ | Tích hợp vào hệ thống SmartHome |

---

*Dựa trên nghiên cứu: TF Micro Speech (Google), Edge Impulse DS-CNN, ESP-SR Docs, WiringGuide_ESP32_Voice.md*
