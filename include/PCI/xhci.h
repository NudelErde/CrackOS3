//
// Created by nudelerde on 18.01.23.
//

#pragma once

#include "PCI/PCI.h"

namespace PCI {

struct xhci {
    constexpr static uint8_t class_code = 0x0C;
    constexpr static uint8_t subclass = 0x03;
    constexpr static uint8_t prog_if = 0x30;
    constexpr static generate_match match = generate_match::class_code | generate_match::subclass | generate_match::prog_if;

    PCI::device_header::BAR bar;
    device_header header;
    volatile uint8_t* capabilites;
    volatile uint32_t* operational;
    volatile uint8_t* runtime;
    volatile uint8_t* doorbell;

    struct DeviceContextBaseAddressArray {
        PhysicalAddress base;// first (index = 0) is reserved for scratchpad buffers
        size_t count;

        [[nodiscard]] PhysicalAddress get(size_t index) const {
            return base.address + index * sizeof(uint64_t);
        }
        void set(size_t index, PhysicalAddress address) const {
            get(index).mapTmp().as<uint64_t*>()[0] = address.address;
        }
    };

    struct SlotContext {
        uint32_t route_string : 20;       // word 0: 0 - 19
        uint32_t reserved : 5;            // word 0: 20 - 24
        uint32_t multitt : 1;             // word 0: 25
        uint32_t hub : 1;                 // word 0: 26
        uint32_t context_entries : 5;     // word 0: 27 - 31
        uint32_t max_exit_latency : 16;   // word 1: 0 - 15
        uint32_t root_hub_port_number : 8;// word 1: 16 - 23
        uint32_t number_of_ports : 8;     // word 1: 24 - 31
        uint32_t parent_hub_slot_id : 8;  // word 2: 0 - 7
        uint32_t parent_port_number : 8;  // word 2: 8 - 15
        uint32_t tt_think_time : 2;       // word 2: 16 - 17
        uint32_t reserved2 : 4;           // word 2: 18 - 21
        uint32_t interrupter_target : 10; // word 2: 22 - 31
        uint32_t usb_device_address : 8;  // word 3: 0 - 7
        uint32_t reserved3 : 19;          // word 3: 8 - 26
        uint32_t slot_state : 5;          // word 3: 27 - 31
        uint32_t reserved4[4];            // word 4 - 7
    } __attribute__((packed));

    struct EndpointContext {
        uint32_t endpoint_state : 3;       // word 0: 0 - 2
        uint32_t reserved : 5;             // word 0: 3 - 7
        uint32_t mult : 2;                 // word 0: 8 - 9
        uint32_t max_primary_streams : 5;  // word 0: 10 - 14
        uint32_t linear_stream_array : 1;  // word 0: 15
        uint32_t interval : 8;             // word 0: 16 - 23
        uint32_t max_esit_payload_high : 8;// word 0: 24 - 31
        uint32_t reserved2 : 1;            // word 1: 0
        uint32_t error_count : 2;          // word 1: 1 - 2
        uint32_t endpoint_type : 3;        // word 1: 3 - 5
        uint32_t reserved3 : 1;            // word 1: 6
        uint32_t host_initiate_disable : 1;// word 1: 7
        uint32_t max_burst_size : 8;       // word 1: 8 - 15
        uint32_t max_packet_size : 16;     // word 1: 16 - 31
        union {
            struct {
                uint64_t deque_cycle_state : 1;// word 2: 0
                uint64_t reserved4 : 63;       // word 2-3: 1 - 63
            };
            uint64_t dequeue_pointer;// word 2 - 3
        };
        uint32_t average_trb_length : 16;  // word 4: 0 - 15
        uint32_t max_esit_payload_low : 16;// word 4: 16 - 31
        uint32_t reserved5[3];             // word 5 - 7
    } __attribute__((packed));

    struct DeviceContext {
        SlotContext slotContext;
        EndpointContext endpointContexts[31];
    } __attribute__((packed));

    explicit xhci(const generic_device& device);
};

}// namespace PCI