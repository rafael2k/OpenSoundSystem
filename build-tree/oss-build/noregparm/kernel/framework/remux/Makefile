# Makefile for Open Sound System module remux

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
OBJECTS=$(OBJDIR)/oss_remux.o
TARGETS=remux
DEPDIR=
CSOURCES=oss_remux.c

all: $(AUTOGEN_SOURCES) objects 
lint: lint_sources 

dep: $(AUTOGEN_SOURCES) dep_local 

include $(TOPDIR)/make.defs

$(OBJDIR)/oss_remux.o:	oss_remux.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) oss_remux.c -o $(OBJDIR)/oss_remux.o $(LIBRARIES)

clean: clean_local

