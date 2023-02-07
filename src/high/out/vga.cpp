#include "out/vga.h"
#include "asm/regs.h"
#include "memory/mem.h"
#include "memory/paging.h"

namespace VGA::Text {

static PhysicalAddress vgaTextBuffer;
static uint32_t charIndex;
static Dimensions dimensions;

void init() {
    vgaTextBuffer = 0xB8000_phys;
    dimensions = {80, 25};
}

bool has_init() {
    return vgaTextBuffer == 0xB8000_phys;
}

static void print_ln(Color color) {
    uint16_t line = getCursorY();
    uint16_t column = getCursorX();
    // clear rest of line
    for (uint16_t i = column; i < dimensions.width; i++) {
        setChar(i, line, ' ', color);
    }
    setCursor(0, line + 1);
    if(line == dimensions.height - 1) {
        // scroll
        auto* buffer = vgaTextBuffer.mapTmp().as<uint16_t*>();
        for (size_t i = 0; i < dimensions.width * (dimensions.height - 1); i++) {
            buffer[i] = buffer[i + dimensions.width];
        }
        for (size_t i = dimensions.width * (dimensions.height - 1); i < dimensions.width * dimensions.height; i++) {
            buffer[i] = 0;
        }
        setCursor(0, dimensions.height - 1);
    }
}

uint32_t getCharIndex() {
    return charIndex;
}
void setCharIndex(uint32_t index) {
    charIndex = index;
}
const Dimensions& getDimensions() {
    return dimensions;
}
void setDimensions(const Dimensions& _dimensions) {
    uint16_t x = getCursorX();
    uint16_t y = getCursorY();
    dimensions = _dimensions;
    setCursor(x, y);
}
void setCursor(uint16_t x, uint16_t y) {
    charIndex = y * dimensions.width + x;
}
uint16_t getCursorX() {
    return charIndex % dimensions.width;
}
uint16_t getCursorY() {
    return charIndex / dimensions.width;
}
void setChar(uint16_t x, uint16_t y, char c, Color color) {
    setChar(y * dimensions.width + x, c, color);
}
void setChar(uint32_t index, char c, Color color) {
    vgaTextBuffer.mapTmp().as<char*>()[index * 2] = c;
    vgaTextBuffer.mapTmp().as<Color*>()[index * 2 + 1] = color;
}
void print(char c, Color color) {
    if(c=='\n') {
        print_ln(color);
        return;
    }
    setChar(charIndex, c, color);
    charIndex++;
}
void print(const char* str, Color color) {
    for (const char* ptr = str; *ptr; ptr++) {
        if (*ptr == '\n') {
            print_ln(color);
        } else {
            print(*ptr, color);
        }
    }
}
void print(uint64_t value, Color color, uint8_t base /*= 10*/, uint8_t minDigits /*= 0*/) {
    constexpr char digits[] = "0123456789ABCDEF";
    char buffer[64];
    memset(buffer, 0, sizeof(buffer));
    uint8_t i = 0;
    do {
        buffer[i++] = digits[value % base];
        value /= base;
    } while (value > 0);
    while (i < minDigits) {
        buffer[i++] = '0';
    }
    for (uint8_t j = 0; j < i / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = tmp;
    }
    print(buffer, color);
}
void print(int64_t value, Color color, uint8_t base /*= 10*/, uint8_t minDigits /*= 0*/) {
    if (value < 0) {
        print('-', color);
        value = -value;
    } else if (minDigits != 0) {
        print(value ? '+' : ' ', color);
    }
    print((uint64_t) value, color, base, minDigits);
}
void clear() {
    memset(vgaTextBuffer.mapTmp().as<void*>(), 0, dimensions.width * dimensions.height * 2);
}

VGAPrinter getPrinter(Color color) {
    return VGAPrinter(color);
}

}// namespace VGA::Text