#include "TM1638_Driver.h"

// Bảng mã 7-segment cho các ký tự 0-F (Common Cathode)
static const uint8_t PROGMEM FONT_7SEG[] = {
    0x3F, // 0: A B C D E F
    0x06, // 1: B C
    0x5B, // 2: A B D E G
    0x4F, // 3: A B C D G
    0x66, // 4: B C F G
    0x6D, // 5: A C D F G
    0x7D, // 6: A C D E F G
    0x07, // 7: A B C
    0x7F, // 8: A B C D E F G
    0x6F, // 9: A B C D F G
    0x77, // A
    0x7C, // b
    0x39, // C
    0x5E, // d
    0x79, // E
    0x71  // F
};

TM1638_Driver::TM1638_Driver(uint8_t stbPin, uint8_t clkPin, uint8_t dioPin) {
    _stbPin = stbPin;
    _clkPin = clkPin;
    _dioPin = dioPin;
    _brightness = 7; // Độ sáng tối đa mặc định
    _displayOn = true;
    memset(_buffer, 0, sizeof(_buffer));
}

void TM1638_Driver::begin() {
    pinMode(_stbPin, OUTPUT);
    pinMode(_clkPin, OUTPUT);
    pinMode(_dioPin, OUTPUT);

    digitalWrite(_stbPin, HIGH);
    digitalWrite(_clkPin, HIGH);
    digitalWrite(_dioPin, HIGH);

    delay(10);
    clear();
}

void TM1638_Driver::writeByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(_clkPin, LOW);
        digitalWrite(_dioPin, (data & (1 << i)) ? HIGH : LOW);
        delayMicroseconds(3);
        digitalWrite(_clkPin, HIGH);
        delayMicroseconds(3);
    }
}

uint8_t TM1638_Driver::readByte() {
    uint8_t data = 0;
    pinMode(_dioPin, INPUT_PULLUP);
    delayMicroseconds(3);
    
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(_clkPin, LOW);
        delayMicroseconds(3);
        if (digitalRead(_dioPin)) {
            data |= (1 << i);
        }
        digitalWrite(_clkPin, HIGH);
        delayMicroseconds(3);
    }
    
    pinMode(_dioPin, OUTPUT);
    return data;
}

void TM1638_Driver::sendCommand(uint8_t cmd) {
    digitalWrite(_stbPin, LOW);
    delayMicroseconds(3);
    writeByte(cmd);
    delayMicroseconds(3);
    digitalWrite(_stbPin, HIGH);
    delayMicroseconds(3);
}

void TM1638_Driver::update() {
    // 1. Gửi lệnh ghi dữ liệu tự động tăng địa chỉ (0x40)
    sendCommand(0x40);

    // 2. Gửi địa chỉ bắt đầu 0xC0 và toàn bộ 16 byte RAM
    digitalWrite(_stbPin, LOW);
    delayMicroseconds(3);
    writeByte(0xC0);
    for (uint8_t i = 0; i < 16; i++) {
        writeByte(_buffer[i]);
    }
    delayMicroseconds(3);
    digitalWrite(_stbPin, HIGH);
    delayMicroseconds(3);

    // 3. Gửi lệnh bật hiển thị và độ sáng (0x88 | brightness)
    sendCommand(_displayOn ? (0x88 | (_brightness & 0x07)) : 0x80);
}

void TM1638_Driver::setBrightness(uint8_t brightness, bool on) {
    _brightness = (brightness > 7) ? 7 : brightness;
    _displayOn = on;
    sendCommand(_displayOn ? (0x88 | (_brightness & 0x07)) : 0x80);
}

void TM1638_Driver::clear() {
    memset(_buffer, 0, sizeof(_buffer));
    update();
}

void TM1638_Driver::testAll() {
    memset(_buffer, 0xFF, sizeof(_buffer));
    update();
}

void TM1638_Driver::setLED(uint8_t position, bool state) {
    if (position >= 8) return;
    // Địa chỉ LED nằm ở các byte lẻ: 1, 3, 5, 7, 9, 11, 13, 15
    _buffer[(position * 2) + 1] = state ? 0x01 : 0x00;
}

void TM1638_Driver::setLEDs(uint8_t mask) {
    for (uint8_t i = 0; i < 8; i++) {
        setLED(i, (mask & (1 << i)) != 0);
    }
}

void TM1638_Driver::setDigit(uint8_t position, uint8_t digit, bool dot) {
    if (position >= 8 || digit > 15) return;
    uint8_t seg = pgm_read_byte(&FONT_7SEG[digit]);
    if (dot) seg |= 0x80;
    // Địa chỉ 7-segment nằm ở các byte chẵn: 0, 2, 4, 6, 8, 10, 12, 14
    _buffer[position * 2] = seg;
}

void TM1638_Driver::setChar(uint8_t position, char c, bool dot) {
    if (position >= 8) return;
    uint8_t seg = 0x00;
    if (c >= '0' && c <= '9') {
        seg = pgm_read_byte(&FONT_7SEG[c - '0']);
    } else if (c >= 'A' && c <= 'F') {
        seg = pgm_read_byte(&FONT_7SEG[c - 'A' + 10]);
    } else if (c >= 'a' && c <= 'f') {
        seg = pgm_read_byte(&FONT_7SEG[c - 'a' + 10]);
    } else if (c == '-') {
        seg = 0x40; // G segment
    } else if (c == '_') {
        seg = 0x08; // D segment
    } else if (c == ' ') {
        seg = 0x00;
    } else if (c == 'H' || c == 'h') {
        seg = 0x76;
    } else if (c == 'L' || c == 'l') {
        seg = 0x38;
    } else if (c == 'P' || c == 'p') {
        seg = 0x73;
    } else if (c == 'O' || c == 'o') {
        seg = 0x3F;
    } else if (c == 'U' || c == 'u') {
        seg = 0x3E;
    } else if (c == 'S' || c == 's') {
        seg = 0x6D;
    } else if (c == 'E' || c == 'e') {
        seg = 0x79;
    } else if (c == 'N' || c == 'n') {
        seg = 0x37;
    }
    
    if (dot) seg |= 0x80;
    _buffer[position * 2] = seg;
}

void TM1638_Driver::setString(const char* str) {
    uint8_t pos = 0;
    while (*str && pos < 8) {
        char c = *str;
        bool dot = false;
        if (*(str + 1) == '.') {
            dot = true;
            str++;
        }
        setChar(pos++, c, dot);
        str++;
    }
    while (pos < 8) {
        setChar(pos++, ' ', false);
    }
}

void TM1638_Driver::setNumber(long number) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%8ld", number);
    setString(buf);
}

void TM1638_Driver::setHex(uint32_t number) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%08X", number);
    setString(buf);
}

uint8_t TM1638_Driver::readButtons() {
    uint8_t buttons = 0;
    
    digitalWrite(_stbPin, LOW);
    delayMicroseconds(3);
    writeByte(0x42); // Lệnh đọc dữ liệu phím quét
    delayMicroseconds(3);
    
    uint8_t b[4];
    for (uint8_t i = 0; i < 4; i++) {
        b[i] = readByte();
    }
    digitalWrite(_stbPin, HIGH);
    delayMicroseconds(3);

    // Ghép 4 byte quét thành 8 nút (S1..S8)
    for (uint8_t i = 0; i < 4; i++) {
        if (b[i] & 0x01) buttons |= (1 << i);       // S1 -> S4
        if (b[i] & 0x10) buttons |= (1 << (i + 4)); // S5 -> S8
    }

    return buttons;
}
