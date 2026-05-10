/*
 * I2C Scanner – Tìm địa chỉ LCD I2C
 * Nạp sketch này, mở Serial Monitor 115200 baud
 */
#include <Wire.h>

void setup() {
    Serial.begin(115200);
    Wire.begin(0, 22);  // SDA=GPIO0, SCL=GPIO22
    Serial.println("\n=== I2C Scanner ===");
}

void loop() {
    int found = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Tìm thấy thiết bị tại: 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0) Serial.println("Không tìm thấy thiết bị nào!");
    else            Serial.printf("Tổng: %d thiết bị\n", found);
    Serial.println("-------------------");
    delay(3000);
}
