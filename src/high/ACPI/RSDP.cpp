//
// Created by nudelerde on 10.12.22.
//

#include "ACPI/RSDP.h"
#include "multiboot2/multiboot2.h"
#include "out/panic.h"
#include "features/string.h"

namespace ACPI {

RootSDT* getRoot(PhysicalAddress multiboot2_header) {
    auto* xsdt_tag = multiboot2::getTag(multiboot2_header, 15);
    if (xsdt_tag) {
        return new XSDT(*reinterpret_cast<RSDPDescriptor2*>(xsdt_tag + 8));
    }
    auto rsdt_tag = multiboot2::getTag(multiboot2_header, 14);
    if (rsdt_tag) {
        return new RSDT(*reinterpret_cast<RSDPDescriptor*>(rsdt_tag + 8));
    }
    return nullptr;
}

size_t RSDT::getSDPCount() {
    auto* ptr = rsdtAddress.as<uint8_t*>();
    size_t size = *reinterpret_cast<uint32_t*>(ptr + 4);
    size -= 4;// Signature
    size -= 4;// Length
    size -= 1;// Revision
    size -= 1;// Checksum
    size -= 6;// OEMID
    size -= 8;// OEM Table ID
    size -= 4;// OEM Revision
    size -= 4;// Creator ID
    size -= 4;// Creator Revision
    return size / 4;
}
SDP RSDT::getSDP(uint64_t index) {
    auto val = *reinterpret_cast<uint32_t*>(rsdtAddress.as<uint8_t*>() + 36 + index * 4);
    return SDP{PhysicalAddress{val}.mapTmp()};
}

size_t XSDT::getSDPCount() {
    auto ptr = xsdtAddress.as<uint8_t*>();
    size_t size = *reinterpret_cast<uint32_t*>(ptr + 4);
    size -= 4;// Signature
    size -= 4;// Length
    size -= 1;// Revision
    size -= 1;// Checksum
    size -= 6;// OEMID
    size -= 8;// OEM Table ID
    size -= 4;// OEM Revision
    size -= 4;// Creator ID
    size -= 4;// Creator Revision
    return size / 8;
}
SDP XSDT::getSDP(uint64_t index) {
    auto val = *reinterpret_cast<uint64_t*>(xsdtAddress.as<uint8_t*>() + 36 + index * 8);
    return SDP{PhysicalAddress{val}.mapTmp()};
}

optional<SDP> RootSDT::getSDP(const char* signature) {
    size_t count = getSDPCount();
    for(size_t i = 0; i < count; ++i) {
        SDP sdp = getSDP(i);
        if(strcmp(signature, sdp.getSignature()) == 0) {
            return sdp;
        }
    }
    return {};
}
}// namespace ACPI
