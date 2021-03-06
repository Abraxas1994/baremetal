vpath %.c . ../common

PREFIX?=/usr/bin
GCC=$(firstword $(wildcard $(PREFIX)/*powerpc*-*-gcc))
OBJCOPY=$(firstword $(wildcard $(PREFIX)/*powerpc*-*-objcopy))

ifeq ($(GCC),)
$(error Failed to find gcc)
endif

# use dwarf-2 as gdb 4.8 doesn't understand dwarf 4.
# Replace w/ '-g'
DEBUG=-gdwarf-2
#DEBUG=-g -O2

# PREP is -mcpu=602
CFLAGS=-mcpu=602 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs $(DEBUG) -Wall -Wextra -I. -I../common
LDFLAGS=-static

all: os.bin bios.bin

os.elf: os.ld init.o main.o common.o
bios.elf: bios.ld bios.o

clean:
	rm -f *.o
	rm -f os.elf os.bin os.map
	rm -f bios.elf bios.bin bios.map

%.o: %.c
	$(GCC) -c $< -o $@ $(CFLAGS)

%.o: %.S
	$(GCC) -c $< -o $@ $(CFLAGS)

%.elf:
	$(GCC) -T$*.ld -Wl,-Map=$*.map -o $@ $(CFLAGS) $(LDFLAGS) $(filter %.o,$^) -lgcc

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
