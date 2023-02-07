//
// Created by Florian on 20.11.2022.
//

#pragma once

#include "out/log.h"

namespace Test {

struct Result {
    bool succeeded;
    const char* message;

    inline static Result success() {
        return {true, nullptr};
    }
    inline static Result failure(const char* message) {
        return {false, message};
    }
};

template<typename T>
void run_test(const char* name, T test) {
    Log::printf(Log::Info, "Tests", "Test '%s' start\n", name);
    Result result = test();
    if (result.succeeded) {
        Log::printf(Log::Info, "Tests", "Test '%s' succeeded\n", name);
    } else {
        Log::printf(Log::Fatal, "Tests", "Test '%s' failed: %s\n", name, result.message);
    }
}

}
