//
// Created by nudelerde on 16.12.22.
//

#include "out/log.h"

namespace Log {

static Level level = Level::Info;

void set_level(Level l) {
    level = l;
}
Level get_level() {
    return level;
}

}