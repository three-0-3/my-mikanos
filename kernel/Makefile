TARGET = kernel.elf
OBJS = main.o graphics.o font.o hankaku.o newlib_support.o console.o pci.o asmfunc.o

CFLAGS   += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone \
            -fno-exceptions -fno-rtti -std=c++17
LDFLAGS += --entry KernelMain -z norelro --image-base 0x100000 --static

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o

kernel.elf: $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o kernel.elf $(OBJS) -lc

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $<

%.o: %.c Makefile
	clang $(CPPFLAGS) $(CFLAGS) -c $<

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<

# Convert font data by '.' and '@' to binary
hankaku.bin: hankaku.txt
	../tools/makefont.py -o $@ $<

# Add elf meta data to the flat binary of font data
hankaku.o: hankaku.bin
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@
