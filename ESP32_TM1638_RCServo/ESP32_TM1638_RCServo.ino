/**
 * ============================================================================
 * Dự án: ESP32-S3 + TM1638 + LCD I2C + 1 ĐỘNG CƠ RC DIGITAL SERVO (50Hz PWM)
 * Nhánh: feature/rc-digital-servo
 * Tác giả: Nguyễn Văn Lân
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI CHÂN CHI TIẾT (ESP32-S3 DEVKITC-1):
 * 
 * 1. NGUỒN CẤP SERVO (Nguồn rời 5V/6V/7.4V - 3A+):
 *    - Dây Đỏ (VCC Servo)   -> +5V / +6V / +7.4V của Nguồn Rời
 *    - Dây Nâu/Đen (GND)    -> (-) GND Nguồn Rời + NỐI CHUNG VỚI GND ESP32-S3
 *    - Dây Cam/Vàng (Signal)-> GPIO 4 trên ESP32-S3
 * 
 * 2. MODULE TM1638:
 *    - STB -> GPIO 15, CLK -> GPIO 16, DIO -> GPIO 17, VCC -> 5V, GND -> GND
 * 
 * 3. LCD 1602 I2C:
 *    - SDA -> GPIO 8, SCL -> GPIO 9, VCC -> 5V, GND -> GND
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "TM1638_Driver.h"

// ================= CẤU HÌNH CHÂN PHẦN CỨNG =================
#define PIN_TM_STB    15
#define PIN_TM_CLK    16
#define PIN_TM_DIO    17

#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   9
#define LCD_COLS      16
#define LCD_ROWS      2

#define PIN_SERVO1    4

#define SERVO_FREQ_HZ 50
#define SERVO_RES_BITS 16
#define SERVO_MIN_US  500
#define SERVO_MAX_US  2500

// ================= KHỞI TẠO BIẾN TOÀN CỤC =================
TM1638_Driver tm(PIN_TM_STB, PIN_TM_CLK, PIN_TM_DIO);
LiquidCrystal_I2C* lcd = nullptr;
uint8_t currentLcdAddr = 0x27;

uint16_t servoAngle = 90; // Góc quay mặc định 90 độ
bool isSweepMode = false;
bool sweepDirUp = true;
uint32_t lastSweepTime = 0;
uint16_t sweepIntervalMs = 15; // Tốc độ quét mượt

// Phím bấm TM1638
uint8_t lastStableButtons = 0x00;
uint8_t rawButtons = 0x00;
uint32_t lastDebounceTime = 0;
const uint32_t DEBOUNCE_DELAY_MS = 25;

uint32_t buttonPressStartTime = 0;
uint32_t lastHoldTime = 0;

uint32_t lastLcdUpdate = 0;
uint32_t lastSerialReport = 0;
char rxBuffer[64];
uint8_t rxIndex = 0;

// ================= HÀM ĐẶT GÓC QUAY SERVO (PWM HARDWARE) =================
void writeServoAngle(uint16_t angle) {
    if (angle > 180) angle = 180;
    servoAngle = angle;

    uint32_t pulseUs = SERVO_MIN_US + ((uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US) / 180);
    // Tính duty cycle 16-bit (Period = 20000us)
    uint32_t duty = (pulseUs * 65535ULL) / 20000ULL;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_SERVO1, duty);
#else
    ledcWrite(0, duty);
#endif
}

// ================= HIỂN THỊ LCD & TM1638 =================
void updateLcdDisplay() {
    if (lcd == nullptr) return;

    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), "1x RC SERVO S1 ");
    snprintf(l2, sizeof(l2), "GOC:%3d\xDF  %s", servoAngle, isSweepMode ? "[SWEEP]" : "[MANUAL]");

    lcd->setCursor(0, 0);
    lcd->print(l1);
    lcd->setCursor(0, 1);
    lcd->print(l2);
}

void updateTM1638Display() {
    char dispStr[9];
    snprintf(dispStr, sizeof(dispStr), "S1- %3d\xDF", servoAngle);
    tm.setString(dispStr);

    // 8 LED đơn:
    // LED 1: Đèn báo chế độ SWEEP
    // LED 2..8: Thanh Mức Góc Quay (0 -> 180 độ)
    uint8_t ledMask = 0;
    if (isSweepMode) ledMask |= (1 << 0);

    uint8_t levelBars = (servoAngle * 7) / 180;
    for (uint8_t i = 0; i <= levelBars && i < 7; i++) {
        ledMask |= (1 << (1 + i));
    }

    tm.setLEDs(ledMask);
    tm.update();
}

uint8_t scanI2CAddress() {
    uint8_t foundAddr = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (foundAddr == 0) foundAddr = addr;
        }
    }
    return (foundAddr != 0) ? foundAddr : 0x27;
}

// ================= GIAO TIẾP SERIAL COMMAND =================
void sendFullStatusToGUI() {
    Serial.printf("SERVOSTAT:S1=%d,SWEEP=%d\n", servoAngle, isSweepMode ? 1 : 0);
}

void executeSerialCommand(const char* cmd) {
    if (strncmp(cmd, "SV1:", 4) == 0) {
        int deg = atoi(cmd + 4);
        writeServoAngle(deg);
        sendFullStatusToGUI();
    }
    else if (strncmp(cmd, "SERVO:ANGLE:", 12) == 0) {
        int deg = atoi(cmd + 12);
        writeServoAngle(deg);
        sendFullStatusToGUI();
    }
    else if (strncmp(cmd, "SERVO:SWEEP:", 12) == 0) {
        isSweepMode = (atoi(cmd + 12) == 1);
        sendFullStatusToGUI();
    }
    else if (strcmp(cmd, "SERVO:CENTER") == 0) {
        writeServoAngle(90);
        sendFullStatusToGUI();
    }
    else if (strcmp(cmd, "STATUS") == 0 || strcmp(cmd, "SYNC") == 0) {
        sendFullStatusToGUI();
    }
}

void handleSerial() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (rxIndex > 0) {
                rxBuffer[rxIndex] = '\0';
                executeSerialCommand(rxBuffer);
                rxIndex = 0;
            }
        } else {
            if (rxIndex < sizeof(rxBuffer) - 1) {
                rxBuffer[rxIndex++] = c;
            }
        }
    }
}

// ================= XỬ LÝ NÚT BẤM TM1638 =================
void handleButtonPress(uint8_t btnMask) {
    if (btnMask == 0) return;

    // S1: Tăng góc (+5°)
    if (btnMask & (1 << 0)) {
        writeServoAngle(servoAngle + 5);
    }
    // S2: Giảm góc (-5°)
    else if (btnMask & (1 << 1)) {
        if (servoAngle >= 5) writeServoAngle(servoAngle - 5);
        else writeServoAngle(0);
    }
    // S3: Đặt góc 0°
    else if (btnMask & (1 << 2)) {
        writeServoAngle(0);
    }
    // S4: Đặt góc 45°
    else if (btnMask & (1 << 3)) {
        writeServoAngle(45);
    }
    // S5: Đặt góc Trung tâm 90°
    else if (btnMask & (1 << 4)) {
        writeServoAngle(90);
    }
    // S6: Đặt góc 135°
    else if (btnMask & (1 << 5)) {
        writeServoAngle(135);
    }
    // S7: Đặt góc 180°
    else if (btnMask & (1 << 6)) {
        writeServoAngle(180);
    }
    // S8: Bật / Tắt chế độ tự động quét SWEEP (0° -> 180°)
    else if (btnMask & (1 << 7)) {
        isSweepMode = !isSweepMode;
    }
}

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== ESP32-S3 SINGLE RC DIGITAL SERVO CONTROLLER (SV1 - GPIO 4) ===");

    // 1. Cấu hình PWM Hardware LEDC cho 1 Servo RC (GPIO 4)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(PIN_SERVO1, SERVO_FREQ_HZ, SERVO_RES_BITS, 0);
#else
    ledcSetup(0, SERVO_FREQ_HZ, SERVO_RES_BITS);
    ledcAttachPin(PIN_SERVO1, 0);
#endif
    writeServoAngle(90); // Đưa Servo về góc 90 độ mặc định

    // 2. Khởi tạo bus I2C & LCD
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
    delay(50);

    currentLcdAddr = scanI2CAddress();
    lcd = new LiquidCrystal_I2C(currentLcdAddr, LCD_COLS, LCD_ROWS);
    lcd->init();
    lcd->backlight();
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("1x RC SERVO S1");
    lcd->setCursor(0, 1);
    lcd->print("SYSTEM READY");

    // 3. Khởi tạo TM1638
    tm.begin();
    tm.setBrightness(7);
    tm.testAll();
    delay(600);
    tm.clear();

    Serial.println("[SYSTEM] KHOI TAO THANH CONG 1 RC DIGITAL SERVO TAI GPIO 4!");
}

// ================= LOOP =================
void loop() {
    handleSerial();

    // Đọc phím TM1638
    uint8_t reading = tm.readButtons();
    if (reading != rawButtons) {
        rawButtons = reading;
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        if (rawButtons != lastStableButtons) {
            uint8_t pressed = rawButtons & ~lastStableButtons;
            lastStableButtons = rawButtons;

            if (pressed != 0) {
                buttonPressStartTime = millis();
                lastHoldTime = millis();
                handleButtonPress(pressed);
            }
        }
    }

    // Nhấn giữ S1 (+) hoặc S2 (-) để tăng/giảm góc từng 1 độ mượt mà
    if ((lastStableButtons & (1 << 0)) || (lastStableButtons & (1 << 1))) {
        uint32_t holdDuration = millis() - buttonPressStartTime;
        if (holdDuration >= 300) {
            if (millis() - lastHoldTime >= 30) { // Cập nhật mượt mỗi 30ms
                lastHoldTime = millis();
                if (lastStableButtons & (1 << 0)) {
                    if (servoAngle < 180) writeServoAngle(servoAngle + 1);
                } else if (lastStableButtons & (1 << 1)) {
                    if (servoAngle > 0) writeServoAngle(servoAngle - 1);
                }
            }
        }
    }

    // Chế độ tự động quét SWEEP (0° -> 180° -> 0°)
    if (isSweepMode && (millis() - lastSweepTime >= sweepIntervalMs)) {
        lastSweepTime = millis();
        if (sweepDirUp) {
            if (servoAngle < 180) writeServoAngle(servoAngle + 1);
            else sweepDirUp = false;
        } else {
            if (servoAngle > 0) writeServoAngle(servoAngle - 1);
            else sweepDirUp = true;
        }
    }

    // Cập nhật LCD & TM1638 định kỳ
    if (millis() - lastLcdUpdate >= 100) {
        lastLcdUpdate = millis();
        updateLcdDisplay();
        updateTM1638Display();
    }

    // Báo cáo Serial định kỳ
    if (millis() - lastSerialReport >= 300) {
        lastSerialReport = millis();
        sendFullStatusToGUI();
    }
}
