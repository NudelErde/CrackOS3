//
// Created by nudelerde on 04.02.23.
//

#pragma once

#include <stdint-gcc.h>

namespace syscall {

//syscalls:
// 0 - disown / unfriend / unadopt [descriptorA]
//   - A will be removed from my children list, friends list or pending_adoption list, if they were my child and isn't adopted they will be killed
// 1 - adopt child [descriptorA]
//   - A will be in my pending_adoption list, if their parent disowns them, they will go to my children list instead of killed
// 2 - make friend [descriptorA] [descriptorB]
//   - B will be in A's friends list
// 3 - create child [memory region list] [entry point]
//   - create a process with the specified memory, start their thread at the specified entry point, add them to my children list
// 4 - set name [string]
//   - set my name to string
// 5 - list processes [descriptorA]
//   - list children and friends of the specified process (self is a really useful descriptor here)
// 6 - send message
// 7 - ask abilities [descriptorA]
//   - list valid message targets on A
// [descriptor] := [short] | number | [string_descriptor]
// [short] := self | parent
// [string_descriptor] := [step_with_pending_adoption] | [step] -> [string_descriptor]
// [step_with_pending_adoption] := [step] | adoption : [name]
// [step] := child : [name] | friend : [name]

struct process_descriptor {
    enum class type_t {
        NUMBER,
        STRING,
        SHORT_DESCRIPTOR
    };
    enum class short_descriptor_t {
        PARENT,
        SELF
    };
    type_t type;
    union {
        uint64_t number;
        char* string;
        short_descriptor_t short_descriptor;
    };
};

struct argument_descriptor {
    enum class type_t {
        number, null_terminated, fixed_length, dynamic_length
    };
    type_t type;
    uint8_t width; // in bytes
    uint64_t length; // only used for fixed_length
};

struct method_descriptor {// always has 64bit return type
    char* name;
    uint64_t argument_count;
    argument_descriptor* arguments;
    uint64_t entry_point; // only used for registration
};

struct memory_descriptor {
    uint64_t source_virtual_address;
    uint64_t target_virtual_address;
    uint64_t size;
    bool writeable;
    bool cache_disabled;
    bool write_through;
};

struct disown_data {
    process_descriptor target;
};
struct adopt_data {
    process_descriptor target;
};
struct make_friend_data {
    // B will be in A's friends list
    process_descriptor targetA;
    process_descriptor targetB;
};
struct create_child_data {
    uint64_t memory_descriptor_count;
    memory_descriptor* memory_descriptors;
    uint64_t entry_point;
};
struct set_name_data {
    char* name;
};
struct process_name_descriptor {
    uint64_t process_id;
    char* name;
};
struct list_processes_data {
    process_descriptor target;
    uint8_t* dynamic_allocation_buffer;
    uint64_t dynamic_allocation_buffer_size;
    process_name_descriptor* children;
    uint64_t children_count;
    uint64_t children_total_count;
    process_name_descriptor* friends;
    uint64_t friends_count;
    uint64_t friends_total_count;
    process_name_descriptor* pending_adoption;
    uint64_t pending_adoption_count;
    uint64_t pending_adoption_total_count;
};
struct send_message_data {
    process_descriptor target;
    uint64_t method_id;
    uint64_t argument_count;
    uint64_t* arguments;
    uint64_t result;
};
struct ask_abilities_data { // target self = register abilities
    process_descriptor target;
    method_descriptor* methods;
    uint64_t method_count;
    uint64_t total_method_count;
    uint8_t* dynamic_allocation_buffer;
    uint64_t dynamic_allocation_buffer_size;
};
}// namespace syscall
