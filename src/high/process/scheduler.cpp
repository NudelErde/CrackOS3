//
// Created by nudelerde on 30.01.23.
//

#include "process/scheduler.h"
#include "ACPI/APIC.h"
#include "process/process.h"

namespace proc {

void scheduler::run_one_slice(duration_t duration) {
    /*APIC::get_current_lapic().notify(
            duration, [](void* self) {
                switch_to_kernel_stack();
            },
            nullptr);

    shared_ptr<process> proc;

    while (proc.get() == nullptr) {
        auto node = processes.first;
        if (node == nullptr) { return; }
        processes.remove(0);
        proc = node->elem.lock();
    }

    processes.push_back(proc);

    proc->main_thread.execute();*/
}

void proc::scheduler::add_process(const shared_ptr<process>& proc) {

}


}// namespace proc