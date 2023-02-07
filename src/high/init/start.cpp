#include "ACPI/APIC.h"
#include "ACPI/RSDP.h"
#include "PCI/PCI.h"
#include "PCI/sata.h"
#include "PCI/xhci.h"
#include "asm/io.h"
#include "asm/regs.h"
#include "asm/util.h"
#include "data/btree.h"
#include "data/linked_list.h"
#include "features/bytes.h"
#include "features/optional.h"
#include "features/types.h"
#include "file/GPT.h"
#include "file/device.h"
#include "file/file.h"
#include "file/filesystem.h"
#include "file/partition.h"
#include "int.h"
#include "interrupt/default_handler.h"
#include "interrupt/interrupt.h"
#include "memory/heap.h"
#include "memory/mem.h"
#include "memory/paging.h"
#include "out/log.h"
#include "out/panic.h"
#include "out/vga.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "test/test.h"

alignas(page_size) char start_stack[0x100000];

uint64_t add(uint64_t a, uint64_t b) {
    return a + b;
}

void print(uint64_t number) {
    Log::info("syscall", "%d\n", number);

}

void print(char* str) {
    Log::info("syscall", "%s\n", str);
}

struct Main {
    PhysicalAddress multiboot_header;
    optional<APIC::MADT> madt;
    optional<PCI::MCFG> mcfg;
    linked_list<PCI::generic_device> pci_devices;
    linked_list<file::filesystem> filesystems;
    shared_ptr<proc::process> kernel_process;

    void init(PhysicalAddress header) {
        multiboot_header = header;
        PhysicalAllocator::init(multiboot_header);
        PageTable::init();
        VGA::Text::init();
        VGA::Text::clear();
        Log::set_level(Log::Info);
        Interrupt::init();
        Interrupt::init_default_handlers();
        kheap::init();
        initACPI();
        initAPIC();
        initPCI();
        PageTable::clear_startup();
        initFiles();
        initProcess();
    }
    void loop() {
    }
    void initACPI() {
        auto rsdt = ACPI::getRoot(multiboot_header);
        if (!rsdt) {
            panic("ACPI: RSDT not found");
        }
        madt = APIC::MADT(rsdt->getSDP("APIC").value_or_panic("ACPI not found"));
        mcfg = PCI::make_mcfg(rsdt->getSDP("MCFG").value_or_panic("PCI not found"));

        assert(madt, "APIC not found");
        assert(mcfg, "PCI not found");
    }
    void initPCI() {
        // Log::LevelGuard guard{Log::Debug};
        assert(mcfg, "PCI not found");

        size_t pci_group_count = mcfg->getEntryCount();
        Log::printf(Log::Info, "Main", "Found %i PCI group(s)\n", pci_group_count);
        for (size_t i = 0; i < pci_group_count; i++) {
            mcfg->getEntry(i).check_all(pci_devices);
        }
        Log::printf(Log::Info, "Main", "Found %i PCI device(s)\n", pci_devices.size);
        for (auto& device : pci_devices) {
            Log::printf(Log::Info, "Main", "PCI device: %s\n", device.human_readable_class());
            // initialisation of the devices is done in the constructor
            // Log::LevelGuard guard{Log::Debug};
            PCI::generate<PCI::Sata, PCI::xhci>(device);
        }
    }
    void initAPIC() {
        //Log::LevelGuard guard{Log::Debug};
        assert(madt, "ACPI not found");
        madt->init();
    }
    void initFiles() {
        Log::LevelGuard guard{Log::Debug};
        for (size_t i = 0; i < file::device_count(); ++i) {
            auto& device = file::get_device(i);
            auto gpt_opt = device.find_partitions();
            if (!gpt_opt) {
                Log::printf(Log::Debug, "Main", "Device %i has no GPT\n", i);
                continue;
            }
            auto& gpt = gpt_opt.value();
            Log::printf(Log::Debug, "Main", "Device %i has up to %i partitions\n", i, gpt.count);
            for (size_t j = 0; j < gpt.count; ++j) {
                auto partition = *gpt.getPartition(j);
                if (partition.exists()) {
                    Log::printf(Log::Debug, "Main", "partition %i: %s - %s\n", j, partition.name, partition.type_name());
                    auto fs = partition.create_filesystem();
                    if (!fs.valid()) continue;
                    filesystems.push_back(fs);
                }
            }
        }
    }
    void initProcess() {
        kernel_process = new proc::process();
        kernel_process->parent = kernel_process;
        kernel_process->self = kernel_process;
        kernel_process->name = "root";
        kernel_process->main_thread.owner = kernel_process;
        kernel_process->add_kernel_method("add", VirtualAddress(add), proc::process::method_descriptor::uint64, proc::process::method_descriptor::uint64);
        kernel_process->add_kernel_method("printStr", VirtualAddress(static_cast<void(*)(char*)>(print)), proc::process::method_descriptor::c_string);
        kernel_process->add_kernel_method("printInt", VirtualAddress(static_cast<void(*)(uint64_t)>(print)), proc::process::method_descriptor::uint64);
        kernel_process->add_kernel_method("panic", VirtualAddress(panic), proc::process::method_descriptor::c_string);

        for(auto& fs : filesystems) {
            auto file = fs.open("/main");
            auto process = proc::from_elf(file);
            kernel_process->children.push_back(process);
            process->parent = kernel_process;
            process->main_thread.execute();
        }
    }
};

static char buffer[sizeof(Main)];

extern "C" [[noreturn]] void high_main(PhysicalAddress multiboot_header) {
    memset(buffer, 0, sizeof(Main));
    auto main = new (buffer) Main();
    main->init(multiboot_header);
    Log::printf(Log::Info, "Main", "Everything initialized\n");
#ifdef TEST_CRACKOS3
    Test::run_test("kheap", kheap::test);
    Test::run_test("btree", btree<int>::test);
#endif
    main->loop();
    panic("exit from loop - sus?");
}