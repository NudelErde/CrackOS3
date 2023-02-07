#pragma once
#include "int.h"
#include "out/printer.h"

namespace VGA::Text {

struct Dimensions {
    uint16_t width;
    uint16_t height;
};

struct Color {
    enum {
        Black = 0,
        Blue = 1,
        Green = 2,
        Cyan = 3,
        Red = 4,
        Magenta = 5,
        Brown = 6,
        LightGray = 7,
        DarkGray = 8,
        LightBlue = 9,
        LightGreen = 10,
        LightCyan = 11,
        LightRed = 12,
        LightMagenta = 13,
        Yellow = 14,
        White = 15
    };
    uint8_t foreground : 4;
    uint8_t background : 4;
};
static_assert(sizeof(Color) == sizeof(uint8_t), "Color must be 1 byte");

void init();
bool has_init();
uint32_t getCharIndex();
void setCharIndex(uint32_t index);
const Dimensions& getDimensions();
void setDimensions(const Dimensions& dimensions);
uint16_t getCursorX();
uint16_t getCursorY();
void setCursor(uint16_t x, uint16_t y);
void setChar(uint16_t x, uint16_t y, char c, Color color);
void setChar(uint32_t index, char c, Color color);
void print(char c, Color color = {Color::White, Color::Black});
void print(const char* str, Color color = {Color::White, Color::Black});
void print(uint64_t value, Color color = {Color::White, Color::Black}, uint8_t base = 10, uint8_t minDigits = 0);
void print(int64_t value, Color color = {Color::White, Color::Black}, uint8_t base = 10, uint8_t minDigits = 0);
inline void print(int8_t value, Color color = {Color::White, Color::Black}, uint8_t base = 10, uint8_t minDigits = 0) {
    print((int64_t)value, color, base, minDigits);
}
inline void print(int16_t value, Color color = {Color::White, Color::Black}, uint8_t base = 10, uint8_t minDigits = 0) {
    print((int64_t)value, color, base, minDigits);
}
inline void print(int32_t value, Color color = {Color::White, Color::Black}, uint8_t base = 10, uint8_t minDigits = 0) {
    print((int64_t)value, color, base, minDigits);
}
inline void print(uint8_t value, Color color = {Color::White, Color::Black}, uint8_t base = 10, uint8_t minDigits = 0) {
    print((uint64_t)value, color, base, minDigits);
}
inline void print(uint16_t value, Color color = {Color::White, Color::Black}, uint8_t base = 10, uint8_t minDigits = 0) {
    print((uint64_t)value, color, base, minDigits);
}
inline void print(uint32_t value, Color color = {Color::White, Color::Black}, uint8_t base = 10, uint8_t minDigits = 0) {
    print((uint64_t)value, color, base, minDigits);
}
void clear();

struct VGAPrinter : public printer_impl{
    void print(char c) override {
        VGA::Text::print(c, color);
    }
    void print(const char* str) override {
        VGA::Text::print(str, color);
    }
    void print(int8_t value, uint8_t base, uint8_t minDigits) override {
        VGA::Text::print((int64_t) value, color, base, minDigits);
    }
    void print(int16_t value, uint8_t base, uint8_t minDigits) override {
        VGA::Text::print((int64_t) value, color, base, minDigits);
    }
    void print(int32_t value, uint8_t base, uint8_t minDigits) override {
        VGA::Text::print((int64_t) value, color, base, minDigits);
    }
    void print(int64_t value, uint8_t base, uint8_t minDigits) override {
        VGA::Text::print(value, color, base, minDigits);
    }
    void print(uint8_t value, uint8_t base, uint8_t minDigits) override {
        VGA::Text::print((uint64_t) value, color, base, minDigits);
    }
    void print(uint16_t value, uint8_t base, uint8_t minDigits) override {
        VGA::Text::print((uint64_t) value, color, base, minDigits);
    }
    void print(uint32_t value, uint8_t base, uint8_t minDigits) override {
        VGA::Text::print((uint64_t) value, color, base, minDigits);
    }
    void print(uint64_t value, uint8_t base, uint8_t minDigits) override {
        VGA::Text::print(value, color, base, minDigits);
    }
    inline Color& getColor() {
        return color;
    }
private:
    explicit VGAPrinter(Color color = {Color::White, Color::Black}) : color(color) {}
    Color color{};
    friend VGAPrinter getPrinter(Color color);
};

VGAPrinter getPrinter(Color color = {Color::White, Color::Black});

}// namespace VGA::Text
