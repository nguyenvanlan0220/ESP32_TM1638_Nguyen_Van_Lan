#ifndef TM1638_DRIVER_H
#define TM1638_DRIVER_H

#include <Arduino.h>

/**
 * @brief Driver TM1638 chuẩn, tối ưu xung nhịp cho ESP32-S3 (240MHz)
 * Sử dụng bộ đệm 16-byte cập nhật liên tục để đảm bảo 100% hiển thị ổn định.
 */
class TM1638_Driver {
private:
    uint8_t _stbPin;
    uint8_t _clkPin;
    uint8_t _dioPin;
    uint8_t _brightness;
    bool _displayOn;
    uint8_t _buffer[16]; // 16 byte RAM của TM1638

    void writeByte(uint8_t data);
    uint8_t readByte();
    void sendCommand(uint8_t cmd);

public:
    TM1638_Driver(uint8_t stbPin, uint8_t clkPin, uint8_t dioPin);
    
    // Khởi tạo chân GPIO và bật màn hình
    void begin();
    
    // Đẩy toàn bộ bộ đệm 16-byte ra màn hình TM1638
    void update();
    
    // Cài đặt độ sáng (0 -> 7)
    void setBrightness(uint8_t brightness, bool on = true);
    
    // Xóa sạch màn hình và tắt LED
    void clear();
    
    // Bật tất cả LED và số 8. để test mạch
    void testAll();
    
    // Điều khiển LED đơn (position: 0 -> 7)
    void setLED(uint8_t position, bool state);
    void setLEDs(uint8_t mask);
    
    // Điều khiển LED 7 đoạn (position: 0 -> 7)
    void setDigit(uint8_t position, uint8_t digit, bool dot = false);
    void setChar(uint8_t position, char c, bool dot = false);
    void setString(const char* str);
    void setNumber(long number);
    void setHex(uint32_t number);
    
    // Đọc trạng thái 8 nút bấm (S1 -> S8)
    uint8_t readButtons();
};

#endif // TM1638_DRIVER_H
