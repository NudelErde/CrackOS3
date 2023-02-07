//
// Created by nudelerde on 15.12.22.
//

#include "PCI/sata.h"

#include "ACPI/APIC.h"
#include "asm/util.h"
#include "file/device.h"
#include "interrupt/interrupt.h"

struct sata_port_device : file::device::implementation {
    explicit sata_port_device(shared_ptr<PCI::Sata::Port> sharedPtr) : port(std::move(sharedPtr)) {
    }
    shared_ptr<PCI::Sata::Port> port;

    result<size_t, file::device::error_t> read(void* buffer, size_t offset, size_t size) override {
        if (VirtualAddress(buffer).address & 0b1) {
            auto* tmp = new uint16_t[size / 2 + 1];
            VirtualAddress v_tmp(tmp);
            v_tmp.address = (v_tmp.address + 1) & ~0b1;// align to 2 bytes
            auto* ptr = v_tmp.as<void*>();
            auto res = read(ptr, offset, size);
            if (res.has_error()) {
                delete[] tmp;
                return res;
            }
            memcpy(buffer, ptr, size);
            delete[] tmp;
            return res;
        }
        if (offset + size > port->sector_count * port->sector_size) {
            return file::device::error_t::OUT_OF_BOUNDS;
        }

        // aligned read
        if(offset % port->sector_size == 0 && size % port->sector_size == 0) {
            auto res = port->sendRead(offset / port->sector_size, size, static_cast<uint16_t*>(buffer));
            if (res.has_error()) {
                return res.error();
            }
            if(!port->wait_for(res.value())) {
                return file::device::error_t::DEVICE_UNAVAILABLE;
            }
            flush_cache();
            return size;
        }

        // unaligned read, in only one sector
        if(offset / port->sector_size == (offset + size) / port->sector_size) {
            uint16_t sector_buffer[port->sector_size / 2];
            auto res = port->sendRead(offset / port->sector_size, port->sector_size, sector_buffer);
            if (res.has_error()) {
                return res.error();
            }
            if(!port->wait_for(res.value())) {
                return file::device::error_t::DEVICE_UNAVAILABLE;
            }
            memcpy(buffer, sector_buffer + (offset % port->sector_size) / 2, size);
            flush_cache();
            return size;
        }

        // unaligned read, in two sectors
        if(offset / port->sector_size + 1 == (offset + size) / port->sector_size) {
            uint16_t sector_buffer[port->sector_size]; // two sectors
            auto res = port->sendRead(offset / port->sector_size, port->sector_size * 2, sector_buffer);
            if (res.has_error()) {
                return res.error();
            }
            if(!port->wait_for(res.value())) {
                return file::device::error_t::DEVICE_UNAVAILABLE;
            }
            memcpy(buffer, sector_buffer + (offset % port->sector_size) / 2, size);
            flush_cache();
            return size;
        }

        struct region {
            size_t offset{};
            size_t size{};
            size_t buffer_size{};
            uint16_t* buffer{};
            optional<result<uint8_t, PCI::Sata::error_t>> request;
        };

        region regions[3];

        regions[0].size = port->sector_size - (offset % port->sector_size);
        regions[2].size = (offset + size) % port->sector_size;
        regions[1].size = size - regions[0].size - regions[2].size;

        regions[0].offset = offset;
        regions[1].offset = regions[0].offset + regions[0].size;
        regions[2].offset = regions[1].offset + regions[1].size;

        regions[0].buffer_size = port->sector_size;
        regions[1].buffer_size = regions[1].size;
        regions[2].buffer_size = port->sector_size;

        uint16_t start_buffer[port->sector_size / 2];
        uint16_t end_buffer[port->sector_size / 2];

        regions[0].buffer = start_buffer;
        regions[2].buffer = end_buffer;
        regions[1].buffer = static_cast<uint16_t*>(buffer) + regions[0].size / 2;

        optional<PCI::Sata::error_t> error;
        for(auto& region : regions) {
            if(region.size == 0) continue;
            region.request = port->sendRead(region.offset / port->sector_size, region.buffer_size, region.buffer);
            if (region.request->has_error()) {
                error = region.request->error();
                break;
            }
        }
        for(auto& region : regions) {
            if(!region.request || region.request->has_error()) continue;
            if(!port->wait_for(region.request->value())) {
                error = PCI::Sata::error_t::UNKNOWN_ERROR;
                break;
            }
        }

        if(error) {
            return error.value();
        }

        memcpy(buffer, regions[0].buffer + (regions[0].offset % port->sector_size) / 2, regions[0].size);
        memcpy(static_cast<uint16_t*>(buffer) + regions[0].size / 2 + regions[1].size / 2, regions[2].buffer, regions[2].size);

        flush_cache();
        return size;
    }

    result<size_t, file::device::error_t> write(void* buffer, size_t offset, size_t size) override {
        if (VirtualAddress(buffer).address & 0b1) {
            auto* tmp = new uint16_t[size / 2 + 1];
            VirtualAddress v_tmp(tmp);
            v_tmp.address = (v_tmp.address + 1) & ~0b1;// align to 2 bytes
            auto* ptr = v_tmp.as<void*>();
            memcpy(ptr, buffer, size);
            auto res = write(ptr, offset, size);
            delete[] tmp;
            return res;
        }

        if (offset + size > port->sector_count * port->sector_size) {
            return file::device::error_t::OUT_OF_BOUNDS;
        }

        // aligned write
        if(offset % port->sector_size == 0 && size % port->sector_size == 0) {
            auto res = port->sendWrite(offset / port->sector_size, size / port->sector_size, static_cast<uint16_t*>(buffer));
            if (res.has_error()) {
                return res.error();
            }
            if(!port->wait_for(res.value())) {
                return file::device::error_t::DEVICE_UNAVAILABLE;
            }
            flush_cache();
            return size;
        }

        // unaligned write, in only one sector
        if(offset / port->sector_size == (offset + size) / port->sector_size) {
            uint16_t sector_buffer[port->sector_size / 2];
            auto res = port->sendRead(offset / port->sector_size, port->sector_size, sector_buffer);
            if (res.has_error()) {
                return res.error();
            }
            if(!port->wait_for(res.value())) {
                return file::device::error_t::DEVICE_UNAVAILABLE;
            }
            memcpy(sector_buffer + (offset % port->sector_size) / 2, buffer, size);
            res = port->sendWrite(offset / port->sector_size, port->sector_size, sector_buffer);
            if (res.has_error()) {
                return res.error();
            }
            if(!port->wait_for(res.value())) {
                return file::device::error_t::DEVICE_UNAVAILABLE;
            }
            flush_cache();
            return size;
        }

        // unaligned write, in two sectors
        if(offset / port->sector_size + 1 == (offset + size) / port->sector_size) {
            uint16_t sector_buffer[port->sector_size]; // two sectors
            auto res = port->sendRead(offset / port->sector_size, port->sector_size * 2, sector_buffer);
            if (res.has_error()) {
                return res.error();
            }
            if(!port->wait_for(res.value())) {
                return file::device::error_t::DEVICE_UNAVAILABLE;
            }
            memcpy(sector_buffer + (offset % port->sector_size) / 2, buffer, size);
            res = port->sendWrite(offset / port->sector_size, port->sector_size * 2, sector_buffer);
            if (res.has_error()) {
                return res.error();
            }
            if(!port->wait_for(res.value())) {
                return file::device::error_t::DEVICE_UNAVAILABLE;
            }
            flush_cache();
            return size;
        }

        // unaligned write, in many sectors
        struct region {
            size_t offset{};
            size_t size{};
            size_t buffer_size{};
            uint16_t* buffer{};
            optional<result<uint8_t, PCI::Sata::error_t>> request;
        };

        region regions[3];
        regions[0].size = port->sector_size - (offset % port->sector_size);
        regions[1].size = size - regions[0].size;
        regions[2].size = (offset + size) % port->sector_size;

        regions[0].offset = offset;
        regions[1].offset = offset + regions[0].size;
        regions[2].offset = offset + size - regions[2].size;

        uint16_t start_buffer[regions[0].size / 2];
        uint16_t end_buffer[regions[2].size / 2];

        regions[0].buffer = start_buffer;
        regions[1].buffer = static_cast<uint16_t*>(buffer) + regions[0].size / 2;
        regions[2].buffer = end_buffer;

        regions[0].buffer_size = port->sector_size;
        regions[1].buffer_size = regions[1].size;
        regions[2].buffer_size = port->sector_size;


        // read first and last sector
        regions[0].request = port->sendRead(regions[0].offset / port->sector_size, port->sector_size, regions[0].buffer);
        regions[2].request = port->sendRead(regions[2].offset / port->sector_size, port->sector_size, regions[2].buffer);
        if(regions[0].request->has_error()) {
            if(regions[2].request->has_value()) {
                port->wait_for(regions[2].request->value()); // finish working request
            }
            return regions[0].request->error();
        }
        if(regions[2].request->has_error()) {
            if(regions[0].request->has_value()) {
                port->wait_for(regions[0].request->value()); // finish working request
            }
            return regions[2].request->error();
        }

        memcpy(regions[0].buffer + (regions[0].offset % port->sector_size) / 2, buffer, regions[0].size);
        memcpy(regions[2].buffer, static_cast<uint16_t*>(buffer) + (regions[0].size + regions[1].size) / 2, regions[2].size);

        // write all sectors
        optional<PCI::Sata::error_t> error;
        for(auto& region : regions) {
            if(region.request->has_value()) {
                region.request = port->sendWrite(region.offset / port->sector_size, region.buffer_size, region.buffer);
                if (region.request->has_error()) {
                    error = region.request->error();
                    break;
                }
            }
        }

        for(auto& region : regions) {
            if(!region.request || region.request->has_error()) continue;
            if(!port->wait_for(region.request->value())) {
                error = PCI::Sata::error_t::UNKNOWN_ERROR;
                break;
            }
        }

        if(error.has_value()) {
            return error.value();
        }

        flush_cache();
        return size;
    }
};

void PCI::Sata::init() {
    auto bar_opt = header.getBAR(5);
    if (!bar_opt.has_value()) {
        Log::printf(Log::Error, "SATA", "No BAR5 found\n");
        return;
    }
    bar = bar_opt.value();
    if (!bar.getAddress()) {
        Log::printf(Log::Error, "SATA", "Invalid BAR5 found\n");
        return;
    }
    Log::printf(Log::Debug, "SATA", "BAR5: %p\n", bar.getAddress().value().as<uint8_t*>());
    mmio = bar.getAddress().value().as<volatile uint8_t*>();
    host_capabilities = *(uint32_t*) (mmio + 0x00);
    global_host_control = *(uint32_t*) (mmio + 0x04);
    ports_implemented = *(uint32_t*) (mmio + 0x0C);
    host_capabilities_extended = *(uint32_t*) (mmio + 0x24);
    Log::printf(Log::Debug, "SATA", "CAP: %x, GHC: %x, PI: %x, CAP2: %x\n", host_capabilities, global_host_control, ports_implemented, host_capabilities_extended);
    optional<Port> ports[32];
    for (int i = 0; i < 32; ++i) {
        if (ports_implemented & (1 << i)) {
            ports[i] = Port{};
            ports[i]->mmio = mmio + 0x100 + i * 0x80;
            Log::printf(Log::Debug, "SATA", "Port %i found\n", i);
        }
    }

    if (!(host_capabilities | (0b1 << 31))) {
        // 64bit addressing not supported
        Log::printf(Log::Error, "SATA", "64bit addressing not supported\n");
        return;
    }
    max_command_slot_count = ((host_capabilities >> 8) & 0b11111) + 1;

    // test if bios handoff is possible and needed
    if (host_capabilities_extended & 0b1) {
        uint32_t bohc = *(volatile uint32_t*) (mmio + 0x28);
        bohc |= 0b10;// request bios handoff
        *(volatile uint32_t*) (mmio + 0x28) = bohc;
        while (((*(volatile uint32_t*) (mmio + 0x28)) & 0b11) != 0b10) {
            APIC::get_current_lapic().sleep(1_ms);
        }
    }

    // reset controller
    global_host_control |= 0b1;
    *(volatile uint32_t*) (mmio + 0x04) = global_host_control;
    while ((*(volatile uint32_t*) (mmio + 0x04)) & 0b1) {
        APIC::get_current_lapic().sleep(1_ms);
    }

    auto free_interrupt = Interrupt::get_free_interrupt_number();
    Interrupt::registerHandler(
            free_interrupt, [](uint8_t, uint64_t, void*, void* data) {
                reinterpret_cast<Sata*>(data)->interrupt_handler();
                Interrupt::sendEOI();
            },
            this);
    if (!header.setup_msi(free_interrupt)) {
        Log::printf(Log::Error, "SATA", "Failed to setup MSI\n");
        return;
    }
    global_host_control = *(volatile uint32_t*) (mmio + 0x04);
    global_host_control |= 0b10;
    *(volatile uint32_t*) (mmio + 0x04) = global_host_control;

    for (auto& port : ports) {
        if (!port) continue;
        port->init();
        if (port->has_device && !port->is_atapi) {
            // register as device
            shared_ptr<Port> port_ptr = shared_ptr<Port>(port.value());
            file::push_device({port_ptr->sector_size * port_ptr->sector_count,
                               new sata_port_device(port_ptr)});
        }
    }
}
void PCI::Sata::interrupt_handler() {
    Log::printf(Log::Debug, "SATA", "Interrupt\n");
}

void PCI::Sata::Port::init() {
    stop();
    PhysicalAddress ppage = PhysicalAllocator::alloc(page_count_per_port).value_or_panic("Failed to allocate page for SATA");
    VirtualAddress vpage = PCI::allocate_uncached_virtual(page_count_per_port);
    for (size_t i = 0; i < page_count_per_port; ++i) {
        PageTable::map(ppage.address + i * page_size, vpage + i * page_size, {.writeable = true, .user = false, .writeThrough = false, .cacheDisabled = true});
    }
    memset(vpage.as<void*>(), 0, page_count_per_port * page_size);

    command_list_ptr = vpage.as<volatile command_list*>();
    received_fis_ptr = (vpage + sizeof(command_list)).as<volatile received_fis*>();
    command_table_ptr = (vpage + page_size).as<volatile command_tables*>();
    uint64_t command_list_base = ppage.address;
    uint64_t received_fis_base = ppage.address + sizeof(command_list);
    *(volatile uint32_t*) (mmio + 0x00) = command_list_base;
    *(volatile uint32_t*) (mmio + 0x04) = command_list_base >> 32;
    *(volatile uint32_t*) (mmio + 0x08) = received_fis_base;
    *(volatile uint32_t*) (mmio + 0x0C) = received_fis_base >> 32;

    for (size_t i = 0; i < 32; ++i) {
        command_list_ptr->entries[i].command_table_base_address_low = ppage.address + page_size + i * sizeof(command_table);
        command_list_ptr->entries[i].command_table_base_address_high = (ppage.address + page_size + i * sizeof(command_table)) >> 32;
    }
    // reset
    volatile auto* sctl = (volatile uint32_t*) (mmio + 0x24);
    *sctl = (*sctl) & ~0b1111 | 0b10;
    APIC::get_current_lapic().sleep(5_ms);
    *sctl = (*sctl) & ~0b1111;

    start();
    // enable interrupts
    *(volatile uint32_t*) (mmio + 0x10) = ~0;
    *(volatile uint32_t*) (mmio + 0x14) = ~0;

    uint32_t ssts = *(volatile uint32_t*) (mmio + 0x28);
    uint8_t det = (ssts >> 0) & 0b1111;
    uint8_t ipm = (ssts >> 8) & 0b1111;
    if (det != 0b11) {
        Log::printf(Log::Debug, "SATA", "No device connected\n");
        return;
    }
    if (ipm != 0b1) {
        Log::printf(Log::Debug, "SATA", "Device not active\n");
        return;
    }

    // read signature
    uint32_t signature = *(volatile uint32_t*) (mmio + 0x24);
    if (signature == 0xEB140101) {
        Log::printf(Log::Info, "SATA", "ATAPI device found\n");
        has_device = true;
        is_atapi = true;
    } else if (signature == 0) {
        Log::printf(Log::Debug, "SATA", "No device found\n");
    } else {
        Log::printf(Log::Info, "SATA", "ATA device found\n");
        has_device = true;
        is_atapi = false;
    }

    if (!has_device) return;
    sendIdentify();
}
void PCI::Sata::Port::stop() {
    running = false;
    volatile auto* cmd = (volatile uint32_t*) (mmio + 0x18);
    *cmd = *cmd & ~(1 << 4);// clear FRE
    *cmd = *cmd & ~(1 << 0);// clear ST
    while (*cmd & (1 << 14) || *cmd & (1 << 15)) {
        APIC::get_current_lapic().sleep(1_ms);
    }// wait for FR and CR to clear
}
void PCI::Sata::Port::start() {
    running = true;
    volatile auto* cmd = (volatile uint32_t*) (mmio + 0x18);
    while (*cmd & (1 << 15)) {
        APIC::get_current_lapic().sleep(1_ms);
    }                      // wait for CR to clear
    *cmd = *cmd | (1 << 4);// set FRE
    *cmd = *cmd | (1 << 0);// set ST
}
alignas(4096) static uint16_t identify_buffer[256]{};
void PCI::Sata::Port::sendIdentify() {
    lock_guard guard(lock);
    if (is_atapi) {
        Log::printf(Log::Warning, "SATA", "ATAPI not supported yet\n");
        return;
    }
    memset(identify_buffer, 0, sizeof(identify_buffer));
    // assume that the device is not busy

    auto fis = reinterpret_cast<volatile h2d_register_fis*>(&command_table_ptr->entries[0].command_fis);
    memset(fis, 0, sizeof(h2d_register_fis));
    fis->fis_type = 0x27;
    fis->command = 0xEC;
    fis->device = 0;
    fis->isCommand = 1;
    auto& slot = command_list_ptr->entries[0];
    slot.command_fis_length = sizeof(h2d_register_fis) / sizeof(uint32_t);
    slot.direction = 0;
    slot.physical_region_descriptor_table_entry_count = 1;

    auto& region_dt = command_table_ptr->entries[0].physical_region_descriptor_table[0];
    PhysicalAddress p_address_buffer = PageTable::get(VirtualAddress(identify_buffer)).value_or_panic("Failed to get physical address of buffer");
    region_dt.byte_count = sizeof(identify_buffer) - 1;
    region_dt.physical_base_Address = p_address_buffer.address;
    region_dt.interrupt_on_completion = 1;
    slot.physical_region_descriptor_table_entry_count = 1;

    issue_slot(0);
    if (!wait_for(0)) {
        Log::printf(Log::Error, "SATA", "Failed to send identify command\n");
        return;
    }

    sector_count = *(uint64_t*) (identify_buffer + 100);
    if (sector_count == 0) {
        sector_count = *(uint32_t*) (identify_buffer + 60);
    }
    sector_size = *(uint32_t*) (identify_buffer + 117);
    if (sector_size == 0) {
        sector_size = 512;
    }
    auto command_support = reinterpret_cast<volatile uint64_t*>(identify_buffer + 85);
    queue_capable = *command_support & (1 << 17);

    Log::printf(Log::Debug, "SATA", "Device size: %b\n", sector_count * sector_size);
}
result<bool, PCI::Sata::error_t> PCI::Sata::Port::test_done(uint8_t slot) {
    auto* command_issue = (volatile uint32_t*) (mmio + 0x38);
    if (*(volatile uint32_t*) (mmio + 0x30)) {
        return error_t::UNKNOWN_ERROR;
    }
    if (*(volatile uint32_t*) (mmio + 0x30) & 0b1) {
        return error_t::UNKNOWN_ERROR;
    }
    if ((*command_issue & (1 << slot)) == 0) {
        scheduled_slots &= ~(1 << slot);
        return true;
    }
    return false;
}
bool PCI::Sata::Port::wait_for(uint8_t slot) {
    while (true) {
        auto res = test_done(slot);
        if (res.has_error()) {
            return false;
        }
        if (res.value()) {
            return true;
        }
        asm("pause");
    }
}
bool PCI::Sata::Port::wait_all() {
    auto* command_issue = (volatile uint32_t*) (mmio + 0x38);

    while (true) {
        if (*(volatile uint32_t*) (mmio + 0x30)) {
            return false;
        }
        if (*(volatile uint32_t*) (mmio + 0x30) & 0b1) {
            return false;
        }
        if (*command_issue == 0) {
            scheduled_slots = 0;
            return true;
        }
        asm("pause");
    }
}

result<uint8_t, PCI::Sata::error_t> PCI::Sata::Port::sendRead(uint64_t lba, uint64_t size, uint16_t* buffer) {
    VirtualAddress address(buffer);
    auto count = (size + sector_size - 1) / sector_size;
    if (auto res = check_command_valid(lba, size, buffer); res) {
        return res.value();
    }
    lock_guard guard(lock);
    if (!queue_capable) {
        if (!wait_all()) {
            return UNKNOWN_ERROR;
        }
    }

    auto slot = find_free_slot();
    if (!slot) return NO_RESOURCES;
    setup_h2d(slot.value(), queue_capable ? 0x26 : 0x25, lba, count, 1 << 6);

    auto res = setup_physical_region(slot.value(), address, size);
    if (!res) return res.error();

    auto command_list = &command_list_ptr->entries[slot.value()];

    command_list->physical_region_descriptor_table_entry_count = res.value();
    command_list->command_fis_length = sizeof(h2d_register_fis) / sizeof(uint32_t);
    command_list->direction = 1;
    command_list->port_multiplier = 0;
    command_list->physical_region_descriptor_byte_count = 0;

    issue_slot(slot.value());

    return slot.value();
}

result<uint8_t, PCI::Sata::error_t> PCI::Sata::Port::sendWrite(uint64_t lba, uint64_t size, uint16_t* buffer) {
    VirtualAddress address(buffer);
    auto count = (size + sector_size - 1) / sector_size;
    if (auto res = check_command_valid(lba, size, buffer); res) {
        return res.value();
    }
    lock_guard guard(lock);
    if (!queue_capable) {
        if (!wait_all()) {
            return UNKNOWN_ERROR;
        }
    }

    auto slot = find_free_slot();
    if (!slot) return NO_RESOURCES;
    setup_h2d(slot.value(), queue_capable ? 0x36 : 0x35, lba, count, 1 << 6);

    auto res = setup_physical_region(slot.value(), address, size);
    if (!res) return res.error();

    auto command_list = &command_list_ptr->entries[slot.value()];

    command_list->physical_region_descriptor_table_entry_count = res.value();
    command_list->command_fis_length = sizeof(h2d_register_fis) / sizeof(uint32_t);
    command_list->direction = 0;
    command_list->port_multiplier = 0;
    command_list->physical_region_descriptor_byte_count = 0;

    issue_slot(slot.value());
    return slot.value();
}
optional<uint8_t> PCI::Sata::Port::find_free_slot() const {
    auto* command_issue = (volatile uint32_t*) (mmio + 0x38);
    for (uint8_t i = 0; i < 32; ++i) {
        if ((*command_issue & (1 << i)) == 0 && (scheduled_slots & (1 << i)) == 0) {
            return i;
        }
    }
    return {};
}
result<uint64_t, PCI::Sata::error_t> PCI::Sata::Port::setup_physical_region(uint64_t slot, VirtualAddress buffer, size_t size) {
    volatile physical_region_descriptor* array = command_table_ptr->entries[slot].physical_region_descriptor_table;
    optional<PhysicalAddress> last_physical;
    int64_t region_index = -1;
    for (uint64_t offset = 0; offset < size;) {
        auto p_address_opt = PageTable::get(buffer + offset);
        if (!p_address_opt) return INVALID_MEMORY;

        auto offset_in_page = (buffer + offset).offset;
        auto sub_size = min(page_size, size - offset, page_size - offset_in_page);

        if (last_physical &&                                               // if we are not the first iteration
            last_physical->address + page_size == p_address_opt->address &&// and the last physical address is the previous page
            region_index >= 0 &&                                           // and we have a region
            array[region_index].byte_count + page_size < (1 << 22)) {      // and the region is not full
            // append to last region if possible
            array[region_index].byte_count += sub_size;
        } else {
            // create new region
            region_index++;
            if (region_index >= physical_region_descriptor_table_entry_count) return FRAGMENTED_MEMORY;
            array[region_index].physical_base_Address = p_address_opt->address;
            array[region_index].byte_count = sub_size - 1;
            array[region_index].interrupt_on_completion = 0;
        }
        last_physical = p_address_opt;
        offset += sub_size;
    }
    return region_index + 1;
}
void PCI::Sata::Port::issue_slot(uint64_t slot) {
    auto* command_issue = (volatile uint32_t*) (mmio + 0x38);
    *command_issue = *command_issue | (1 << slot);
    scheduled_slots |= (1 << slot);
}
optional<PCI::Sata::error_t> PCI::Sata::Port::check_command_valid(uint64_t lba, uint64_t size, uint16_t* buffer) {
    VirtualAddress address(buffer);
    auto count = (size + sector_size - 1) / sector_size;
    if (address.address & 0b1) return INVALID_ALIGNMENT;
    if (!has_device) return NO_DEVICE;
    if (is_atapi) return NOT_SUPPORTED;
    if (lba + count > sector_count) return OUT_OF_BOUNDS;
    if (count >= (1 << 16)) return NOT_SUPPORTED;
    if (count == 0) return SUCCESS;
    return {};
}
void PCI::Sata::Port::setup_h2d(uint64_t slot, uint8_t command, uint64_t lba, uint16_t count, uint8_t device) {
    auto command_table = &command_table_ptr->entries[slot];
    auto fis = reinterpret_cast<volatile h2d_register_fis*>(&command_table->command_fis);
    memset(fis, 0, sizeof(h2d_register_fis));
    fis->fis_type = 0x27;
    fis->command = command;
    fis->device = device;
    fis->isCommand = 1;
    fis->lba0 = lba & 0xFF;
    fis->lba1 = (lba >> 8) & 0xFF;
    fis->lba2 = (lba >> 16) & 0xFF;
    fis->lba3 = (lba >> 24) & 0xFF;
    fis->lba4 = (lba >> 32) & 0xFF;
    fis->lba5 = (lba >> 40) & 0xFF;
    fis->count = count;
}