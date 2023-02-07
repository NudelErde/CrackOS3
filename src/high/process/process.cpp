//
// Created by nudelerde on 30.01.23.
//

#include "process/process.h"
#include "ACPI/APIC.h"
#include "asm/util.h"
#include "data/array.h"
#include "features/lock.h"
#include "file/file.h"
#include "interrupt/interrupt.h"
#include "process/scheduler.h"

namespace proc {

extern "C" __attribute__((optimize("O0"))) void enter(uint64_t stack, uint64_t code, uint64_t* old_stack, uint64_t* old_code) {
    asm volatile(R"(
        push %%rax
        push %%rbx
        push %%rcx
        push %%rdx
        push %%rbp
        push %%rsi
        push %%rdi
        push %%r8
        push %%r9
        push %%r10
        push %%r11
        push %%r12
        push %%r13
        push %%r14
        push %%r15
        mov %%rsp, (%2)
        lea continue_from_here(%%rip), %%r15
        mov %%r15, (%3)
        mov %0, %%rsp
        jmp *%1
continue_from_here:
        pop %%r15
        pop %%r14
        pop %%r13
        pop %%r12
        pop %%r11
        pop %%r10
        pop %%r9
        pop %%r8
        pop %%rdi
        pop %%rsi
        pop %%rbp
        pop %%rdx
        pop %%rcx
        pop %%rbx
        pop %%rax
)" ::"a"(stack),
                 "b"(code), "c"(old_stack), "d"(old_code));
}

static bool has_init = false;
static execute_context* kernel_context = nullptr;
static thread** current_thread = nullptr;
static spinlock lock;
extern "C" void syscall();
asm(R"(
    .text
    .globl syscall
    .type syscall, @function
syscall:
    call syscall_handler
    iretq
)");

static void init() {
    if (has_init) return;
    kernel_context = new execute_context[APIC::get_core_count()];
    current_thread = new thread*[APIC::get_core_count()];
    Interrupt::setNativeInterruptHandler(VirtualAddress(syscall), 0x80, 0);
    has_init = true;
}

static execute_context& get_kernel_context() {
    init();
    return kernel_context[APIC::get_current_lapic().get_id()];
}

static thread*& access_current_thread() {
    init();
    return current_thread[APIC::get_current_lapic().get_id()];
}

void switch_to_kernel_stack() {
    auto& t = access_current_thread();
    auto& kc = get_kernel_context();
    enter(kc.stack_ptr, kc.code_ptr, &t->context.stack_ptr, &t->context.code_ptr);
}

void thread::execute() {
    load();
    get_current()->load();
    flush_cache();

    auto& kc = get_kernel_context();
    access_current_thread() = this;
    enter(context.stack_ptr, context.code_ptr, &kc.stack_ptr, &kc.code_ptr);
}

void thread::load() {
    for (auto& region : memory) {
        for (size_t i = 0; i < region.size; i += page_size) {
            PageTable::map(region.phys.address + i, region.virt + i, region.flags);
        }
    }
}
void thread::on_syscall_disown(syscall::disown_data* data) {
    if (data->target.type == syscall::process_descriptor::type_t::SHORT_DESCRIPTOR) {
        Log::error("Process", "Suicide and parricide are prohibited\n");
        return;
    }
    if (data->target.type == syscall::process_descriptor::type_t::NUMBER) {
        auto proc = get_current();
        shared_ptr<process> target;
        size_t index = 0;
        for (auto& child : proc->children) {
            if (child->pid == data->target.number) {
                target = child;
                break;
            }
            index++;
        }
        if (target) {
            target->handle_disown();
            proc->children.remove(index);
            return;
        }

        index = 0;
        for (auto& friend_weak : proc->friends) {
            auto friend_proc = friend_weak.lock();
            if (friend_proc->pid == data->target.number) {
                break;
            }
            index++;
        }
        if (index < proc->friends.size) {
            proc->friends.remove(index);
            return;
        }

        index = 0;
        for (auto& adopt_weak : proc->pending_adoption) {
            auto adopt_proc = adopt_weak.lock();
            if (adopt_proc->pid == data->target.number) {
                target = adopt_proc;
                break;
            }
            index++;
        }
        if (target) {
            target->adopter.reset();
            proc->pending_adoption.remove(index);
            return;
        }
        Log::error("process", "disown not successful: could not find id\n");
        return;
    }
    if (data->target.type == syscall::process_descriptor::type_t::STRING) {
        char* string = data->target.string;
        auto len = strlen(string);
        auto proc = get_current();
        if (len >= sizeof("child") && memcmp(string, "child", sizeof("child") - 1) == 0 && string[sizeof("child")] == ':') {
            string += sizeof("child");
            len -= sizeof("child");
            shared_ptr<process> target;
            size_t index = 0;
            for (auto& child : proc->children) {
                if (len == child->name.length && memcmp(string, child->name.data, len) == 0) {
                    target = child;
                    return;
                }
                index++;
            }
            if (target) {
                target->handle_disown();
                proc->children.remove(index);
                return;
            }
            Log::error("process", "disown not successful: could not find child\n");
            return;
        }
        if (len >= sizeof("friend") && memcmp(string, "friend", sizeof("friend") - 1) == 0 && string[sizeof("friend")] == ':') {
            string += sizeof("friend");
            len -= sizeof("friend");
            size_t index = 0;
            for (auto& friend_weak : proc->friends) {
                auto friend_proc = friend_weak.lock();
                if (len == friend_proc->name.length && memcmp(string, friend_proc->name.data, len) == 0) {
                    break;
                }
                index++;
            }
            if (index < proc->friends.size) {
                proc->friends.remove(index);
                return;
            }
            Log::error("process", "disown not successful: could not find friend\n");
            return;
        }
        if (len >= sizeof("adoption") && memcmp(string, "adoption", sizeof("adoption") - 1) == 0 && string[sizeof("adoption")] == ':') {
            string += sizeof("adoption");
            len -= sizeof("adoption");
            shared_ptr<process> target;
            size_t index = 0;
            for (auto& adopt_weak : proc->pending_adoption) {
                auto adopt_proc = adopt_weak.lock();
                if (len == adopt_proc->name.length && memcmp(string, adopt_proc->name.data, len) == 0) {
                    target = adopt_proc;
                    break;
                }
                index++;
            }
            if (target) {
                target->adopter.reset();
                proc->pending_adoption.remove(index);
                return;
            }
            Log::error("process", "disown not successful: could not find pending adoption\n");
            return;
        }
        Log::error("process", "disown not successful: invalid descriptor\n");
        return;
    }
    Log::error("process", "disown not successful: invalid descriptor\n");
}
void thread::on_syscall_adopt(syscall::adopt_data* data) {
    auto proc = get_current();
    auto target = proc->get_process_by_descriptor(data->target);
    if (target.get() == nullptr) {
        Log::error("process", "adopt not successful: could not find target\n");
        return;
    }
    if (target.get() == proc.get()) {
        Log::error("process", "adopt not successful: cannot adopt self\n");
        return;
    }
    auto parent = target->parent.lock();
    while (parent && parent.get() != parent->parent.lock().get()) {
        if (target.get() == parent.get()) {
            Log::error("process", "adopt not successful: cannot adopt (grand) parent\n");
            return;
        }
        parent = parent->parent.lock();
    }

    if (target->adopter.lock()) {
        Log::error("process", "adopt not successful: target already has an adopter\n");
        return;
    }
    target->adopter = proc;
    proc->pending_adoption.push_back(target);
}
void thread::on_syscall_make_friend(syscall::make_friend_data* data) {
    auto proc = get_current();
    auto targetA = proc->get_process_by_descriptor(data->targetA);
    auto targetB = proc->get_process_by_descriptor(data->targetB);
    if (targetA.get() == nullptr || targetB.get() == nullptr) {
        Log::error("process", "make_friend not successful: could not find target\n");
        return;
    }
    targetB->friends.push_back(targetA);
}
void thread::on_syscall_create_child(syscall::create_child_data* data) {
    auto proc = get_current();
    shared_ptr<process> child = new process();
    child->self = child;
    child->parent = proc;
    child->main_thread.owner = child;
    child->main_thread.context.code_ptr = data->entry_point;

    // stack
    auto stack = PhysicalAllocator::alloc(1_Mi / page_size).value_or_panic("out of memory, todo: implement fragmented stack");
    child->main_thread.memory.push_back(memory_area{
            .virt = VirtualAddress(32_Ti - 1_Mi),
            .phys = stack,
            .flags = {.writeable = true, .user = true, .writeThrough = false, .cacheDisabled = false},
            .size = 1_Mi});
    child->main_thread.context.stack_ptr = VirtualAddress(32_Ti - 8).address;

    // process memory
    for (size_t i = 0; i < data->memory_descriptor_count; ++i) {
        auto& desc = data->memory_descriptors[i];
        size_t size = desc.size;
        size += desc.target_virtual_address % page_size;
        size += page_size - 1;
        auto phys = PhysicalAllocator::alloc(size / page_size).value_or_panic("out of memory, todo: implement fragmented memory");
        memset(phys.mapTmp().as<uint8_t*>(), 0, size);
        memcpy(phys.mapTmp().as<uint8_t*>(), VirtualAddress(desc.source_virtual_address).as<uint8_t*>(), desc.size);
        child->memory.push_back(memory_area{
                .virt = VirtualAddress(desc.target_virtual_address),
                .phys = phys,
                .flags = {.writeable = desc.writeable, .user = true, .writeThrough = desc.write_through, .cacheDisabled = desc.cache_disabled},
                .size = size});
    }
    flush_cache();
    proc->children.push_back(child);
    scheduler::add_process(child);
}
void thread::on_syscall_set_name(syscall::set_name_data* data) {
    size_t len = strlen(data->name);
    get_current()->name = string::from_char_array(data->name, len);
}
void thread::on_syscall_list_processes(syscall::list_processes_data* data) {
    auto proc = get_current()->get_process_by_descriptor(data->target);
    if (proc.get() == nullptr) {
        Log::error("process", "list_processes not successful: could not find target\n");
        return;
    }
    proc->cleanup_dead();
    uint8_t* buffer = data->dynamic_allocation_buffer;
    size_t buffer_size = data->dynamic_allocation_buffer_size;
    data->children_total_count = proc->children.size;
    data->friends_total_count = proc->friends.size;
    data->pending_adoption_total_count = proc->pending_adoption.size;
    data->children = reinterpret_cast<syscall::process_name_descriptor*>(buffer);
    for (auto& child : proc->children) {
        if (buffer_size < sizeof(syscall::process_name_descriptor)) {
            return;
        }
        reinterpret_cast<syscall::process_name_descriptor*>(buffer)->process_id = child->pid;
        buffer += sizeof(syscall::process_name_descriptor);
        buffer_size -= sizeof(syscall::process_name_descriptor);
        data->children_count++;
    }
    data->friends = reinterpret_cast<syscall::process_name_descriptor*>(buffer);
    for (auto& friend_weak : proc->friends) {
        if (buffer_size < sizeof(syscall::process_name_descriptor)) {
            return;
        }
        if (auto friend_proc = friend_weak.lock()) {
            reinterpret_cast<syscall::process_name_descriptor*>(buffer)->process_id = friend_proc->pid;
            buffer += sizeof(syscall::process_name_descriptor);
            buffer_size -= sizeof(syscall::process_name_descriptor);
            data->friends_count++;
        }
    }
    data->pending_adoption = reinterpret_cast<syscall::process_name_descriptor*>(buffer);
    for (auto& pending_adoption_weak : proc->pending_adoption) {
        if (buffer_size < sizeof(syscall::process_name_descriptor)) {
            return;
        }
        if (auto pending_adoption_proc = pending_adoption_weak.lock()) {
            reinterpret_cast<syscall::process_name_descriptor*>(buffer)->process_id = pending_adoption_proc->pid;
            buffer += sizeof(syscall::process_name_descriptor);
            buffer_size -= sizeof(syscall::process_name_descriptor);
            data->pending_adoption_count++;
        }
    }
    size_t i = 0;
    for (auto& child : proc->children) {
        auto name_size = child->name.length;
        if (buffer_size < name_size + 1) {
            return;
        }
        data->children[i].name = reinterpret_cast<char*>(buffer);
        memcpy(buffer, child->name.data, name_size);
        buffer[name_size] = 0;
        buffer += name_size + 1;
        buffer_size -= name_size + 1;
        i++;
    }
    i = 0;
    for (auto& friend_weak : proc->friends) {
        if (auto friend_proc = friend_weak.lock()) {
            auto name_size = friend_proc->name.length;
            if (buffer_size < name_size + 1) {
                return;
            }
            data->friends[i].name = reinterpret_cast<char*>(buffer);
            memcpy(buffer, friend_proc->name.data, name_size);
            buffer[name_size] = 0;
            buffer += name_size + 1;
            buffer_size -= name_size + 1;
            i++;
        }
    }
    i = 0;
    for (auto& pending_adoption_weak : proc->pending_adoption) {
        if (auto pending_adoption_proc = pending_adoption_weak.lock()) {
            auto name_size = pending_adoption_proc->name.length;
            if (buffer_size < name_size + 1) {
                return;
            }
            data->pending_adoption[i].name = reinterpret_cast<char*>(buffer);
            memcpy(buffer, pending_adoption_proc->name.data, name_size);
            buffer[name_size] = 0;
            buffer += name_size + 1;
            buffer_size -= name_size + 1;
            i++;
        }
    }
}

static size_t null_terminated_length(VirtualAddress va, size_t width) {
    auto ptr = va.as<uint8_t*>();
    size_t size = 0;
    for(;;++size) {
        bool found_null = true;
        for(size_t i = 0; i < width; ++i) {
            if(ptr[size * width + i] != 0) {
                found_null = false;
                break;
            }
        }
        if(found_null) {
            return size;
        }
    }
}

extern "C" uint64_t call_indirect(uint64_t function_pointer,
                                  uint64_t* arguments, size_t argument_count);
asm(R"(
    .globl call_indirect
    .type call_indirect, @function
call_indirect:
    push %rbp
    mov %rsp, %rbp
    push %r15
    mov %rdx, %r15
    test $1, %rdx
    jz call_indirect_prepare_stack
    cmpq $6, %rdx
    jle call_indirect_stack_ready
    subq $8, %rsp
call_indirect_prepare_stack:
    cmpq $6, %rdx
    jle call_indirect_stack_ready
    movq -8(%rsi, %rdx, 8), %rax
    push %rax
    subq $1, %rdx
    jmp call_indirect_prepare_stack
call_indirect_stack_ready:
    mov %rdi, %rax
    mov %rsi, %rdi
    cmpq $5, %r15
    jle call_indirect_r9_done
    mov 40(%rdi), %r9
call_indirect_r9_done:
    cmpq $4, %r15
    jle call_indirect_r8_done
    mov 32(%rdi), %r8
call_indirect_r8_done:
    cmpq $3, %r15
    jle call_indirect_rcx_done
    mov 24(%rdi), %rcx
call_indirect_rcx_done:
    cmpq $2, %r15
    jle call_indirect_rdx_done
    mov 16(%rdi), %rdx
call_indirect_rdx_done:
    cmpq $1, %r15
    jle call_indirect_rsi_done
    mov 8(%rdi), %rsi
call_indirect_rsi_done:
    cmpq $0, %r15
    jle call_indirect_rdi_done
    mov (%rdi), %rdi
call_indirect_rdi_done:
    call *%rax
    cmp $6, %r15
    jle call_indirect_stack_cleanup_done
    sub $6, %r15
    test $1, %r15
    jz call_indirect_fixed_arg_cleanup_count
    addq $1, %r15
call_indirect_fixed_arg_cleanup_count:
    salq $3, %r15
    addq %r15, %rsp
call_indirect_stack_cleanup_done:
    pop %r15
    pop %rbp
    ret
)");

void thread::on_syscall_send_message(syscall::send_message_data* data) {
    auto proc = get_current()->get_process_by_descriptor(data->target);
    if (proc.get() == nullptr) {
        Log::error("process", "send_message not successful: could not find target\n");
        return;
    }
    if(data->method_id >= proc->methods.size) {
        Log::error("process", "send_message not successful: method id out of bounds\n");
        return;
    }
    working_in.push_back(proc);
    auto& method = proc->methods[data->method_id];

    linked_list<VirtualAddress> area_keys;
    auto cleanup = [&](){
        for(auto& key : area_keys) {
            proc->method_call_argument_memory.remove(key);
        }
        working_in.pop_back();
    };
    size_t data_arg_index = 0;
    for(size_t i = 0; i < method.argument_count; ++i) {
        const auto& arg = method.arguments[i];
        size_t size = 0;
        uint64_t ptr = 0;
        if(arg.type == process::method_descriptor::argument_descriptor::type_t::fixed_length) {
            size = arg.width * arg.length;
            ptr = data->arguments[data_arg_index];
        } else if(arg.type == process::method_descriptor::argument_descriptor::type_t::dynamic_length) {
            if(data_arg_index + 1 >= data->argument_count) {
                Log::error("process", "send_message not successful: not enough arguments\n");
                cleanup();
                return;
            }
            ptr = data->arguments[data_arg_index];
            size = arg.width * data->arguments[data_arg_index + 1];
            data_arg_index++;
        } else if(arg.type == process::method_descriptor::argument_descriptor::type_t::null_terminated) {
            ptr = data->arguments[data_arg_index];
            size = null_terminated_length(data->arguments[data_arg_index], arg.width);
        }
        data_arg_index++;
        if(size != 0) {
            // update size because we can only map whole pages
            auto unused_size_at_start = ptr % page_size;
            auto unused_size_at_end = page_size - (size + unused_size_at_start) % page_size;
            size += unused_size_at_start + unused_size_at_end;
            // map
            // find free virtual area in target process
            size_t last_end = 16_Ti;
            bool found = false;
            proc->method_call_argument_memory.iterate_kv([&](const VirtualAddress& key, const memory_area& area)->bool{
                auto start = area.virt;
                if(start.address - last_end >= size) {
                    found = true;
                    return false;
                }
                last_end = start.address + area.size;

                // align to page size
                last_end = (last_end + page_size - 1) & (page_size - 1);
                return true;
            });
            if(!found) {
                if(32_Ti - last_end < size) {
                    Log::error("process", "send_message not successful: not enough virtual memory\n");
                    cleanup();
                    return;
                }
            }
            // map to that area (we may need multiple areas as source physical memory is maybe not contiguous)
            optional<memory_area> insert_area;
            for(size_t offset = 0; offset < size; offset += page_size) {
                auto phys = PageTable::get(ptr + offset);
                if(!phys) {
                    Log::error("process", "send_message not successful: could not find physical memory\n");
                    cleanup();
                    return;
                }
                if(insert_area) {
                    auto current_offset_to_area = ptr + offset - insert_area->virt.address;
                    if(insert_area->phys.address + current_offset_to_area == phys.value().address) {
                        insert_area->size += page_size;
                    } else {
                        proc->method_call_argument_memory.insert(insert_area->virt, *insert_area);
                        area_keys.push_back(insert_area->virt);
                        insert_area = memory_area{VirtualAddress(last_end + offset), phys.value(),{true, true, false, false}, page_size};
                    }
                } else {
                    insert_area = memory_area{VirtualAddress(last_end + offset), phys.value(),{true, true, false, false}, page_size};
                }
            }
            if(insert_area) {
                proc->method_call_argument_memory.insert(insert_area->virt, *insert_area);
                area_keys.push_back(insert_area->virt);
            }
        }
    }
    auto* arguments = new uint64_t[data->argument_count];
    volatile uint64_t argument_count = data->argument_count;
    memcpy(arguments, data->arguments, argument_count * sizeof(uint64_t));

    proc->load();
    data->result = call_indirect(method.call_address.address, arguments, argument_count);
    delete[] arguments;
    cleanup();
}
void thread::on_syscall_ask_abilities(syscall::ask_abilities_data* data) {
    auto proc = get_current()->get_process_by_descriptor(data->target);
    if(data->target.type == syscall::process_descriptor::type_t::SHORT_DESCRIPTOR && data->target.short_descriptor == syscall::process_descriptor::short_descriptor_t::SELF) {
        // register abilities
        proc->methods.clear();
        for(size_t i = 0; i < data->method_count; ++i) {
            auto method = data->methods[i];
            size_t name_length = strlen(method.name);
            process::method_descriptor result;
            result.name = string::from_char_array(method.name, name_length);
            result.call_address = method.entry_point;
            result.arguments = new process::method_descriptor::argument_descriptor[method.argument_count];
            result.expected_argument_count = 0;
            for(size_t j = 0; j < method.argument_count; ++j) {
                result.arguments[j] = method.arguments[j];
                result.expected_argument_count++;
                if(result.arguments[j].type == process::method_descriptor::argument_descriptor::type_t::dynamic_length) {
                    result.expected_argument_count++;
                }
            }
            result.argument_count = method.argument_count;
        }
    } else {
        // read abilities from process
        uint8_t* buffer = data->dynamic_allocation_buffer;
        size_t buffer_size = data->dynamic_allocation_buffer_size;
        data->total_method_count = proc->methods.size;
        data->method_count = 0;
        data->methods = reinterpret_cast<syscall::method_descriptor*>(buffer);
        for(size_t i = 0; i < proc->methods.size; ++i) {
            if(buffer_size < sizeof(syscall::method_descriptor)) {
                break;
            }
            data->methods[i].argument_count = proc->methods[i].argument_count;
            data->methods[i].name = nullptr;
            data->methods[i].arguments = nullptr;
            data->method_count++;
            buffer_size -= sizeof(syscall::method_descriptor);
            buffer += sizeof(syscall::method_descriptor);
        }
        for(size_t i = 0; i < proc->methods.size; ++i) {
            if(buffer_size < sizeof(syscall::method_descriptor) * proc->methods[i].argument_count) {
                break;
            }
            data->methods[i].arguments = reinterpret_cast<syscall::argument_descriptor*>(buffer);
            for(size_t j = 0; j < proc->methods[i].argument_count; ++j) {
                data->methods[i].arguments[j] = proc->methods[i].arguments[j];
            }
            buffer_size -= sizeof(syscall::method_descriptor) * proc->methods[i].argument_count;
            buffer += sizeof(syscall::method_descriptor) * proc->methods[i].argument_count;
        }
        for(size_t i = 0; i < proc->methods.size; ++i) {
            size_t len = proc->methods[i].name.length + 1;
            if(buffer_size < len) {
                break;
            }
            data->methods[i].name = reinterpret_cast<char*>(buffer);
            memcpy(buffer, proc->methods[i].name.data, len - 1);
            buffer[len - 1] = 0;
            buffer_size -= len;
            buffer += len;
        }
    }
}

void process::load() {
    for (auto& region : memory) {
        for (size_t i = 0; i < region.size; i += page_size) {
            PageTable::map(region.phys.address + i, region.virt + i, region.flags);
        }
    }

    method_call_argument_memory.iterate_kv([](auto& key, auto& region) -> bool {
        for (size_t i = 0; i < region.size; i += page_size) {
            PageTable::map(region.phys.address + i, region.virt + i, region.flags);
        }
        return true;
    });
}

extern "C" [[maybe_unused]] void syscall_handler(void* syscallStruct, uint64_t syscallNumber) {
    lock_guard guard(lock);
    auto ptr = access_current_thread();
    if (ptr == nullptr) {
        Log::fatal("Syscall", "Syscall %d called without active thread\n", syscallNumber);
        return;
    }
    switch (syscallNumber) {
        case 0:
            ptr->on_syscall_disown(static_cast<syscall::disown_data*>(syscallStruct));
            return;
        case 1:
            ptr->on_syscall_adopt(static_cast<syscall::adopt_data*>(syscallStruct));
            return;
        case 2:
            ptr->on_syscall_make_friend(static_cast<syscall::make_friend_data*>(syscallStruct));
            return;
        case 3:
            ptr->on_syscall_create_child(static_cast<syscall::create_child_data*>(syscallStruct));
            return;
        case 4:
            ptr->on_syscall_set_name(static_cast<syscall::set_name_data*>(syscallStruct));
            return;
        case 5:
            ptr->on_syscall_list_processes(static_cast<syscall::list_processes_data*>(syscallStruct));
            return;
        case 6:
            ptr->on_syscall_send_message(static_cast<syscall::send_message_data*>(syscallStruct));
            return;
        case 7:
            ptr->on_syscall_ask_abilities(static_cast<syscall::ask_abilities_data*>(syscallStruct));
            return;
        case 69:
            Log::fatal("Syscall", "Debug syscall called from %s, halting\n", ptr->owner.lock()->name);
            return;
        default:
            Log::fatal("Syscall", "Syscall %d not implemented yet\n", syscallNumber);
            return;
    }
}

static pid_t next_pid = 0;
process::process() {
    pid = next_pid++;
}

static process* find_process(process* self, char* descriptor, bool with_adoption = false) {
    auto len = strlen(descriptor);
    if (len >= sizeof("child") && memcmp(descriptor, "child", sizeof("child") - 1) == 0) {
        if (descriptor[sizeof("child")] != ':') return nullptr;
        descriptor += sizeof("child");
        size_t size = 0;
        for (; descriptor[size] && descriptor[size] != '>'; ++size) {}

        for (auto& child : self->children) {
            if (child->name.length == size && memcmp(child->name.data, descriptor, size) == 0) {
                if (descriptor[size] == '\0') return child.get();
                return find_process(child.get(), descriptor + size + 1);
            }
        }
        return nullptr;
    }
    if (len >= sizeof("friend") && memcmp(descriptor, "friend", sizeof("friend") - 1) == 0) {
        if (descriptor[sizeof("friend")] != ':') return nullptr;
        descriptor += sizeof("friend");
        size_t size = 0;
        for (; descriptor[size] && descriptor[size] != '>'; ++size) {}

        for (auto& friend_ptr : self->friends) {
            auto friend_ = friend_ptr.lock();
            if (friend_->name.length == size && memcmp(friend_->name.data, descriptor, size) == 0) {
                if (descriptor[size] == '\0') return friend_.get();
                return find_process(friend_.get(), descriptor + size + 1);
            }
        }
        return nullptr;
    }

    if (with_adoption && len >= sizeof("adoption") && memcmp(descriptor, "adoption", sizeof("adoption") - 1) == 0) {
        if (descriptor[sizeof("adoption")] != ':') return nullptr;
        descriptor += sizeof("adoption");
        size_t size = 0;
        for (; descriptor[size] && descriptor[size] != '>'; ++size) {}

        for (auto& adoption_ptr : self->pending_adoption) {
            auto adoption = adoption_ptr.lock();
            if (adoption->name.length == size && memcmp(adoption->name.data, descriptor, size) == 0) {
                if (descriptor[size] == '\0') return adoption.get();
                return nullptr;
            }
        }
        return nullptr;
    }
    return nullptr;
}

shared_ptr<process> process::get_process_by_descriptor(const syscall::process_descriptor& descriptor, bool with_adoption) {
    switch (descriptor.type) {
        case syscall::process_descriptor::type_t::SHORT_DESCRIPTOR:
            switch (descriptor.short_descriptor) {
                case syscall::process_descriptor::short_descriptor_t::SELF:
                    return self.lock();
                case syscall::process_descriptor::short_descriptor_t::PARENT:
                    return parent.lock();
                default:
                    return nullptr;
            }
        case syscall::process_descriptor::type_t::NUMBER: {
            if (pid == descriptor.number) return this;
            if (auto ptr = parent.lock(); ptr && ptr->pid == descriptor.number) return ptr.get();
            for (auto& child : children) {
                if (child->pid == descriptor.number) return child.get();
            }
            for (auto& friend_ptr : friends) {
                if (auto ptr = friend_ptr.lock(); ptr && ptr->pid == descriptor.number) return ptr.get();
            }
            for (auto& pending : pending_adoption) {
                if (auto ptr = pending.lock(); ptr && ptr->pid == descriptor.number) return ptr.get();
            }
            for (auto& child : children) {
                if (auto res = child->get_process_by_descriptor(descriptor, with_adoption)) return res;
            }
            for (auto& friend_ptr : friends) {
                if (auto ptr = friend_ptr.lock()) {
                    if (auto res = ptr->get_process_by_descriptor(descriptor, with_adoption)) return res;
                }
            }
            return nullptr;
        }
        case syscall::process_descriptor::type_t::STRING:
            return find_process(this, descriptor.string, with_adoption);
        default:
            return nullptr;
    }
}
void process::handle_disown() {
    if (auto ptr = adopter.lock()) {
        size_t adoption_index = 0;
        shared_ptr<process> self;
        for (const auto& tmp : ptr->pending_adoption) {
            if (auto self_tmp = tmp.lock(); self_tmp.get() == this) {
                self = self_tmp;
                break;
            }
            adoption_index++;
        }
        if (!self) {
            Log::warning("process", "Process wanted to be adopted by someone that doesnt want to adopt that process\n");
        } else {
            ptr->pending_adoption.remove(adoption_index);
            ptr->children.push_back(self);
        }
    }
}
void process::cleanup_dead() {
    bool work;
    do {
        work = false;
        size_t i = 0;
        for (auto& friend_ptr : friends) {
            if (!friend_ptr.lock()) {
                work = true;
                friends.remove(i);
                break;
            }
            i++;
        }
    } while (work);
    do {
        work = false;
        size_t i = 0;
        for (auto& adoption_ptr : pending_adoption) {
            if (!adoption_ptr.lock()) {
                work = true;
                pending_adoption.remove(i);
                break;
            }
            i++;
        }
    } while (work);
}
void process::add_kernel_method_by_array(const string& name, VirtualAddress call_address, const process::method_descriptor::argument_descriptor* arguments, size_t argument_count) {
    method_descriptor descriptor{};
    descriptor.name = name;
    descriptor.call_address = call_address;
    if (argument_count > 0) {
        descriptor.arguments = new method_descriptor::argument_descriptor[argument_count];
        descriptor.argument_count = argument_count;
        for (size_t i = 0; i < argument_count; ++i) {
            descriptor.arguments.get()[i] = arguments[i];
            if (arguments[i].type == method_descriptor::argument_descriptor::type_t::dynamic_length) {
                descriptor.expected_argument_count += 2;
            } else {
                descriptor.expected_argument_count += 1;
            }
        }
    }
    methods.push_back(descriptor);
}

shared_ptr<process> from_elf(file::file& file) {
    auto upper_bound = 16_Ti;

    struct header {
        uint8_t magic[4];
        uint8_t bits;
        uint8_t endian;
        uint8_t version;
        uint8_t abi;
        uint8_t padding[8];
        uint16_t type;
        uint16_t instruction_set;
        uint32_t elf_version;
        uint64_t entry_point;
        uint64_t program_header_offset;
        uint64_t section_header_offset;
        uint32_t flags;
        uint16_t header_size;
        uint16_t program_header_entry_size;
        uint16_t program_header_entry_count;
        uint16_t section_header_entry_size;
        uint16_t section_header_entry_count;
        uint16_t section_header_string_table_index;
    };
    static_assert(sizeof(header) == 64);
    header file_header{};
    file.read(&file_header, 0, sizeof(file_header));
    // check magic
    if (file_header.magic[0] != 0x7f || file_header.magic[1] != 'E' || file_header.magic[2] != 'L' || file_header.magic[3] != 'F') {
        return nullptr;
    }
    if (file_header.bits != 2) { return nullptr; }
    if (file_header.endian != 1) { return nullptr; }
    if (file_header.instruction_set != 0x3e) { return nullptr; }

    struct program_header {
        uint32_t type;
        uint32_t flags;
        uint64_t file_offset;
        uint64_t virtual_address;
        uint64_t physical_address;
        uint64_t file_size;
        uint64_t memory_size;
        uint64_t alignment;
    };
    static_assert(sizeof(program_header) == 56);

    shared_ptr<process> proc(new process());
    array<program_header> headers(file_header.program_header_entry_count, file_header.program_header_offset);
    file.read(headers.data(), file_header.program_header_offset, file_header.program_header_entry_count * file_header.program_header_offset);
    for (auto& header : headers) {
        if (header.type == 1) {
            // allocate physical memory
            if (header.virtual_address + header.memory_size > upper_bound) {
                panic("program too big");
            }
            auto page_count = (header.memory_size + page_size - 1) / page_size;
            auto phys = PhysicalAllocator::alloc(page_count).value_or_panic("out of memory, todo: implement fragmented program headers");
            proc->memory.push_back(memory_area{
                    .virt = VirtualAddress(header.virtual_address),
                    .phys = phys,
                    .flags = {.writeable = true, .user = true, .writeThrough = false, .cacheDisabled = false},
                    .size = header.memory_size});
        }
    }
    proc->load();
    flush_cache();
    for (auto& header : headers) {
        if (header.type == 1) {
            memset(VirtualAddress(header.virtual_address), 0, header.memory_size);
            // copy data
            file.read(VirtualAddress(header.virtual_address), header.file_offset, header.file_size);
        }
    }

    // allocate stack
    auto stack = PhysicalAllocator::alloc(1_Mi / page_size).value_or_panic("out of memory, todo: implement fragmented stack");
    proc->main_thread.memory.push_back(memory_area{
            .virt = VirtualAddress(upper_bound - 1_Mi),
            .phys = stack,
            .flags = {.writeable = true, .user = true, .writeThrough = false, .cacheDisabled = false},
            .size = 1_Mi});

    proc->main_thread.context.stack_ptr = VirtualAddress(upper_bound - 16).address;
    proc->main_thread.context.code_ptr = file_header.entry_point;
    proc->self = proc;
    proc->main_thread.owner = proc;
    flush_cache();
    return proc;
}

}// namespace proc