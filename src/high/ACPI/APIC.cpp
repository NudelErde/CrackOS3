//
// Created by nudelerde on 16.12.22.
//

#include "ACPI/APIC.h"
#include "asm/io.h"
#include "asm/regs.h"
#include "asm/util.h"
#include "features/lock.h"
#include "features/math.h"
#include "interrupt/interrupt.h"
#include "out/log.h"

namespace APIC {

struct io_interrupt_source_override {
    uint8_t from;
    uint32_t to;
};

PhysicalAddress local_apic_address;
CPU_Core* cores = nullptr;
size_t core_count;
spinlock core_init_lock;
IOAPIC* ioapics = nullptr;
size_t ioapic_count;
io_interrupt_source_override* io_interrupt_source_overrides = nullptr;
size_t io_interrupt_source_override_count;

struct Table {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed));

union io_apic_entry {
    uint32_t raw;
    struct {
        uint32_t vector : 8;
        uint32_t delivery_mode : 3;
        uint32_t destination_mode : 1;
        uint32_t delivery_status : 1;
        uint32_t polarity : 1;
        uint32_t remote_irr : 1;
        uint32_t trigger_mode : 1;
        uint32_t mask : 1;
        uint32_t reserved : 15;
    };
};

static uint8_t* search(uint8_t*& iter, const uint8_t* end, uint8_t type) {
    struct Entry {
        uint8_t type;
        uint8_t length;
    } __attribute__((packed));
    while (iter < end) {
        auto* entry = reinterpret_cast<Entry*>(iter);
        if (entry->type == type) {
            uint8_t* ret = iter;
            iter += entry->length;
            return ret;
        }
        iter += entry->length;
    }
    return nullptr;
}

static size_t count_madt_entries(Table* table, uint8_t type) {
    uint8_t* iter = reinterpret_cast<uint8_t*>(table) + sizeof(Table);
    uint8_t* end = reinterpret_cast<uint8_t*>(table) + table->length;
    size_t count = 0;
    while (iter < end) {
        if (search(iter, end, type)) {
            count++;
        }
    }
    return count;
}

static void initIOAPIC();

void MADT::init() {
    auto* table = ptr.as<Table*>();
    local_apic_address = PhysicalAddress(table->local_apic_address);
    uint8_t* iter = ptr.as<uint8_t*>() + sizeof(Table);
    uint8_t* end = ptr.as<uint8_t*>() + table->length;

    // count cores
    core_count = count_madt_entries(table, 0);

    Log::printf(Log::Debug, "APIC", "Found %i cores\n", core_count);

    cores = new CPU_Core[core_count];
    uint8_t core_index = 0;

    iter = ptr.as<uint8_t*>() + sizeof(Table);
    while (iter < end) {
        if (auto res = search(iter, end, 0)) {
            struct Entry {
                uint8_t type;
                uint8_t length;
                uint8_t acpi_processor_id;
                uint8_t apic_id;
                uint32_t flags;
            } __attribute__((packed));
            auto* entry = reinterpret_cast<Entry*>(res);
            CPU_Core& core = cores[core_index];
            core.acpi_id = entry->acpi_processor_id;
            core.apic_id = entry->apic_id;
            core.os_id = core_index;
            core_index++;
            Log::printf(Log::Debug, "APIC", "Found core %i with acpi id %i and apic id %i\n", core.os_id, core.acpi_id, core.apic_id);
        }
    }

    iter = ptr.as<uint8_t*>() + sizeof(Table);
    if (auto* res = search(iter, end, 5)) {
        struct Entry {
            uint8_t type;
            uint8_t length;
            uint16_t reserved;
            uint64_t address;
        } __attribute__((packed));
        auto* entry = reinterpret_cast<Entry*>(res);
        local_apic_address = PhysicalAddress(entry->address);
    }

    // count ioapics
    ioapic_count = count_madt_entries(table, 1);
    ioapics = new IOAPIC[ioapic_count];
    iter = ptr.as<uint8_t*>() + sizeof(Table);
    uint8_t ioapic_index = 0;
    while (iter < end) {
        if (auto res = search(iter, end, 1)) {
            struct Entry {
                uint8_t type;
                uint8_t length;
                uint8_t ioapic_id;
                uint8_t reserved;
                uint32_t address;
                uint32_t global_system_interrupt_base;
            } __attribute__((packed));
            auto* entry = reinterpret_cast<Entry*>(res);
            auto& ioapic = ioapics[ioapic_index];
            ioapic.id = entry->ioapic_id;
            ioapic.address = entry->address;
            ioapic.global_system_interrupt_base = entry->global_system_interrupt_base;
            Log::printf(Log::Debug, "APIC", "Found ioapic %i with address %x and gsi base %i\n", ioapic.id, ioapic.address, ioapic.global_system_interrupt_base);
            ioapic_index++;
        }
    }

    io_interrupt_source_override_count = count_madt_entries(table, 2);
    io_interrupt_source_overrides = new io_interrupt_source_override[io_interrupt_source_override_count];

    // interrupt source override
    iter = ptr.as<uint8_t*>() + sizeof(Table);
    uint8_t io_interrupt_source_override_index = 0;
    while (iter < end) {
        if (auto res = search(iter, end, 2)) {
            struct Entry {
                uint8_t type;
                uint8_t length;
                uint8_t bus;
                uint8_t source;
                uint32_t global_system_interrupt;
                uint16_t flags;
            } __attribute__((packed));
            auto* entry = reinterpret_cast<Entry*>(res);
            auto& override = io_interrupt_source_overrides[io_interrupt_source_override_index];
            override.from = entry->source;
            override.to = entry->global_system_interrupt;
            Log::printf(Log::Debug, "APIC", "Found interrupt source override %i(%i) with gsi %i\n", entry->source, entry->bus, entry->global_system_interrupt);
            io_interrupt_source_override_index++;
        }
    }

    initIOAPIC();

    initLocalAPIC();
}

static void initIOAPIC() {
    uint32_t offset = 32;
    for (size_t i = 0; i < ioapic_count; ++i) {
        auto ioapic = ioapics[i];
        volatile auto* select = PhysicalAddress(ioapic.address).mapTmp().as<volatile uint32_t*>();
        volatile auto* window = PhysicalAddress(ioapic.address + 0x10).mapTmp().as<volatile uint32_t*>();
        uint32_t count;
        *select = 1;
        uint32_t value = *window;
        count = (((value) >> 16) & 0xFF) + 1;
        Log::printf(Log::Debug, "APIC", "Found ioapic %i with %i interrupts\n", ioapics[i].id, count);
        for (size_t j = 0; j < count; ++j) {
            uint32_t low_add = 0x10 + 2 * j;
            uint32_t high_add = 0x10 + 2 * j + 1;
            *select = low_add;
            io_apic_entry entry{};
            entry.vector = offset;
            entry.delivery_mode = 0;
            entry.destination_mode = 0;
            entry.delivery_status = 0;
            entry.polarity = 0;
            entry.trigger_mode = 0;
            entry.mask = 1;
            *window = entry.raw;
            *select = high_add;
            *window = (get_current_lapic().get_id() << 24);
            offset++;
            Interrupt::registerHandler(entry.vector, [](uint8_t vector, uint64_t error_code, void* stack_ptr, void* user_data) {
                Log::printf(Log::Fatal, "APIC", "Unhandled IO-APIC interrupt %i\n", vector - 32);
            });
        }
    }
}

struct timer_data {
    uint64_t ticks_needed;
    uint64_t prev_init;
    void (*callback)(void*);
    void* data;
};

static volatile bool pit_bootstrap_timer_running = false;
static uint32_t ticks_per_ms;
static timer_data* timer_data_array;
static uint8_t timer_interrupt;
static void initLocalAPICTimer() {
    Interrupt::Guard i_guard;
    // prepare PIT timer
    IOAPIC::interrupt_handler(
            0, [](void*) {
                pit_bootstrap_timer_running = false;
                Interrupt::sendEOI();
            },
            nullptr);
    IOAPIC::interrupt_set_mask(0, false);

    uint64_t average_count = 0;

    // prepare local APIC timer
    uint64_t repeat = 10;
    uint64_t ms = 10;
    Log::printf(Log::Debug, "APIC", "Calibrating local APIC timer\n");
    for (int i = 0; i < repeat; ++i) {
        auto apic = get_current_lapic();
        apic.divide_configuration() = 0x3;
        apic.initial_count() = 0xFFFFFFFF;
        apic.lvt_timer() = 0xFF;

        // 1ms pit timer
        ioWrite8(0x43, 0b00110000);// channel 0 | lobyte/hibyte | mode 0 | binary mode
        uint16_t pit_ticks = 0x0419 * ms;
        ioWrite8(0x40, pit_ticks & 0xFF);
        ioWrite8(0x40, pit_ticks >> 8);
        pit_bootstrap_timer_running = true;
        Interrupt::enable();
        while (pit_bootstrap_timer_running) {
            asm("pause");
        }
        Interrupt::disable();
        uint32_t count = apic.current_count();
        apic.lvt_timer() = 0xFF | (1 << 16);
        // stop timer
        count = 0xFFFFFFFF - count;
        average_count += count;
    }
    average_count /= repeat * ms;
    ticks_per_ms = average_count;
    Log::printf(Log::Debug, "APIC", "Average ticks per ms: %i\n", ticks_per_ms);
    IOAPIC::interrupt_set_mask(0, true);
    timer_data_array = new timer_data[core_count];
    timer_interrupt = Interrupt::get_free_interrupt_number();
}

LocalAPIC get_current_lapic() {
    return {local_apic_address.mapTmp()};
}

static volatile bool ap_response = false;
static dtr_t idt_ptr;
void initLocalAPIC() {
    if (cores == nullptr)
        panic("apic not initialized");
    ioWrite8(0xa1, 0xff);
    ioWrite8(0x21, 0xff);
    auto apic = get_current_lapic();
    apic.spurious_interrupt_vector() = 0x100 | 0xff;
    Interrupt::enable();

    initLocalAPICTimer();

    uint64_t trampoline_address = saveReadSymbol("trampoline_entry_16");
    if (trampoline_address == 0)
        panic("trampoline not found");
    if (trampoline_address % page_size != 0)
        panic("trampoline not page aligned");
    uint64_t trampoline_vector = trampoline_address >> 12;
    if (trampoline_vector > 0xff)
        panic("trampoline address too large");
    Log::printf(Log::Debug, "APIC", "trampoline vector: %x\n", trampoline_vector);

    auto* pagetable_ptr = PhysicalAddress(saveReadSymbol("trampoline_data_pagetable_l4")).mapTmp().as<uint64_t*>();
    *pagetable_ptr = getCR3();

    auto* gdt_ptr = PhysicalAddress(saveReadSymbol("trampoline_data_gdt_ptr")).mapTmp().as<uint64_t*>();
    asm("sgdt %0"
        : "=m"(*gdt_ptr));

    auto* stack_ptr = PhysicalAddress(saveReadSymbol("trampoline_stack_ptr")).mapTmp().as<uint64_t*>();
    idt_ptr = getIDTR();

    for (size_t i = 1; i < core_count; i++) {
        assert(apic.error_status() == 0, "APIC error");
        if (cores[i].apic_id == apic.get_id()) {
            continue;
        }

        auto stack = new uint8_t[page_size];
        *stack_ptr = (uint64_t) stack + page_size - 8;

        Log::printf(Log::Debug, "APIC", "Starting core for apic %i with stack 0x%x\n", cores[i].apic_id, *stack_ptr);

        apic.send_interrupt(LocalAPIC::InterruptType::INIT, 0, cores[i].apic_id);// 0x8000 | 0x4000 | 0x0500
        assert(apic.error_status() == 0, "APIC error");
        while (apic.is_interrupt_pending()) { asm("pause"); }
        apic.sleep(10_ms);
        apic.send_interrupt(LocalAPIC::InterruptType::DE_INIT, 0, cores[i].apic_id);
        assert(apic.error_status() == 0, "APIC error");
        while (apic.is_interrupt_pending()) { asm("pause"); }
        apic.sleep(10_ms);
        ap_response = false;

        apic.send_interrupt(LocalAPIC::InterruptType::SIPI, trampoline_vector, cores[i].apic_id);
        assert(apic.error_status() == 0, "APIC error");
        while (apic.is_interrupt_pending()) { asm("pause"); }
        apic.sleep(1_ms);

        if (!ap_response) {
            apic.send_interrupt(LocalAPIC::InterruptType::SIPI, trampoline_vector, cores[i].apic_id);
            assert(apic.error_status() == 0, "APIC error");
            while (apic.is_interrupt_pending()) { asm("pause"); }
        }
        apic.sleep(100_ms);
        while (!ap_response) {}
    }
}

CPU_Core* get_current_cpu() {
    auto id = get_current_lapic().get_id();
    if (cores == nullptr) {
        return nullptr;
    }
    for (uint64_t i = 0; i < core_count; i++) {
        if (cores[i].apic_id == id) {
            return &cores[i];
        }
    }
    return nullptr;
}
size_t get_core_count() {
    return core_count;
}

void LocalAPIC::send_interrupt(InterruptType type, uint8_t vector, uint32_t local_apic_id) {
    union interrupt_command_low_t {
        uint32_t raw;
        struct {
            uint8_t vector : 8;
            uint8_t interrupt_type : 3;
            uint8_t destination_mode : 1;
            bool delivery_status : 1;
            bool reserved : 1;
            bool level : 1;
            bool trigger_mode : 1;
            uint8_t destination_type : 2;
            uint32_t reserved2 : 12;
        };
    };
    interrupt_command_high() = local_apic_id << 24;
    interrupt_command_low_t cmd{};
    cmd.vector = vector;
    cmd.interrupt_type = static_cast<uint8_t>(type);
    cmd.level = cmd.interrupt_type == static_cast<uint8_t>(InterruptType::INIT);// level only if INIT
    if (type == InterruptType::DE_INIT) {
        cmd.interrupt_type = static_cast<uint8_t>(InterruptType::INIT);
    }
    cmd.trigger_mode = cmd.interrupt_type == static_cast<uint8_t>(InterruptType::INIT);// trigger if INIT or DE_INIT
    cmd.destination_mode = 0;                                                          // physical
    cmd.delivery_status = false;
    cmd.destination_type = 0;
    interrupt_command_low() = cmd.raw;
}
bool LocalAPIC::is_interrupt_pending() {
    asm volatile("pause"
                 :
                 :
                 : "memory");
    uint32_t cmd = interrupt_command_low();
    return (cmd & (0b1 << 12)) != 0;
}
static constexpr uint64_t max_timer_ticks = (0b1ul << 31) - 1;
static void onLocalTimer(uint8_t, uint64_t, void* stack_ptr, void* user_data) {
    auto& timer_data = timer_data_array[get_current_lapic().get_id()];
    timer_data.ticks_needed -= timer_data.prev_init;
    if (timer_data.ticks_needed == 0) {
        if (timer_data.callback) timer_data.callback(timer_data.data);
    } else {
        timer_data.prev_init = min(max_timer_ticks, timer_data.ticks_needed);
        get_current_lapic().initial_count() = timer_data.prev_init;
    }
    if (stack_ptr) Interrupt::sendEOI();
}
void LocalAPIC::sleep() {
    Interrupt::Guard guard;
    Interrupt::enable();
    volatile auto& timer_data = timer_data_array[get_id()];
    while (timer_data.ticks_needed > 0) {
        uint32_t current = current_count();
        asm("pause");
    }
}
void LocalAPIC::sleep(duration_t duration) {
    auto& timer_data = timer_data_array[get_id()];
    if (timer_data.ticks_needed != 0) {
        panic("timer already running");
    }
    notify(
            duration, [](void*) {}, nullptr);
    sleep();
}
void LocalAPIC::notify(duration_t duration, void (*callback)(void*), void* data) {
    if (timer_data_array == nullptr)
        return;
    auto& timer_data = timer_data_array[get_id()];
    if (timer_data.ticks_needed)
        return;

    timer_data.ticks_needed = duration.nanoseconds * ticks_per_ms / 1000000;
    timer_data.callback = callback;
    timer_data.data = data;
    timer_data.prev_init = 0;
    Interrupt::registerHandler(timer_interrupt, onLocalTimer);
    lvt_timer() = timer_interrupt;
    onLocalTimer(timer_interrupt, 0, nullptr, nullptr);
}

static void (*io_interrupt_handlers[256])(void*);
void IOAPIC::interrupt_handler(uint8_t io_vector, void (*handler)(void*), void* data) {
    uint32_t gas = io_vector;
    for (size_t i = 0; i < io_interrupt_source_override_count; i++) {
        if (io_interrupt_source_overrides[i].from == io_vector) {
            gas = io_interrupt_source_overrides[i].to;
            break;
        }
    }
    uint32_t vector = gas + 32;
    if (vector > 255) {
        panic("io vector too large");
    }
    io_interrupt_handlers[vector] = handler;
    Interrupt::registerHandler(vector, [](uint8_t vector, uint64_t, void*, void* user_data) {
        io_interrupt_handlers[vector](user_data);
    }, data);
}
void IOAPIC::interrupt_set_mask(uint8_t io_vector, bool mask) {
    uint32_t gas = io_vector;
    for (size_t i = 0; i < io_interrupt_source_override_count; i++) {
        if (io_interrupt_source_overrides[i].from == io_vector) {
            gas = io_interrupt_source_overrides[i].to;
            break;
        }
    }
    for (size_t i = 0; i < ioapic_count; ++i) {
        auto ioapic = ioapics[i];
        volatile auto* select = PhysicalAddress(ioapic.address).mapTmp().as<volatile uint32_t*>();
        volatile auto* window = PhysicalAddress(ioapic.address + 0x10).mapTmp().as<volatile uint32_t*>();
        uint32_t count;
        *select = 1;
        uint32_t value = *window;
        count = (((value) >> 16) & 0xFF) + 1;
        if (gas < count + ioapic.global_system_interrupt_base && gas >= ioapic.global_system_interrupt_base) {
            uint32_t low_add = 0x10 + 2 * gas;
            *select = low_add;
            io_apic_entry entry{};
            entry.raw = *window;
            entry.mask = mask;
            *window = entry.raw;
            return;
        }
    }
}

extern "C" void ap_cpu() {
    ap_response = true;
    setIDTR(idt_ptr);
    Interrupt::enable();
    auto id = get_current_lapic().get_id();
    Log::printf(Log::Info, "APIC", "AP CPU %i started\n", id);
    while (true) {
        cli();
        hlt();
    }
}

}// namespace APIC