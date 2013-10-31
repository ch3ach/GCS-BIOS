##
## This file is part of the GCS-BIOS project.
##
## Copyright (C) 2013
##

LIBNAME         = libdevices.a

PREFIX	       ?= arm-none-eabi

CC              = $(PREFIX)-gcc
AR              = $(PREFIX)-ar

TOOLCHAIN_DIR   ?= /usr/arm-none-eabi
LIBOPENCM3_DIR  ?= ../libopencm3

CFLAGS		+= -Os -g -Wall -Wextra -I$(LIBOPENCM3_DIR)/include \
		   -fno-common -mcpu=cortex-m4 -mthumb \
		   -mfloat-abi=hard -mfpu=fpv4-sp-d16 -MD -DSTM32F4
OBJS		+= signal_routing.o dac_ltc2632.o

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
NULL := 2>/dev/null
endif

$(LIBNAME): $(OBJS)
	@printf "  AR      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(AR) rcs $(LIBNAME) $(OBJS)

%.o: %.c
	@printf "  CC      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(Q)rm -f *.o
	$(Q)rm $(LIBNAME)
