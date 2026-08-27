/**
 * ============================================================================
 * Dự án: ESP32-S3 + TM1638 + LCD I2C + DRIVER HYBRID SERVO JMC 2HSS57
 * Động cơ: YAKO YK257EC56E1-03 (Closed-Loop Stepper Motor)
 * Tác giả: Nguyễn Văn Lân
 * Nhánh: feature/motor-driver
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI CHÂN (PINOUT):
 * 
 * 1. MODULE TM1638:
 *    TM1638            ESP32-S3
 *    -----------------------------
 *    VCC          ->   5V (VIN / VBUS)
 *    GND          ->   GND
 *    STB (Strobe) ->   GPIO 15
 *    CLK (Clock)  ->   GPIO 16
 *    DIO (Data)   ->   GPIO 17
 * 
 * 2. MÀN HÌNH LCD 1602 / 2004 I2C:
 *    Module I2C        ESP32-S3
 *    -----------------------------
 *    VCC          ->   5V (hoặc VIN)
 *    GND          ->   GND
 *    SDA          ->   GPIO 8
 *    SCL          ->   GPIO 9
 * 
 * 3. DRIVER JMC 2HSS57 (Kiểu Common Cathode - Nối âm chung):
 *    Driver 2HSS57     ESP32-S3
 *    -----------------------------
 *    PUL+ (Pulse) ->   GPIO 4
 *    DIR+ (Dir)   ->   GPIO 5
 *    ENA+ (Enable)->   GPIO 6
 *    ALM+ (Alarm) ->   GPIO 7  (Kèm trở kéo lên 4.7k-10k nếu cần, phát hiện lỗi)
 *    PUL-, DIR-,  ->   GND (Nối chung mass với ESP32)
 *    ENA-, ALM-
 * 
 *    * Cổng Encoder & Motor đấu trực tiếp từ động cơ YAKO vào Driver 2HSS57.
 *    * Nguồn cấp Driver: DC 24V - 48V cấp vào V+ và GND của driver.
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "TM1638_Driver.h"

// ================= CẤU HÌNH CHÂN PHẦN CỨNG =================
// Chân TM1638
#define PIN_TM_STB    15
#define PIN_TM_CLK    16
#define PIN_TM_DIO    17

// Chân I2C (ESP32-S3)
#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   9
#define LCD_COLS      16
#define LCD_ROWS      2

// Chân điều khiển Driver JMC 2HSS57
#define PIN_MOTOR_PUL 4   // Xung bước
#define PIN_MOTOR_DIR 5   // Hướng quay (HIGH: CW, LOW: CCW)
#define PIN_MOTOR_ENA 6   // Bật/Tắt driver (HIGH: Enable, LOW: Free motor)
#define PIN_MOTOR_ALM 7   // Báo lỗi từ driver (LOW: Alarm)

// ================= CẤU HÌNH THÔNG SỐ ĐỘNG CƠ =================
// Vi bước cài đặt trên switch Driver 2HSS57 (Mặc định 1000 hoặc 1600 hoặc 3200 steps/vòng)
const uint32_t STEPS_PER_REV = 1600; 

// Giới hạn tốc độ (xung/giây - Steps Per Second)
const uint32_t MIN_SPEED_SPS = 200;    // ~ 7.5 RPM (ở 1600 step/rev)
const uint32_t MAX_SPEED_SPS = 16000;  // ~ 600 RPM
const uint32_t SPEED_STEP_SPS = 400;   // Mỗi lần tăng/giảm tốc độ

// Chế độ vận hành
enum MotorMode {
    MODE_CONTINUOUS = 0, // Quay liên tục
    MODE_POSITION   = 1, // Quay định vị theo số bước / góc
    MODE_JOG        = 2  // Nhấp giữ phím chạy, nhả phím dừng
};

// ================= KHỞI TẠO ĐỐI TƯỢNG VÀ BIẾN TOÀN CỤC =================
TM1638_Driver tm(PIN_TM_STB, PIN_TM_CLK, PIN_TM_DIO);
LiquidCrystal_I2C* lcd = nullptr;
uint8_t currentLcdAddr = 0x27;

// Biến trạng thái động cơ
volatile bool isRunning = false;
volatile bool isDirCW = true;          // true: CW (Thuận), false: CCW (Nghịch)
volatile bool isEnabled = true;        // true: Khóa trục/sẵn sàng, false: Thả trôi
volatile uint32_t currentSpeedSPS = 1600; // Tốc độ hiện tại (1600 SPS = 1 vòng/s = 60 RPM)
volatile int64_t currentPosition = 0;  // Vị trí tích lũy (số bước)
volatile int64_t targetPosition = 0;   // Vị trí mục tiêu (dùng cho MODE_POSITION)
MotorMode currentMode = MODE_CONTINUOUS;

// Timer phần cứng tạo xung chính xác cao cho động cơ (ESP32 Timer)
hw_timer_t* stepTimer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Biến giao tiếp và điều khiển
uint8_t lastStableButtons = 0x00;
uint8_t rawButtons = 0x00;
uint32_t lastDebounceTime = 0;
const uint32_t DEBOUNCE_DELAY_MS = 30;

uint32_t lastLcdUpdate = 0;
uint32_t lastSerialReport = 0;
char rxBuffer[64];
uint8_t rxIndex = 0;

volatile bool pulseState = false;

// ================= HÀM NGẮT TIMER PHÁT XUNG (STEP ISR - 50% DUTY CYCLE) =================
void IRAM_ATTR onStepTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    
    if (isRunning && isEnabled) {
        if (!pulseState) {
            // Cạnh lên (RISING EDGE)
            if (currentMode == MODE_POSITION) {
                if (currentPosition < targetPosition) {
                    digitalWrite(PIN_MOTOR_DIR, HIGH); // CW
                    digitalWrite(PIN_MOTOR_PUL, HIGH);
                    pulseState = true;
                } else if (currentPosition > targetPosition) {
                    digitalWrite(PIN_MOTOR_DIR, LOW);  // CCW
                    digitalWrite(PIN_MOTOR_PUL, HIGH);
                    pulseState = true;
                } else {
                    isRunning = false;
                    digitalWrite(PIN_MOTOR_PUL, LOW);
                    pulseState = false;
                }
            } else {
                digitalWrite(PIN_MOTOR_DIR, isDirCW ? HIGH : LOW);
                digitalWrite(PIN_MOTOR_PUL, HIGH);
                pulseState = true;
            }
        } else {
            // Cạnh xuống (FALLING EDGE) -> Hoàn thành 1 xung bước
            digitalWrite(PIN_MOTOR_PUL, LOW);
            pulseState = false;

            if (currentMode == MODE_POSITION) {
                if (currentPosition < targetPosition) {
                    currentPosition++;
                    if (currentPosition >= targetPosition) isRunning = false;
                } else if (currentPosition > targetPosition) {
                    currentPosition--;
                    if (currentPosition <= targetPosition) isRunning = false;
                }
            } else {
                if (isDirCW) currentPosition++;
                else currentPosition--;
            }
        }
    } else {
        digitalWrite(PIN_MOTOR_PUL, LOW);
        pulseState = false;
    }

    portEXIT_CRITICAL_ISR(&timerMux);
}

// Cập nhật chu kỳ ngắt timer theo tốc độ SPS (ngắt ở tần số 2 * SPS để tạo xung vuông 50%)
void updateTimerSpeed(uint32_t sps) {
    if (sps < MIN_SPEED_SPS) sps = MIN_SPEED_SPS;
    if (sps > MAX_SPEED_SPS) sps = MAX_SPEED_SPS;

    currentSpeedSPS = sps;
    // Ngắt mỗi nửa chu kỳ xung (Half-period)
    uint64_t halfPeriodUs = 500000ULL / currentSpeedSPS;
    if (halfPeriodUs < 10) halfPeriodUs = 10;
    
    if (stepTimer != nullptr) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        timerAlarm(stepTimer, halfPeriodUs, true, 0);
#else
        timerAlarmWrite(stepTimer, halfPeriodUs, true);
#endif
    }
}

// ================= HỖ TRỢ HIỂN THỊ LCD & TM1638 =================
void updateLcdDisplay() {
    if (lcd == nullptr) return;

    // Dòng 1: [TRẠNG THÁI] TỐC ĐỘ (RPM) [HƯỚNG]
    float rpm = ((float)currentSpeedSPS / STEPS_PER_REV) * 60.0;
    char l1[17];
    const char* runStr = isRunning ? "RUN " : "STOP";
    const char* dirStr = isDirCW ? "CW " : "CCW";
    
    snprintf(l1, sizeof(l1), "%-4s %4.0fRPM %3s", runStr, rpm, dirStr);

    // Dòng 2: [CHẾ ĐỘ] VỊ TRÍ BƯỚC / TRẠNG THÁI ENA
    char l2[17];
    const char* modeStr = (currentMode == MODE_CONTINUOUS) ? "CONT" : ((currentMode == MODE_POSITION) ? "POS " : "JOG ");
    bool alarmActive = (digitalRead(PIN_MOTOR_ALM) == LOW); // LOW khi driver báo lỗi
    
    if (alarmActive) {
        snprintf(l2, sizeof(l2), "ALARM! CHECK DRV");
    } else {
        snprintf(l2, sizeof(l2), "%s P:%+6ld %s", modeStr, (long)(currentPosition % 1000000), isEnabled ? "ON" : "OFF");
    }

    lcd->setCursor(0, 0);
    lcd->print(l1);
    lcd->setCursor(0, 1);
    lcd->print(l2);
}

void updateTM1638Display() {
    // 1. Hiển thị 8 LED 7 đoạn: Hiển thị vị trí hoặc tốc độ
    char dispStr[9];
    if (currentMode == MODE_POSITION) {
        snprintf(dispStr, sizeof(dispStr), "P%7ld", (long)currentPosition);
    } else {
        float rpm = ((float)currentSpeedSPS / STEPS_PER_REV) * 60.0;
        snprintf(dispStr, sizeof(dispStr), "S%4.0f%3s", rpm, isDirCW ? " F" : " r");
    }
    tm.setString(dispStr);

    // 2. Cập nhật 8 LED đơn:
    // LED 1: RUN (Đang chạy)
    // LED 2: DIR (Sáng: CW, Tắt: CCW)
    // LED 3: ENA (Sáng: Khóa động cơ, Tắt: Thả lỏng)
    // LED 4..8: Thanh đo tốc độ (Speed Level)
    uint8_t ledMask = 0;
    if (isRunning) ledMask |= (1 << 0);
    if (isDirCW)   ledMask |= (1 << 1);
    if (isEnabled) ledMask |= (1 << 2);

    // Tính mức tốc độ 1..5 cho LED 4..8
    uint8_t speedLevel = map(currentSpeedSPS, MIN_SPEED_SPS, MAX_SPEED_SPS, 1, 5);
    for (uint8_t i = 0; i < speedLevel; i++) {
        ledMask |= (1 << (3 + i));
    }

    tm.setLEDs(ledMask);
    tm.update();
}

// Tự động quét I2C
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

    // S1: RUN / STOP
    if (btnMask & (1 << 0)) {
        portENTER_CRITICAL(&timerMux);
        isRunning = !isRunning;
        portEXIT_CRITICAL(&timerMux);
        Serial.printf("[MOTOR] %s\n", isRunning ? "RUNNING" : "STOPPED");
    }
    // S2: Đổi chiều CW / CCW
    else if (btnMask & (1 << 1)) {
        portENTER_CRITICAL(&timerMux);
        isDirCW = !isDirCW;
        portEXIT_CRITICAL(&timerMux);
        Serial.printf("[MOTOR] DIRECTION: %s\n", isDirCW ? "CW (THUAN)" : "CCW (NGHICH)");
    }
    // S3: Tăng tốc độ (+)
    else if (btnMask & (1 << 2)) {
        if (currentSpeedSPS + SPEED_STEP_SPS <= MAX_SPEED_SPS) {
            updateTimerSpeed(currentSpeedSPS + SPEED_STEP_SPS);
        } else {
            updateTimerSpeed(MAX_SPEED_SPS);
        }
        Serial.printf("[MOTOR] SPEED: %lu SPS\n", currentSpeedSPS);
    }
    // S4: Giảm tốc độ (-)
    else if (btnMask & (1 << 3)) {
        if (currentSpeedSPS >= MIN_SPEED_SPS + SPEED_STEP_SPS) {
            updateTimerSpeed(currentSpeedSPS - SPEED_STEP_SPS);
        } else {
            updateTimerSpeed(MIN_SPEED_SPS);
        }
        Serial.printf("[MOTOR] SPEED: %lu SPS\n", currentSpeedSPS);
    }
    // S5: Quay 1 vòng (360 độ = STEPS_PER_REV)
    else if (btnMask & (1 << 4)) {
        currentMode = MODE_POSITION;
        portENTER_CRITICAL(&timerMux);
        if (isDirCW) targetPosition = currentPosition + STEPS_PER_REV;
        else targetPosition = currentPosition - STEPS_PER_REV;
        isRunning = true;
        portEXIT_CRITICAL(&timerMux);
        Serial.printf("[MOTOR] MOVE 1 REV: Target = %lld\n", targetPosition);
    }
    // S6: Chuyển đổi chế độ (Continuous -> Position -> Jog)
    else if (btnMask & (1 << 5)) {
        currentMode = (MotorMode)((currentMode + 1) % 3);
        if (currentMode == MODE_CONTINUOUS) Serial.println("[MODE] CONTINUOUS");
        else if (currentMode == MODE_POSITION) Serial.println("[MODE] POSITION");
        else Serial.println("[MODE] JOG");
    }
    // S7: Bật / Tắt Driver (Enable / Free)
    else if (btnMask & (1 << 6)) {
        isEnabled = !isEnabled;
        digitalWrite(PIN_MOTOR_ENA, isEnabled ? LOW : HIGH); // LOW: Khóa trục, HIGH: Thả tự do
        Serial.printf("[MOTOR] ENABLE: %s\n", isEnabled ? "LOCKED (ENA)" : "FREE (DIS)");
    }
    // S8: Reset vị trí về 0 (Home/Zero)
    else if (btnMask & (1 << 7)) {
        portENTER_CRITICAL(&timerMux);
        currentPosition = 0;
        targetPosition = 0;
        portEXIT_CRITICAL(&timerMux);
        Serial.println("[MOTOR] POSITION RESET TO 0");
    }
}

// ================= GIAO TIẾP SERIAL COMMAND =================
void executeSerialCommand(const char* cmd) {
    if (strcmp(cmd, "MOTOR:RUN") == 0) {
        isRunning = true;
        Serial.println("OK:MOTOR:RUN");
    }
    else if (strcmp(cmd, "MOTOR:STOP") == 0) {
        isRunning = false;
        Serial.println("OK:MOTOR:STOP");
    }
    else if (strcmp(cmd, "MOTOR:DIR:CW") == 0) {
        isDirCW = true;
        Serial.println("OK:MOTOR:DIR:CW");
    }
    else if (strcmp(cmd, "MOTOR:DIR:CCW") == 0) {
        isDirCW = false;
        Serial.println("OK:MOTOR:DIR:CCW");
    }
    else if (strncmp(cmd, "MOTOR:SPEED:", 12) == 0) {
        uint32_t spd = (uint32_t)atoi(cmd + 12);
        updateTimerSpeed(spd);
        Serial.printf("OK:MOTOR:SPEED:%lu\n", currentSpeedSPS);
    }
    else if (strncmp(cmd, "MOTOR:MOVE:", 11) == 0) {
        int32_t steps = atoi(cmd + 11);
        currentMode = MODE_POSITION;
        portENTER_CRITICAL(&timerMux);
        targetPosition = currentPosition + steps;
        isRunning = true;
        portEXIT_CRITICAL(&timerMux);
        Serial.printf("OK:MOTOR:MOVE:%ld\n", steps);
    }
    else if (strcmp(cmd, "MOTOR:ZERO") == 0) {
        portENTER_CRITICAL(&timerMux);
        currentPosition = 0;
        targetPosition = 0;
        portEXIT_CRITICAL(&timerMux);
        Serial.println("OK:MOTOR:ZERO");
    }
    else if (strcmp(cmd, "STATUS") == 0) {
        float rpm = ((float)currentSpeedSPS / STEPS_PER_REV) * 60.0;
        Serial.printf("MSTAT:RUN=%d,DIR=%s,SPD=%lu,RPM=%.1f,POS=%lld,ENA=%d,MODE=%d\n",
                      isRunning ? 1 : 0, isDirCW ? "CW" : "CCW", currentSpeedSPS, rpm,
                      currentPosition, isEnabled ? 1 : 0, (int)currentMode);
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

// ================= HÀM SETUP =================
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== HE THONG ESP32-S3 + TM1638 + LCD + JMC 2HSS57 DRIVER ===");

    // 1. Cấu hình chân GPIO Driver JMC 2HSS57
    pinMode(PIN_MOTOR_PUL, OUTPUT);
    pinMode(PIN_MOTOR_DIR, OUTPUT);
    pinMode(PIN_MOTOR_ENA, OUTPUT);
    pinMode(PIN_MOTOR_ALM, INPUT_PULLUP);

    digitalWrite(PIN_MOTOR_PUL, LOW);
    digitalWrite(PIN_MOTOR_DIR, HIGH); // Mặc định CW
    digitalWrite(PIN_MOTOR_ENA, LOW);  // LOW = Enable (Khóa trục/Sẵn sàng chạy) cho Driver 2HSS57

    // 2. Cấu hình Timer ngắt phần cứng cho phát xung bước mượt mà
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    stepTimer = timerBegin(1000000); // 1 MHz -> 1 us tick
    timerAttachInterrupt(stepTimer, &onStepTimer);
    updateTimerSpeed(currentSpeedSPS);
#else
    stepTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(stepTimer, &onStepTimer, true);
    updateTimerSpeed(currentSpeedSPS);
    timerAlarmEnable(stepTimer);
#endif

    // 3. Khởi tạo bus I2C & Màn hình LCD 1602
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
    lcd->print("JMC 2HSS57 READY");
    lcd->setCursor(0, 1);
    lcd->print("MOTOR CONTROLLER");

    // 4. Khởi tạo TM1638
    tm.begin();
    tm.setBrightness(7);
    tm.testAll();
    delay(600);
    tm.clear();

    Serial.println("[SYSTEM] KHOI TAO THANH CONG!");
}

// ================= HÀM LOOP CHÍNH =================
void loop() {
    // 1. Nhận và thực thi lệnh Serial từ PC
    handleSerial();

    // 2. Đọc phím TM1638 với chống rung
    uint8_t reading = tm.readButtons();
    if (reading != rawButtons) {
        rawButtons = reading;
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        if (rawButtons != lastStableButtons) {
            uint8_t pressed = rawButtons & ~lastStableButtons; // Phím vừa được nhấn xuống
            lastStableButtons = rawButtons;

            if (pressed != 0) {
                handleButtonPress(pressed);
            }

            // Xử lý chế độ JOG (Nhấn giữ chạy, nhả dừng)
            if (currentMode == MODE_JOG) {
                if (lastStableButtons & (1 << 0)) {
                    isRunning = true;
                } else if (lastStableButtons == 0) {
                    isRunning = false;
                }
            }
        }
    }

    // 3. Cập nhật màn hình LCD I2C định kỳ (mỗi 150ms)
    if (millis() - lastLcdUpdate >= 150) {
        lastLcdUpdate = millis();
        updateLcdDisplay();
        updateTM1638Display();
    }

    // 4. Báo cáo trạng thái lên Serial định kỳ (mỗi 500ms)
    if (millis() - lastSerialReport >= 500) {
        lastSerialReport = millis();
        float rpm = ((float)currentSpeedSPS / STEPS_PER_REV) * 60.0;
        Serial.printf("POS:%lld | SPD:%lu SPS (%.1f RPM) | DIR:%s | RUN:%d | MODE:%d\n",
                      currentPosition, currentSpeedSPS, rpm, isDirCW ? "CW" : "CCW", isRunning ? 1 : 0, (int)currentMode);
    }
}
