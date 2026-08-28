#ifndef TM1638_DRIVER_H
#define TM1638_DRIVER_H

#include <Arduino.h>

class TM1638_Driver {
private:
    uint8_t _stbPin;
    uint8_t _clkPin;
    uint8_t _dioPin;
    uint8_t _brightness;
    bool _displayOn;
    uint8_t _buffer[16];

    void writeByte(uint8_t data);
    uint8_t readByte();
    void sendCommand(uint8_t cmd);

public:
    TM1638_Driver(uint8_t stbPin, uint8_t clkPin, uint8_t dioPin);
    void begin();
    void update();
    void setBrightness(uint8_t brightness, bool on = true);
    void clear();
    void testAll();
    void setLED(uint8_t position, bool state);
    void setLEDs(uint8_t mask);
    void setDigit(uint8_t position, uint8_t digit, bool dot = false);
    void setChar(uint8_t position, char c, bool dot = false);
    void setString(const char* str);
    void setNumber(long number);
    void setHex(uint32_t number);
    uint8_t readButtons();
};

#endif // TM1638_DRIVER_H
