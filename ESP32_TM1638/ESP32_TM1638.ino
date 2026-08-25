/**
 * ============================================================================
 * Dự án: ESP32-S3 & TM1638 - Điều Khiển Song Song Hai Chiều (Font 7 Đoạn Đầy Đủ)
 * Tác giả: Nguyễn Văn Lân
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI CHÂN (PINOUT):
 * TM1638 Module        ESP32-S3
 * --------------------------------
 * VCC             ->   5V (hoặc VIN/VBUS)
 * GND             ->   GND
 * STB (Strobe)    ->   GPIO 15
 * CLK (Clock)     ->   GPIO 16
 * DIO (Data)      ->   GPIO 17
 * ============================================================================
 */

#include <TM1638plus.h>

// Định nghĩa chân TM1638
#define STB_PIN   15
#define CLK_PIN   16
#define DIO_PIN   17

// Tham số high_freq = true cho chip ESP32-S3 (240MHz)
TM1638plus tm(STB_PIN, CLK_PIN, DIO_PIN, true);

// Biến trạng thái hệ thống
uint32_t counter = 0;
uint32_t lastCounterUpdate = 0;
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

// Bảng mã Font 7 đoạn chuẩn tối ưu cho tất cả các ký tự A-Z, 0-9 và ký hiệu
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
        case 'M': return 0x37; // M (gần giống n)
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
        case 'X': return 0x76; // H / X
        case 'Y': return 0x6E; // y
        case 'Z': return 0x5B; // Z

        // Ký tự đặc biệt
        case '-': return 0x40; // Dấu gạch ngang
        case '_': return 0x08; // Dấu gạch dưới
        case '=': return 0x48; // Dấu bằng
        case '[': return 0x39;
        case ']': return 0x0F;
        case ' ': return 0x00; // Khoảng trắng
        case 'o': return 0x5C; // o nhỏ
        case 'c': return 0x58; // c nhỏ
        case 'h': return 0x74; // h nhỏ
        case 'u': return 0x1C; // u nhỏ
        case 'r': return 0x50; // r nhỏ
        default:  return 0x00;
    }
}

// Hàm hiển thị chuỗi ký tự bất kỳ lên 8 LED 7 đoạn (hỗ trợ cả dấu chấm ".")
void displayCustomText(const char* str) {
    uint8_t pos = 0;
    for (int i = 0; str[i] != '\0' && pos < 8; i++) {
        char c = str[i];
        if (c == '.' && pos > 0) {
            // Đã được xử lý ở ký tự trước
            continue;
        }

        uint8_t seg = get7SegSegment(c);
        // Nếu ký tự tiếp theo là dấu chấm thì bật DP (0x80)
        if (str[i + 1] == '.') {
            seg |= 0x80;
            i++;
        }

        tm.display7Seg(pos, seg);
        pos++;
    }

    // Xóa trắng các vị trí còn lại nếu chuỗi ngắn hơn 8 ký tự
    while (pos < 8) {
        tm.display7Seg(pos, 0x00);
        pos++;
    }
}

void sendStatusToGUI() {
    Serial.printf("DISP:%s\n", currentDisplayText);
    Serial.printf("LEDS:%02X\n", currentLeds);
    Serial.printf("BTN:%02X\n", lastStableButtons);
}

void executeCommand(const char* cmd) {
    if (strlen(cmd) == 0) return;

    if (strncmp(cmd, "TEXT:", 5) == 0) {
        // Lệnh TEXT: ví dụ "TEXT:HELLO" hoặc "TEXT:LAN 2026"
        const char* text = cmd + 5;
        strncpy(currentDisplayText, text, sizeof(currentDisplayText) - 1);
        currentDisplayText[sizeof(currentDisplayText) - 1] = '\0';
        
        isCounterRunning = false; // Dừng đếm khi gửi text thủ công
        displayCustomText(currentDisplayText);
        Serial.printf("DISP:%s\n", currentDisplayText);
    }
    else if (strncmp(cmd, "LEDS:", 5) == 0) {
        // Lệnh LEDS: ví dụ "LEDS:FF"
        uint8_t mask = (uint8_t) strtol(cmd + 5, NULL, 16);
        currentLeds = mask;
        tm.setLEDs(currentLeds);
        Serial.printf("LEDS:%02X\n", currentLeds);
    }
    else if (strncmp(cmd, "BRIGHTNESS:", 11) == 0) {
        // Lệnh BRIGHTNESS: ví dụ "BRIGHTNESS:7"
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
    }
    else if (strcmp(cmd, "COUNTER:RESET") == 0) {
        counter = 0;
        snprintf(currentDisplayText, sizeof(currentDisplayText), "%8lu", counter);
        displayCustomText(currentDisplayText);
        Serial.printf("DISP:%s\n", currentDisplayText);
    }
    else if (strcmp(cmd, "SYNC") == 0) {
        sendStatusToGUI();
    }
}

// Đọc Serial từng ký tự không gây nghẽn (Non-blocking)
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
    delay(300);

    // Khởi tạo TM1638
    tm.displayBegin();
    tm.brightness(currentBrightness);
    tm.reset();

    // 1. Kiểm tra phần cứng: Sáng toàn bộ trong 800ms
    for (int i = 0; i < 8; i++) {
        tm.display7Seg(i, 0xFF);
        tm.setLED(i, 1);
    }
    delay(800);

    // 2. Chào mừng
    tm.reset();
    displayCustomText("HELLO   ");
    delay(600);
    tm.reset();

    snprintf(currentDisplayText, sizeof(currentDisplayText), "%8lu", counter);
    displayCustomText(currentDisplayText);
    sendStatusToGUI();
}

void loop() {
    // 1. Xử lý nhận lệnh Serial từ máy tính (Zero-latency)
    handleSerialInput();

    // 2. Đọc phím bấm và khử dội phím (Debounce 25ms)
    uint8_t reading = tm.readButtons();
    if (reading != rawButtons) {
        rawButtons = reading;
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        if (rawButtons != lastStableButtons) {
            lastStableButtons = rawButtons;

            // Báo ngay lập tức về GUI Visual Studio
            Serial.printf("BTN:%02X\n", lastStableButtons);

            if (lastStableButtons != 0) {
                // Đang nhấn nút: Sáng LED tương ứng và hiển thị mã nút
                tm.setLEDs(lastStableButtons);
                char str[9];
                snprintf(str, sizeof(str), "Btn  %02X ", lastStableButtons);
                displayCustomText(str);

                // Nút S8: Reset bộ đếm
                if (lastStableButtons & (1 << 7)) {
                    counter = 0;
                }
            } else {
                // Nhả nút: Khôi phục lại trạng thái hiển thị
                tm.setLEDs(currentLeds);
                displayCustomText(currentDisplayText);
            }
        }
    }

    // 3. Xử lý bộ đếm tự động (Khi không có nút nào đang nhấn và đang ở chế độ đếm)
    if (isCounterRunning && lastStableButtons == 0) {
        if (millis() - lastCounterUpdate >= 100) {
            lastCounterUpdate = millis();
            counter++;

            // Hiển thị số đếm lên LED 7 đoạn
            snprintf(currentDisplayText, sizeof(currentDisplayText), "%8lu", counter);
            displayCustomText(currentDisplayText);

            // Hiệu ứng LED chạy đuổi
            uint8_t ledIndex = (counter / 2) % 8;
            currentLeds = (1 << ledIndex);
            tm.setLEDs(currentLeds);

            // Gửi dữ liệu về máy tính mỗi 200ms
            if (counter % 2 == 0) {
                Serial.printf("DISP:%s\n", currentDisplayText);
                Serial.printf("LEDS:%02X\n", currentLeds);
            }
        }
    }
}
