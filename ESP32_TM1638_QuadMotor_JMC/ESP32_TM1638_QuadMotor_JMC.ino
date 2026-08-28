/**
 * ============================================================================
 * Dự án: ESP32-S3 + TM1638 + LCD I2C + 4 DRIVER HYBRID SERVO JMC 2HSS57
 * Nguồn: 2 Bộ nguồn 24V COSEL PBA300F-24 (14A)
 * Động cơ: 4 Động cơ bước servo lai (M1, M2, M3, M4)
 * Tác giả: Nguyễn Văn Lân
 * Nhánh: feature/quad-motor-driver
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI CHÂN CHI TIẾT (ESP32-S3 DEVKITC-1):
 * 
 * 1. NGUỒN CẤP (2 x COSEL PBA300F-24 - 24V 14A):
 *    - AC 220V        -> Đấu song song vào cọc (L, N, FG) của cả 2 nguồn COSEL.
 *    - COSEL 1 (+24V) -> Cấp nguồn VCC cho Driver M1 và Driver M2.
 *    - COSEL 2 (+24V) -> Cấp nguồn VCC cho Driver M3 và Driver M4.
 *    - COSEL 1 (-V)   ─┐
 *    - COSEL 2 (-V)   ─┼─> NỐI CHUNG TOÀN BỘ VỚI GND ESP32 VÀ CHÂN GND DRIVER.
 *    - LM2596 (Buck)  -> Lấy 24V từ Nguồn 1 hạ xuống 5V nuôi ESP32 và TM1638, LCD.
 * 
 * 2. ĐỘNG CƠ 1 (MOTOR 1 - M1):
 *    Driver JMC (1)           ESP32-S3
 *    ---------------------------------
 *    PUL1+ (Pulse)      ->   GPIO 4
 *    DIR1+ (Dir)        ->   GPIO 5
 *    ENA1+ (Enable)     ->   GPIO 6
 *    PUL1-, DIR1-, ENA1-->   GND (Chung Mass ESP32 & Nguồn)
 * 
 * 3. ĐỘNG CƠ 2 (MOTOR 2 - M2):
 *    Driver JMC (2)           ESP32-S3
 *    ---------------------------------
 *    PUL2+ (Pulse)      ->   GPIO 18
 *    DIR2+ (Dir)        ->   GPIO 19
 *    ENA2+ (Enable)     ->   GPIO 20
 *    PUL2-, DIR2-, ENA2-->   GND (Chung Mass ESP32 & Nguồn)
 * 
 * 4. ĐỘNG CƠ 3 (MOTOR 3 - M3):
 *    Driver JMC (3)           ESP32-S3
 *    ---------------------------------
 *    PUL3+ (Pulse)      ->   GPIO 1
 *    DIR3+ (Dir)        ->   GPIO 2
 *    ENA3+ (Enable)     ->   GPIO 42
 *    PUL3-, DIR3-, ENA3-->   GND (Chung Mass ESP32 & Nguồn)
 * 
 * 5. ĐỘNG CƠ 4 (MOTOR 4 - M4):
 *    Driver JMC (4)           ESP32-S3
 *    ---------------------------------
 *    PUL4+ (Pulse)      ->   GPIO 38
 *    DIR4+ (Dir)        ->   GPIO 39
 *    ENA4+ (Enable)     ->   GPIO 40
 *    PUL4-, DIR4-, ENA4-->   GND (Chung Mass ESP32 & Nguồn)
 * 
 * 6. MODULE TM1638:
 *    STB -> GPIO 15, CLK -> GPIO 16, DIO -> GPIO 17, VCC -> 5V, GND -> GND
 * 
 * 7. LCD 1602 / 2004 I2C:
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

// Driver Động cơ 3 (M3)
#define PIN_M3_PUL    1
#define PIN_M3_DIR    2
#define PIN_M3_ENA    42

// Driver Động cơ 4 (M4)
#define PIN_M4_PUL    38
#define PIN_M4_DIR    39
#define PIN_M4_ENA    40

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
    TARGET_M3   = 2,
    TARGET_M4   = 3,
    TARGET_ALL  = 4
};

// ================= KHỞI TẠO BIẾN TOÀN CỤC 4 ĐỘNG CƠ =================
TM1638_Driver tm(PIN_TM_STB, PIN_TM_CLK, PIN_TM_DIO);
LiquidCrystal_I2C* lcd = nullptr;
uint8_t currentLcdAddr = 0x27;

// Cấu trúc trạng thái cho 1 động cơ
struct MotorState {
    volatile bool isRunning;
    volatile bool isDirCW;
    volatile bool isEnabled;
    volatile uint32_t speedSPS;
    volatile int64_t currentPos;
    volatile int64_t targetPos;
    volatile bool pulseState;
    uint8_t pinPul;
    uint8_t pinDir;
    uint8_t pinEna;
};

// Khởi tạo 4 Động cơ
MotorState motors[4] = {
    { false, true, true, 1600, 0, 0, false, PIN_M1_PUL, PIN_M1_DIR, PIN_M1_ENA },
    { false, true, true, 1600, 0, 0, false, PIN_M2_PUL, PIN_M2_DIR, PIN_M2_ENA },
    { false, true, true, 1600, 0, 0, false, PIN_M3_PUL, PIN_M3_DIR, PIN_M3_ENA },
    { false, true, true, 1600, 0, 0, false, PIN_M4_PUL, PIN_M4_DIR, PIN_M4_ENA }
};

// Chế độ chung & mục tiêu đang chọn
MotorMode currentMode = MODE_CONTINUOUS;
SelectedMotorTarget selectedTarget = TARGET_M1;

// 4 Timer ngắt phần cứng riêng cho 4 động cơ
hw_timer_t* timers[4] = { nullptr, nullptr, nullptr, nullptr };
portMUX_TYPE timerMuxes[4] = {
    portMUX_INITIALIZER_UNLOCKED,
    portMUX_INITIALIZER_UNLOCKED,
    portMUX_INITIALIZER_UNLOCKED,
    portMUX_INITIALIZER_UNLOCKED
};

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

// ================= HÀM XỬ LÝ PHÁT XUNG CHO TỪNG ĐỘNG CƠ TRONG NGẮT =================
inline void IRAM_ATTR stepMotorISR(uint8_t idx) {
    portENTER_CRITICAL_ISR(&timerMuxes[idx]);
    MotorState &m = motors[idx];

    if (m.isRunning && m.isEnabled) {
        if (!m.pulseState) {
            if (currentMode == MODE_POSITION) {
                if (m.currentPos < m.targetPos) {
                    digitalWrite(m.pinDir, HIGH);
                    digitalWrite(m.pinPul, HIGH);
                    m.pulseState = true;
                } else if (m.currentPos > m.targetPos) {
                    digitalWrite(m.pinDir, LOW);
                    digitalWrite(m.pinPul, HIGH);
                    m.pulseState = true;
                } else {
                    m.isRunning = false;
                    digitalWrite(m.pinPul, LOW);
                    m.pulseState = false;
                }
            } else {
                digitalWrite(m.pinDir, m.isDirCW ? HIGH : LOW);
                digitalWrite(m.pinPul, HIGH);
                m.pulseState = true;
            }
        } else {
            digitalWrite(m.pinPul, LOW);
            m.pulseState = false;

            if (currentMode == MODE_POSITION) {
                if (m.currentPos < m.targetPos) {
                    m.currentPos++;
                    if (m.currentPos >= m.targetPos) m.isRunning = false;
                } else if (m.currentPos > m.targetPos) {
                    m.currentPos--;
                    if (m.currentPos <= m.targetPos) m.isRunning = false;
                }
            } else {
                if (m.isDirCW) m.currentPos++;
                else m.currentPos--;
            }
        }
    } else {
        digitalWrite(m.pinPul, LOW);
        m.pulseState = false;
    }

    portEXIT_CRITICAL_ISR(&timerMuxes[idx]);
}

// 4 ISR Handler riêng biệt cho 4 Timer
void IRAM_ATTR onTimerM1() { stepMotorISR(0); }
void IRAM_ATTR onTimerM2() { stepMotorISR(1); }
void IRAM_ATTR onTimerM3() { stepMotorISR(2); }
void IRAM_ATTR onTimerM4() { stepMotorISR(3); }

// ================= CẬP NHẬT TỐC ĐỘ ĐỘNG CƠ =================
void updateMotorSpeed(uint8_t idx, uint32_t sps) {
    if (idx >= 4) return;
    if (sps < MIN_SPEED_SPS) sps = MIN_SPEED_SPS;
    if (sps > MAX_SPEED_SPS) sps = MAX_SPEED_SPS;

    motors[idx].speedSPS = sps;
    uint64_t halfPeriodUs = 500000ULL / motors[idx].speedSPS;
    if (halfPeriodUs < 10) halfPeriodUs = 10;
    
    if (timers[idx] != nullptr) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        timerAlarm(timers[idx], halfPeriodUs, true, 0);
#else
        timerAlarmWrite(timers[idx], halfPeriodUs, true);
#endif
    }
}

// ================= HỖ TRỢ HIỂN THỊ LCD & TM1638 =================
void updateLcdDisplay() {
    if (lcd == nullptr) return;

    // Dòng 1: M1 & M2 (Kèm ký hiệu * cho động cơ đang chọn)
    float rpm1 = ((float)motors[0].speedSPS / STEPS_PER_REV) * 60.0;
    float rpm2 = ((float)motors[1].speedSPS / STEPS_PER_REV) * 60.0;
    char l1[17];
    snprintf(l1, sizeof(l1), "1%c%c%3.0f 2%c%c%3.0f",
             (selectedTarget == TARGET_M1 || selectedTarget == TARGET_ALL) ? '*' : ':',
             motors[0].isRunning ? (motors[0].isDirCW ? '+' : '-') : 'S', rpm1,
             (selectedTarget == TARGET_M2 || selectedTarget == TARGET_ALL) ? '*' : ':',
             motors[1].isRunning ? (motors[1].isDirCW ? '+' : '-') : 'S', rpm2);

    // Dòng 2: M3 & M4
    float rpm3 = ((float)motors[2].speedSPS / STEPS_PER_REV) * 60.0;
    float rpm4 = ((float)motors[3].speedSPS / STEPS_PER_REV) * 60.0;
    char l2[17];
    snprintf(l2, sizeof(l2), "3%c%c%3.0f 4%c%c%3.0f",
             (selectedTarget == TARGET_M3 || selectedTarget == TARGET_ALL) ? '*' : ':',
             motors[2].isRunning ? (motors[2].isDirCW ? '+' : '-') : 'S', rpm3,
             (selectedTarget == TARGET_M4 || selectedTarget == TARGET_ALL) ? '*' : ':',
             motors[3].isRunning ? (motors[3].isDirCW ? '+' : '-') : 'S', rpm4);

    lcd->setCursor(0, 0);
    lcd->print(l1);
    lcd->setCursor(0, 1);
    lcd->print(l2);
}

void updateTM1638Display() {
    // 8 LED 7 đoạn: Hiển thị trạng thái động cơ đang chọn
    char dispStr[9];
    if (selectedTarget < TARGET_ALL) {
        uint8_t idx = (uint8_t)selectedTarget;
        float rpm = ((float)motors[idx].speedSPS / STEPS_PER_REV) * 60.0;
        snprintf(dispStr, sizeof(dispStr), "%d-%4.0f%2s", idx + 1, rpm, motors[idx].isDirCW ? " F" : " r");
    } else {
        snprintf(dispStr, sizeof(dispStr), "ALL- 4M ");
    }
    tm.setString(dispStr);

    // 8 LED đơn:
    // LED 1: M1 RUN / STOP
    // LED 2: M2 RUN / STOP
    // LED 3: M3 RUN / STOP
    // LED 4: M4 RUN / STOP
    // LED 5..8: Chỉ báo động cơ đang chọn (1..4) hoặc sáng hết nếu chọn ALL
    uint8_t ledMask = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (motors[i].isRunning) ledMask |= (1 << i);
    }

    if (selectedTarget < TARGET_ALL) {
        ledMask |= (1 << (4 + (uint8_t)selectedTarget));
    } else {
        ledMask |= 0xF0; // Sáng 4 LED bên phải khi chọn ALL
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

// ================= XỬ LÝ NÚT BẤM TM1638 =================
void handleButtonPress(uint8_t btnMask) {
    if (btnMask == 0) return;

    // S1: RUN / STOP (Động cơ đang chọn hoặc tất cả)
    if (btnMask & (1 << 0)) {
        if (selectedTarget < TARGET_ALL) {
            uint8_t idx = (uint8_t)selectedTarget;
            portENTER_CRITICAL(&timerMuxes[idx]);
            motors[idx].isRunning = !motors[idx].isRunning;
            portEXIT_CRITICAL(&timerMuxes[idx]);
        } else {
            bool anyRun = false;
            for (uint8_t i = 0; i < 4; i++) if (motors[i].isRunning) anyRun = true;
            for (uint8_t i = 0; i < 4; i++) {
                portENTER_CRITICAL(&timerMuxes[i]);
                motors[i].isRunning = !anyRun;
                portEXIT_CRITICAL(&timerMuxes[i]);
            }
        }
    }
    // S2: CHỌN ĐỘNG CƠ (M1 -> M2 -> M3 -> M4 -> ALL)
    else if (btnMask & (1 << 1)) {
        selectedTarget = (SelectedMotorTarget)((selectedTarget + 1) % 5);
    }
    // S3: Tăng tốc độ (Speed +)
    else if (btnMask & (1 << 2)) {
        if (selectedTarget < TARGET_ALL) {
            uint8_t idx = (uint8_t)selectedTarget;
            updateMotorSpeed(idx, motors[idx].speedSPS + SPEED_STEP_SPS);
        } else {
            for (uint8_t i = 0; i < 4; i++) updateMotorSpeed(i, motors[i].speedSPS + SPEED_STEP_SPS);
        }
    }
    // S4: Giảm tốc độ (Speed -)
    else if (btnMask & (1 << 3)) {
        if (selectedTarget < TARGET_ALL) {
            uint8_t idx = (uint8_t)selectedTarget;
            if (motors[idx].speedSPS >= MIN_SPEED_SPS + SPEED_STEP_SPS)
                updateMotorSpeed(idx, motors[idx].speedSPS - SPEED_STEP_SPS);
        } else {
            for (uint8_t i = 0; i < 4; i++) {
                if (motors[i].speedSPS >= MIN_SPEED_SPS + SPEED_STEP_SPS)
                    updateMotorSpeed(i, motors[i].speedSPS - SPEED_STEP_SPS);
            }
        }
    }
    // S5: Đổi chiều quay (CW / CCW)
    else if (btnMask & (1 << 4)) {
        if (selectedTarget < TARGET_ALL) {
            uint8_t idx = (uint8_t)selectedTarget;
            portENTER_CRITICAL(&timerMuxes[idx]);
            motors[idx].isDirCW = !motors[idx].isDirCW;
            portEXIT_CRITICAL(&timerMuxes[idx]);
        } else {
            for (uint8_t i = 0; i < 4; i++) {
                portENTER_CRITICAL(&timerMuxes[i]);
                motors[i].isDirCW = !motors[i].isDirCW;
                portEXIT_CRITICAL(&timerMuxes[i]);
            }
        }
    }
    // S6: Khóa/Mở trục (Enable / Disable Driver)
    else if (btnMask & (1 << 5)) {
        if (selectedTarget < TARGET_ALL) {
            uint8_t idx = (uint8_t)selectedTarget;
            motors[idx].isEnabled = !motors[idx].isEnabled;
            digitalWrite(motors[idx].pinEna, motors[idx].isEnabled ? LOW : HIGH);
        } else {
            bool en = !motors[0].isEnabled;
            for (uint8_t i = 0; i < 4; i++) {
                motors[i].isEnabled = en;
                digitalWrite(motors[i].pinEna, en ? LOW : HIGH);
            }
        }
    }
    // S7: Đổi chế độ MODE (CONT -> POS -> JOG)
    else if (btnMask & (1 << 6)) {
        currentMode = (MotorMode)((currentMode + 1) % 3);
    }
    // S8: ZERO ALL (Reset vị trí cả 4 động cơ về 0)
    else if (btnMask & (1 << 7)) {
        for (uint8_t i = 0; i < 4; i++) {
            portENTER_CRITICAL(&timerMuxes[i]);
            motors[i].currentPos = 0;
            motors[i].targetPos = 0;
            portEXIT_CRITICAL(&timerMuxes[i]);
        }
    }
}

// ================= GIAO TIẾP SERIAL COMMAND 4 ĐỘNG CƠ =================
void sendFullStatusToGUI() {
    float rpm1 = ((float)motors[0].speedSPS / STEPS_PER_REV) * 60.0;
    float rpm2 = ((float)motors[1].speedSPS / STEPS_PER_REV) * 60.0;
    float rpm3 = ((float)motors[2].speedSPS / STEPS_PER_REV) * 60.0;
    float rpm4 = ((float)motors[3].speedSPS / STEPS_PER_REV) * 60.0;

    Serial.printf("QUADSTAT:M1_RUN=%d,M1_DIR=%s,M1_SPD=%lu,M1_RPM=%.1f,M1_POS=%lld,M1_ENA=%d,"
                  "M2_RUN=%d,M2_DIR=%s,M2_SPD=%lu,M2_RPM=%.1f,M2_POS=%lld,M2_ENA=%d,"
                  "M3_RUN=%d,M3_DIR=%s,M3_SPD=%lu,M3_RPM=%.1f,M3_POS=%lld,M3_ENA=%d,"
                  "M4_RUN=%d,M4_DIR=%s,M4_SPD=%lu,M4_RPM=%.1f,M4_POS=%lld,M4_ENA=%d,"
                  "SEL=%d,MODE=%d\n",
                  motors[0].isRunning ? 1 : 0, motors[0].isDirCW ? "CW" : "CCW", motors[0].speedSPS, rpm1, motors[0].currentPos, motors[0].isEnabled ? 1 : 0,
                  motors[1].isRunning ? 1 : 0, motors[1].isDirCW ? "CW" : "CCW", motors[1].speedSPS, rpm2, motors[1].currentPos, motors[1].isEnabled ? 1 : 0,
                  motors[2].isRunning ? 1 : 0, motors[2].isDirCW ? "CW" : "CCW", motors[2].speedSPS, rpm3, motors[2].currentPos, motors[2].isEnabled ? 1 : 0,
                  motors[3].isRunning ? 1 : 0, motors[3].isDirCW ? "CW" : "CCW", motors[3].speedSPS, rpm4, motors[3].currentPos, motors[3].isEnabled ? 1 : 0,
                  (int)selectedTarget, (int)currentMode);
}

void executeSerialCommand(const char* cmd) {
    // Động cơ 1 -> 4
    for (int i = 0; i < 4; i++) {
        char prefix[8];
        snprintf(prefix, sizeof(prefix), "M%d:", i + 1);
        if (strncmp(cmd, prefix, 3) == 0) {
            const char* sub = cmd + 3;
            if (strcmp(sub, "RUN") == 0) { motors[i].isRunning = true; sendFullStatusToGUI(); return; }
            if (strcmp(sub, "STOP") == 0) { motors[i].isRunning = false; sendFullStatusToGUI(); return; }
            if (strcmp(sub, "DIR:CW") == 0) { motors[i].isDirCW = true; sendFullStatusToGUI(); return; }
            if (strcmp(sub, "DIR:CCW") == 0) { motors[i].isDirCW = false; sendFullStatusToGUI(); return; }
            if (strncmp(sub, "SPEED:", 6) == 0) { updateMotorSpeed(i, atoi(sub + 6)); sendFullStatusToGUI(); return; }
            if (strncmp(sub, "MOVE:", 5) == 0) {
                currentMode = MODE_POSITION;
                motors[i].targetPos = motors[i].currentPos + atoi(sub + 5);
                motors[i].isRunning = true;
                sendFullStatusToGUI();
                return;
            }
        }
    }

    // Điều khiển tổng thể
    if (strcmp(cmd, "MOTOR:RUN_ALL") == 0) { for (int i = 0; i < 4; i++) motors[i].isRunning = true; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "MOTOR:STOP_ALL") == 0) { for (int i = 0; i < 4; i++) motors[i].isRunning = false; sendFullStatusToGUI(); }
    else if (strcmp(cmd, "MOTOR:ZERO_ALL") == 0) { for (int i = 0; i < 4; i++) { motors[i].currentPos = 0; motors[i].targetPos = 0; } sendFullStatusToGUI(); }
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
    Serial.println("\n=== ESP32-S3 QUAD MOTOR (M1..M4) + TM1638 + LCD CONTROLLER ===");

    // 1. Cấu hình GPIO 4 Động cơ
    void (*isrList[4])() = { onTimerM1, onTimerM2, onTimerM3, onTimerM4 };

    for (uint8_t i = 0; i < 4; i++) {
        pinMode(motors[i].pinPul, OUTPUT);
        pinMode(motors[i].pinDir, OUTPUT);
        pinMode(motors[i].pinEna, OUTPUT);

        digitalWrite(motors[i].pinPul, LOW);
        digitalWrite(motors[i].pinDir, HIGH);
        digitalWrite(motors[i].pinEna, LOW); // LOW = Enable

        // 2. Cấu hình Timer ngắt phần cứng
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        timers[i] = timerBegin(1000000);
        timerAttachInterrupt(timers[i], isrList[i]);
        updateMotorSpeed(i, motors[i].speedSPS);
#else
        timers[i] = timerBegin(i, 80, true);
        timerAttachInterrupt(timers[i], isrList[i], true);
        updateMotorSpeed(i, motors[i].speedSPS);
        timerAlarmEnable(timers[i]);
#endif
    }

    // 3. Khởi tạo bus I2C & LCD
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
    lcd->print("QUAD MOTOR READY");
    lcd->setCursor(0, 1);
    lcd->print("4 MOTORS ACTIVE");

    // 4. Khởi tạo TM1638
    tm.begin();
    tm.setBrightness(7);
    tm.testAll();
    delay(600);
    tm.clear();

    Serial.println("[SYSTEM] KHOI TAO THANH CONG 4 DONG CO!");
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
                    if (selectedTarget < TARGET_ALL) motors[(uint8_t)selectedTarget].isRunning = true;
                    else for (int i = 0; i < 4; i++) motors[i].isRunning = true;
                } else if (lastStableButtons == 0) {
                    for (int i = 0; i < 4; i++) motors[i].isRunning = false;
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
                    if (selectedTarget < TARGET_ALL) {
                        uint8_t idx = (uint8_t)selectedTarget;
                        updateMotorSpeed(idx, motors[idx].speedSPS + stepVal);
                    } else {
                        for (int i = 0; i < 4; i++) updateMotorSpeed(i, motors[i].speedSPS + stepVal);
                    }
                } else if (lastStableButtons & (1 << 3)) {
                    if (selectedTarget < TARGET_ALL) {
                        uint8_t idx = (uint8_t)selectedTarget;
                        if (motors[idx].speedSPS >= MIN_SPEED_SPS + stepVal) updateMotorSpeed(idx, motors[idx].speedSPS - stepVal);
                    } else {
                        for (int i = 0; i < 4; i++) {
                            if (motors[i].speedSPS >= MIN_SPEED_SPS + stepVal) updateMotorSpeed(i, motors[i].speedSPS - stepVal);
                        }
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
    bool anyMotorRunning = false;
    for (int i = 0; i < 4; i++) if (motors[i].isRunning) anyMotorRunning = true;

    uint32_t reportInterval = anyMotorRunning ? 200 : 500;
    if (millis() - lastSerialReport >= reportInterval) {
        lastSerialReport = millis();
        sendFullStatusToGUI();
    }
}
