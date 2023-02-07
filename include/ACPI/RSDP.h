//
// Created by nudelerde on 10.12.22.
//

#pragma once

#include "features/optional.h"
#include "memory/mem.h"

namespace ACPI {

struct RSDPDescriptor {
    char signature[8];
    uint8_t checksum;
    char OEMID[6];
    uint8_t revision;
    uint32_t rsdtAddress;
} __attribute__((packed));

struct RSDPDescriptor2 {
    RSDPDescriptor firstPart;
    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t extendedChecksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct SDP {
private:
    char signature[5];// null terminated

protected:
    VirtualAddress ptr;

public:
    explicit SDP(VirtualAddress ptr) : ptr(ptr), signature{} {
        memcpy(signature, ptr.as<char*>(), 4);
        signature[4] = '\0';
    }

    const char* getSignature() { return signature; }
};

struct RootSDT {
    virtual size_t getSDPCount() = 0;
    virtual SDP getSDP(uint64_t index) = 0;
    optional<SDP> getSDP(const char* signature);
};

RootSDT* getRoot(PhysicalAddress multiboot2_header);

struct RSDT : public RootSDT {
    RSDPDescriptor descriptor;
    VirtualAddress rsdtAddress;

    explicit RSDT(RSDPDescriptor descriptor) : descriptor(descriptor) {
        rsdtAddress = PhysicalAddress(static_cast<uint64_t>(descriptor.rsdtAddress)).mapTmp();
    }

    size_t getSDPCount() override;
    SDP getSDP(uint64_t index) override;
};

struct XSDT : public RootSDT {
    RSDPDescriptor2 descriptor;
    VirtualAddress xsdtAddress;

    explicit XSDT(RSDPDescriptor2 descriptor) : descriptor(descriptor) {
        xsdtAddress = PhysicalAddress(descriptor.xsdtAddress).mapTmp();
    }

    size_t getSDPCount() override;
    SDP getSDP(uint64_t index) override;
};

}// namespace ACPI
