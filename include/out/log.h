//
// Created by Florian on 20.11.2022.
//

#pragma once

#include "panic.h"
#include "vga.h"

namespace Log {

enum Level {
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

void set_level(Level l);
Level get_level();

struct LevelGuard {
    Level prev;
    explicit LevelGuard(Level l) {
        prev = get_level();
        set_level(l);
    }
    ~LevelGuard() {
        set_level(prev);
    }
};

static VGA::Text::Color get_color(Level l) {
    switch (l) {
        case Debug:
            return {VGA::Text::Color::LightGray, VGA::Text::Color::Black};
        case Info:
            return {VGA::Text::Color::LightGreen, VGA::Text::Color::Black};
        case Warning:
            return {VGA::Text::Color::Yellow, VGA::Text::Color::Black};
        case Error:
            return {VGA::Text::Color::LightRed, VGA::Text::Color::Black};
        case Fatal:
        default:
            return {VGA::Text::Color::Red, VGA::Text::Color::Black};
    }
}

template<typename... Args>
void debug(const char* module, const char* format, Args... args) {
    printf(Level::Debug, module, format, args...);
}
template<typename... Args>
void info(const char* module, const char* format, Args... args) {
    printf(Level::Info, module, format, args...);
}
template<typename... Args>
void warning(const char* module, const char* format, Args... args) {
    printf(Level::Warning, module, format, args...);
}
template<typename... Args>
void error(const char* module, const char* format, Args... args) {
    printf(Level::Error, module, format, args...);
}
template<typename... Args>
void fatal(const char* module, const char* format, Args... args) {
    printf(Level::Fatal, module, format, args...);
}

template<typename... Args>
void printf(Level l, const char* module, const char* format, Args... args) {
    if (l < get_level()) {
        return;
    }
    auto printer_impl = VGA::Text::getPrinter(get_color(l));
    printer printer{&printer_impl};
    switch (l) {
        case Level::Debug:
            printer.print("[DEBUG|");
            break;
        case Level::Info:
            printer.print("[INFO |");
            break;
        case Level::Warning:
            printer.print("[WARN |");
            break;
        case Level::Error:
            printer.print("[ERROR|");
            break;
        case Level::Fatal:
        default:
            printer.print("[FATAL|");
            break;
    }
    printer.printf("%s] ", module);
    printer.printf(format, args...);
    if (l == Level::Fatal) {
        panic("Fatal error");
    }
}

}// namespace Log
