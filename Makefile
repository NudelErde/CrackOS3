cpp_files := $(shell find src -name *.cpp)
cpp_object_files := $(patsubst src/%.cpp, build/%.cpp.o, $(cpp_files))

object_files := $(cpp_object_files)

$(cpp_object_files): build/%.cpp.o : src/%.cpp
	mkdir -p $(dir $@) && \
	x86_64-elf-g++ -O0 -g -c -fPIC -I include -std=c++17 -fno-strict-aliasing -fno-asynchronous-unwind-tables -Wno-address-of-packed-member -Wno-multichar -Wno-literal-suffix -fno-exceptions -fno-rtti -fno-common -mno-red-zone -mgeneral-regs-only -ffreestanding $(patsubst build/%.cpp.o, src/%.cpp, $@) -o $@

.PHONY: build-x86_64
build-x86_64: $(object_files)
	mkdir -p dist && \
	x86_64-elf-ld --no-relax -n -o dist/kernel.bin -T targets/linker.ld $(object_files) && \
	objdump -dC dist/kernel.bin > dist/kernel.asm && \
	cp dist/kernel.bin targets/iso/boot/kernel.bin && \
	grub-mkrescue -d /usr/lib/grub/i386-pc -o dist/kernel.iso targets/iso

.PHONY: clean
clean:
	rm -rf build && \
	rm -rf dist

.PHONY: all
all: clean build-x86_64
