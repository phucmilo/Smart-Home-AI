# 🔌 Hướng Dẫn Nối Chân – ESP32 AI Giọng Nói

**Phiên bản:** 1.0 | **Ngày:** 30/03/2026 | **Dự án:** Smart Home VKU v2.0

---

## 📋 Danh Sách Linh Kiện Cần Thiết

| Linh Kiện | Số Lượng | Ghi Chú |
|---|---|---|
| ESP32 DevKit V1 | 1 | 38-pin, chip ESP32-WROOM-32 |
| INMP441 (I2S MEMS Mic) | 1 | Microphone thu âm giọng nói |
| MAX98357A | 1 | Khuếch đại âm thanh Class D I2S, 3W |
| Loa 4Ω/3W hoặc 8Ω/3W | 1 | Phản hồi âm thanh |
| Tụ điện 100μF/16V | 1 | Lọc nguồn MAX98357A |
| Tụ điện 470μF/16V | 1 | Lọc nguồn MAX98357A |
| LED đỏ 3mm + điện trở 330Ω | 1 | Cảnh báo |
| Dây Dupont M-F | ~20 sợi | Nối các module |
| Breadboard 830 lỗ | 1 | Prototype |

---

## 🎙️ Sơ Đồ Nối INMP441 → ESP32 (I2S0 – Microphone Input)

### Bảng Nối Chân

| INMP441 Pin | ESP32 GPIO | Mô Tả | Màu Dây Gợi Ý |
|---|---|---|---|
| **VDD** | **3.3V** | Nguồn 3.3V | Đỏ |
| **GND** | **GND** | Đất | Đen |
| **SCK** (BCLK) | **GPIO14** | I2S Bit Clock | Vàng |
| **WS** (L/R Select) | **GPIO15** | I2S Word Select | Xanh lá |
| **SD** (Data Out) | **GPIO32** | I2S Data từ Mic | Xanh dương |
| **L/R** | **GND** | Chọn kênh Left (Mono) | Đen |

### ⚠️ Lưu Ý INMP441

```
┌─────────────────────────────────────────────────────────────┐
│  QUAN TRỌNG: Chân L/R của INMP441 PHẢI nối GND              │
│  → Chọn kênh Left, dữ liệu sẽ xuất hiện trong phần LEFT     │
│    của I2S frame (16-bit stereo, left = data, right = 0)    │
│                                                             │
│  VDD chỉ dùng 3.3V – KHÔNG dùng 5V (phá hỏng chip)        │
│                                                             │
│  Kéo dài dây SCK/WS/SD tối đa ~30cm để tránh nhiễu         │
└─────────────────────────────────────────────────────────────┘
```

### Sơ Đồ Text

```
         INMP441                    ESP32 DevKit V1
        ┌────────┐                  ┌─────────────────┐
        │  VDD   ├──────────────────┤ 3.3V            │
        │  GND   ├──────────────────┤ GND             │
        │  L/R   ├──────────────────┤ GND             │
        │  SCK   ├──────────────────┤ GPIO14 (BCLK)   │
        │  WS    ├──────────────────┤ GPIO15 (WS/LRC) │
        │  SD    ├──────────────────┤ GPIO32 (DIN)    │
        └────────┘                  └─────────────────┘
```

---

## 🔊 Sơ Đồ Nối MAX98357A → ESP32 (I2S1 – Speaker Output)

### Bảng Nối Chân

| MAX98357A Pin | ESP32 GPIO | Mô Tả | Màu Dây Gợi Ý |
|---|---|---|---|
| **VIN** | **5V** | Nguồn 5V | Đỏ |
| **GND** | **GND** | Đất | Đen |
| **BCLK** | **GPIO26** | I2S Bit Clock | Vàng |
| **LRC** (WS) | **GPIO25** | I2S Word Select | Xanh lá |
| **DIN** | **GPIO27** | I2S Data vào Amp | Xanh dương |
| **GAIN** | *Để hở* | Gain mặc định 9dB | – |
| **SD** (Shutdown) | *Để hở / 3.3V* | Enable khuếch đại | – |
| **Out+** | **Loa (+)** | Ngõ ra loa dương | Cam |
| **Out-** | **Loa (-)** | Ngõ ra loa âm | Trắng |

### Mạch Lọc Nguồn (BẮT BUỘC)

```
5V ──┬── 100μF ──┬── MAX98357A VIN
     └── 470μF ──┘
                 │
                GND
```

> **Lý do:** MAX98357A Class D amp tạo dòng nhiễu spike khi chuyển mạch.
> Tụ lọc ngăn nhiễu lan sang ESP32 và các module khác.

### ⚠️ Lưu Ý MAX98357A

```
┌─────────────────────────────────────────────────────────────┐
│  VIN dùng 5V (không dùng 3.3V → âm thanh nhỏ và méo)       │
│                                                             │
│  PHẢI đặt tụ lọc 100μF + 470μF song song, sát chân VIN     │
│                                                             │
│  Chân GAIN để hở = 9dB (mặc định). Nối GND = 6dB,         │
│  nối 100kΩ lên 3.3V = 12dB                                  │
│                                                             │
│  Chân SD (Shutdown): để hở = enable; kéo xuống GND = tắt   │
│  → Có thể điều khiển shutdown qua GPIO để tiết kiệm điện   │
│                                                             │
│  Loa phải dùng 4Ω hoặc 8Ω – KHÔNG nối trực tiếp với nhau   │
│  vì MAX98357A là BTL (Bridge-Tied Load) – không có GND chung│
└─────────────────────────────────────────────────────────────┘
```

### Sơ Đồ Text

```
         MAX98357A                  ESP32 DevKit V1
        ┌──────────┐                ┌─────────────────┐
        │  VIN     ├──[100μF//470μF]┤ 5V              │
        │  GND     ├────────────────┤ GND             │
        │  BCLK    ├────────────────┤ GPIO26 (BCLK)   │
        │  LRC/WS  ├────────────────┤ GPIO25 (LRC/WS) │
        │  DIN     ├────────────────┤ GPIO27 (DOUT)   │
        │  GAIN    │ (để hở)        └─────────────────┘
        │  SD      │ (để hở)
        │  Out+    ├────────────────┐
        │  Out-    ├────────────────┤ Loa 4Ω/3W
        └──────────┘                └────────────────
```

---

## 💡 LED Trạng Thái

| LED | ESP32 GPIO | Điện Trở | Chức Năng |
|---|---|---|---|
| LED onboard (xanh) | **GPIO2** | Tích hợp | Trạng thái hệ thống |
| LED đỏ ngoài | **GPIO4** | **330Ω** | Cảnh báo / lỗi |

```
GPIO4 ──── 330Ω ──── LED(+) ──── LED(-) ──── GND
```

---

## 🗺️ Tổng Hợp GPIO Mapping

| GPIO | Chức Năng | Module | Giao Tiếp |
|---|---|---|---|
| GPIO14 | I2S0 BCLK (Mic) | INMP441 SCK | I2S Master |
| GPIO15 | I2S0 WS (Mic) | INMP441 WS | I2S Master |
| GPIO32 | I2S0 DIN (Mic) | INMP441 SD | I2S Master |
| GPIO26 | I2S1 BCLK (Loa) | MAX98357A BCLK | I2S Master |
| GPIO25 | I2S1 WS (Loa) | MAX98357A LRC | I2S Master |
| GPIO27 | I2S1 DOUT (Loa) | MAX98357A DIN | I2S Master |
| GPIO2 | LED onboard | LED xanh | Digital OUT |
| GPIO4 | LED cảnh báo | LED đỏ | Digital OUT |

---

## ⚡ Nguồn Điện

```
Adapter 5V/2A
     │
     ├──────────── ESP32 DevKit V1 (qua USB hoặc VIN)
     │                    │ (3.3V LDO onboard)
     │                    └──── INMP441 (3.3V)
     │
     └── [100μF // 470μF] ── MAX98357A VIN (5V riêng)
```

> **Lưu ý nguồn:**
> - ESP32 + INMP441: dùng USB 5V hoặc VIN pin
> - MAX98357A: nối thẳng vào nguồn 5V qua tụ lọc
> - Tổng dòng ước tính: ESP32 (~200mA) + MAX98357A peak (~600mA) = ~800mA → dùng adapter 5V/2A

---

## 🔧 Kiểm Tra Trước Khi Cấp Nguồn

- [ ] Đo 3.3V giữa VDD và GND của INMP441 → phải đo được 3.3V
- [ ] Đo 5V giữa VIN và GND của MAX98357A → phải đo được 5V
- [ ] Kiểm tra không có ngắn mạch: đo điện trở giữa 3.3V và GND (phải > 100Ω khi tắt nguồn)
- [ ] Kiểm tra dây SD INMP441 đúng chiều (Data từ mic VÀO ESP32, không phải ra)
- [ ] Kiểm tra chân L/R của INMP441 đã nối GND

---

## 📸 Sơ Đồ Tổng Thể (ASCII)

```
                    ┌─────────────────────────────────┐
                    │       ESP32 DevKit V1            │
                    │                                  │
  INMP441 ──SCK────►│ GPIO14                          │
  INMP441 ──WS─────►│ GPIO15    WiFi )))              │
  INMP441 ──SD─────►│ GPIO32                          │
                    │                                  │
  MAX98357A◄─BCLK──│ GPIO26                           │
  MAX98357A◄─LRC───│ GPIO25                           │
  MAX98357A◄─DIN───│ GPIO27                           │
                    │                                  │
  LED xanh ◄───────│ GPIO2                            │
  LED đỏ   ◄───────│ GPIO4                            │
                    └─────────────────────────────────┘
                              │ USB (nạp firmware)
                              ▼
                          PC/Laptop
```

---

*Tài liệu này thuộc dự án Smart Home VKU v2.0 – Phase 1.3 + Phase 4*
