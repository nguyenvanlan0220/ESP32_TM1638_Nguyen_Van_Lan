/**
 * ============================================================================
 * Dự án: ESP32-S3 + TM1638 + MÀN HÌNH LCD I2C (1602/2004) (Tự Động Quét Địa Chỉ)
 * Tác giả: Nguyễn Văn Lân
 * Nhánh: develop
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI CHÂN (PINOUT CHI TIẾT):
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
 *    GND          ->   GND (Nối mass chung)
 *    SDA          ->   GPIO 8
 *    SCL          ->   GPIO 9
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1638plus.h>

// ================= CẤU HÌNH CHÂN PHẦN CỨNG =================
// Chân TM1638
#define STB_PIN   15
#define CLK_PIN   16
#define DIO_PIN   17

// Chân I2C (ESP32-S3)
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

#define LCD_COLS  16
#define LCD_ROWS  2

// Con trỏ đối tượng LCD (sẽ tự động gán đúng địa chỉ 0x27 / 0x3F khi khởi động)
LiquidCrystal_I2C* lcd = nullptr;
uint8_t currentLcdAddr = 0x27;

// Khởi tạo đối tượng TM1638
TM1638plus tm(STB_PIN, CLK_PIN, DIO_PIN, true);

// Biến trạng thái hệ thống
uint32_t counter = 0;
uint32_t lastCounterUpdate = 0;
uint32_t lastLcdUpdate = 0;
bool isCounterRunning = true;

uint8_t currentLeds = 0x00;
uint8_t lastStableButtons = 0x00;
uint8_t rawButtons = 0x00;
uint32_t lastDebounceTime = 0;
const uint32_t DEBOUNCE_DELAY_MS = 25; // Chống dội phím 25ms

uint8_t currentBrightness = 7;
char currentDisplayText[16] = "        ";

// Bộ đệm nhận lệnh Serial không nghẽn
char rxBuffer[64];
uint8_t rxIndex = 0;

// Bảng mã Font 7 đoạn chuẩn cho TM1638
uint8_t get7SegSegment(char c) {
    if (c >= 'a' && c <= 'z') c -= 32; // Chuyển chữ thường thành HOA
    
    switch (c) {
        // Chữ số 0-9
        case '0': return 0x3F;
        case '1': return 0x06;
        case '2': return 0x5B;
        case '3': return 0x4F;
        case '4': return 0x66;
        case '5': return 0x6D;
        case '6': return 0x7D;
        case '7': return 0x07;
        case '8': return 0x7F;
        case '9': return 0x6F;

        // Bảng chữ cái A-Z
        case 'A': return 0x77; // A
        case 'B': return 0x7C; // b
        case 'C': return 0x39; // C
        case 'D': return 0x5E; // d
        case 'E': return 0x79; // E
        case 'F': return 0x71; // F
        case 'G': return 0x3D; // G
        case 'H': return 0x76; // H
        case 'I': return 0x06; // I
        case 'J': return 0x1E; // J
        case 'K': return 0x75; // K
        case 'L': return 0x38; // L
        case 'M': return 0x37; // M
        case 'N': return 0x54; // n
        case 'O': return 0x3F; // O
        case 'P': return 0x73; // P
        case 'Q': return 0x67; // q
        case 'R': return 0x50; // r
        case 'S': return 0x6D; // S
        case 'T': return 0x78; // t
        case 'U': return 0x3E; // U
        case 'V': return 0x1C; // v (u)
        case 'W': return 0x1D; // w
        case 'X': return 0x76; // X / H
        case 'Y': return 0x6E; // y
        case 'Z': return 0x5B; // Z

        // Ký tự đặc biệt
        case '-': return 0x40; // Gạch ngang
        case '_': return 0x08; // Gạch dưới
        case '=': return 0x48; // Dấu bằng
        case '[': return 0x39;
        case ']': return 0x0F;
        case ' ': return 0x00; // Trắng
        default:  return 0x00;
    }
}

// Hiển thị chuỗi lên 8 LED 7 đoạn TM1638
void displayCustomText(const char* str) {
    uint8_t pos = 0;
    for (int i = 0; str[i] != '\0' && pos < 8; i++) {
        char c = str[i];
        if (c == '.' && pos > 0) continue;

        uint8_t seg = get7SegSegment(c);
        if (str[i + 1] == '.') {
            seg |= 0x80; // Bật dấu chấm DP
            i++;
        }

        tm.display7Seg(pos, seg);
        pos++;
    }

    while (pos < 8) {
        tm.display7Seg(pos, 0x00);
        pos++;
    }
}

// Cập nhật nội dung lên màn hình LCD I2C 1602 (16 ký tự mỗi dòng)
void updateLcdLines(const char* line1, const char* line2) {
    if (lcd == nullptr) return;
    char buf1[17];
    char buf2[17];
    snprintf(buf1, sizeof(buf1), "%-16s", line1);
    snprintf(buf2, sizeof(buf2), "%-16s", line2);

    lcd->setCursor(0, 0);
    lcd->print(buf1);
    lcd->setCursor(0, 1);
    lcd->print(buf2);
}

// Tự động quét địa chỉ I2C của màn hình (0x27, 0x3F...)
uint8_t scanI2CAddress() {
    Serial.println("\n[I2C] Dang quet cac thiet bi tren bus I2C...");
    uint8_t foundAddr = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            Serial.printf("[I2C] >>> TIM THAY THIET BI TAI DIA CHI: 0x%02X <<<\n", addr);
            if (foundAddr == 0) foundAddr = addr;
        }
    }
    if (foundAddr == 0) {
        Serial.println("[I2C] Canh bao: Khong tim thay thiet bi I2C nao tren GPIO 8 & 9!");
        Serial.println("[I2C] Mac dinh thu dung dia chi 0x27...");
        foundAddr = 0x27;
    }
    return foundAddr;
}

void sendStatusToGUI() {
    Serial.printf("DISP:%s\n", currentDisplayText);
    Serial.printf("LEDS:%02X\n", currentLeds);
    Serial.printf("BTN:%02X\n", lastStableButtons);
}

void executeCommand(const char* cmd) {
    if (strlen(cmd) == 0) return;

    if (strncmp(cmd, "TEXT:", 5) == 0) {
        const char* text = cmd + 5;
        strncpy(currentDisplayText, text, sizeof(currentDisplayText) - 1);
        currentDisplayText[sizeof(currentDisplayText) - 1] = '\0';
        
        isCounterRunning = false; // Dừng đếm khi đặt text thủ công
        displayCustomText(currentDisplayText);
        
        char l1[17];
        char l2[17];
        snprintf(l1, sizeof(l1), "TEXT: %-10s", currentDisplayText);
        snprintf(l2, sizeof(l2), "LED: 0x%02X  BR:%d", currentLeds, currentBrightness);
        updateLcdLines(l1, l2);

        Serial.printf("DISP:%s\n", currentDisplayText);
    }
    else if (strncmp(cmd, "LEDS:", 5) == 0) {
        uint8_t mask = (uint8_t) strtol(cmd + 5, NULL, 16);
        currentLeds = mask;
        tm.setLEDs(currentLeds);

        char l2[17];
        snprintf(l2, sizeof(l2), "LED: 0x%02X  BR:%d", currentLeds, currentBrightness);
        if (lcd != nullptr) {
            lcd->setCursor(0, 1);
            char buf2[17];
            snprintf(buf2, sizeof(buf2), "%-16s", l2);
            lcd->print(buf2);
        }

        Serial.printf("LEDS:%02X\n", currentLeds);
    }
    else if (strncmp(cmd, "BRIGHTNESS:", 11) == 0) {
        int b = atoi(cmd + 11);
        if (b >= 0 && b <= 7) {
            currentBrightness = (uint8_t)b;
            tm.brightness(currentBrightness);
        }
    }
    else if (strcmp(cmd, "COUNTER:START") == 0) {
        isCounterRunning = true;
    }
    else if (strcmp(cmd, "COUNTER:STOP") == 0) {
        isCounterRunning = false;
        char l1[17];
        snprintf(l1, sizeof(l1), "PAUSED: %-8lu", counter);
        if (lcd != nullptr) {
            lcd->setCursor(0, 0);
            char buf1[17];
            snprintf(buf1, sizeof(buf1), "%-16s", l1);
            lcd->print(buf1);
        }
    }
    else if (strcmp(cmd, "COUNTER:RESET") == 0) {
        counter = 0;
        snprintf(currentDisplayText, sizeof(currentDisplayText), "%8lu", counter);
        displayCustomText(currentDisplayText);
        updateLcdLines("RESET: 0", "TM1638 + LCD OK");
        Serial.printf("DISP:%s\n", currentDisplayText);
    }
    else if (strcmp(cmd, "SYNC") == 0) {
        sendStatusToGUI();
    }
}

void handleSerialInput() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (rxIndex > 0) {
                rxBuffer[rxIndex] = '\0';
                executeCommand(rxBuffer);
                rxIndex = 0;
            }
        } else {
            if (rxIndex < sizeof(rxBuffer) - 1) {
                rxBuffer[rxIndex++] = c;
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(400);

    // 1. Khởi tạo bus I2C cho ESP32-S3 (SDA = 8, SCL = 9, tốc độ chuẩn 100kHz)
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    delay(100);

    // 2. Tự động tìm địa chỉ I2C của màn hình LCD (0x27 hoặc 0x3F)
    currentLcdAddr = scanI2CAddress();
    lcd = new LiquidCrystal_I2C(currentLcdAddr, LCD_COLS, LCD_ROWS);
    lcd->init();
    lcd->backlight();
    lcd->clear();
    updateLcdLines("ESP32-S3 SYSTEM", "TM1638 + LCD I2C");

    // 3. Khởi tạo TM1638
    tm.displayBegin();
    tm.brightness(currentBrightness);
    tm.reset();

    // 4. Test toàn bộ LED
    for (int i = 0; i < 8; i++) {
        tm.display7Seg(i, 0xFF);
        tm.setLED(i, 1);
    }
    delay(1000);

    // 5. Chào mừng
    tm.reset();
    displayCustomText("HELLO   ");
    updateLcdLines("READY TO RUN!", "Nguyen Van Lan");
    delay(800);
    tm.reset();

    snprintf(currentDisplayText, sizeof(currentDisplayText), "%8lu", counter);
    displayCustomText(currentDisplayText);
    sendStatusToGUI();
}

void loop() {
    // 1. Xử lý nhận lệnh Serial từ GUI
    handleSerialInput();

    // 2. Đọc nút bấm TM1638 (Khử dội phím 25ms)
    uint8_t reading = tm.readButtons();
    if (reading != rawButtons) {
        rawButtons = reading;
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        if (rawButtons != lastStableButtons) {
            lastStableButtons = rawButtons;

            // Báo về GUI
            Serial.printf("BTN:%02X\n", lastStableButtons);

            if (lastStableButtons != 0) {
                tm.setLEDs(lastStableButtons);
                char str[9];
                snprintf(str, sizeof(str), "Btn  %02X ", lastStableButtons);
                displayCustomText(str);

                // Hiển thị tên các nút đang bấm lên LCD I2C
                String btnText = "BTN: ";
                for (int i = 0; i < 8; i++) {
                    if (lastStableButtons & (1 << i)) {
                        btnText += "S" + String(i + 1) + " ";
                    }
                }
                char l1[17];
                char l2[17];
                snprintf(l1, sizeof(l1), "KEY: 0x%02X", lastStableButtons);
                snprintf(l2, sizeof(l2), "%-16s", btnText.c_str());
                updateLcdLines(l1, l2);

                if (lastStableButtons & (1 << 7)) {
                    counter = 0;
                }
            } else {
                // Khôi phục hiển thị khi nhả phím
                tm.setLEDs(currentLeds);
                displayCustomText(currentDisplayText);
            }
        }
    }

    // 3. Xử lý bộ đếm tự động
    if (isCounterRunning && lastStableButtons == 0) {
        if (millis() - lastCounterUpdate >= 100) {
            lastCounterUpdate = millis();
            counter++;

            // Hiển thị LED 7 đoạn
            snprintf(currentDisplayText, sizeof(currentDisplayText), "%8lu", counter);
            displayCustomText(currentDisplayText);

            // LED đơn chạy đuổi
            uint8_t ledIndex = (counter / 2) % 8;
            currentLeds = (1 << ledIndex);
            tm.setLEDs(currentLeds);

            // Gửi về máy tính mỗi 200ms
            if (counter % 2 == 0) {
                Serial.printf("DISP:%s\n", currentDisplayText);
                Serial.printf("LEDS:%02X\n", currentLeds);
            }

            // Cập nhật màn hình LCD I2C mỗi 300ms
            if (millis() - lastLcdUpdate >= 300) {
                lastLcdUpdate = millis();
                char l1[17];
                char l2[17];
                snprintf(l1, sizeof(l1), "COUNT: %-9lu", counter);
                snprintf(l2, sizeof(l2), "LED%d  BR:%d OK", ledIndex + 1, currentBrightness);
                updateLcdLines(l1, l2);
            }
        }
    }
}
