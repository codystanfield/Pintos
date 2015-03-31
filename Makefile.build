# -*- makefile -*-

# WARNING: Don't edit me!  I am automatically generated. 

SRCDIR = ../..

all: kernel.bin loader.bin

include ../../Make.config
include ../Make.vars
include ../../tests/Make.tests

# Compiler and assembler options.
kernel.bin: CPPFLAGS += -I$(SRCDIR)/lib/kernel

# Core kernel.
kernel_SRC  = kernel/start.S		# Startup code.
kernel_SRC += kernel/init.c		# Main program.
kernel_SRC += kernel/thread.c		# Thread management core.
kernel_SRC += kernel/switch.S		# Thread switch routine.
kernel_SRC += kernel/interrupt.c	# Interrupt core.
kernel_SRC += kernel/intr-stubs.S	# Interrupt stubs.
kernel_SRC += kernel/synch.c		# Synchronization.
kernel_SRC += kernel/palloc.c		# Page allocator.
kernel_SRC += kernel/malloc.c		# Subpage allocator.

# Device driver code.
devices_SRC  = devices/pit.c		# Programmable interrupt timer chip.
devices_SRC += devices/timer.c		# Periodic timer device.
devices_SRC += devices/kbd.c		# Keyboard device.
devices_SRC += devices/vga.c		# Video device.
devices_SRC += devices/serial.c		# Serial port device.
devices_SRC += devices/block.c		# Block device abstraction layer.
devices_SRC += devices/partition.c	# Partition block device.
devices_SRC += devices/ide.c		# IDE disk block device.
devices_SRC += devices/input.c		# Serial and keyboard input.
devices_SRC += devices/intq.c		# Interrupt queue.
devices_SRC += devices/rtc.c		# Real-time clock.
devices_SRC += devices/shutdown.c	# Reboot and power off.
devices_SRC += devices/speaker.c	# PC speaker.

# Library code shared between kernel and user programs.
lib_SRC  = lib/debug.c			# Debug helpers.
lib_SRC += lib/random.c			# Pseudo-random numbers.
lib_SRC += lib/stdio.c			# I/O library.
lib_SRC += lib/stdlib.c			# Utility functions.
lib_SRC += lib/string.c			# String functions.
lib_SRC += lib/arithmetic.c		# 64-bit arithmetic for GCC.
lib_SRC += lib/ustar.c			# Unix standard tar format utilities.

# Kernel-specific library code.
lib/kernel_SRC  = lib/kernel/debug.c	# Debug helpers.
lib/kernel_SRC += lib/kernel/list.c	# Doubly-linked lists.
lib/kernel_SRC += lib/kernel/bitmap.c	# Bitmaps.
lib/kernel_SRC += lib/kernel/hash.c	# Hash tables.
lib/kernel_SRC += lib/kernel/console.c	# printf(), putchar().

# User process code.
kernel_SRC += kernel/process.c		# Process loading.
kernel_SRC += kernel/pagedir.c		# Page directories.
kernel_SRC += kernel/exception.c	# User exception handler.
kernel_SRC += kernel/syscall.c		# System call handler.
kernel_SRC += kernel/gdt.c		# GDT initialization.
kernel_SRC += kernel/tss.c		# TSS management.

# No virtual memory code yet.
#vm_SRC = vm/file.c			# Some file.

# Filesystem code.
filesys_SRC  = filesys/filesys.c	# Filesystem core.
filesys_SRC += filesys/free-map.c	# Free sector bitmap.
filesys_SRC += filesys/file.c		# Files.
filesys_SRC += filesys/directory.c	# Directories.
filesys_SRC += filesys/inode.c		# File headers.
filesys_SRC += filesys/fsutil.c		# Utilities.

SOURCES = $(foreach dir,$(KERNEL_SUBDIRS),$($(dir)_SRC))
OBJECTS = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(SOURCES)))
DEPENDS = $(patsubst %.o,%.d,$(OBJECTS))

kernel/kernel.lds.s: CPPFLAGS += -P
kernel/kernel.lds.s: kernel/kernel.lds.S kernel/loader.h

kernel.o: kernel/kernel.lds.s $(OBJECTS) 
	$(LD) -T $< -o $@ $(OBJECTS)

kernel.bin: kernel.o
	$(OBJCOPY) -R .note -R .comment -S $< $@

kernel/loader.o: kernel/loader.S
	$(CC) -c $< -o $@ $(ASFLAGS) $(CPPFLAGS) $(DEFINES)

loader.bin: kernel/loader.o
	$(LD) -N -e 0 -Ttext 0x7c00 --oformat binary -o $@ $<

os.dsk: kernel.bin
	cat $^ > $@

clean::
	rm -f $(OBJECTS) $(DEPENDS) 
	rm -f kernel/loader.o kernel/kernel.lds.s kernel/loader.d
	rm -f kernel.bin.tmp
	rm -f kernel.o kernel.lds.s
	rm -f kernel.bin loader.bin
	rm -f bochsout.txt bochsrc.txt
	rm -f results grade

Makefile: $(SRCDIR)/Makefile.build
	cp $< $@

-include $(DEPENDS)
