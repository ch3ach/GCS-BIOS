##
## This file is part of the GCS-BIOS project.
##
## Copyright (C) 2013
##

BINARY          = cdcacm

LDSCRIPT        = ../stm32f4-discovery.ld

PREFIX	       ?= arm-none-eabi
# PREFIX         ?= arm-elf
CC              = $(PREFIX)-gcc
LD              = $(PREFIX)-gcc
OBJCOPY         = $(PREFIX)-objcopy
OBJDUMP         = $(PREFIX)-objdump
GDB             = $(PREFIX)-gdb
FLASH           = $(shell which st-flash)
UTIL            = $(shell which st-util)
SUDO            = $(shell which sudo)

TOOLCHAIN_DIR   ?= /usr/arm-none-eabi
LIBOPENCM3_DIR  ?= ../libopencm3

CFLAGS		+= -Os -g -Wall -Wextra -I$(LIBOPENCM3_DIR)/include \
		   -fno-common -mcpu=cortex-m4 -mthumb \
		   -mfloat-abi=hard -mfpu=fpv4-sp-d16 -MD -DSTM32F4
LDSCRIPT	?= $(BINARY).ld
LDFLAGS		+= --static -lc -lnosys -L$(LIBOPENCM3_DIR)/lib -lopencm3_stm32f4 \
			 -L$(LIBOPENCM3_DIR)/lib/stm32/f4 \
		   -T$(LDSCRIPT) -nostartfiles -Wl,--gc-sections \
		   -mthumb -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
OBJS		+= cdcacm.o SCSIUnknowns.o usbmanager.o usbstorage.o ramdisk.o main.o

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
NULL := 2>/dev/null
else
LDFLAGS += -Wl,--print-gc-sections
endif

.SUFFIXES: .elf .bin .hex .srec .list .images
.SECONDEXPANSION:
.SECONDARY:

all: images

images: $(BINARY).images
flash: $(BINARY).stlink-flash
debug: $(BINARY).stlink-debug

%.images: %.bin %.hex %.srec %.symb %.list
	@echo "*** $* images generated ***"

%.bin: %.elf
	@printf "  OBJCOPY $(*).bin\n"
	$(Q)$(OBJCOPY) -Obinary $(*).elf $(*).bin

%.hex: %.elf
	@printf "  OBJCOPY $(*).hex\n"
	$(Q)$(OBJCOPY) -Oihex $(*).elf $(*).hex

%.srec: %.elf
	@printf "  OBJCOPY $(*).srec\n"
	$(Q)$(OBJCOPY) -Osrec $(*).elf $(*).srec

%.symb: %.elf
	@printf "  OBJDUMP $(*).symb\n"
	$(Q)$(OBJDUMP) -t $(*).elf > $(*).symb

%.list: %.elf
	@printf "  OBJDUMP $(*).list\n"
	$(Q)$(OBJDUMP) -S $(*).elf > $(*).list

%.elf: $(OBJS) $(LDSCRIPT) $(LIBOPENCM3_DIR)/lib/libopencm3_stm32f4.a
	@printf "  LD      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(LD) -o $(*).elf $(OBJS) $(LDFLAGS)

%.o: %.c
	@printf "  CC      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(Q)rm -f *.o
	$(Q)rm -f *.d
	$(Q)rm -f *.elf
	$(Q)rm -f *.bin
	$(Q)rm -f *.hex
	$(Q)rm -f *.srec
	$(Q)rm -f *.list
	
%.stlink-flash: %.bin
	@printf "  FLASH   $<\n"
	$(Q)$(SUDO) $(FLASH) write $(*).bin 0x8000000
	
%.stlink-debug: %.elf
	@printf "  DEBUG   $<\n"
	$(Q)$(SUDO) $(UTIL) $(*).elf
	
	
