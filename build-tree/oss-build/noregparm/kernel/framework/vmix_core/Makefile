# Makefile for Open Sound System module vmix_core

CC=cc
HOSTCC=cc
CPLUSPLUS=g++ -fno-rtti -fno-exceptions -I.
OSFLAGS=-Wall -DOSS_LITTLE_ENDIAN
OS=Linux
ARCH=x86_64
TOPDIR=../../..
OBJDIR=$(TOPDIR)/target/objects
TMPDIR=.
MODDIR=$(TOPDIR)/target/modules
BINDIR=$(TOPDIR)/target/bin
LIBDIR=$(TOPDIR)/target/lib
SBINDIR=$(TOPDIR)/target/sbin
OSSLIBDIR="/usr/lib/oss"
THISOS=kernel/OS/Linux
CFLAGS += -D_KERNEL -fno-pic
CFLAGS += -O2 -fno-common -mcmodel=kernel -mno-red-zone -fno-tree-slp-vectorize -fno-pie -fno-asynchronous-unwind-tables -ffreestanding -fno-stack-protector
OBJECTS=$(OBJDIR)/vmix_core.o $(OBJDIR)/vmix_input.o $(OBJDIR)/vmix_output.o
TARGETS=vmix_core
DEPDIR=
CSOURCES=vmix_core.c vmix_input.c vmix_output.c

all: $(AUTOGEN_SOURCES) objects 
lint: lint_sources 

dep: $(AUTOGEN_SOURCES) dep_local 

include $(TOPDIR)/make.defs

$(OBJDIR)/vmix_core.o:	vmix_core.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) vmix_core.c -o $(OBJDIR)/vmix_core.o $(LIBRARIES)

$(OBJDIR)/vmix_input.o:	vmix_input.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) vmix_input.c -o $(OBJDIR)/vmix_input.o $(LIBRARIES)

$(OBJDIR)/vmix_output.o:	vmix_output.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) vmix_output.c -o $(OBJDIR)/vmix_output.o $(LIBRARIES)

clean: clean_local

