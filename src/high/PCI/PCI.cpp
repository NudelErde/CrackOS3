//
// Created by nudelerde on 11.12.22.
//

#include "PCI/PCI.h"
#include "asm/util.h"
#include "data/btree.h"
#include "data/pair.h"
#include "features/bytes.h"
#include "features/string.h"

namespace PCI {

optional<MCFG> make_mcfg(ACPI::SDP sdp) {
    if (strcmp(sdp.getSignature(), "MCFG") == 0) {
        return MCFG(sdp);
    }
    return {};
}
pci_group MCFG::getEntry(size_t index) {
    auto entry = ptr + 44 + index * 16;
    uint64_t base = *entry.as<uint64_t*>();
    uint16_t segment_number = *(entry + 8).as<uint16_t*>();
    uint8_t start_number = *(entry + 10).as<uint8_t*>();
    uint8_t end_number = *(entry + 11).as<uint8_t*>();
    return {PhysicalAddress{base}, segment_number, start_number, end_number};
}
size_t MCFG::getEntryCount() {
    auto size = ptr.as<uint32_t*>()[1];
    size -= 44;
    return size / 16;
}

void pci_group::check_device(uint8_t bus, uint8_t device, linked_list<generic_device>& list) {
    if (readConfigSpace(bus, device, 0, &generic_device::header_t::header_type) & 0x80) {
        for (uint8_t function = 0; function < 8; ++function) {
            check_function(bus, device, function, list);
        }
    } else {
        check_function(bus, device, 0, list);
    }
}
void pci_group::check_function(uint8_t bus, uint8_t device, uint8_t function, linked_list<generic_device>& list) {
    auto address = getPhysical(bus, device, function);
    generic_device::header_t header{};
    read_configuration_space(address, &header, sizeof(generic_device::header_t));
    if (header.vendor_id != 0xFFFF) {
        generic_device genericDevice = {header, address, bus, device, function};
        list.push_back(genericDevice);
        if (header.class_code == 0x06 && header.subclass == 0x04) {
            pci_bridge bridge{genericDevice};
            auto secondary_bus = bridge.data.secondary_bus;
            check_bus(secondary_bus, list);
        }
    }
}
void pci_group::check_bus(uint8_t bus, linked_list<generic_device>& list) {
    for (uint8_t device = 0; device < 32; ++device) {
        check_device(bus, device, list);
    }
}
void pci_group::check_all(linked_list<generic_device>& list) {
    if (readConfigSpace(0, 0, 0, &generic_device::header_t::header_type) & 0x80) {
        for (uint8_t function = 0; function < 8; ++function) {
            if (readConfigSpace(0, 0, function, &generic_device::header_t::vendor_id) != 0xFFFF) {
                check_bus(function, list);
                break;
            }
        }
    } else {
        check_bus(0, list);
    }
}
void pci_group::read_configuration_space(PhysicalAddress address, void* buffer, size_t size) {
    // only 32bit aligned access is allowed
    if (address.address % sizeof(uint32_t) != 0) {
        auto aligned_address = address;
        uint8_t offset = 0;
        while (aligned_address.address % sizeof(uint32_t) != 0) {
            aligned_address.address--;
            offset++;
        }
        volatile auto* ptr = aligned_address.mapTmp().as<uint32_t*>();
        uint32_t data = *ptr;
        uint64_t align_start_size = sizeof(uint32_t) - offset;
        if (align_start_size > size) {
            align_start_size = size;
        }
        memcpy(buffer, (uint8_t*) &data + offset, align_start_size);
        size -= align_start_size;
        address.address += align_start_size;
        buffer = (uint8_t*) buffer + align_start_size;
    }
    if (size == 0)
        return;
    volatile auto* ptr = address.mapTmp().as<uint32_t*>();
    for (; size >= sizeof(uint32_t); size -= sizeof(uint32_t)) {
        *reinterpret_cast<uint32_t*>(buffer) = *ptr;
        ptr++;
        buffer = (uint8_t*) buffer + sizeof(uint32_t);
    }
    if (size > 0) {
        uint32_t data = *ptr;
        memcpy(buffer, &data, size);
    }
}
void pci_group::write_configuration_space(PhysicalAddress address, const void* buffer, size_t size) {
    if (address.address % sizeof(uint32_t) != 0) {
        auto aligned_address = address;
        uint8_t offset = 0;
        while (aligned_address.address % sizeof(uint32_t) != 0) {
            aligned_address.address--;
            offset++;
        }
        uint32_t data = *aligned_address.mapTmp().as<uint32_t*>();
        uint64_t align_start_size = sizeof(uint32_t) - offset;
        if (align_start_size > size) {
            align_start_size = size;
        }
        memcpy(&data + offset, buffer, align_start_size);
        *aligned_address.mapTmp().as<uint32_t*>() = data;
        size -= align_start_size;
        address.address += align_start_size;
        buffer = (uint8_t*) buffer + align_start_size;
    }
    if (size == 0)
        return;
    volatile auto* ptr = address.mapTmp().as<uint32_t*>();
    for (; size >= sizeof(uint32_t); size -= sizeof(uint32_t)) {
        *ptr = *reinterpret_cast<const uint32_t*>(buffer);
        ptr++;
        buffer = (uint8_t*) buffer + sizeof(uint32_t);
    }
    if (size > 0) {
        uint32_t data = *ptr;
        memcpy(&data, buffer, size);
        *ptr = data;
    }
}
const char* generic_device::human_readable_class() const {
    switch (header.class_code) {
        case 0x00:
            switch (header.subclass) {
                case 0x00:
                    return "Unclassified | Non-VGA-Compatible Unclassified Device";
                case 0x01:
                    return "Unclassified | VGA-Compatible Unclassified Device";
                default:
                    break;
            }
            return "Unclassified";
        case 0x01:
            switch (header.subclass) {
                case 0x00:
                    return "Mass Storage Controller | SCSI Bus Controller";
                case 0x01:
                    return "Mass Storage Controller | IDE Controller";
                case 0x02:
                    return "Mass Storage Controller | Floppy Disk Controller";
                case 0x03:
                    return "Mass Storage Controller | IPI Bus Controller";
                case 0x04:
                    return "Mass Storage Controller | RAID Controller";
                case 0x05:
                    return "Mass Storage Controller | ATA Controller";
                case 0x06:
                    return "Mass Storage Controller | SATA Controller";
                case 0x07:
                    return "Mass Storage Controller | Serial Attached SCSI Controller";
                case 0x08:
                    return "Mass Storage Controller | Non-Volatile Memory Controller";
                default:
                    break;
            }
            return "Mass Storage Controller";
        case 0x02:
            switch (header.subclass) {
                case 0x00:
                    return "Network Controller | Ethernet Controller";
                case 0x01:
                    return "Network Controller | Token Ring Controller";
                case 0x02:
                    return "Network Controller | FDDI Controller";
                case 0x03:
                    return "Network Controller | ATM Controller";
                case 0x04:
                    return "Network Controller | ISDN Controller";
                case 0x05:
                    return "Network Controller | WorldFip Controller";
                case 0x06:
                    return "Network Controller | PICMG 2.14 Multi Computing";
                case 0x07:
                    return "Network Controller | InfiniBand Controller";
                case 0x08:
                    return "Network Controller | Fabric Controller";
                default:
                    break;
            }
            return "Network Controller";
        case 0x03:
            switch (header.subclass) {
                case 0x00:
                    return "Display Controller | VGA Compatible Controller";
                case 0x01:
                    return "Display Controller | XGA Compatible Controller";
                case 0x02:
                    return "Display Controller | 3D Controller (Not VGA-Compatible)";
                default:
                    break;
            }
            return "Display Controller";
        case 0x04:
            switch (header.subclass) {
                case 0x00:
                    return "Multimedia Controller | Multimedia Video Controller";
                case 0x01:
                    return "Multimedia Controller | Multimedia Audio Controller";
                case 0x02:
                    return "Multimedia Controller | Computer Telephony Device";
                case 0x03:
                    return "Multimedia Controller | Audio Device";
                default:
                    break;
            }
            return "Multimedia Controller";
        case 0x05:
            switch (header.subclass) {
                case 0x00:
                    return "Memory Controller | RAM Controller";
                case 0x01:
                    return "Memory Controller | Flash Controller";
                default:
                    break;
            }
            return "Memory Controller";
        case 0x06:
            switch (header.subclass) {
                case 0x00:
                    return "Bridge Device | Host Bridge";
                case 0x01:
                    return "Bridge Device | ISA Bridge";
                case 0x02:
                    return "Bridge Device | EISA Bridge";
                case 0x03:
                    return "Bridge Device | MicroChannel Bridge";
                case 0x04:
                    return "Bridge Device | PCI-to-PCI Bridge";
                case 0x05:
                    return "Bridge Device | PCMCIA Bridge";
                case 0x06:
                    return "Bridge Device | NuBus Bridge";
                case 0x07:
                    return "Bridge Device | CardBus Bridge";
                case 0x08:
                    return "Bridge Device | RACEway Bridge";
                case 0x09:
                    return "Bridge Device | PCI-to-PCI Bridge";
                case 0x0A:
                    return "Bridge Device | InfiniBand-to-PCI Host Bridge";
                default:
                    break;
            }
            return "Bridge Device";
        case 0x07:
            switch (header.subclass) {
                case 0x00:
                    return "Simple Communication Controller | Serial Controller";
                case 0x01:
                    return "Simple Communication Controller | Parallel Controller";
                case 0x02:
                    return "Simple Communication Controller | Multiport Serial Controller";
                case 0x03:
                    return "Simple Communication Controller | Modem";
                case 0x04:
                    return "Simple Communication Controller | GPIB (IEEE 488.1/2) Controller";
                case 0x05:
                    return "Simple Communication Controller | Smart Card";
                default:
                    break;
            }
            return "Simple Communication Controller";
        case 0x08:
            switch (header.subclass) {
                case 0x00:
                    return "Base System Peripherals | PIC";
                case 0x01:
                    return "Base System Peripherals | DMA Controller";
                case 0x02:
                    return "Base System Peripherals | Timer";
                case 0x03:
                    return "Base System Peripherals | RTC Controller";
                case 0x04:
                    return "Base System Peripherals | PCI Hot-Plug Controller";
                case 0x05:
                    return "Base System Peripherals | SD Host Controller";
                case 0x06:
                    return "Base System Peripherals | IOMMU";
                default:
                    break;
            }
            return "Base System Peripheral";
        case 0x09:
            switch (header.subclass) {
                case 0x00:
                    return "Input Devices | Keyboard Controller";
                case 0x01:
                    return "Input Devices | Digitizer (Pen)";
                case 0x02:
                    return "Input Devices | Mouse Controller";
                case 0x03:
                    return "Input Devices | Scanner Controller";
                case 0x04:
                    return "Input Devices | Gameport Controller";
                default:
                    break;
            }
            return "Input Device";
        case 0x0A:
            return "Docking Station";
        case 0x0B:
            switch (header.subclass) {
                case 0x00:
                    return "Processor | 386";
                case 0x01:
                    return "Processor | 486";
                case 0x02:
                    return "Processor | Pentium";
                case 0x10:
                    return "Processor | Alpha";
                case 0x20:
                    return "Processor | PowerPC";
                case 0x30:
                    return "Processor | MIPS";
                case 0x40:
                    return "Processor | Co-Processor";
                default:
                    break;
            }
            return "Processor";
        case 0x0C:
            switch (header.subclass) {
                case 0x00:
                    return "Serial Bus Controller | FireWire (IEEE 1394)";
                case 0x01:
                    return "Serial Bus Controller | ACCESS Bus";
                case 0x02:
                    return "Serial Bus Controller | SSA";
                case 0x03:
                    return "Serial Bus Controller | USB Controller";
                case 0x04:
                    return "Serial Bus Controller | Fibre Channel";
                case 0x05:
                    return "Serial Bus Controller | SMBus";
                case 0x06:
                    return "Serial Bus Controller | InfiniBand";
                case 0x07:
                    return "Serial Bus Controller | IPMI Interface";
                case 0x08:
                    return "Serial Bus Controller | SERCOS Interface Standard (IEC 61491)";
                case 0x09:
                    return "Serial Bus Controller | CANbus";
                default:
                    break;
            }
            return "Serial Bus Controller";
        case 0x0D:
            switch (header.subclass) {
                case 0x00:
                    return "Wireless Controller | iRDA Compatible Controller";
                case 0x01:
                    return "Wireless Controller | Consumer IR Controller";
                case 0x10:
                    return "Wireless Controller | RF Controller";
                case 0x11:
                    return "Wireless Controller | Bluetooth";
                case 0x12:
                    return "Wireless Controller | Broadband";
                case 0x20:
                    return "Wireless Controller | Ethernet (802.11a - 5 GHz)";
                case 0x21:
                    return "Wireless Controller | Ethernet (802.11b - 2.4 GHz)";
                default:
                    break;
            }
            return "Wireless Controller";
        case 0x0E:
            switch (header.subclass) {
                case 0x00:
                    return "Intelligent I/O Controller | I20";
                default:
                    break;
            }
            return "Intelligent I/O Controller";
        case 0x0F:
            switch (header.subclass) {
                case 0x01:
                    return "Satellite Communication Controller | TV";
                case 0x02:
                    return "Satellite Communication Controller | Audio";
                case 0x03:
                    return "Satellite Communication Controller | Voice";
                case 0x04:
                    return "Satellite Communication Controller | Data";
                default:
                    break;
            }
            return "Satellite Communication Controller";
        case 0x10:
            switch (header.subclass) {
                case 0x00:
                    return "Encryption Controller | Network and Computing Encrpytion/Decryption";
                case 0x10:
                    return "Encryption Controller | Entertainment Encryption/Decryption";
                default:
                    break;
            }
            return "Encryption Controller";
        case 0x11:
            switch (header.subclass) {
                case 0x00:
                    return "Data Acquisition and Signal Processing Controller | DPIO Modules";
                case 0x01:
                    return "Data Acquisition and Signal Processing Controller | Performance Counters";
                case 0x10:
                    return "Data Acquisition and Signal Processing Controller | Communications Syncrhonization + Time and Frequency Test/Measurment";
                case 0x20:
                    return "Data Acquisition and Signal Processing Controller | Management Card";
                default:
                    break;
            }
            return "Data Acquisition and Signal Processing Controller";
        case 0x12:
            return "Processing Accelerator";
        case 0x13:
            return "non-Essential Instrumentation";
        case 0x40:
            return "Co-Processor";
        case 0xFF:
            return "Unassigned Class";
        default:
            return "Unknown Class";
    }
}

static bool pci_has_init;
static btree_map<uint64_t, device_header::BAR>* pci_bar_memory;
static VirtualAddress pci_bar_memory_next;
static void init() {
    if (pci_has_init) return;
    pci_has_init = true;
    pci_bar_memory = new btree_map<uint64_t, device_header::BAR>();
    pci_bar_memory_next = VirtualAddress{126_Ti};
}

VirtualAddress allocate_uncached_virtual(size_t count) {
    init();
    auto address = pci_bar_memory_next;
    pci_bar_memory_next += count * page_size;
    return address;
}

optional<device_header::BAR> device_header::getBAR(size_t index) const {
    init();
    uint64_t bar = getBARRaw(index);
    if (bar == 0) return {};
    if ((bar & 0b111) == 0b100) {
        // 64 bit
        bar |= static_cast<uint64_t>(getBARRaw(index + 1)) << 32;
    }

    auto bar_cache = pci_bar_memory->find(bar);
    if (bar_cache.has_value()) {
        return bar_cache.value();
    }
    auto size_opt = getBARSize(index);
    if (!size_opt.has_value()) return {};
    auto size = size_opt.value();
    if (bar & 0b1) {
        // io bar
        BAR result(static_cast<uint16_t>(bar & ~0b11), size);
        pci_bar_memory->insert(bar, result);
        return result;
    } else {
        bool prefetchable = false;

        if ((bar & 0b1111) == 0b1000) {
            prefetchable = true;
        }

        uint64_t physical_address;
        physical_address = bar & ~0b1111;

        BAR res;
        if (prefetchable) {
            auto address = PhysicalAddress{physical_address}.mapTmp();
            res = BAR(address, size);
        } else {
            auto page_count = (size + page_size - 1) / page_size;
            auto address = allocate_uncached_virtual(page_count);
            PageTable::map(PhysicalAddress{physical_address}, address, {.writeable = true, .user = false, .writeThrough = true, .cacheDisabled = true});
            res = BAR(address, size);
        }
        pci_bar_memory->insert(bar, res);
        return res;
    }
}
optional<size_t> device_header::getBARSize(size_t index) const {
    if (index >= 6) return {};
    auto barOffset = 0x10 + index * 4;
    volatile auto* ptr = device.address.mapTmp().as<uint32_t*>() + barOffset / 4;
    uint64_t bar = *ptr;
    if ((bar & 0b111) == 0b100) {
        if (index == 5) return {};
        // 64 bit
        bar |= static_cast<uint64_t>(*(ptr + 1)) << 32;
        *(ptr + 1) = ~static_cast<uint32_t>(0);
    }
    *ptr = ~static_cast<uint32_t>(0);
    flush_cache();
    uint64_t size = *ptr;
    if (bar & 0b1) {
        // IO
        size &= ~0b11;
    } else {
        // Memory
        size &= ~0b1111;
    }
    if ((bar & 0b111) == 0b100) {
        // 64 bit
        uint32_t value = *(ptr + 1);
        value &= ~0b1111;
        size |= static_cast<uint64_t>(value) << 32;
        *(ptr + 1) = static_cast<uint32_t>(bar >> 32);
    }
    *ptr = static_cast<uint32_t>(bar);
    flush_cache();
    if (size == 0) return {};
    size = ~size + 1;
    if ((bar & 0b111) != 0b100) {
        size &= ~uint32_t(0);
    }
    return size;
}
uint32_t device_header::getBARRaw(size_t index) const {
    switch (index) {
        case 0:
            return data.bar0;
        case 1:
            return data.bar1;
        case 2:
            return data.bar2;
        case 3:
            return data.bar3;
        case 4:
            return data.bar4;
        case 5:
            return data.bar5;
        default:
            return 0;
    }
}
void device_header::printBAR(size_t index, bool printUnused) const {
    auto bar = getBAR(index);
    if (!bar.has_value()) {
        if (printUnused) Log::printf(Log::Level::Debug, "PCI", "BAR%d: Unused\n", index);
        return;
    }
    auto raw = getBARRaw(index);
    const char* type_str;
    auto type_value = raw & 0b1111;
    switch (type_value) {
        case 0b0000:
            type_str = "Mem 32-bit !pref";
            break;
        case 0b1000:
            type_str = "Mem 32-bit  pref";
            break;
        case 0b0100:
            type_str = "Mem 64-bit !pref";
            break;
        case 0b1100:
            type_str = "Mem 64-bit  pref";
            break;
        case 0b0001:
        case 0b0011:
        case 0b0101:
        case 0b0111:
        case 0b1001:
        case 0b1011:
        case 0b1101:
        case 0b1111:
            type_str = "I/O";
            break;
        default:
            type_str = "Unknown";
            break;
    }
    Log::printf(Log::Debug, "PCI", "BAR%d: %s, Size: 0x%x, Pointer: 0x%x\n", index, type_str, bar->size,
                bar->getAddress()
                        .map<uint64_t>([](const VirtualAddress& address) -> uint64_t {
                            return address.address;
                        })
                        .value_or_create([&]() {
                            return static_cast<uint64_t>(bar->getIOAddress().value_or_panic("Invalid BAR"));
                        }));
}
bool device_header::setup_msi(uint8_t interrupt_vector) {
    // find msi or msi_x capability
    auto msi_cap = get_capability(0x05);
    if (!msi_cap) return false;
    uint16_t control;
    pci_group::read_configuration_space(msi_cap->current.address + 0x2, &control, sizeof(control));
    control &= ~(0b1110000);// disable multiple messages
    control |= 0b1;         // enable msi
    if (!(control & (1 << 7)))
        return false;
    uint64_t msi_address = 0xFEE00000 | (0 << 12);
    uint16_t msi_data = interrupt_vector;
    pci_group::write_configuration_space(msi_cap->current.address + 0x4, &msi_address, sizeof(msi_address));
    pci_group::write_configuration_space(msi_cap->current.address + 0xC, &msi_data, sizeof(msi_data));
    pci_group::write_configuration_space(msi_cap->current.address + 0x2, &control, sizeof(control));
    return true;
}
}// namespace PCI