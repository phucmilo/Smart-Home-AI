# Hướng Dẫn Nối Chân – ESP32 Main (Khóa Cửa Thông Minh)

**Phiên bản:** 1.0 | **Ngày:** 13/04/2026 | **Dự án:** Smart Home VKU v2.0

---

## Linh Kiện Cần Thiết

| Linh Kiện | Số Lượng | Ghi Chú |
|---|---|---|
| ESP32 DevKit V1 | 1 | 38-pin, WROOM-32 |
| RFID RC522 | 1 | Kèm thẻ và móc khóa |
| Bàn phím số 4×4 | 1 | Màng dẻo, 8 dây |
| Servo SG90 / MG90S | 1 | SG90 đủ dùng, MG90S tốt hơn cho tải thực |
| LED xanh lá 3mm | 1 | + điện trở 330Ω |
| LED đỏ 3mm | 1 | + điện trở 330Ω |
| Buzzer 5V (active) | 1 | Active buzzer (có oscillator bên trong) |
| Điện trở 330Ω | 2 | Cho 2 LED |
| Dây Dupont M-F | ~25 sợi | |
| Breadboard 830 lỗ | 1 | |

---

## RC522 RFID → ESP32 (SPI – VSPI)

| RC522 Pin | ESP32 GPIO | Ghi Chú |
|---|---|---|
| **3.3V** | **3.3V** | KHÔNG dùng 5V – phá hỏng chip |
| **GND** | **GND** | |
| **SCK** | **GPIO18** | VSPI CLK |
| **MOSI** | **GPIO23** | VSPI MOSI |
| **MISO** | **GPIO19** | VSPI MISO |
| **SDA (SS)** | **GPIO5** | Chip Select |
| **RST** | **GPIO4** | Reset |
| **IRQ** | *Không nối* | Không cần (polling mode) |

```
         RC522                     ESP32 DevKit V1
        ┌──────┐                   ┌────────────────┐
        │ 3.3V ├───────────────────┤ 3.3V           │
        │ GND  ├───────────────────┤ GND            │
        │ SCK  ├───────────────────┤ GPIO18 (SCK)   │
        │ MOSI ├───────────────────┤ GPIO23 (MOSI)  │
        │ MISO ├───────────────────┤ GPIO19 (MISO)  │
        │ SDA  ├───────────────────┤ GPIO5  (SS)    │
        │ RST  ├───────────────────┤ GPIO4  (RST)   │
        └──────┘                   └────────────────┘
```

> **Lưu ý:** RC522 hoạt động ở 3.3V. Dùng 5V sẽ hỏng chip ngay lập tức.

---

## Bàn Phím 4×4 → ESP32

Bàn phím 8 dây: 4 hàng (ROW) + 4 cột (COL).

| Bàn phím Dây | ESP32 GPIO | Vai Trò |
|---|---|---|
| **ROW1** (hàng 1: 1,2,3,A) | **GPIO13** | |
| **ROW2** (hàng 2: 4,5,6,B) | **GPIO12** | |
| **ROW3** (hàng 3: 7,8,9,C) | **GPIO14** | |
| **ROW4** (hàng 4: *,0,#,D) | **GPIO27** | |
| **COL1** (cột 1: 1,4,7,*)  | **GPIO26** | |
| **COL2** (cột 2: 2,5,8,0)  | **GPIO25** | |
| **COL3** (cột 3: 3,6,9,#)  | **GPIO33** | |
| **COL4** (cột 4: A,B,C,D)  | **GPIO32** | |

```
Layout bàn phím:
  ┌───┬───┬───┬───┐
  │ 1 │ 2 │ 3 │ A │  ← ROW1 (GPIO13)
  ├───┼───┼───┼───┤
  │ 4 │ 5 │ 6 │ B │  ← ROW2 (GPIO12)
  ├───┼───┼───┼───┤
  │ 7 │ 8 │ 9 │ C │  ← ROW3 (GPIO14)
  ├───┼───┼───┼───┤
  │ * │ 0 │ # │ D │  ← ROW4 (GPIO27)
  └───┴───┴───┴───┘
    ↑   ↑   ↑   ↑
  COL1 COL2 COL3 COL4
  G26  G25  G33  G32

  # = Xác nhận PIN
  * = Xóa/Hủy
```

---

## Servo → ESP32

| Servo Dây | ESP32 | Ghi Chú |
|---|---|---|
| **Đỏ** (VCC) | **5V** | Servo cần 5V – lấy từ VIN pin hoặc nguồn ngoài |
| **Nâu/Đen** (GND) | **GND** | |
| **Cam/Vàng** (Signal) | **GPIO2** | PWM điều khiển góc |

```
SG90/MG90S                   ESP32 DevKit V1
  ┌──────┐                   ┌────────────────┐
  │  5V  ├───────────────────┤ VIN (5V)       │
  │  GND ├───────────────────┤ GND            │
  │  SIG ├───────────────────┤ GPIO2 (PWM)    │
  └──────┘                   └────────────────┘
```

> **Lưu ý servo:**
> - **SG90** (nhựa): Phù hợp demo, torque ~1.8 kg·cm
> - **MG90S** (bánh răng kim loại): Khuyến nghị cho cửa thực, torque ~2.2 kg·cm
> - Nếu servo bị rung khi đứng yên: code đã có `doorServo.detach()` sau khi xoay xong
> - Servo lấy điện từ VIN (5V từ USB). Nếu chạy bằng adapter nối VIN, cần nguồn ≥ 1A

---

## LEDs + Buzzer → ESP32

| Linh Kiện | ESP32 GPIO | Mạch |
|---|---|---|
| **LED Xanh** (cửa mở) | **GPIO15** | GPIO15 → 330Ω → LED(+) → LED(−) → GND |
| **LED Đỏ**  (cửa khóa) | **GPIO16** | GPIO16 → 330Ω → LED(+) → LED(−) → GND |
| **Buzzer** | **GPIO17** | GPIO17 → Buzzer(+) → Buzzer(−) → GND |

```
GPIO15 ──── 330Ω ──── LED_xanh(+) ──── LED_xanh(−) ──── GND
GPIO16 ──── 330Ω ──── LED_đỏ(+)   ──── LED_đỏ(−)   ──── GND
GPIO17 ──────────────  Buzzer(+)   ──── Buzzer(−)   ──── GND
```

---

## Tổng Hợp GPIO Mapping

| GPIO | Chức Năng | Module | Giao Tiếp |
|---|---|---|---|
| GPIO18 | SPI CLK | RC522 SCK | SPI VSPI |
| GPIO19 | SPI MISO | RC522 MISO | SPI VSPI |
| GPIO23 | SPI MOSI | RC522 MOSI | SPI VSPI |
| GPIO5 | SPI SS | RC522 SDA | SPI VSPI |
| GPIO4 | RFID Reset | RC522 RST | Digital OUT |
| GPIO13 | Keypad ROW1 | Bàn phím | Digital I/O |
| GPIO12 | Keypad ROW2 | Bàn phím | Digital I/O |
| GPIO14 | Keypad ROW3 | Bàn phím | Digital I/O |
| GPIO27 | Keypad ROW4 | Bàn phím | Digital I/O |
| GPIO26 | Keypad COL1 | Bàn phím | Digital I/O |
| GPIO25 | Keypad COL2 | Bàn phím | Digital I/O |
| GPIO33 | Keypad COL3 | Bàn phím | Digital I/O |
| GPIO32 | Keypad COL4 | Bàn phím | Digital I/O |
| GPIO2 | Servo PWM | Servo SG90 | PWM OUT |
| GPIO15 | LED Xanh | LED | Digital OUT |
| GPIO16 | LED Đỏ | LED | Digital OUT |
| GPIO17 | Buzzer | Buzzer | Digital OUT |

---

## Sơ Đồ Tổng Thể (ASCII)

```
                ┌──────────────────────────────────────┐
                │         ESP32 DevKit V1               │
                │                                       │
  RC522 SCK ───►│ GPIO18                               │
  RC522 MOSI───►│ GPIO23   WiFi )))  MQTT Broker        │
  RC522 MISO◄───│ GPIO19                               │
  RC522 SS  ───►│ GPIO5                                │
  RC522 RST ───►│ GPIO4                                │
                │                                       │
  Keypad R1 ───►│ GPIO13                               │
  Keypad R2 ───►│ GPIO12                               │
  Keypad R3 ───►│ GPIO14                               │
  Keypad R4 ───►│ GPIO27                               │
  Keypad C1 ◄──►│ GPIO26                               │
  Keypad C2 ◄──►│ GPIO25                               │
  Keypad C3 ◄──►│ GPIO33                               │
  Keypad C4 ◄──►│ GPIO32                               │
                │                                       │
  Servo SIG ◄───│ GPIO2  (PWM)                         │
                │                                       │
  LED Xanh  ◄───│ GPIO15 → 330Ω → LED                  │
  LED Đỏ    ◄───│ GPIO16 → 330Ω → LED                  │
  Buzzer    ◄───│ GPIO17                               │
                └──────────────────────────────────────┘
                          │ USB
                          ▼ Arduino IDE / arduino-cli
```

---

## Nguồn Điện

```
USB 5V/1A (hoặc Adapter 5V/1A qua VIN)
     │
     ├─── ESP32 DevKit V1 (VIN/USB)
     │         │ (3.3V LDO onboard)
     │         ├─── RC522    (3.3V, ~13mA)
     │         └─── Bàn phím (3.3V qua pull-up nội, ~1mA)
     │
     └─── Servo SG90 (5V, ~200mA idle / ~700mA stall)
          [Nối VIN của ESP32 DevKit – dùng chung bus 5V]
```

> Tổng dòng tối đa ≈ ESP32(200mA) + Servo stall(700mA) + LEDs(20mA×2) = ~940mA  
> → Dùng nguồn **5V/2A** để đảm bảo ổn định

---

## Checklist Trước Khi Cấp Nguồn

- [ ] RC522 nối 3.3V (không phải 5V)
- [ ] SPI đúng thứ tự: MOSI→23, MISO→19, SCK→18, SS→5
- [ ] Servo Signal nối GPIO2 (không phải VCC)
- [ ] Điện trở 330Ω trước mỗi LED
- [ ] Buzzer đúng cực (+/−)
- [ ] Bàn phím ROW/COL không bị đảo ngược

---

## Cách Đọc UID Thẻ RFID

1. Upload firmware lên ESP32 Main
2. Mở **Serial Monitor**, baud **115200**
3. Quét thẻ/móc khóa RFID
4. Serial in ra: `[RFID] Phát hiện thẻ: A1 B2 C3 D4`
5. Copy UID đó vào `config.h` → mảng `AUTHORIZED_UIDS`
6. Upload lại firmware

---

*Tài liệu này thuộc dự án Smart Home VKU v2.0 – ESP32 Main Module*
