/**
 * ============================================================================
 * Dự án: ESP32-S3 + TM1638 + LCD I2C + 4 ĐỘNG CƠ RC DIGITAL SERVO (50Hz PWM)
 * Nhánh: feature/rc-digital-servo
 * Tác giả: Nguyễn Văn Lân
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI CHÂN CHI TIẾT (ESP32-S3 DEVKITC-1):
 * 
 * 1. NGUỒN CẤP SERVO (Nguồn rời 5V/6V/7.4V - 3A+):
 *    - VCC Servo (+5V đến +7.4V) -> Chân Đỏ (VCC) của 4 Servo
 *    - GND Servo                 -> Chân Đen/Nâu (GND) của 4 Servo + NỐI CHUNG MASS VỚI ESP32 GND
 * 
 * 2. ĐỘNG CƠ SERVO (CHÂN TÍN HIỆU CAM / VÀNG):
 *    - Servo 1 (SV1) -> GPIO 4
 *    - Servo 2 (SV2) -> GPIO 5
 *    - Servo 3 (SV3) -> GPIO 6
 *    - Servo 4 (SV4) -> GPIO 7
 * 
 * 3. MODULE TM1638:
 *    - STB -> GPIO 15, CLK -> GPIO 16, DIO -> GPIO 17, VCC -> 5V, GND -> GND
 * 
 * 4. LCD 1602 / 2004 I2C:
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

#define PIN_SV1       4
#define PIN_SV2       5
#define PIN_SV3       6
#define PIN_SV4       7

#define SERVO_FREQ_HZ 50
#define SERVO_RES_BITS 16
#define SERVO_MIN_US  500
#define SERVO_MAX_US  2500

enum SelectedServoTarget {
    TARGET_SV1  = 0,
    TARGET_SV2  = 1,
    TARGET_SV3  = 2,
    TARGET_SV4  = 3,
    TARGET_ALL  = 4
};

// ================= KHỞI TẠO BIẾN TOÀN CỤC =================
TM1638_Driver tm(PIN_TM_STB, PIN_TM_CLK, PIN_TM_DIO);
LiquidCrystal_I2C* lcd = nullptr;
uint8_t currentLcdAddr = 0x27;

const uint8_t servoPins[4] = { PIN_SV1, PIN_SV2, PIN_SV3, PIN_SV4 };
uint16_t servoAngles[4] = { 90, 90, 90, 90 };
SelectedServoTarget selectedTarget = TARGET_SV1;

bool isSweepMode = false;
bool sweepDirUp = true;
uint32_t lastSweepTime = 0;
uint16_t sweepIntervalMs = 20;

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
void writeServoAngle(uint8_t idx, uint16_t angle) {
    if (idx >= 4) return;
    if (angle > 180) angle = 180;
    servoAngles[idx] = angle;

    uint32_t pulseUs = SERVO_MIN_US + ((uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US) / 180);
    // Tính duty cycle 16-bit (Period = 20000us)
    uint32_t duty = (pulseUs * 65535ULL) / 20000ULL;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(servoPins[idx], duty);
#else
    ledcWrite(idx, duty);
#endif
}

// ================= HIỂN THỊ LCD & TM1638 =================
void updateLcdDisplay() {
    if (lcd == nullptr) return;

    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), "1%c%3d\xDF 2%c%3d\xDF",
             (selectedTarget == TARGET_SV1 || selectedTarget == TARGET_ALL) ? '*' : ':',
             servoAngles[0],
             (selectedTarget == TARGET_SV2 || selectedTarget == TARGET_ALL) ? '*' : ':',
             servoAngles[1]);

    snprintf(l2, sizeof(l2), "3%c%3d\xDF 4%c%3d\xDF %s",
             (selectedTarget == TARGET_SV3 || selectedTarget == TARGET_ALL) ? '*' : ':',
             servoAngles[2],
             (selectedTarget == TARGET_SV4 || selectedTarget == TARGET_ALL) ? '*' : ':',
             servoAngles[3],
             isSweepMode ? "SWP" : "   ");

    lcd->setCursor(0, 0);
    lcd->print(l1);
    lcd->setCursor(0, 1);
    lcd->print(l2);
}

void updateTM1638Display() {
    char dispStr[9];
    if (selectedTarget < TARGET_ALL) {
        uint8_t idx = (uint8_t)selectedTarget;
        snprintf(dispStr, sizeof(dispStr), "%d- %3d-P", idx + 1, servoAngles[idx]);
    } else {
        snprintf(dispStr, sizeof(dispStr), "ALL- 4SV");
    }
    tm.setString(dispStr);

    uint8_t ledMask = 0;
    if (selectedTarget < TARGET_ALL) {
        ledMask |= (1 << (4 + (uint8_t)selectedTarget));
    } else {
        ledMask |= 0xF0;
    }
    if (isSweepMode) ledMask |= 0x0F;

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

// ================= GIAO TIẾP SERIAL =================
void sendFullStatusToGUI() {
    Serial.printf("SERVOSTAT:S1=%d,S2=%d,S3=%d,S4=%d,SEL=%d,SWEEP=%d\n",
                  servoAngles[0], servoAngles[1], servoAngles[2], servoAngles[3],
                  (int)selectedTarget, isSweepMode ? 1 : 0);
}

void executeSerialCommand(const char* cmd) {
    // Servo 1 -> 4
    for (int i = 0; i < 4; i++) {
        char prefix[8];
        snprintf(prefix, sizeof(prefix), "SV%d:", i + 1);
        if (strncmp(cmd, prefix, 4) == 0) {
            int deg = atoi(cmd + 4);
            writeServoAngle(i, deg);
            sendFullStatusToGUI();
            return;
        }
    }

    if (strncmp(cmd, "SERVO:SEL:", 10) == 0) {
        selectedTarget = (SelectedServoTarget)atoi(cmd + 10);
        sendFullStatusToGUI();
    }
    else if (strncmp(cmd, "SERVO:SWEEP:", 12) == 0) {
        isSweepMode = (atoi(cmd + 12) == 1);
        sendFullStatusToGUI();
    }
    else if (strcmp(cmd, "SERVO:CENTER_ALL") == 0) {
        for (int i = 0; i < 4; i++) writeServoAngle(i, 90);
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
        if (selectedTarget < TARGET_ALL) {
            uint8_t idx = (uint8_t)selectedTarget;
            writeServoAngle(idx, servoAngles[idx] + 5);
        } else {
            for (int i = 0; i < 4; i++) writeServoAngle(i, servoAngles[i] + 5);
        }
    }
    // S2: Giảm góc (-5°)
    else if (btnMask & (1 << 1)) {
        if (selectedTarget < TARGET_ALL) {
            uint8_t idx = (uint8_t)selectedTarget;
            if (servoAngles[idx] >= 5) writeServoAngle(idx, servoAngles[idx] - 5);
            else writeServoAngle(idx, 0);
        } else {
            for (int i = 0; i < 4; i++) {
                if (servoAngles[i] >= 5) writeServoAngle(i, servoAngles[i] - 5);
                else writeServoAngle(i, 0);
            }
        }
    }
    // S3: Chọn Servo (SV1 -> SV2 -> SV3 -> SV4 -> ALL)
    else if (btnMask & (1 << 2)) {
        selectedTarget = (SelectedServoTarget)((selectedTarget + 1) % 5);
    }
    // S4: Về vị trí Trung tâm (90°)
    else if (btnMask & (1 << 3)) {
        if (selectedTarget < TARGET_ALL) {
            writeServoAngle((uint8_t)selectedTarget, 90);
        } else {
            for (int i = 0; i < 4; i++) writeServoAngle(i, 90);
        }
    }
    // S5: Bật / Tắt chế độ quét tự động SWEEP
    else if (btnMask & (1 << 4)) {
        isSweepMode = !isSweepMode;
    }
    // S6: Đặt nhanh góc 0°
    else if (btnMask & (1 << 5)) {
        if (selectedTarget < TARGET_ALL) writeServoAngle((uint8_t)selectedTarget, 0);
        else for (int i = 0; i < 4; i++) writeServoAngle(i, 0);
    }
    // S7: Đặt nhanh góc 90°
    else if (btnMask & (1 << 6)) {
        if (selectedTarget < TARGET_ALL) writeServoAngle((uint8_t)selectedTarget, 90);
        else for (int i = 0; i < 4; i++) writeServoAngle(i, 90);
    }
    // S8: Đặt nhanh góc 180°
    else if (btnMask & (1 << 7)) {
        if (selectedTarget < TARGET_ALL) writeServoAngle((uint8_t)selectedTarget, 180);
        else for (int i = 0; i < 4; i++) writeServoAngle(i, 180);
    }
}

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== ESP32-S3 QUAD RC DIGITAL SERVO CONTROLLER ===");

    // 1. Cấu hình PWM Hardware LEDC cho 4 Servo RC
    for (uint8_t i = 0; i < 4; i++) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcAttachChannel(servoPins[i], SERVO_FREQ_HZ, SERVO_RES_BITS, i);
#else
        ledcSetup(i, SERVO_FREQ_HZ, SERVO_RES_BITS);
        ledcAttachPin(servoPins[i], i);
#endif
        writeServoAngle(i, 90);
    }

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
    lcd->print("RC SERVO READY");
    lcd->setCursor(0, 1);
    lcd->print("4 SERVOS ACTIVE");

    // 3. Khởi tạo TM1638
    tm.begin();
    tm.setBrightness(7);
    tm.testAll();
    delay(600);
    tm.clear();

    Serial.println("[SYSTEM] KHOI TAO THANH CONG 4 RC DIGITAL SERVO!");
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

    // Nhấn giữ S1 (+) hoặc S2 (-) để tăng/giảm góc liên tục
    if ((lastStableButtons & (1 << 0)) || (lastStableButtons & (1 << 1))) {
        uint32_t holdDuration = millis() - buttonPressStartTime;
        if (holdDuration >= 300) {
            if (millis() - lastHoldTime >= 40) {
                lastHoldTime = millis();
                if (lastStableButtons & (1 << 0)) {
                    if (selectedTarget < TARGET_ALL) writeServoAngle((uint8_t)selectedTarget, servoAngles[(uint8_t)selectedTarget] + 1);
                    else for (int i = 0; i < 4; i++) writeServoAngle(i, servoAngles[i] + 1);
                } else if (lastStableButtons & (1 << 1)) {
                    if (selectedTarget < TARGET_ALL) {
                        uint8_t idx = (uint8_t)selectedTarget;
                        if (servoAngles[idx] > 0) writeServoAngle(idx, servoAngles[idx] - 1);
                    } else {
                        for (int i = 0; i < 4; i++) if (servoAngles[i] > 0) writeServoAngle(i, servoAngles[i] - 1);
                    }
                }
            }
        }
    }

    // Chế độ tự động quét SWEEP
    if (isSweepMode && (millis() - lastSweepTime >= sweepIntervalMs)) {
        lastSweepTime = millis();
        uint16_t curAng = servoAngles[0];
        if (sweepDirUp) {
            if (curAng < 180) curAng++;
            else sweepDirUp = false;
        } else {
            if (curAng > 0) curAng--;
            else sweepDirUp = true;
        }
        for (int i = 0; i < 4; i++) writeServoAngle(i, curAng);
    }

    // Cập nhật LCD & TM1638 định kỳ
    if (millis() - lastLcdUpdate >= 150) {
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
