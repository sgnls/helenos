#
# Copyright (C) 2005 Martin Decky
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# - The name of the author may not be used to endorse or promote products
#   derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

## Kernel release
#

VERSION = 0
PATCHLEVEL = 1
SUBLEVEL = 0
EXTRAVERSION = 
NAME = Dawn
RELEASE = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

## Include configuration
#

-include Makefile.config

## Common compiler flags
#

DEFS = -D$(ARCH) -DARCH=\"$(ARCH)\" -DRELEASE=\"$(RELEASE)\" "-DNAME=\"$(NAME)\"" -DKERNEL
CFLAGS = -fno-builtin -fomit-frame-pointer -Wall -Werror-implicit-function-declaration -Wmissing-prototypes -Werror -O3 -nostdlib -nostdinc -Igeneric/include/ 
LFLAGS = -M
AFLAGS =

ifdef REVISION
	DEFS += "-DREVISION=\"$(REVISION)\""
endif

ifdef TIMESTAMP
	DEFS += "-DTIMESTAMP=\"$(TIMESTAMP)\""
endif

## Setup kernel configuration
#

-include arch/$(ARCH)/Makefile.inc
-include genarch/Makefile.inc

ifeq ($(CONFIG_DEBUG),y)
	DEFS += -DCONFIG_DEBUG
endif
ifeq ($(CONFIG_DEBUG_SPINLOCK),y)
	DEFS += -DCONFIG_DEBUG_SPINLOCK
endif
ifeq ($(CONFIG_DEBUG_AS_WATCHPOINT),y)
	DEFS += -DCONFIG_DEBUG_AS_WATCHPOINT
endif
ifeq ($(CONFIG_FPU_LAZY),y)
	DEFS += -DCONFIG_FPU_LAZY
endif
ifeq ($(CONFIG_DEBUG_ALLREGS),y)
	DEFS += -DCONFIG_DEBUG_ALLREGS
endif
ifeq ($(CONFIG_VHPT),y)
	DEFS += -DCONFIG_VHPT
endif
ifeq ($(CONFIG_FB),y)
	DEFS += -DCONFIG_VESA_WIDTH=$(CONFIG_VESA_WIDTH)
	DEFS += -DCONFIG_VESA_HEIGHT=$(CONFIG_VESA_HEIGHT)
	DEFS += -DCONFIG_VESA_BPP=$(CONFIG_VESA_BPP)
endif

## Toolchain configuration
#

ifeq ($(COMPILER),native)
	CC = gcc
	AS = as
	LD = ld
	OBJCOPY = objcopy
	OBJDUMP = objdump
else
	CC = $(TOOLCHAIN_DIR)/$(TARGET)-gcc
	AS = $(TOOLCHAIN_DIR)/$(TARGET)-as
	LD = $(TOOLCHAIN_DIR)/$(TARGET)-ld
	OBJCOPY = $(TOOLCHAIN_DIR)/$(TARGET)-objcopy
	OBJDUMP = $(TOOLCHAIN_DIR)/$(TARGET)-objdump
endif

## Generic kernel sources
#

GENERIC_SOURCES = \
	generic/src/adt/bitmap.c \
	generic/src/adt/btree.c \
	generic/src/adt/hash_table.c \
	generic/src/adt/list.c \
	generic/src/console/chardev.c \
	generic/src/console/console.c \
	generic/src/console/kconsole.c \
	generic/src/console/cmd.c \
	generic/src/cpu/cpu.c \
	generic/src/ddi/ddi.c \
	generic/src/interrupt/interrupt.c \
	generic/src/main/main.c \
	generic/src/main/kinit.c \
	generic/src/main/uinit.c \
	generic/src/main/version.c \
	generic/src/proc/scheduler.c \
	generic/src/proc/thread.c \
	generic/src/proc/task.c \
	generic/src/proc/the.c \
	generic/src/syscall/syscall.c \
	generic/src/syscall/copy.c \
	generic/src/mm/buddy.c \
	generic/src/mm/frame.c \
	generic/src/mm/page.c \
	generic/src/mm/tlb.c \
	generic/src/mm/as.c \
	generic/src/mm/slab.c \
	generic/src/lib/func.c \
	generic/src/lib/memstr.c \
	generic/src/lib/sort.c \
	generic/src/lib/elf.c \
	generic/src/printf/printf_core.c \
	generic/src/printf/printf.c \
	generic/src/printf/sprintf.c \
	generic/src/printf/snprintf.c \
	generic/src/printf/vprintf.c \
	generic/src/printf/vsprintf.c \
	generic/src/printf/vsnprintf.c \
	generic/src/debug/symtab.c \
	generic/src/time/clock.c \
	generic/src/time/timeout.c \
	generic/src/time/delay.c \
	generic/src/preempt/preemption.c \
	generic/src/synch/spinlock.c \
	generic/src/synch/condvar.c \
	generic/src/synch/rwlock.c \
	generic/src/synch/mutex.c \
	generic/src/synch/semaphore.c \
	generic/src/synch/waitq.c \
	generic/src/synch/futex.c \
	generic/src/smp/ipi.c \
	generic/src/ipc/ipc.c \
	generic/src/ipc/sysipc.c \
	generic/src/ipc/ipcrsc.c \
	generic/src/ipc/irq.c \
	generic/src/security/cap.c \
	generic/src/sysinfo/sysinfo.c

## Test sources
#

ifneq ($(CONFIG_TEST),)
	DEFS += -DCONFIG_TEST
	GENERIC_SOURCES += test/$(CONFIG_TEST)/test.c
endif

GENERIC_OBJECTS := $(addsuffix .o,$(basename $(GENERIC_SOURCES)))
ARCH_OBJECTS := $(addsuffix .o,$(basename $(ARCH_SOURCES)))
GENARCH_OBJECTS := $(addsuffix .o,$(basename $(GENARCH_SOURCES)))

.PHONY: all build config distclean clean archlinks depend disasm

all:
	tools/config.py default $(NARCH)
ifdef NARCH
 ifneq ($(ARCH), $(NARCH))
	$(MAKE) -C . clean
 endif
endif
	$(MAKE) -C . build

build: kernel.bin disasm

config:
	-rm Makefile.depend
	tools/config.py

-include Makefile.depend

distclean: clean
	-rm Makefile.config

clean:
	-rm -f kernel.bin kernel.raw kernel.map kernel.map.pre kernel.objdump kernel.disasm generic/src/debug/real_map.bin Makefile.depend* generic/include/arch generic/include/genarch arch/$(ARCH)/_link.ld
	find generic/src/ arch/*/src/ genarch/src/ test/ -name '*.o' -follow -exec rm \{\} \;
	for arch in arch/*; do \
	    [ -e $$arch/_link.ld ] && rm $$arch/_link.ld 2>/dev/null;\
	done;exit 0

archlinks:
	ln -sfn ../../arch/$(ARCH)/include/ generic/include/arch
	ln -sfn ../../genarch/include/ generic/include/genarch

depend: archlinks
	-makedepend $(DEFS) $(CFLAGS) -f - $(ARCH_SOURCES) $(GENARCH_SOURCES) $(GENERIC_SOURCES) > Makefile.depend 2> /dev/null

arch/$(ARCH)/_link.ld: arch/$(ARCH)/_link.ld.in
	$(CC) $(DEFS) $(CFLAGS) -D__ASM__ -E -x c $< | grep -v "^\#" > $@

generic/src/debug/real_map.bin: depend arch/$(ARCH)/_link.ld $(ARCH_OBJECTS) $(GENARCH_OBJECTS) $(GENERIC_OBJECTS)
	$(OBJCOPY) -I binary -O $(BFD_NAME) -B $(BFD_ARCH) --prefix-sections=symtab Makefile generic/src/debug/empty_map.o
	$(LD) -T arch/$(ARCH)/_link.ld $(LFLAGS) $(ARCH_OBJECTS) $(GENARCH_OBJECTS) $(GENERIC_OBJECTS) generic/src/debug/empty_map.o -o $@ -Map kernel.map.pre
	$(OBJDUMP) -t $(ARCH_OBJECTS) $(GENARCH_OBJECTS) $(GENERIC_OBJECTS) > kernel.objdump
	tools/genmap.py kernel.map.pre kernel.objdump generic/src/debug/real_map.bin 
	# Do it once again, this time to get correct even the symbols
	# on architectures, that have bss after symtab
	$(OBJCOPY) -I binary -O $(BFD_NAME) -B $(BFD_ARCH) --prefix-sections=symtab generic/src/debug/real_map.bin generic/src/debug/sizeok_map.o
	$(LD) -T arch/$(ARCH)/_link.ld $(LFLAGS) $(ARCH_OBJECTS) $(GENARCH_OBJECTS) $(GENERIC_OBJECTS) generic/src/debug/sizeok_map.o -o $@ -Map kernel.map.pre
	$(OBJDUMP) -t $(ARCH_OBJECTS) $(GENARCH_OBJECTS) $(GENERIC_OBJECTS) > kernel.objdump
	tools/genmap.py kernel.map.pre kernel.objdump generic/src/debug/real_map.bin 

generic/src/debug/real_map.o: generic/src/debug/real_map.bin
	$(OBJCOPY) -I binary -O $(BFD_NAME) -B $(BFD_ARCH) --prefix-sections=symtab $< $@

kernel.raw: depend arch/$(ARCH)/_link.ld $(ARCH_OBJECTS) $(GENARCH_OBJECTS) $(GENERIC_OBJECTS) generic/src/debug/real_map.o
	$(LD) -T arch/$(ARCH)/_link.ld $(LFLAGS) $(ARCH_OBJECTS) $(GENARCH_OBJECTS) $(GENERIC_OBJECTS) generic/src/debug/real_map.o -o $@ -Map kernel.map

kernel.bin: kernel.raw
	$(OBJCOPY) -O $(BFD) kernel.raw kernel.bin

disasm: kernel.raw
	$(OBJDUMP) -d kernel.raw > kernel.disasm

%.o: %.S
	$(CC) $(DEFS) $(CFLAGS) -D__ASM__ -c $< -o $@

%.o: %.s
	$(AS) $(AFLAGS) $< -o $@

%.o: %.c
	$(CC) $(DEFS) $(CFLAGS) -c $< -o $@
