#pragma once
#include "int.h"
#include "out/panic.h"

inline uint64_t getCR4() {
    uint64_t cr4;
    asm volatile("mov %%cr4, %0"
                 : "=r"(cr4));
    return cr4;
}

inline void setCR4(uint64_t cr4) {
    asm volatile("mov %0, %%cr4"
                 :
                 : "r"(cr4));
}

inline uint64_t getCR3() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0"
                 : "=r"(cr3));
    return cr3;
}

inline void setCR3(uint64_t cr3) {
    asm volatile("mov %0, %%cr3"
                 :
                 : "r"(cr3));
}

inline uint64_t getCR0() {
    uint64_t cr0;
    asm volatile("mov %%cr0, %0"
                 : "=r"(cr0));
    return cr0;
}

inline void setCR0(uint64_t cr0) {
    asm volatile("mov %0, %%cr0"
                 :
                 : "r"(cr0));
}

inline uint64_t getCR2() {
    uint64_t cr2;
    asm volatile("mov %%cr2, %0"
                 : "=r"(cr2));
    return cr2;
}

inline uint64_t getCR8() {
    uint64_t cr8;
    asm volatile("mov %%cr8, %0"
                 : "=r"(cr8));
    return cr8;
}

inline void setCR8(uint64_t cr8) {
    asm volatile("mov %0, %%cr8"
                 :
                 : "r"(cr8));
}

inline uint64_t getFlags() {
    uint64_t flags;
    asm volatile("pushfq\n pop %0"
                 : "=r"(flags));
    return flags;
}

inline void setFlags(uint64_t flags) {
    asm volatile("push %0\n popfq"
                 :
                 : "r"(flags));
}

inline uint64_t getMSR(uint64_t msr) {
    uint64_t value;
    asm volatile(R"(
        rdmsr
        )"
                 : "=a"(value)
                 : "c"(msr));
    return value;
}

inline void setMSR(uint64_t msr, uint64_t value) {
    asm volatile(R"(
        wrmsr
        )"
                 :
                 : "c"(msr), "a"(value));
}

inline uint64_t getEFER() {
    return getMSR(0xc0000080);
}

inline void setEFER(uint64_t efer) {
    setMSR(0xc0000080, efer);
}

inline uint64_t getFSBase() {
    return getMSR(0xc0000100);
}

inline void setFSBase(uint64_t base) {
    setMSR(0xc0000100, base);
}

inline uint64_t getGSBase() {
    return getMSR(0xc0000101);
}

inline void setGSBase(uint64_t base) {
    setMSR(0xc0000101, base);
}

inline uint64_t getKernelGSBase() {
    return getMSR(0xc0000102);
}

inline void setKernelGSBase(uint64_t base) {
    setMSR(0xc0000102, base);
}

inline void swapGS() {
    asm volatile("swapgs"
                 :
                 :
                 : "memory");
}

inline uint64_t getCS() {
    uint64_t cs;
    asm volatile("mov %%cs, %0"
                 : "=r"(cs));
    return cs;
}

inline uint64_t getDS() {
    uint64_t ds;
    asm volatile("mov %%ds, %0"
                 : "=r"(ds));
    return ds;
}

inline uint64_t getES() {
    uint64_t es;
    asm volatile("mov %%es, %0"
                 : "=r"(es));
    return es;
}

inline uint64_t getFS() {
    uint64_t fs;
    asm volatile("mov %%fs, %0"
                 : "=r"(fs));
    return fs;
}

inline uint64_t getGS() {
    uint64_t gs;
    asm volatile("mov %%gs, %0"
                 : "=r"(gs));
    return gs;
}

inline uint64_t getSS() {
    uint64_t ss;
    asm volatile("mov %%ss, %0"
                 : "=r"(ss));
    return ss;
}

inline uint64_t getDR7() {
    uint64_t dr7;
    asm volatile("mov %%dr7, %0"
                 : "=r"(dr7));
    return dr7;
}

inline void setDR7(uint64_t dr7) {
    asm volatile("mov %0, %%dr7"
                 :
                 : "r"(dr7));
}

inline uint64_t getDR6() {
    uint64_t dr6;
    asm volatile("mov %%dr6, %0"
                 : "=r"(dr6));
    return dr6;
}

inline uint64_t getDR0() {
    uint64_t dr0;
    asm volatile("mov %%dr0, %0"
                 : "=r"(dr0));
    return dr0;
}

inline uint64_t getDR1() {
    uint64_t dr1;
    asm volatile("mov %%dr1, %0"
                 : "=r"(dr1));
    return dr1;
}

inline uint64_t getDR2() {
    uint64_t dr2;
    asm volatile("mov %%dr2, %0"
                 : "=r"(dr2));
    return dr2;
}

inline uint64_t getDR3() {
    uint64_t dr3;
    asm volatile("mov %%dr3, %0"
                 : "=r"(dr3));
    return dr3;
}

inline void setDR0(uint64_t dr0) {
    asm volatile("mov %0, %%dr0"
                 :
                 : "r"(dr0));
}

inline void setDR1(uint64_t dr1) {
    asm volatile("mov %0, %%dr1"
                 :
                 : "r"(dr1));
}

inline void setDR2(uint64_t dr2) {
    asm volatile("mov %0, %%dr2"
                 :
                 : "r"(dr2));
}

inline void setDR3(uint64_t dr3) {
    asm volatile("mov %0, %%dr3"
                 :
                 : "r"(dr3));
}

inline void cpuid(uint32_t in, uint32_t* out_a, uint32_t* out_b, uint32_t* out_c, uint32_t* out_d) {
    uint32_t unused;
    if (!out_a) out_a = &unused;
    if (!out_b) out_b = &unused;
    if (!out_c) out_c = &unused;
    if (!out_d) out_d = &unused;
    asm volatile("cpuid"
                 : "=a"(*out_a), "=b"(*out_b), "=c"(*out_c), "=d"(*out_d)
                 : "a"(in));
}

inline uint64_t getRAX() {
    uint64_t rax;
    asm volatile("mov %%rax, %0"
                 : "=r"(rax));
    return rax;
}

inline uint64_t getRBX() {
    uint64_t rbx;
    asm volatile("mov %%rbx, %0"
                 : "=r"(rbx));
    return rbx;
}

inline uint64_t getRCX() {
    uint64_t rcx;
    asm volatile("mov %%rcx, %0"
                 : "=r"(rcx));
    return rcx;
}

inline uint64_t getRDX() {
    uint64_t rdx;
    asm volatile("mov %%rdx, %0"
                 : "=r"(rdx));
    return rdx;
}

inline uint64_t getRSI() {
    uint64_t rsi;
    asm volatile("mov %%rsi, %0"
                 : "=r"(rsi));
    return rsi;
}

inline uint64_t getRDI() {
    uint64_t rdi;
    asm volatile("mov %%rdi, %0"
                 : "=r"(rdi));
    return rdi;
}

inline uint64_t getRBP() {
    uint64_t rbp;
    asm volatile("mov %%rbp, %0"
                 : "=r"(rbp));
    return rbp;
}

inline uint64_t getR8() {
    uint64_t r8;
    asm volatile("mov %%r8, %0"
                 : "=r"(r8));
    return r8;
}

inline uint64_t getR9() {
    uint64_t r9;
    asm volatile("mov %%r9, %0"
                 : "=r"(r9));
    return r9;
}

inline uint64_t getR10() {
    uint64_t r10;
    asm volatile("mov %%r10, %0"
                 : "=r"(r10));
    return r10;
}

inline uint64_t getR11() {
    uint64_t r11;
    asm volatile("mov %%r11, %0"
                 : "=r"(r11));
    return r11;
}

inline uint64_t getR12() {
    uint64_t r12;
    asm volatile("mov %%r12, %0"
                 : "=r"(r12));
    return r12;
}

inline uint64_t getR13() {
    uint64_t r13;
    asm volatile("mov %%r13, %0"
                 : "=r"(r13));
    return r13;
}

inline uint64_t getR14() {
    uint64_t r14;
    asm volatile("mov %%r14, %0"
                 : "=r"(r14));
    return r14;
}

inline uint64_t getR15() {
    uint64_t r15;
    asm volatile("mov %%r15, %0"
                 : "=r"(r15));
    return r15;
}

inline uint64_t getRSP() {
    uint64_t rsp;
    asm volatile("mov %%rsp, %0"
                 : "=r"(rsp));
    return rsp;
}

inline uint64_t getRIP() {
    uint64_t rip;
    asm volatile("mov %%rip, %0"
                 : "=r"(rip));
    return rip;
}

struct dtr_t {
    uint64_t size;
    uint64_t base;
};

struct RegisterFile {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rsp, rip, rflags;
    uint64_t cs, ss, ds, es, fs, gs;
    uint64_t fs_base, gs_base;
    uint64_t cr0, cr2, cr3, cr4;
    uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
    dtr_t gdtr, idtr;
};

inline void getIDTR(uint64_t& address, uint64_t& size) {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr;
    asm volatile("sidt %0"
                 : "=m"(idtr));
    address = idtr.base;
    size = idtr.limit + 1;
}

inline void getGDTR(uint64_t& address, uint64_t& size) {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr;
    asm volatile("sgdt %0"
                 : "=m"(gdtr));
    address = gdtr.base;
    size = gdtr.limit + 1;
}

inline dtr_t getGDTR() {
    dtr_t gdtr{};
    getGDTR(gdtr.base, gdtr.size);
    return gdtr;
}

inline dtr_t getIDTR() {
    dtr_t idtr{};
    getIDTR(idtr.base, idtr.size);
    return idtr;
}

inline void setGDTR(uint64_t address, uint64_t size) {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr;
    static_assert(sizeof(gdtr) == 10);
    gdtr.limit = size - 1;
    gdtr.base = address;
    asm volatile("lgdt %0"
                 :
                 : "m"(gdtr));
}

inline void setIDTR(uint64_t address, uint64_t size) {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr;
    static_assert(sizeof(idtr) == 10);
    idtr.limit = size - 1;
    idtr.base = address;
    asm volatile("lidt %0"
                 :
                 : "m"(idtr)
                 : "memory");
    auto check = getIDTR();
    if (check.base != address || check.size != size) {
        panic("Failed to set IDTR");
    }
}

inline void setGDTR(dtr_t gdtr) {
    setGDTR(gdtr.base, gdtr.size);
}

inline void setIDTR(dtr_t idtr) {
    setIDTR(idtr.base, idtr.size);
}

inline RegisterFile get_registers() {
    // clang-format off
    return {
            getRAX(),getRBX(),getRCX(),getRDX(),getRSI(),getRDI(),getRBP(),
                getR8(),getR9(),getR10(),getR11(),getR12(),getR13(),getR14(),getR15(),
            getRSP(),getRIP(),getFlags(),
            getCS(),getSS(),getDS(),getES(),getFS(),getGS(),
            getFSBase(),getGSBase(),
            getCR0(),getCR2(),getCR3(),getCR4(),
            getDR0(),getDR1(),getDR2(),getDR3(),getDR6(),getDR7(),
            getGDTR(),getIDTR()
    };
}