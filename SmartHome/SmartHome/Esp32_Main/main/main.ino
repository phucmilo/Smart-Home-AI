/*
 * ============================================================
 * ESP32 Main – Điều khiển servo qua Home Assistant (MQTT)
 * Smart Home VKU v2.0
 *
 * Flow:
 *   HA nhấn "Mở"  → MQTT publish "UNLOCK" → servo 90°
 *   HA nhấn "Khóa"→ MQTT publish "LOCK"   → servo 0°
 *   Sau AUTO_LOCK_MS giây → tự khóa lại
 *   ESP32 publish trạng thái → HA cập nhật icon realtime
 *
 * Thư viện cần cài (Arduino Library Manager):
 *   - ESP32Servo   (Kevin Harrington)  v0.13+
 *   - PubSubClient (Nick O'Leary)      v2.8+
 *
 * Board: ESP32 Dev Module | Core: 2.0.x | Baud: 115200
 * ============================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <Adafruit_Fingerprint.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

// Keypad layout
static const char KP_KEYS[KP_ROWS][KP_COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};
static byte kpRowPins[KP_ROWS] = {KP_R1, KP_R2, KP_R3, KP_R4};
static byte kpColPins[KP_COLS] = {KP_C1, KP_C2, KP_C3, KP_C4};
Keypad keypad = Keypad(makeKeymap(KP_KEYS), kpRowPins, kpColPins, KP_ROWS, KP_COLS);

// ============================================================
// Đối tượng toàn cục
// ============================================================
Servo        doorServo;
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

HardwareSerial       fpSerial(2);
Adafruit_Fingerprint finger(&fpSerial);

MFRC522            rfid(RFID_SS_PIN, RFID_RST_PIN);
Preferences        prefs;
LiquidCrystal_I2C  lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

bool          isDoorOpen        = false;
bool          lastPublishedState = true;   // khác isDoorOpen → đảm bảo publish lần đầu
unsigned long unlockStart        = 0;
unsigned long lastMqttRetry      = 0;

// ── Touch state ───────────────────────────────────────────────
bool lastTouchState = false;

// ── RFID state ────────────────────────────────────────────────
bool rfidAddNext = false;

// ── Keypad state ──────────────────────────────────────────────
String kpBuffer = "";

// ── Gas + LCD + Buzzer state ──────────────────────────────────
unsigned long lastLcdUpdate  = 0;
unsigned long setupTime      = 0;
int           lastGasRaw     = 0;
#define WARMUP_MS 30000   // MQ cần 30s ổn định sau khi cấp điện


// ── Fingerprint state ─────────────────────────────────────────
enum EnrollState { ENROLL_IDLE, ENROLL_WAIT_FIRST, ENROLL_WAIT_REMOVE, ENROLL_WAIT_SECOND };
EnrollState   enrollState     = ENROLL_IDLE;
uint8_t       enrollId        = 0;
unsigned long enrollStepStart = 0;
unsigned long lastFpCheck     = 0;

// ============================================================
// Khai báo hàm
// ============================================================
void wifiConnect();
void mqttConnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void openDoor();
void closeDoor();
void servoMove(int angle);
void publishState();
void publishFpStatus(const char* status);
void fpEnrollStart(uint8_t id);
void fpHandleEnroll();
void fpCheckFinger();
void handleTouch();
void rfidCheck();
bool rfidIsAllowed(String uid);
bool rfidAddCard(String uid);
void rfidRemoveCard(String uid);
String rfidGetUID();
void handleKeypad();
String kpGetPassword();
void kpSetPassword(String p);
void updateLCD();
void updateBuzzer();

// ============================================================
// Fingerprint
// ============================================================
void publishFpStatus(const char* status) {
    if (!mqtt.connected()) return;
    mqtt.publish(TOPIC_FP_STATUS, status, false);
    Serial.print(F("[FP] → "));
    Serial.println(status);
}

void fpEnrollStart(uint8_t id) {
    enrollId        = id;
    enrollState     = ENROLL_WAIT_FIRST;
    enrollStepStart = millis();
    publishFpStatus("place_finger");
    Serial.printf("[FP] Bắt đầu đăng ký ID #%d\n", id);
}

void fpHandleEnroll() {
    unsigned long now = millis();

    if (now - enrollStepStart > FP_ENROLL_TIMEOUT) {
        publishFpStatus("fail:timeout");
        enrollState = ENROLL_IDLE;
        Serial.println(F("[FP] Timeout – huỷ đăng ký"));
        return;
    }

    switch (enrollState) {
        case ENROLL_WAIT_FIRST: {
            if (finger.getImage() != FINGERPRINT_OK) return;
            if (finger.image2Tz(1) != FINGERPRINT_OK) {
                publishFpStatus("fail:convert1");
                enrollState = ENROLL_IDLE;
                return;
            }
            publishFpStatus("remove_finger");
            enrollState     = ENROLL_WAIT_REMOVE;
            enrollStepStart = now;
            break;
        }
        case ENROLL_WAIT_REMOVE: {
            if (finger.getImage() == FINGERPRINT_NOFINGER) {
                publishFpStatus("place_again");
                enrollState     = ENROLL_WAIT_SECOND;
                enrollStepStart = now;
            }
            break;
        }
        case ENROLL_WAIT_SECOND: {
            if (finger.getImage() != FINGERPRINT_OK) return;
            if (finger.image2Tz(2) != FINGERPRINT_OK) {
                publishFpStatus("fail:convert2");
                enrollState = ENROLL_IDLE;
                return;
            }
            if (finger.createModel() != FINGERPRINT_OK) {
                publishFpStatus("fail:model");
                enrollState = ENROLL_IDLE;
                return;
            }
            char buf[20];
            if (finger.storeModel(enrollId) == FINGERPRINT_OK) {
                snprintf(buf, sizeof(buf), "ok:%d", enrollId);
                Serial.printf("[FP] Đăng ký thành công ID #%d\n", enrollId);
            } else {
                snprintf(buf, sizeof(buf), "fail:store");
            }
            publishFpStatus(buf);
            enrollState = ENROLL_IDLE;
            break;
        }
        default: break;
    }
}

void fpCheckFinger() {
    if (finger.getImage()    != FINGERPRINT_OK) return;
    if (finger.image2Tz(1)   != FINGERPRINT_OK) return;
    if (finger.fingerSearch() != FINGERPRINT_OK) return;

    Serial.printf("[FP] Khớp ID #%d (conf=%d) → Mở cửa\n",
                  finger.fingerID, finger.confidence);
    if (!isDoorOpen) openDoor();
}

// ============================================================
// RFID RC522
// ============================================================
String rfidGetUID() {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (i > 0) uid += ":";
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid;
}

bool rfidIsAllowed(String uid) {
    prefs.begin("rfid", true);
    int count = prefs.getInt("count", 0);
    bool found = false;
    for (int i = 0; i < count && !found; i++) {
        if (prefs.getString(("uid" + String(i)).c_str(), "") == uid)
            found = true;
    }
    prefs.end();
    return found;
}

bool rfidAddCard(String uid) {
    if (rfidIsAllowed(uid)) return false;
    prefs.begin("rfid", false);
    int count = prefs.getInt("count", 0);
    if (count >= 20) { prefs.end(); return false; }
    prefs.putString(("uid" + String(count)).c_str(), uid);
    prefs.putInt("count", count + 1);
    prefs.end();
    return true;
}

void rfidRemoveCard(String uid) {
    prefs.begin("rfid", false);
    int count = prefs.getInt("count", 0);
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (prefs.getString(("uid" + String(i)).c_str(), "") == uid) {
            found = i; break;
        }
    }
    if (found >= 0) {
        for (int i = found; i < count - 1; i++) {
            String next = prefs.getString(("uid" + String(i + 1)).c_str(), "");
            prefs.putString(("uid" + String(i)).c_str(), next);
        }
        prefs.remove(("uid" + String(count - 1)).c_str());
        prefs.putInt("count", count - 1);
    }
    prefs.end();
}

void rfidCheck() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

    String uid = rfidGetUID();
    char buf[64];

    if (rfidAddNext) {
        rfidAddNext = false;
        if (rfidAddCard(uid)) {
            snprintf(buf, sizeof(buf), "added:%s", uid.c_str());
            Serial.printf("[RFID] Đã thêm: %s\n", uid.c_str());
        } else {
            snprintf(buf, sizeof(buf), "already:%s", uid.c_str());
        }
    } else if (rfidIsAllowed(uid)) {
        snprintf(buf, sizeof(buf), "ok:%s", uid.c_str());
        Serial.printf("[RFID] Hợp lệ: %s → Mở cửa\n", uid.c_str());
        if (!isDoorOpen) openDoor();
    } else {
        snprintf(buf, sizeof(buf), "denied:%s", uid.c_str());
        Serial.printf("[RFID] Từ chối: %s\n", uid.c_str());
    }

    mqtt.publish(TOPIC_RFID_SCAN, buf, false);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}

// ============================================================
// Gas Sensor + LCD
// ============================================================
void updateLCD() {
    unsigned long now = millis();
    lcd.clear();

    // Warmup 30 giây sau khi bật
    if (now - setupTime < WARMUP_MS) {
        int remaining = (WARMUP_MS - (now - setupTime)) / 1000;
        lcd.setCursor(0, 0);
        lcd.print("Khoi dong sensor");
        lcd.setCursor(0, 1);
        lcd.print("Warmup: ");
        lcd.print(remaining);
        lcd.print("s    ");
        return;
    }

    int raw = analogRead(GAS_PIN);  // 0–4095
    lastGasRaw = raw;

    // Publish lên MQTT
    if (mqtt.connected()) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", raw);
        mqtt.publish(TOPIC_GAS, buf, true);
    }

    // Dòng 1: giá trị khí gas
    lcd.setCursor(0, 0);
    lcd.print("Khi gas: ");
    lcd.print(raw);
    lcd.print("    ");

    // Dòng 2: trạng thái
    lcd.setCursor(0, 1);
    if (raw >= GAS_DANGER_LEVEL) {
        lcd.print("!!! NGUY HIEM !!!");
    } else if (raw >= GAS_WARN_LEVEL) {
        lcd.print("* CANH BAO *    ");
    } else {
        lcd.print("Binh thuong     ");
    }

    Serial.printf("[GAS] ADC=%d\n", raw);
}

void updateBuzzer() {
    // Warmup: tắt buzzer
    if (millis() - setupTime < WARMUP_MS) {
        digitalWrite(BUZZER_PIN, LOW);
        return;
    }
    if (lastGasRaw >= GAS_DANGER_LEVEL) {
        // Nguy hiểm: kêu liên tục
        digitalWrite(BUZZER_PIN, HIGH);
    } else if (lastGasRaw >= GAS_WARN_LEVEL) {
        // Cảnh báo: beep mỗi 500ms
        digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
    } else {
        digitalWrite(BUZZER_PIN, LOW);
    }
}

// ============================================================
// Keypad 4x4
// ============================================================
String kpGetPassword() {
    prefs.begin("door", true);
    String p = prefs.getString("pass", KP_DEFAULT_PASS);
    prefs.end();
    return p;
}

void kpSetPassword(String p) {
    prefs.begin("door", false);
    prefs.putString("pass", p);
    prefs.end();
}

void handleKeypad() {
    char key = keypad.getKey();
    if (!key) return;

    Serial.printf("[KP] Phím: %c\n", key);

    if (key == '*') {
        kpBuffer = "";
        mqtt.publish(TOPIC_KP_STATUS, "clear", false);
        return;
    }

    if (key == '#') {
        if (kpBuffer.length() == 0) return;
        if (kpBuffer == kpGetPassword()) {
            Serial.println(F("[KP] Đúng mật khẩu → Mở cửa"));
            mqtt.publish(TOPIC_KP_STATUS, "ok", false);
            if (!isDoorOpen) openDoor();
        } else {
            Serial.println(F("[KP] Sai mật khẩu"));
            mqtt.publish(TOPIC_KP_STATUS, "wrong", false);
        }
        kpBuffer = "";
        return;
    }

    // Chỉ nhận chữ số
    if (key >= '0' && key <= '9') {
        if (kpBuffer.length() < KP_MAX_LEN)
            kpBuffer += key;
    }
}

// ============================================================
// Touch TTP223
// ============================================================
void handleTouch() {
    bool current = digitalRead(TOUCH_PIN);
    if (current == HIGH && lastTouchState == LOW) {
        Serial.println(F("[TOUCH] Chạm → toggle cửa"));
        if (isDoorOpen) closeDoor();
        else            openDoor();
        delay(50); // debounce
    }
    lastTouchState = current;
}

// ============================================================
// setup()
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("\n=== ESP32 Door Servo – Home Assistant ==="));

    // LCD I2C
    Wire.begin(LCD_SDA, LCD_SCL);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0); lcd.print("Smart Home VKU");
    lcd.setCursor(0, 1); lcd.print("Dang khoi dong..");
    Serial.println(F("[LCD] OK"));

    // Buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // MQ Gas Sensor
    pinMode(GAS_PIN, INPUT);
    setupTime = millis();
    Serial.println(F("[GAS] MQ sensor khoi dong – warmup 30s"));

    // Touch TTP223
    pinMode(TOUCH_PIN, INPUT);

    // Servo về vị trí khóa khi khởi động
    servoMove(SERVO_LOCKED_ANGLE);
    Serial.println(F("[SERVO] Vị trí khởi động: LOCKED (0°)"));

    wifiConnect();

    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqttConnect();

    // RFID RC522
    SPI.begin();
    rfid.PCD_Init();
    Serial.println(F("[RFID] RC522 OK"));
    prefs.begin("rfid", true);
    Serial.printf("[RFID] Thẻ đã đăng ký: %d\n", prefs.getInt("count", 0));
    prefs.end();

    // Fingerprint DY50
    fpSerial.begin(FP_BAUD, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);
    finger.begin(FP_BAUD);
    if (finger.verifyPassword()) {
        Serial.println(F("[FP] DY50 OK"));
        Serial.printf("[FP] Số vân tay đã lưu: %d\n", finger.templateCount);
    } else {
        Serial.println(F("[FP] DY50 không tìm thấy – kiểm tra dây"));
    }

    Serial.println(F("[SYSTEM] Sẵn sàng – chờ lệnh từ Home Assistant / vân tay"));
}

// ============================================================
// loop()
// ============================================================
void loop() {
    unsigned long now = millis();

    if (!mqtt.connected()) {
        if (now - lastMqttRetry >= MQTT_RETRY_MS) {
            lastMqttRetry = now;
            mqttConnect();
        }
    }
    mqtt.loop();

    // Touch TTP223
    handleTouch();

    // RFID
    rfidCheck();

    // Keypad
    handleKeypad();

    // Gas + LCD (cập nhật mỗi 1 giây)
    if (now - lastLcdUpdate >= 1000) {
        lastLcdUpdate = now;
        updateLCD();
    }

    // Buzzer (kiểm tra liên tục để beep chính xác)
    updateBuzzer();


    // Fingerprint: đăng ký hoặc kiểm tra mỗi 100ms
    if (enrollState != ENROLL_IDLE) {
        fpHandleEnroll();
    } else if (now - lastFpCheck >= 100) {
        lastFpCheck = now;
        fpCheckFinger();
    }

#if AUTO_LOCK_MS > 0
    if (isDoorOpen && (now - unlockStart >= AUTO_LOCK_MS)) {
        Serial.println(F("[DOOR] Tự khóa lại"));
        closeDoor();
    }
#endif
}

// ============================================================
// Điều khiển cửa
// ============================================================
void openDoor() {
    isDoorOpen  = true;
    unlockStart = millis();
    servoMove(SERVO_UNLOCKED_ANGLE);
    publishState();
    Serial.println(F("[DOOR] MỞ (90°)"));
}

void closeDoor() {
    isDoorOpen = false;
    servoMove(SERVO_LOCKED_ANGLE);
    publishState();
    Serial.println(F("[DOOR] KHÓA (0°)"));
}

void servoMove(int angle) {
    if (!doorServo.attached()) doorServo.attach(SERVO_PIN);
    doorServo.write(angle);
    delay(SERVO_MOVE_DELAY_MS);
    doorServo.detach();   // tắt PWM → servo không rung khi giữ tải
}

// ============================================================
// MQTT
// ============================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg(reinterpret_cast<char*>(payload), length);
    String t(topic);

    Serial.print(F("[MQTT] ← "));
    Serial.print(t); Serial.print(": ");
    Serial.println(msg);

    if (t == TOPIC_CMD) {
        if (msg == CMD_UNLOCK) { if (!isDoorOpen) openDoor(); }
        else if (msg == CMD_LOCK) { if (isDoorOpen)  closeDoor(); }

    } else if (t == TOPIC_FP_ENROLL) {
        uint8_t id = (uint8_t)msg.toInt();
        if (id >= 1 && id <= 127 && enrollState == ENROLL_IDLE)
            fpEnrollStart(id);
        else
            publishFpStatus("fail:busy_or_bad_id");

    } else if (t == TOPIC_KP_CMD) {
        if (msg.startsWith("setpass:")) {
            String newPass = msg.substring(8);
            if (newPass.length() >= 4 && newPass.length() <= KP_MAX_LEN) {
                kpSetPassword(newPass);
                mqtt.publish(TOPIC_KP_STATUS, "changed", false);
                Serial.println(F("[KP] Mật khẩu đã đổi"));
            } else {
                mqtt.publish(TOPIC_KP_STATUS, "fail:length", false);
            }
        }

    } else if (t == TOPIC_RFID_CMD) {
        if (msg == "add_next") {
            rfidAddNext = true;
            Serial.println(F("[RFID] Chế độ thêm thẻ – quét thẻ mới"));
        } else if (msg.startsWith("remove:")) {
            String uid = msg.substring(7);
            rfidRemoveCard(uid);
            char buf[64];
            snprintf(buf, sizeof(buf), "removed:%s", uid.c_str());
            mqtt.publish(TOPIC_RFID_SCAN, buf, false);
        }

    } else if (t == TOPIC_FP_DELETE) {
        uint8_t id = (uint8_t)msg.toInt();
        char buf[20];
        if (finger.deleteModel(id) == FINGERPRINT_OK)
            snprintf(buf, sizeof(buf), "deleted:%d", id);
        else
            snprintf(buf, sizeof(buf), "fail:delete");
        publishFpStatus(buf);
    }
}

void publishState() {
    if (!mqtt.connected()) return;
    if (isDoorOpen == lastPublishedState) return;
    lastPublishedState = isDoorOpen;

    const char* state = isDoorOpen ? STATE_UNLOCKED : STATE_LOCKED;
    mqtt.publish(TOPIC_STATE, state, true);
    Serial.print(F("[MQTT] → state: "));
    Serial.println(state);
}

void mqttConnect() {
    if (mqtt.connected()) return;
    Serial.print(F("[MQTT] Kết nối broker..."));

    bool ok = mqtt.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS_WORD);
    if (ok) {
        Serial.println(F(" OK"));
        mqtt.subscribe(TOPIC_CMD);
        mqtt.subscribe(TOPIC_FP_ENROLL);
        mqtt.subscribe(TOPIC_FP_DELETE);
        mqtt.subscribe(TOPIC_RFID_CMD);
        mqtt.subscribe(TOPIC_KP_CMD);
        lastPublishedState = !isDoorOpen;
        publishState();
    } else {
        Serial.printf(" thất bại (rc=%d) – thử lại sau 5s\n", mqtt.state());
    }
}

// ============================================================
// WiFi
// ============================================================
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
