#include "out/panic.h"
#include "out/vga.h"

[[noreturn]] void panic(const char* message) {
    if (VGA::Text::has_init()) {
        constexpr VGA::Text::Color color{VGA::Text::Color::Red, VGA::Text::Color::Black};
        VGA::Text::setCursor(0, VGA::Text::getDimensions().height - 1);
        VGA::Text::print("PANIC: ", color);
        VGA::Text::print(message, color);
    }
    while (true) {
        asm("cli");
        asm("hlt");
    }
}