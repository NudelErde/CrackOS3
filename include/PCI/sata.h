//
// Created by nudelerde on 15.12.22.
//

#pragma once

#include "PCI/PCI.h"
#include "features/lock.h"
#include "features/result.h"
#include "features/smart_pointer.h"

namespace PCI {

struct Sata {

    enum error_t {
        SUCCESS = 0,
        NO_DEVICE = 1,
        INVALID_ALIGNMENT = 2,
        OUT_OF_BOUNDS = 3,
        FRAGMENTED_MEMORY = 4,
        NOT_SUPPORTED = 5,
        INVALID_MEMORY = 6,
        NO_RESOURCES = 7,
        UNKNOWN_ERROR = 8,
    };

    constexpr static uint8_t class_code = 0x01;
    constexpr static uint8_t subclass = 0x06;
    constexpr static uint8_t prog_if = 0x01;
    constexpr static generate_match match = generate_match::class_code | generate_match::subclass | generate_match::prog_if;

    struct Port {
        constexpr static size_t page_count_per_port = 4;
        struct h2d_register_fis {
            uint8_t fis_type;     // FIS_TYPE_REG_H2D
            uint8_t pmport : 4;   // Port multiplier
            uint8_t rsv0 : 3;     // Reserved
            uint8_t isCommand : 1;// 1: Command, 0: Control
            uint8_t command;      // Command register
            uint8_t featurel;     // Feature register, 7:0
            uint8_t lba0;         // LBA low register, 7:0
            uint8_t lba1;         // LBA mid register, 15:8
            uint8_t lba2;         // LBA high register, 23:16
            uint8_t device;       // Device register
            uint8_t lba3;         // LBA register, 31:24
            uint8_t lba4;         // LBA register, 39:32
            uint8_t lba5;         // LBA register, 47:40
            uint8_t featureh;     // Feature register, 15:8
            uint16_t count;       // Count register
            uint8_t icc;          // Isochronous command completion
            uint8_t control;      // Control register
            uint8_t rsv1[4];      // Reserved
        } __attribute__((packed));
        struct d2h_register_fis {
            uint8_t fis_type;
            uint8_t pmport : 4;
            uint8_t rsv0 : 2;
            uint8_t interrupt : 1;
            uint8_t rsv1 : 1;
            uint8_t status;
            uint8_t error;
            uint8_t lba0;
            uint8_t lba1;
            uint8_t lba2;
            uint8_t device;
            uint8_t lba3;
            uint8_t lba4;
            uint8_t lba5;
            uint8_t rsv2;
            uint16_t count;
            uint8_t rsv3[2];
            uint8_t rsv4[4];
        } __attribute__((packed));
        struct pio_setup_fis {
            uint8_t fis_type;
            uint8_t pmport : 4;
            uint8_t rsv0 : 1;
            uint8_t data_direction : 1;
            uint8_t interrupt : 1;
            uint8_t rsv1 : 1;
            uint8_t status;
            uint8_t error;
            uint8_t lba0;
            uint8_t lba1;
            uint8_t lba2;
            uint8_t device;
            uint8_t lba3;
            uint8_t lba4;
            uint8_t lba5;
            uint8_t rsv2;
            uint16_t count;
            uint8_t rsv3;
            uint8_t e_status;
            uint16_t transfer_count;
            uint8_t rsv4[2];
        } __attribute__((packed));
        struct dma_setup_fis {
            uint8_t fis_type;
            uint8_t pmport : 4;
            uint8_t rsv0 : 1;
            uint8_t data_direction : 1;
            uint8_t interrupt : 1;
            uint8_t auto_activate : 1;
            uint8_t rsv1[2];
            uint64_t dma_buffer_id;
            uint32_t rsv2;
            uint32_t dma_buffer_offset;
            uint32_t transfer_count;
            uint32_t rsv3;
        } __attribute__((packed));
        union received_fis {
            uint8_t raw[256];
            struct {
                dma_setup_fis dma_setup;
                uint8_t padding[4];
                pio_setup_fis pio_setup;
                uint8_t padding2[12];
                d2h_register_fis d2h_register;
                uint8_t padding3[4];
                uint64_t sdbfis;
                uint8_t ufis[64];
                uint8_t rsv[96];
            } __attribute__((packed));
        } __attribute__((packed));
        struct command_header {
            uint8_t command_fis_length : 5;// Command FIS length in DWORDS, 2 ~ 16
            uint8_t atapi : 1;
            uint8_t direction : 1;// 1 = D2H, 0 = H2D
            uint8_t prefetchable : 1;
            uint8_t reset : 1;
            uint8_t bist : 1;
            uint8_t clear_busy : 1;
            uint8_t reserved : 1;
            uint8_t port_multiplier : 4;
            uint16_t physical_region_descriptor_table_entry_count;
            uint32_t physical_region_descriptor_byte_count;
            uint32_t command_table_base_address_low;
            uint32_t command_table_base_address_high;
            uint32_t reserved2[4];
        } __attribute__((packed));
        struct command_list {
            command_header entries[32];
        } __attribute__((packed));
        struct physical_region_descriptor {
            uint64_t physical_base_Address;
            uint32_t reserved;
            uint32_t byte_count : 22;
            uint32_t reserved1 : 9;
            uint32_t interrupt_on_completion : 1;
        } __attribute__((packed));
        constexpr static uint64_t physical_region_descriptor_table_entry_count = (((page_count_per_port - 1) * page_size / 32) - 0x80) / sizeof(physical_region_descriptor);
        struct command_table {
            uint8_t command_fis[64];
            uint8_t atapi_command[16];
            uint8_t reserved[48];
            physical_region_descriptor physical_region_descriptor_table[physical_region_descriptor_table_entry_count];
        } __attribute__((packed));
        struct command_tables {
            command_table entries[32];
        } __attribute__((packed));
        static_assert(sizeof(received_fis) == 256);
        static_assert(sizeof(command_list) == 1024);
        static_assert(sizeof(command_table) * 32 == (page_count_per_port - 1) * page_size);
        static_assert(sizeof(command_tables) == (page_count_per_port - 1) * page_size);

    private:
        spinlock lock;
        [[nodiscard]] optional<uint8_t> find_free_slot() const;
        result<uint64_t, error_t> setup_physical_region(uint64_t slot, VirtualAddress buffer, size_t size);
        void setup_h2d(uint64_t slot, uint8_t command, uint64_t lba, uint16_t count, uint8_t device);
        optional<error_t> check_command_valid(uint64_t lba, uint64_t size, uint16_t* buffer);
        void issue_slot(uint64_t slot);

    public:
        bool has_device = false;
        bool is_atapi = false;
        uint64_t sector_count = 0;
        uint64_t sector_size = 0;
        bool queue_capable = false;
        bool running = false;
        uint32_t scheduled_slots = 0;

        volatile uint8_t* mmio{};
        void init();
        volatile command_list* command_list_ptr{};
        volatile received_fis* received_fis_ptr{};
        volatile command_tables* command_table_ptr{};
        void stop();
        void start();
        void sendIdentify();
        bool wait_for(uint8_t slot);
        [[nodiscard]] result<bool, error_t> test_done(uint8_t slot);
        [[nodiscard]] bool wait_all();
        result<uint8_t, error_t> sendRead(uint64_t lba, uint64_t size, uint16_t* buffer);
        result<uint8_t, error_t> sendWrite(uint64_t lba, uint64_t size, uint16_t* buffer);
    };

    PCI::device_header::BAR bar;
    device_header header;
    volatile uint8_t* mmio{};
    uint32_t host_capabilities{};
    uint32_t global_host_control{};
    uint32_t ports_implemented{};
    uint32_t host_capabilities_extended{};
    uint8_t max_command_slot_count{};

    explicit Sata(const generic_device& device) : header(device) {
        init();
    }

    void interrupt_handler();

    void init();
};

}// namespace PCI

template<>
inline const char* error_to_string(PCI::Sata::error_t err) {
    switch (err) {
        case PCI::Sata::SUCCESS:
            return "SUCCESS";
        case PCI::Sata::FRAGMENTED_MEMORY:
            return "FRAGMENTED_MEMORY";
        case PCI::Sata::NO_DEVICE:
            return "NO_DEVICE";
        case PCI::Sata::INVALID_ALIGNMENT:
            return "INVALID_ALIGNMENT";
        case PCI::Sata::OUT_OF_BOUNDS:
            return "OUT_OF_BOUNDS";
        case PCI::Sata::NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        case PCI::Sata::INVALID_MEMORY:
            return "INVALID_MEMORY";
        case PCI::Sata::NO_RESOURCES:
            return "NO_RESOURCES";
        case PCI::Sata::UNKNOWN_ERROR:
            return "UNKNOWN_ERROR";
    }
    return "Unknown error";
}
