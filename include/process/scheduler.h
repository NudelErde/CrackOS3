//
// Created by nudelerde on 30.01.23.
//

#pragma once

#include "process.h"
#include "util/time.h"
#include "features/smart_pointer.h"
#include "data/linked_list.h"

namespace proc {

namespace scheduler {
    void init();
    void add_process(const shared_ptr<process>& proc);
    void run_one_slice(duration_t duration);
}

}