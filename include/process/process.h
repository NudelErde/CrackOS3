//
// Created by nudelerde on 30.01.23.
//

#pragma once

#include "data/linked_list.h"
#include "data/string.h"
#include "data/btree.h"
#include "features/smart_pointer.h"
#include "file/file.h"
#include "int.h"
#include "syscall_data.h"

namespace proc {

struct memory_area {
    VirtualAddress virt;
    PhysicalAddress phys;
    PageTable::Flags flags{};
    size_t size{};
};

using pid_t = uint64_t;

struct process;

struct execute_context {
    uint64_t stack_ptr;
    uint64_t code_ptr;
};

struct thread {
    execute_context context;
    weak_ptr<process> owner;
    linked_list<shared_ptr<process>> working_in;
    linked_list<memory_area> memory;

    [[nodiscard]] inline shared_ptr<process> get_current() const {
        if (working_in.size == 0) {
            return owner.lock();
        }
        return working_in.last->elem;
    }

    VirtualAddress stack_ptr;
    VirtualAddress code_ptr;

    void execute();
    void load();

    void on_syscall_disown(syscall::disown_data* data);
    void on_syscall_adopt(syscall::adopt_data* data);
    void on_syscall_make_friend(syscall::make_friend_data* data);
    void on_syscall_create_child(syscall::create_child_data* data);
    void on_syscall_set_name(syscall::set_name_data* data);
    void on_syscall_list_processes(syscall::list_processes_data* data);
    void on_syscall_send_message(syscall::send_message_data* data);
    void on_syscall_ask_abilities(syscall::ask_abilities_data* data);
};

void switch_to_kernel_stack();

struct process {
    struct method_descriptor {
        using argument_descriptor = syscall::argument_descriptor;
        static constexpr argument_descriptor uint64 = {argument_descriptor::type_t::number, 8, 0};
        static constexpr argument_descriptor uint32 = {argument_descriptor::type_t::number, 4, 0};
        static constexpr argument_descriptor uint16 = {argument_descriptor::type_t::number, 2, 0};
        static constexpr argument_descriptor uint8 = {argument_descriptor::type_t::number, 1, 0};
        static constexpr argument_descriptor int64 = {argument_descriptor::type_t::number, 8, 0};
        static constexpr argument_descriptor int32 = {argument_descriptor::type_t::number, 4, 0};
        static constexpr argument_descriptor int16 = {argument_descriptor::type_t::number, 2, 0};
        static constexpr argument_descriptor int8 = {argument_descriptor::type_t::number, 1, 0};
        static constexpr argument_descriptor c_string = {argument_descriptor::type_t::null_terminated, 1, 0};
        string name;
        shared_ptr<argument_descriptor[]> arguments;
        size_t argument_count;
        size_t expected_argument_count;
        VirtualAddress call_address;
    };
    linked_list<method_descriptor> methods;

    string name;
    pid_t pid{};
    thread main_thread;

    weak_ptr<process> parent;
    linked_list<shared_ptr<process>> children;
    linked_list<weak_ptr<process>> friends;
    linked_list<weak_ptr<process>> pending_adoption;
    weak_ptr<process> adopter;
    weak_ptr<process> self;

    linked_list<memory_area> memory; // 0 - (16TiB-stack_size)
    // shared memory between A and B could be implemented by A calling a B function and returning to A
    // that way A keeps its thread but guarantees that B is running and can map memory in the method_call_argument_memory
    btree_map<VirtualAddress, memory_area> method_call_argument_memory; // 16TiB - 32TiB

    template<typename ...ArgumentDescriptor>
    void add_kernel_method(const string& method_name, VirtualAddress call_address, ArgumentDescriptor... descriptors) {
        method_descriptor::argument_descriptor args[] { descriptors... };
        add_kernel_method_by_array(method_name, call_address, args, sizeof...(ArgumentDescriptor));
    }
    void add_kernel_method_by_array(const string& name, VirtualAddress call_address, const method_descriptor::argument_descriptor* arguments, size_t argument_count);

    void cleanup_dead();

    process();

    void handle_disown();

    void load();

    shared_ptr<process> get_process_by_descriptor(const syscall::process_descriptor& descriptor, bool with_adoption = false);
};

shared_ptr<process> from_elf(file::file& file);

}// namespace proc
