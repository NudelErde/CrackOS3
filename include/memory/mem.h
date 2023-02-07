#pragma once
#include "int.h"
#include "memory/mem.h"

template<typename T>
struct optional;

#define saveReadSymbol(name) [&]() -> uint64_t { \
    uint64_t tmp;                                \
    asm volatile("movabs $" name ", %0"          \
                 : "=r"(tmp)::);                 \
    return tmp;                                  \
}()


enum class Segment {
    KERNEL_CODE = 1,
    KERNEL_DATA = 2,
    USER_CODE = 3,
    USER_DATA = 4,
};


struct SegmentSelector {
    uint8_t requestedPrivilegeLevel : 2;
    bool inLocalTable : 1;
    uint16_t index : 13;

    SegmentSelector() : requestedPrivilegeLevel(0), inLocalTable(false), index(0) {}

    explicit SegmentSelector(Segment segment) {
        if (segment < Segment::USER_CODE) {
            requestedPrivilegeLevel = 0;
        } else {
            requestedPrivilegeLevel = 3;
        }
        inLocalTable = false;
        index = (uint16_t) segment;
    }
};

constexpr uint64_t page_size = 4096;

union VirtualAddress;

struct PhysicalAddress {
    uint64_t address;

    constexpr PhysicalAddress() : address(0){};
    constexpr PhysicalAddress(uint64_t address) : address(address) {}

    VirtualAddress mapTmp() const;

    bool operator==(const PhysicalAddress& other) const { return address == other.address; }
    bool operator!=(const PhysicalAddress& other) const { return address != other.address; }
    bool operator<(const PhysicalAddress& other) const { return address < other.address; }
    bool operator>(const PhysicalAddress& other) const { return address > other.address; }
    bool operator<=(const PhysicalAddress& other) const { return address <= other.address; }
    bool operator>=(const PhysicalAddress& other) const { return address >= other.address; }
};

static_assert(sizeof(PhysicalAddress) == 8);

union VirtualAddress {
    uint64_t address;
    struct {
        uint64_t offset : 12;
        uint64_t l1Offset : 9;
        uint64_t l2Offset : 9;
        uint64_t l3Offset : 9;
        uint64_t l4Offset : 9;
        uint64_t padding : 16;
    };

    [[nodiscard]] optional<uint64_t> getOffset(uint8_t level) const;

    operator void*() const {
        return (void*) address;
    }

    template<typename T>
    T as() const {
        return (T) address;
    }

    template<typename T>
    constexpr explicit VirtualAddress(T* ptr) : address((uint64_t) ptr) {}

    constexpr VirtualAddress(uint64_t data) : address(data) {}

    constexpr VirtualAddress() : address(0) {}
    constexpr VirtualAddress(uint16_t offset, uint16_t l1, uint16_t l2, uint16_t l3, uint16_t l4) : offset(offset), l1Offset(l1), l2Offset(l2), l3Offset(l3), l4Offset(l4) {}

    friend VirtualAddress& operator+=(VirtualAddress& lhs, size_t rhs) {
        lhs.address += rhs;
        return lhs;
    }
    friend VirtualAddress operator+(VirtualAddress lhs, size_t rhs) {
        return {lhs.address + rhs};
    }
    friend VirtualAddress& operator-=(VirtualAddress& lhs, size_t rhs) {
        lhs.address += rhs;
        return lhs;
    }
    friend VirtualAddress operator-(VirtualAddress lhs, size_t rhs) {
        return {lhs.address - rhs};
    }

    friend VirtualAddress& operator+=(VirtualAddress& lhs, VirtualAddress rhs) {
        lhs.address += rhs.address;
        return lhs;
    }
    friend VirtualAddress operator+(VirtualAddress lhs, VirtualAddress rhs) {
        return {lhs.address + rhs.address};
    }
    friend VirtualAddress& operator-=(VirtualAddress& lhs, VirtualAddress rhs) {
        lhs.address += rhs.address;
        return lhs;
    }
    friend VirtualAddress operator-(VirtualAddress lhs, VirtualAddress rhs) {
        return {lhs.address - rhs.address};
    }
};

static_assert(sizeof(VirtualAddress) == 8);

namespace PhysicalAllocator {

void init(PhysicalAddress multiboot_info);
optional<PhysicalAddress> alloc(uint64_t count);
void free(PhysicalAddress address, uint64_t count);
void clear_startup();

}// namespace PhysicalAllocator

inline int memcmp(const void* a, const void* b, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        if (((uint8_t*) a)[i] != ((uint8_t*) b)[i]) {
            return ((uint8_t*) a)[i] - ((uint8_t*) b)[i];
        }
    }
    return 0;
}

inline void* memcpy(void* dst, const void* src, uint64_t count) {
    asm volatile("rep movsb"
                 : "+S"(src), "+c"(count)
                 : "D"(dst));
    return dst;
}

inline void* memset(void* dst, uint8_t value, uint64_t count) {
    asm volatile("rep stosb"
                 : "+c"(count), "+a"(value)
                 : "D"(dst));
    return dst;
}

inline volatile void* memset(volatile void* dst, uint8_t value, uint64_t count) {
    for(uint64_t i = 0; i < count; i++) {
        ((volatile uint8_t*) dst)[i] = value;
    }
    return dst;
}

inline PhysicalAddress operator"" _phys(unsigned long long address) {
    return {address};
}

inline VirtualAddress operator"" _virt(unsigned long long address) {
    return {address};
}

// inline placement new
inline void* operator new(size_t size, void* address) {
    return address;
}