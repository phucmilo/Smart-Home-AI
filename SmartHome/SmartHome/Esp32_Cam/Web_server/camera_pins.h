#pragma once
// ============================================
// camera_pins.h – Pin mapping AI Thinker ESP32-CAM
// Board: AI Thinker ESP32-CAM v1.5
// Camera: OV2640 2MP
// PSRAM: 4MB (BẮT BUỘC enabled)
// ============================================
//
// Lưu ý: Các GPIO dưới đây được dùng bởi camera module.
// KHÔNG sử dụng các pin này cho mục đích khác!
// GPIO còn trống cho user: GPIO12, GPIO13, GPIO14, GPIO15 (chia sẻ với SD card)
// GPIO2 cũng có thể dùng (kết nối với LED_STATUS nội bộ qua GPIO33)

#define CAMERA_MODEL_AI_THINKER

// ===== Camera Data Pins (D0-D7) =====
#define Y2_GPIO_NUM        5
#define Y3_GPIO_NUM       18
#define Y4_GPIO_NUM       19
#define Y5_GPIO_NUM       21
#define Y6_GPIO_NUM       36
#define Y7_GPIO_NUM       39
#define Y8_GPIO_NUM       34
#define Y9_GPIO_NUM       35

// ===== Camera Control Pins =====
#define XCLK_GPIO_NUM      0    // Clock output to camera
#define PCLK_GPIO_NUM     22    // Pixel clock from camera
#define VSYNC_GPIO_NUM    25    // Vertical sync
#define HREF_GPIO_NUM     23    // Horizontal reference

// ===== SCCB (I2C-like) for camera config =====
#define SIOD_GPIO_NUM     26    // SDA
#define SIOC_GPIO_NUM     27    // SCL

// ===== Power & Reset =====
#define PWDN_GPIO_NUM     32    // Power down (active high)
#define RESET_GPIO_NUM    -1    // Reset (-1 = not connected)

// ===== Flash LED =====
#define LED_GPIO_NUM       4    // Bright white flash LED
