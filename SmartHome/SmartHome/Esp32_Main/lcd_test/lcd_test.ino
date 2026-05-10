/*
 * LCD I2C Test – thử cả 2 địa chỉ phổ biến
 */
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Thử 0x27 trước, nếu không lên đổi thành 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
    Serial.begin(115200);
    Wire.begin(0, 22);  // SDA=GPIO0, SCL=GPIO22
    delay(500);

    lcd.init();
    delay(100);
    lcd.backlight();
    delay(100);

    lcd.setCursor(0, 0);
    lcd.print("Smart Home VKU");
    lcd.setCursor(0, 1);
    lcd.print("LCD OK!");

    Serial.println("LCD init done");
}

void loop() {}
