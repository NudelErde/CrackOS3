//
// Created by nudelerde on 11.12.22.
//

#pragma once

#include "ACPI/RSDP.h"
#include "data/linked_list.h"
#include "features/variant.h"

namespace PCI {

struct pci_group;

struct generic_device {
    struct header_t {
        uint16_t vendor_id;
        uint16_t device_id;
        uint16_t command;
        uint16_t status;
        uint8_t revision_id;
        uint8_t prog_if;
        uint8_t subclass;
        uint8_t class_code;
        uint8_t cache_line_size;
        uint8_t latency_timer;
        uint8_t header_type;
        uint8_t bist;
    } __attribute__((packed));
    header_t header{};
    PhysicalAddress address;
    uint8_t bus{};
    uint8_t device{};
    uint8_t function{};

    [[nodiscard]] const char* human_readable_class() const;
};

enum class generate_match : uint8_t {
    class_code = 1 << 0,
    subclass = 1 << 1,
    prog_if = 1 << 2
};

constexpr generate_match operator|(generate_match a, generate_match b) {
    return static_cast<generate_match>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

constexpr generate_match operator&(generate_match a, generate_match b) {
    return static_cast<generate_match>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

constexpr bool is_set(generate_match a, generate_match b) {
    return (a & b) == b;
}

struct nothing {};

template<typename Variant>
void generate_fill(Variant& variant, const generic_device& device) {
    variant = nothing{};
}

template<typename Variant, typename Type, typename... Types>
void generate_fill(Variant& variant, const generic_device& device) {
    if ((!is_set(Type::match, generate_match::class_code) || device.header.class_code == Type::class_code) && (!is_set(Type::match, generate_match::subclass) || device.header.subclass == Type::subclass) && (!is_set(Type::match, generate_match::prog_if) || device.header.prog_if == Type::prog_if)) {
        variant = Type{device};
    } else {
        generate_fill<Variant, Types...>(variant, device);
    }
}

template<typename... Types>
variant<nothing, Types...> generate(const generic_device& device) {
    using variant_t = variant<nothing, Types...>;
    variant_t result;
    generate_fill<variant_t, Types...>(result, device);
    return result;
}

struct pci_bridge {
    generic_device device;
    struct data_t {
        uint32_t bar0;
        uint32_t bar1;
        uint8_t primary_bus;
        uint8_t secondary_bus;
        uint8_t subordinate_bus;
        uint8_t secondary_latency_timer;
        uint8_t io_base;
        uint8_t io_limit;
        uint16_t secondary_status;
        uint16_t memory_base;
        uint16_t memory_limit;
        uint16_t prefetchable_memory_base;
        uint16_t prefetchable_memory_limit;
        uint32_t prefetchable_memory_base_upper;
        uint32_t prefetchable_memory_limit_upper;
        uint16_t io_base_upper;
        uint16_t io_limit_upper;
        uint8_t capability_pointer;
        uint8_t reserved[3];
        uint32_t expansion_rom_base;
        uint8_t interrupt_line;
        uint8_t interrupt_pin;
        uint16_t bridge_control;
    } __attribute__((packed));
    data_t data{};
    explicit pci_bridge(generic_device device) : device(device) {
        data = *(device.address.mapTmp() + sizeof(generic_device::header_t)).as<data_t*>();
    }
};

VirtualAddress allocate_uncached_virtual(size_t count);

struct MCFG : public ACPI::SDP {

    size_t getEntryCount();
    pci_group getEntry(size_t index);

private:
    explicit MCFG(ACPI::SDP sdp) : ACPI::SDP(sdp) {}

    friend optional<MCFG> make_mcfg(ACPI::SDP sdp);
};

optional<MCFG> make_mcfg(ACPI::SDP sdp);

struct pci_group {
private:
    PhysicalAddress base;
    uint16_t segment_number{};
    uint8_t start_number{};
    uint8_t end_number{};

    friend class MCFG;

    pci_group(PhysicalAddress base, uint16_t segment_number, uint8_t start_number, uint8_t end_number) : base(base), segment_number(segment_number), start_number(start_number), end_number(end_number) {}

    void check_bus(uint8_t bus, linked_list<generic_device>& list);
    void check_device(uint8_t bus, uint8_t device, linked_list<generic_device>& list);
    void check_function(uint8_t bus, uint8_t device, uint8_t function, linked_list<generic_device>& list);

protected:
    [[nodiscard]] PhysicalAddress getPhysical(uint8_t bus, uint8_t device, uint8_t function) const {
        return {base.address + (bus << 20) + (device << 15) + (function << 12)};
    }

    template<typename ReturnType, typename ConfigurationSpaceObject>
    ReturnType readConfigSpace(uint8_t bus, uint8_t device, uint8_t function, ReturnType ConfigurationSpaceObject::*member) const {
        ReturnType buffer;
        auto offset = reinterpret_cast<uint64_t>(&(reinterpret_cast<ConfigurationSpaceObject*>((void*) nullptr)->*member));
        read_configuration_space(PhysicalAddress{getPhysical(bus, device, function).address + offset}, &buffer, sizeof(ReturnType));
        return buffer;
    }

    [[nodiscard]] uint32_t getHeaderAt(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t size) const {
        uint32_t buffer;
        PhysicalAddress address = getPhysical(bus, device, function);
        memcpy(&buffer, address.mapTmp() + offset, size);
        return buffer;
    }

public:
    pci_group() = default;

    [[nodiscard]] uint16_t getSegmentNumber() const { return segment_number; }
    [[nodiscard]] uint8_t getStartNumber() const { return start_number; }
    [[nodiscard]] uint8_t getEndNumber() const { return end_number; }

    void check_all(linked_list<generic_device>& list);

    static void read_configuration_space(PhysicalAddress address, void* buffer, size_t size);
    static void write_configuration_space(PhysicalAddress address, const void* buffer, size_t size);
};

struct device_header {
    static bool is_normal_device(const generic_device& device) {
        return is_normal_device(device.header);
    }
    static bool is_normal_device(const generic_device::header_t& header) {
        return is_normal_device(header.header_type);
    }
    static bool is_normal_device(uint8_t header_type) {
        return (header_type & 0x7F) == 0;
    }
    generic_device device;
    struct data_t {
        uint32_t bar0;
        uint32_t bar1;
        uint32_t bar2;
        uint32_t bar3;
        uint32_t bar4;
        uint32_t bar5;
        uint32_t cardbus_cis_pointer;
        uint16_t subsystem_vendor_id;
        uint16_t subsystem_id;
        uint32_t expansion_rom_base_address;
        uint8_t capabilities_pointer;
        uint8_t reserved[7];
        uint8_t interrupt_line;
        uint8_t interrupt_pin;
        uint8_t min_grant;
        uint8_t max_latency;
    } __attribute__((packed));
    data_t data{};
    explicit device_header(generic_device device) : device(device) {
        pci_group::read_configuration_space(PhysicalAddress{device.address.address + sizeof(generic_device::header_t)}, &data, sizeof(data_t));
    }

    struct capabilities {
        PhysicalAddress device_base;
        PhysicalAddress current;

        [[nodiscard]] bool has_next() const {
            return current != device_base;
        }

        [[nodiscard]] capabilities next() const {
            uint8_t next_offset = 0;
            pci_group::read_configuration_space(current.address + 1, &next_offset, 1);
            return {device_base, device_base.address + next_offset};
        }

        [[nodiscard]] uint8_t type() const {
            uint8_t type = 0;
            pci_group::read_configuration_space(current.address, &type, 1);
            return type;
        }
    };

    [[nodiscard]] capabilities get_capabilities() const {
        return capabilities{device.address, PhysicalAddress{(device.address.address + data.capabilities_pointer) & ~0x3}};
    }

    [[nodiscard]] optional<capabilities> get_capability(uint8_t type) const {
        auto cap = get_capabilities();
        while (cap.has_next()) {
            auto cap_type = cap.type();
            if (cap_type == type) {
                return cap;
            }
            cap = cap.next();
        }
        return {};
    }

    struct BAR {
        BAR() : size(0), is_memory(false), io_address(0) {}
        BAR(uint16_t io_address, size_t size) : size(size), io_address(io_address), is_memory(false) {}
        BAR(VirtualAddress address, size_t size) : size(size), address(address), is_memory(true) {}

        size_t size{};

        [[nodiscard]] optional<VirtualAddress> getAddress() const {
            if (is_memory) {
                return address;
            } else {
                return {};
            }
        }

        [[nodiscard]] optional<uint16_t> getIOAddress() const {
            if (!is_memory) {
                return io_address;
            } else {
                return {};
            }
        }

    private:
        union {
            VirtualAddress address;
            uint16_t io_address = 0;
        };
        bool is_memory = true;
    };

    [[nodiscard]] optional<BAR> getBAR(size_t index) const;
    [[nodiscard]] optional<size_t> getBARSize(size_t index) const;
    void printBAR(size_t index, bool printUnused) const;
    bool setup_msi(uint8_t interrupt_vector);

private:
    [[nodiscard]] uint32_t getBARRaw(size_t index) const;
};

}// namespace PCI
