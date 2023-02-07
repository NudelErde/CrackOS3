//
// Created by nudelerde on 18.01.23.
//

#include "PCI/xhci.h"

namespace PCI {

xhci::xhci(const generic_device& device) : header(device) {
    auto bar_opt = header.getBAR(0);
    if (!bar_opt.has_value()) {
        Log::printf(Log::Error, "xhci", "No BAR0 found\n");
        return;
    }
    bar = bar_opt.value();

    volatile auto* data = bar.getAddress().value().as<uint8_t*>();

    uint8_t cap_length = data[0];
    uint16_t hci_version = *(uint16_t*) (data + 2);
    uint32_t param1 = *(uint32_t*) (data + 4);
    uint32_t param2 = *(uint32_t*) (data + 8);
    uint32_t param3 = *(uint32_t*) (data + 0xC);
    uint32_t param4 = *(uint32_t*) (data + 0x10);

    uint32_t max_ports = (param1 >> 24) & 0xFF;
    uint32_t max_interrupters = (param1 >> 8) & 0x7FF;
    uint32_t max_slots = (param1 >> 0) & 0xFF;
    uint32_t scratchpad_buffers = (param2 >> 27) & 0x1F + (param2 >> 21) & 0x1F;
    bool is_64bit = param4 & 0b1;

    if (!is_64bit) {
        Log::printf(Log::Error, "xhci", "xHCI is not 64bit\n");
        return;
    }
    if(scratchpad_buffers > 0) {
        Log::printf(Log::Error, "xhci", "xHCI scratchpad buffers not yet implemented\n");
        return;
    }

    capabilites = data;
    operational = reinterpret_cast<volatile uint32_t*>(data + cap_length);
    runtime = data + *(uint32_t*) (data + 0x18);
    doorbell = data + *(uint32_t*) (data + 0x14);

    Log::printf(Log::Info, "xhci", "cap_length: %i, hci_version: %i\n", cap_length, hci_version);
    Log::printf(Log::Info, "xhci", "max_ports: %i, max_interrupters: %i, max_slots: %i, scratchpads: %i\n", max_ports, max_interrupters, max_slots, scratchpad_buffers);

    //reset controller
    operational[0] = operational[0] & ~0b1;// stop
    while ((operational[1] & 0b1) == 0) {  // wait for stopped
        asm("pause");
    }
    operational[0] = operational[0] | 0b10; // reset
    while ((operational[0] & 0b10) != 0) {  // wait for reset
        asm("pause");
    }

    for(uint32_t i = 0; i < max_ports; i++) {
        uint32_t port_status_control = operational[0x100 + i];
        if(port_status_control & 0b1) {
            uint8_t state = (port_status_control >> 5) & 0b1111;
            Log::printf(Log::Info, "xhci", "Port %i is connected (%i) \n", i, state);
        }

    }
}

}// namespace PCI