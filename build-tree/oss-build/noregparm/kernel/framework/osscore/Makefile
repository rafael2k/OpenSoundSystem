# Makefile for Open Sound System module osscore

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
OBJECTS=$(OBJDIR)/oss_core_options.o $(OBJDIR)/oss_core_services.o $(OBJDIR)/oss_memblk.o
TARGETS=osscore
DEPDIR=
CSOURCES=oss_core_options.c oss_core_services.c oss_memblk.c

all: $(AUTOGEN_SOURCES) objects 
lint: lint_sources 

dep: $(AUTOGEN_SOURCES) dep_local 

include $(TOPDIR)/make.defs

$(OBJDIR)/oss_core_options.o:	oss_core_options.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) oss_core_options.c -o $(OBJDIR)/oss_core_options.o $(LIBRARIES)

$(OBJDIR)/oss_core_services.o:	oss_core_services.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) oss_core_services.c -o $(OBJDIR)/oss_core_services.o $(LIBRARIES)

$(OBJDIR)/oss_memblk.o:	oss_memblk.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) oss_memblk.c -o $(OBJDIR)/oss_memblk.o $(LIBRARIES)

clean: clean_local

