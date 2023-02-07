//
// Created by nudelerde on 16.12.22.
//

#include "RSDP.h"
#include "util/time.h"

namespace APIC {

struct MADT : public ACPI::SDP{
    explicit MADT(ACPI::SDP sdp) : SDP(sdp) {}

    void init();
};

struct LocalAPIC {
    VirtualAddress base;
    [[nodiscard]] volatile uint32_t& access(uint64_t offset) const { return *(base + offset).as<uint32_t*>(); }
    [[nodiscard]] volatile uint32_t& lapic_id() const { return access(0x20); }
    [[nodiscard]] volatile uint32_t& lapic_version() const { return access(0x30); }
    [[nodiscard]] volatile uint32_t& task_priority() const { return access(0x80); }
    [[nodiscard]] volatile uint32_t& arbitration_priority() const { return access(0x90); }
    [[nodiscard]] volatile uint32_t& processor_priority() const { return access(0xA0); }
    [[nodiscard]] volatile uint32_t& end_of_interrupt() const { return access(0xB0); }
    [[nodiscard]] volatile uint32_t& remote_read() const { return access(0xC0); }
    [[nodiscard]] volatile uint32_t& logical_destination() const { return access(0xD0); }
    [[nodiscard]] volatile uint32_t& destination_format() const { return access(0xE0); }
    [[nodiscard]] volatile uint32_t& spurious_interrupt_vector() const { return access(0xF0); }
    [[nodiscard]] volatile uint32_t& in_service(uint8_t index) const { return access(0x100 + index * 0x10); }
    [[nodiscard]] volatile uint32_t& trigger_mode(uint8_t index) const { return access(0x180 + index * 0x10); }
    [[nodiscard]] volatile uint32_t& interrupt_request(uint8_t index) const { return access(0x200 + index * 10); }
    [[nodiscard]] volatile uint32_t& error_status() const { return access(0x280); }
    [[nodiscard]] volatile uint32_t& lvt_corrected_machine_check() const { return access(0x2F0); }
    [[nodiscard]] volatile uint32_t& interrupt_command_low() const { return access(0x300); }
    [[nodiscard]] volatile uint32_t& interrupt_command_high() const { return access(0x310); }
    [[nodiscard]] volatile uint32_t& lvt_timer() const { return access(0x320); }
    [[nodiscard]] volatile uint32_t& lvt_thermal_sensor() const { return access(0x330); }
    [[nodiscard]] volatile uint32_t& lvt_performance_monitor() const { return access(0x340); }
    [[nodiscard]] volatile uint32_t& lvt_lint0() const { return access(0x350); }
    [[nodiscard]] volatile uint32_t& lvt_lint1() const { return access(0x360); }
    [[nodiscard]] volatile uint32_t& lvt_error() const { return access(0x370); }
    [[nodiscard]] volatile uint32_t& initial_count() const { return access(0x380); }
    [[nodiscard]] volatile uint32_t& current_count() const { return access(0x390); }
    [[nodiscard]] volatile uint32_t& divide_configuration() const { return access(0x3E0); }
    enum class InterruptType : uint8_t {
        Normal = 0,
        Lowest_priority = 1,
        SMI = 2,
        NMI = 4,
        INIT = 5,
        SIPI = 6,
        DE_INIT = 255, // SPECIAL
    };

    void send_interrupt(InterruptType type, uint8_t vector, uint32_t local_apic_id);
    bool is_interrupt_pending();
    void sleep();
    void sleep(duration_t duration);
    void notify(duration_t duration, void(*callback)(void*), void* data);
    inline uint32_t get_id() { return lapic_id() >> 24; }
};

struct CPU_Core {
    LocalAPIC local_apic;
    uint64_t os_id{};
    uint8_t acpi_id{};
    uint8_t apic_id{};
};

struct IOAPIC {
    uint8_t id{};
    uint32_t address;
    uint32_t global_system_interrupt_base;

    static void interrupt_handler(uint8_t io_vector, void(*handler)(void* data), void* data);
    static void interrupt_set_mask(uint8_t io_vector, bool mask);
};

void initLocalAPIC();
CPU_Core* get_current_cpu();
LocalAPIC get_current_lapic();
size_t get_core_count();

}
