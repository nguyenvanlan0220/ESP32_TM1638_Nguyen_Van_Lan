/**
 * ============================================================================
 * Dự án: ESP32-S3 + TM1638 + LCD I2C + 2 DRIVER HYBRID SERVO JMC 2HSS57
 * Động cơ: 2 Động cơ bước servo lai (YAKO / NEMA23)
 * Tác giả: Nguyễn Văn Lân
 * Nhánh: feature/dual-motor-driver
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI CHÂN (PINOUT CHI TIẾT 2 ĐỘNG CƠ):
 * 
 * 1. ĐỘNG CƠ 1 (MOTOR 1 - M1):
 *    Driver JMC 2HSS57 (1)    ESP32-S3
 *    ---------------------------------
 *    PUL1+ (Pulse)      ->   GPIO 4
 *    DIR1+ (Dir)        ->   GPIO 5
 *    ENA1+ (Enable)     ->   GPIO 6
 *    PUL1-, DIR1-, ENA1->   GND (Nối chung mass ESP32)
 * 
 * 2. ĐỘNG CƠ 2 (MOTOR 2 - M2):
 *    Driver JMC 2HSS57 (2)    ESP32-S3
 *    ---------------------------------
 *    PUL2+ (Pulse)      ->   GPIO 18
 *    DIR2+ (Dir)        ->   GPIO 19
 *    ENA2+ (Enable)     ->   GPIO 20
 *    PUL2-, DIR2-, ENA2->   GND (Nối chung mass ESP32)
 * 
 * 3. MODULE TM1638:
 *    STB -> GPIO 15, CLK -> GPIO 16, DIO -> GPIO 17, VCC -> 5V, GND -> GND
 * 
 * 4. LCD 1602 / 2004 I2C:
 *    SDA -> GPIO 8, SCL -> GPIO 9, VCC -> 5V, GND -> GND
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "TM1638_Driver.h"

// ================= CẤU HÌNH CHÂN PHẦN CỨNG =================
// TM1638
#define PIN_TM_STB    15
#define PIN_TM_CLK    16
#define PIN_TM_DIO    17

// I2C LCD
#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   9
#define LCD_COLS      16
#define LCD_ROWS      2

// Driver Động cơ 1 (M1)
#define PIN_M1_PUL    4
#define PIN_M1_DIR    5
#define PIN_M1_ENA    6

// Driver Động cơ 2 (M2)
#define PIN_M2_PUL    18
#define PIN_M2_DIR    19
#define PIN_M2_ENA    20

// ================= THÔNG SỐ ĐỘNG CƠ =================
const uint32_t STEPS_PER_REV = 1600;
const uint32_t MIN_SPEED_SPS = 200;
const uint32_t MAX_SPEED_SPS = 16000;
const uint32_t SPEED_STEP_SPS = 400;

enum MotorMode {
    MODE_CONTINUOUS = 0,
    MODE_POSITION   = 1,
    MODE_JOG        = 2
};

enum SelectedMotorTarget {
    TARGET_M1   = 0,
    TARGET_M2   = 1,
    TARGET_BOTH = 2
};

// ================= KHỞI TẠO BIẾN TOÀN CỤC 2 ĐỘNG CƠ =================
TM1638_Driver tm(PIN_TM_STB, PIN_TM_CLK, PIN_TM_DIO);
LiquidCrystal_I2C* lcd = nullptr;
uint8_t currentLcdAddr = 0x27;

// Động cơ 1 (M1) State
volatile bool isRunning1 = false;
volatile bool isDirCW1 = true;
volatile bool isEnabled1 = true;
volatile uint32_t speedSPS1 = 1600;
volatile int64_t currentPos1 = 0;
volatile int64_t targetPos1 = 0;
volatile bool pulseState1 = false;

// Động cơ 2 (M2) State
volatile bool isRunning2 = false;
volatile bool isDirCW2 = true;
volatile bool isEnabled2 = true;
volatile uint32_t speedSPS2 = 1600;
volatile int64_t currentPos2 = 0;
volatile int64_t targetPos2 = 0;
volatile bool pulseState2 = false;

// Chế độ chung & mục tiêu đang chọn
MotorMode currentMode = MODE_CONTINUOUS;
SelectedMotorTarget selectedTarget = TARGET_M1;

// Timer ngắt phần cứng cho 2 động cơ
hw_timer_t* timerM1 = nullptr;
hw_timer_t* timerM2 = nullptr;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux2 = portMUX_INITIALIZER_UNLOCKED;

// Biến phím bấm & chống rung
uint8_t lastStableButtons = 0x00;
uint8_t rawButtons = 0x00;
uint32_t lastDebounceTime = 0;
const uint32_t DEBOUNCE_DELAY_MS = 25;

uint32_t buttonPressStartTime = 0;
uint32_t lastSpeedHoldTime = 0;
const uint32_t HOLD_INITIAL_DELAY_MS = 300;
const uint32_t HOLD_REPEAT_INTERVAL_MS = 65;

uint32_t lastLcdUpdate = 0;
uint32_t lastSerialReport = 0;
char rxBuffer[64];
uint8_t rxIndex = 0;

// ================= HÀM NGẮT TIMER PHÁT XUNG M1 (TIMER 0) =================
void IRAM_ATTR onTimerM1() {
    portENTER_CRITICAL_ISR(&timerMux1);
    
    if (isRunning1 && isEnabled1) {
        if (!pulseState1) {
            if (currentMode == MODE_POSITION) {
                if (currentPos1 < targetPos1) {
                    digitalWrite(PIN_M1_DIR, HIGH);
                    digitalWrite(PIN_M1_PUL, HIGH);
                    pulseState1 = true;
                } else if (currentPos1 > targetPos1) {
                    digitalWrite(PIN_M1_DIR, LOW);
                    digitalWrite(PIN_M1_PUL, HIGH);
                    pulseState1 = true;
                } else {
                    isRunning1 = false;
                    digitalWrite(PIN_M1_PUL, LOW);
                    pulseState1 = false;
                }
            } else {
                digitalWrite(PIN_M1_DIR, isDirCW1 ? HIGH : LOW);
                digitalWrite(PIN_M1_PUL, HIGH);
                pulseState1 = true;
            }
        } else {
            digitalWrite(PIN_M1_PUL, LOW);
            pulseState1 = false;

            if (currentMode == MODE_POSITION) {
                if (currentPos1 < targetPos1) {
                    currentPos1++;
                    if (currentPos1 >= targetPos1) isRunning1 = false;
                } else if (currentPos1 > targetPos1) {
                    currentPos1--;
                    if (currentPos1 <= targetPos1) isRunning1 = false;
                }
            } else {
                if (isDirCW1) currentPos1++;
                else currentPos1--;
            }
        }
    } else {
        digitalWrite(PIN_M1_PUL, LOW);
        pulseState1 = false;
    }

    portEXIT_CRITICAL_ISR(&timerMux1);
}

// ================= HÀM NGẮT TIMER PHÁT XUNG M2 (TIMER 1) =================
void IRAM_ATTR onTimerM2() {
    portENTER_CRITICAL_ISR(&timerMux2);
    
    if (isRunning2 && isEnabled2) {
        if (!pulseState2) {
            if (currentMode == MODE_POSITION) {
                if (currentPos2 < targetPos2) {
                    digitalWrite(PIN_M2_DIR, HIGH);
                    digitalWrite(PIN_M2_PUL, HIGH);
                    pulseState2 = true;
                } else if (currentPos2 > targetPos2) {
                    digitalWrite(PIN_M2_DIR, LOW);
                    digitalWrite(PIN_M2_PUL, HIGH);
                    pulseState2 = true;
                } else {
                    isRunning2 = false;
                    digitalWrite(PIN_M2_PUL, LOW);
                    pulseState2 = false;
                }
            } else {
                digitalWrite(PIN_M2_DIR, isDirCW2 ? HIGH : LOW);
                digitalWrite(PIN_M2_PUL, HIGH);
                pulseState2 = true;
            }
        } else {
            digitalWrite(PIN_M2_PUL, LOW);
            pulseState2 = false;

            if (currentMode == MODE_POSITION) {
                if (currentPos2 < targetPos2) {
                    currentPos2++;
                    if (currentPos2 >= targetPos2) isRunning2 = false;
                } else if (currentPos2 > targetPos2) {
                    currentPos2--;
                    if (currentPos2 <= targetPos2) isRunning2 = false;
                }
            } else {
                if (isDirCW2) currentPos2++;
                else currentPos2--;
            }
        }
    } else {
        digitalWrite(PIN_M2_PUL, LOW);
        pulseState2 = false;
    }

    portEXIT_CRITICAL_ISR(&timerMux2);
}

// Cập nhật tốc độ M1
void updateTimerM1Speed(uint32_t sps) {
    if (sps < MIN_SPEED_SPS) sps = MIN_SPEED_SPS;
    if (sps > MAX_SPEED_SPS) sps = MAX_SPEED_SPS;

    speedSPS1 = sps;
    uint64_t halfPeriodUs = 500000ULL / speedSPS1;
    if (halfPeriodUs < 10) halfPeriodUs = 10;
    
    if (timerM1 != nullptr) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        timerAlarm(timerM1, halfPeriodUs, true, 0);
#else
        timerAlarmWrite(timerM1, halfPeriodUs, true);
#endif
    }
}

// Cập nhật tốc độ M2
void updateTimerM2Speed(uint32_t sps) {
    if (sps < MIN_SPEED_SPS) sps = MIN_SPEED_SPS;
    if (sps > MAX_SPEED_SPS) sps = MAX_SPEED_SPS;

    speedSPS2 = sps;
    uint64_t halfPeriodUs = 500000ULL / speedSPS2;
    if (halfPeriodUs < 10) halfPeriodUs = 10;
    
    if (timerM2 != nullptr) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        timerAlarm(timerM2, halfPeriodUs, true, 0);
#else
        timerAlarmWrite(timerM2, halfPeriodUs, true);
#endif
    }
}

// ================= HỖ TRỢ HIỂN THỊ LCD & TM1638 2 ĐỘNG CƠ =================
void updateLcdDisplay() {
    if (lcd == nullptr) return;

    float rpm1 = ((float)speedSPS1 / STEPS_PER_REV) * 60.0;
    float rpm2 = ((float)speedSPS2 / STEPS_PER_REV) * 60.0;

    // Dòng 1: M1:[RUN/STOP] [RPM] [CW/CCW]
    char l1[17];
    snprintf(l1, sizeof(l1), "M1:%-4s%4.0f%3s %c", 
             isRunning1 ? "RUN" : "STOP", rpm1, isDirCW1 ? "CW" : "CCW",
             (selectedTarget == TARGET_M1 || selectedTarget == TARGET_BOTH) ? '*' : ' ');

    // Dòng 2: M2:[RUN/STOP] [RPM] [CW/CCW]
    char l2[17];
    snprintf(l2, sizeof(l2), "M2:%-4s%4.0f%3s %c", 
             isRunning2 ? "RUN" : "STOP", rpm2, isDirCW2 ? "CW" : "CCW",
             (selectedTarget == TARGET_M2 || selectedTarget == TARGET_BOTH) ? '*' : ' ');

    lcd->setCursor(0, 0);
    lcd->print(l1);
    lcd->setCursor(0, 1);
    lcd->print(l2);
}

void updateTM1638Display() {
    // Hiển thị 8 LED 7 đoạn:
    // Hiển thị dạng "1- 120RPM" hoặc "2- 300RPM" hoặc "ALL 120"
    char dispStr[9];
    if (selectedTarget == TARGET_M1) {
        float rpm1 = ((float)speedSPS1 / STEPS_PER_REV) * 60.0;
        snprintf(dispStr, sizeof(dispStr), "1-%4.0f%3s", rpm1, isDirCW1 ? " F" : " r");
    } else if (selectedTarget == TARGET_M2) {
        float rpm2 = ((float)speedSPS2 / STEPS_PER_REV) * 60.0;
        snprintf(dispStr, sizeof(dispStr), "2-%4.0f%3s", rpm2, isDirCW2 ? " F" : " r");
    } else {
        snprintf(dispStr, sizeof(dispStr), "ALL-bOTH");
    }
    tm.setString(dispStr);

    // 8 LED đơn:
    // LED 1: M1 RUN
    // LED 2: M2 RUN
    // LED 3: M1 DIR (CW/CCW)
    // LED 4: M2 DIR (CW/CCW)
    // LED 5: TARGET (Sáng: M1, Tắt: M2, Nháy: BOTH)
    // LED 6..8: Mức tốc độ
    uint8_t ledMask = 0;
    if (isRunning1) ledMask |= (1 << 0);
    if (isRunning2) ledMask |= (1 << 1);
    if (isDirCW1)   ledMask |= (1 << 2);
    if (isDirCW2)   ledMask |= (1 << 3);

    if (selectedTarget == TARGET_M1) ledMask |= (1 << 4);
    else if (selectedTarget == TARGET_BOTH) ledMask |= (1 << 4) | (1 << 5);

    uint32_t curSpd = (selectedTarget == TARGET_M2) ? speedSPS2 : speedSPS1;
    uint8_t speedLevel = map(curSpd, MIN_SPEED_SPS, MAX_SPEED_SPS, 1, 3);
    for (uint8_t i = 0; i < speedLevel; i++) ledMask |= (1 << (5 + i));

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

// ================= XỬ LÝ NÚT BẤM TM1638 CỦA 2 ĐỘNG CƠ =================
void handleButtonPress(uint8_t btnMask) {
    if (btnMask == 0) return;

    // S1: RUN / STOP ALL (Hoặc động cơ đang chọn)
    if (btnMask & (1 << 0)) {
        portENTER_CRITICAL(&timerMux1);
        portENTER_CRITICAL(&timerMux2);
        
        if (selectedTarget == TARGET_M1) {
            isRunning1 = !isRunning1;
        } else if (selectedTarget == TARGET_M2) {
            isRunning2 = !isRunning2;
        } else {
            bool anyRun = isRunning1 || isRunning2;
            isRunning1 = !anyRun;
            isRunning2 = !anyRun;
        }
        
        portEXIT_CRITICAL(&timerMux2);
        portEXIT_CRITICAL(&timerMux1);
    }
    // S2: SELECT MOTOR TARGET (M1 -> M2 -> BOTH)
    else if (btnMask & (1 << 1)) {
        selectedTarget = (SelectedMotorTarget)((selectedTarget + 1) % 3);
    }
    // S3: Tăng tốc độ động cơ đang chọn (Speed +)
    else if (btnMask & (1 << 2)) {
        if (selectedTarget == TARGET_M1 || selectedTarget == TARGET_BOTH) {
            updateTimerM1Speed(speedSPS1 + SPEED_STEP_SPS);
        }
        if (selectedTarget == TARGET_M2 || selectedTarget == TARGET_BOTH) {
            updateTimerM2Speed(speedSPS2 + SPEED_STEP_SPS);
        }
    }
    // S4: Giảm tốc độ động cơ đang chọn (Speed -)
    else if (btnMask & (1 << 3)) {
        if (selectedTarget == TARGET_M1 || selectedTarget == TARGET_BOTH) {
            if (speedSPS1 >= MIN_SPEED_SPS + SPEED_STEP_SPS) updateTimerM1Speed(speedSPS1 - SPEED_STEP_SPS);
        }
        if (selectedTarget == TARGET_M2 || selectedTarget == TARGET_BOTH) {
            if (speedSPS2 >= MIN_SPEED_SPS + SPEED_STEP_SPS) updateTimerM2Speed(speedSPS2 - SPEED_STEP_SPS);
        }
    }
    // S5: Đổi chiều M1 (CW / CCW)
    else if (btnMask & (1 << 4)) {
        portENTER_CRITICAL(&timerMux1);
        isDirCW1 = !isDirCW1;
        portEXIT_CRITICAL(&timerMux1);
    }
    // S6: Đổi chiều M2 (CW / CCW)
    else if (btnMask & (1 << 5)) {
        portENTER_CRITICAL(&timerMux2);
        isDirCW2 = !isDirCW2;
        portEXIT_CRITICAL(&timerMux2);
    }
    // S7: Đổi chế độ MODE (CONT -> POS -> JOG)
    else if (btnMask & (1 << 6)) {
        currentMode = (MotorMode)((currentMode + 1) % 3);
    }
    // S8: ZERO ALL (Reset vị trí M1 & M2 về 0)
    else if (btnMask & (1 << 7)) {
        portENTER_CRITICAL(&timerMux1);
        portENTER_CRITICAL(&timerMux2);
        currentPos1 = 0; targetPos1 = 0;
        currentPos2 = 0; targetPos2 = 0;
        portEXIT_CRITICAL(&timerMux2);
        portEXIT_CRITICAL(&timerMux1);
    }
}

// ================= GIAO TIẾP SERIAL COMMAND 2 ĐỘNG CƠ =================
void sendFullStatusToGUI() {
    float rpm1 = ((float)speedSPS1 / STEPS_PER_REV) * 60.0;
    float rpm2 = ((float)speedSPS2 / STEPS_PER_REV) * 60.0;

    Serial.printf("DUALSTAT:M1_RUN=%d,M1_DIR=%s,M1_SPD=%lu,M1_RPM=%.1f,M1_POS=%lld,M1_ENA=%d,"
                  "M2_RUN=%d,M2_DIR=%s,M2_SPD=%lu,M2_RPM=%.1f,M2_POS=%lld,M2_ENA=%d,SEL=%d,MODE=%d\n",
                  isRunning1 ? 1 : 0, isDirCW1 ? "CW" : "CCW", speedSPS1, rpm1, currentPos1, isEnabled1 ? 1 : 0,
                  isRunning2 ? 1 : 0, isDirCW2 ? "CW" : "CCW", speedSPS2, rpm2, currentPos2, isEnabled2 ? 1 : 0,
                  (int)selectedTarget, (int)currentMode);
    
    char dispStr[9];
    if (selectedTarget == TARGET_M1) {
        snprintf(dispStr, sizeof(dispStr), "1-%4.0f%3s", rpm1, isDirCW1 ? " F" : " r");
    } else if (selectedTarget == TARGET_M2) {
        snprintf(dispStr, sizeof(dispStr), "2-%4.0f%3s", rpm2, isDirCW2 ? " F" : " r");
    } else {
        snprintf(dispStr, sizeof(dispStr), "ALL-bOTH");
    }
    Serial.printf("DISP:%s\n", dispStr);

    uint8_t ledMask = 0;
    if (isRunning1) ledMask |= (1 << 0);
    if (isRunning2) ledMask |= (1 << 1);
    if (isDirCW1)   ledMask |= (1 << 2);
    if (isDirCW2)   ledMask |= (1 << 3);
    if (selectedTarget == TARGET_M1) ledMask |= (1 << 4);
    else if (selectedTarget == TARGET_BOTH) ledMask |= (1 << 4) | (1 << 5);
    Serial.printf("LEDS:%02X\n", ledMask);
    Serial.printf("BTN:%02X\n", lastStableButtons);
}

void executeSerialCommand(const char* cmd) {
    // Động cơ 1
    if (strcmp(cmd, "M1:RUN") == 0) { isRunning1 = true; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "M1:STOP") == 0) { isRunning1 = false; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "M1:DIR:CW") == 0) { isDirCW1 = true; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "M1:DIR:CCW") == 0) { isDirCW1 = false; sendFullStatusToGUI(); }
    else if (strncmp(cmd, "M1:SPEED:", 9) == 0) { updateTimerM1Speed(atoi(cmd + 9)); sendFullStatusToGUI(); }
    else if (strncmp(cmd, "M1:MOVE:", 8) == 0) {
        currentMode = MODE_POSITION;
        targetPos1 = currentPos1 + atoi(cmd + 8);
        isRunning1 = true;
        sendFullStatusToGUI();
    }
    // Động cơ 2
    else if (strcmp(cmd, "M2:RUN") == 0) { isRunning2 = true; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "M2:STOP") == 0) { isRunning2 = false; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "M2:DIR:CW") == 0) { isDirCW2 = true; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "M2:DIR:CCW") == 0) { isDirCW2 = false; sendFullStatusToGUI(); }
    else if (strncmp(cmd, "M2:SPEED:", 9) == 0) { updateTimerM2Speed(atoi(cmd + 9)); sendFullStatusToGUI(); }
    else if (strncmp(cmd, "M2:MOVE:", 8) == 0) {
        currentMode = MODE_POSITION;
        targetPos2 = currentPos2 + atoi(cmd + 8);
        isRunning2 = true;
        sendFullStatusToGUI();
    }
    // Cả 2 Động cơ
    else if (strcmp(cmd, "MOTOR:RUN_ALL") == 0) { isRunning1 = true; isRunning2 = true; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "MOTOR:STOP_ALL") == 0) { isRunning1 = false; isRunning2 = false; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "MOTOR:ZERO_ALL") == 0) { currentPos1 = 0; targetPos1 = 0; currentPos2 = 0; targetPos2 = 0; sendFullStatusToGUI(); }
    else if (strncmp(cmd, "MOTOR:SEL:", 10) == 0) { selectedTarget = (SelectedMotorTarget)atoi(cmd + 10); sendFullStatusToGUI(); }
    else if (strncmp(cmd, "MOTOR:MODE:", 11) == 0) { currentMode = (MotorMode)atoi(cmd + 11); sendFullStatusToGUI(); }
    else if (strcmp(cmd, "STATUS") == 0 || strcmp(cmd, "SYNC") == 0) { sendFullStatusToGUI(); }
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

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== ESP32-S3 DUAL MOTOR (M1 + M2) + TM1638 + LCD CONTROLLER ===");

    // 1. Cấu hình GPIO 2 Động cơ
    pinMode(PIN_M1_PUL, OUTPUT); pinMode(PIN_M1_DIR, OUTPUT); pinMode(PIN_M1_ENA, OUTPUT);
    pinMode(PIN_M2_PUL, OUTPUT); pinMode(PIN_M2_DIR, OUTPUT); pinMode(PIN_M2_ENA, OUTPUT);

    digitalWrite(PIN_M1_PUL, LOW); digitalWrite(PIN_M1_DIR, HIGH); digitalWrite(PIN_M1_ENA, LOW); // LOW = Enable
    digitalWrite(PIN_M2_PUL, LOW); digitalWrite(PIN_M2_DIR, HIGH); digitalWrite(PIN_M2_ENA, LOW); // LOW = Enable

    // 2. Cấu hình 2 Timer ngắt riêng biệt cho M1 và M2
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    timerM1 = timerBegin(1000000);
    timerAttachInterrupt(timerM1, &onTimerM1);
    updateTimerM1Speed(speedSPS1);

    timerM2 = timerBegin(1000000);
    timerAttachInterrupt(timerM2, &onTimerM2);
    updateTimerM2Speed(speedSPS2);
#else
    timerM1 = timerBegin(0, 80, true);
    timerAttachInterrupt(timerM1, &onTimerM1, true);
    updateTimerM1Speed(speedSPS1);
    timerAlarmEnable(timerM1);

    timerM2 = timerBegin(1, 80, true);
    timerAttachInterrupt(timerM2, &onTimerM2, true);
    updateTimerM2Speed(speedSPS2);
    timerAlarmEnable(timerM2);
#endif

    // 3. Khởi tạo bus I2C & LCD 1602
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
    lcd->print("DUAL MOTOR READY");
    lcd->setCursor(0, 1);
    lcd->print("M1 & M2 ACTIVE");

    // 4. Khởi tạo TM1638
    tm.begin();
    tm.setBrightness(7);
    tm.testAll();
    delay(600);
    tm.clear();

    Serial.println("[SYSTEM] KHOI TAO THANH CONG!");
}

// ================= LOOP =================
void loop() {
    handleSerial();

    // Đọc phím TM1638 với chống rung & nhấn giữ
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
                lastSpeedHoldTime = millis();
                handleButtonPress(pressed);
            }

            if (currentMode == MODE_JOG) {
                if (lastStableButtons & (1 << 0)) {
                    if (selectedTarget == TARGET_M1 || selectedTarget == TARGET_BOTH) isRunning1 = true;
                    if (selectedTarget == TARGET_M2 || selectedTarget == TARGET_BOTH) isRunning2 = true;
                } else if (lastStableButtons == 0) {
                    isRunning1 = false;
                    isRunning2 = false;
                }
            }
        }
    }

    // Nhấn giữ S3 (Speed+) hoặc S4 (Speed-)
    if ((lastStableButtons & (1 << 2)) || (lastStableButtons & (1 << 3))) {
        uint32_t holdDuration = millis() - buttonPressStartTime;
        if (holdDuration >= HOLD_INITIAL_DELAY_MS) {
            uint32_t stepVal = SPEED_STEP_SPS;
            uint32_t interval = HOLD_REPEAT_INTERVAL_MS;
            if (holdDuration > 1200) { interval = 40; stepVal = SPEED_STEP_SPS * 2; }

            if (millis() - lastSpeedHoldTime >= interval) {
                lastSpeedHoldTime = millis();
                if (lastStableButtons & (1 << 2)) {
                    if (selectedTarget == TARGET_M1 || selectedTarget == TARGET_BOTH) updateTimerM1Speed(speedSPS1 + stepVal);
                    if (selectedTarget == TARGET_M2 || selectedTarget == TARGET_BOTH) updateTimerM2Speed(speedSPS2 + stepVal);
                } else if (lastStableButtons & (1 << 3)) {
                    if (selectedTarget == TARGET_M1 || selectedTarget == TARGET_BOTH) {
                        if (speedSPS1 >= MIN_SPEED_SPS + stepVal) updateTimerM1Speed(speedSPS1 - stepVal);
                    }
                    if (selectedTarget == TARGET_M2 || selectedTarget == TARGET_BOTH) {
                        if (speedSPS2 >= MIN_SPEED_SPS + stepVal) updateTimerM2Speed(speedSPS2 - stepVal);
                    }
                }
            }
        }
    }

    // Cập nhật LCD & TM1638 định kỳ
    if (millis() - lastLcdUpdate >= 150) {
        lastLcdUpdate = millis();
        updateLcdDisplay();
        updateTM1638Display();
    }

    // Báo cáo Serial định kỳ
    uint32_t reportInterval = (isRunning1 || isRunning2) ? 200 : 500;
    if (millis() - lastSerialReport >= reportInterval) {
        lastSerialReport = millis();
        sendFullStatusToGUI();
    }
}
