# Makefile for Open Sound System module ossxmix

CC=cc
HOSTCC=cc
CPLUSPLUS=g++ -fno-rtti -fno-exceptions -I.
LDFLAGS=`pkg-config gtk+-2.0 --libs`
OSFLAGS=-Wall -DOSS_LITTLE_ENDIAN
OS=Linux
ARCH=x86_64
TOPDIR=../..
OBJDIR=$(TOPDIR)/target/objects
TMPDIR=.
MODDIR=$(TOPDIR)/target/modules
BINDIR=$(TOPDIR)/target/bin
LIBDIR=$(TOPDIR)/target/lib
SBINDIR=$(TOPDIR)/target/sbin
OSSLIBDIR="/usr/lib/oss"
THISOS=kernel/OS/Linux
CFLAGS+=-O
CFLAGS += `pkg-config gtk+-2.0 --cflags`
OBJECTS=$(TMPDIR)/gtkvu.o $(TMPDIR)/ossxmix.o
TARGETS=$(BINDIR)/ossxmix
DEPDIR=$(BINDIR)/
CSOURCES=gtkvu.c ossxmix.c

all: $(TARGETS) 
lint: lint_sources 

dep: $(AUTOGEN_SOURCES) dep_local 

include $(TOPDIR)/make.defs

ossxmix:	$(BINDIR)/ossxmix

$(BINDIR)/ossxmix:	$(OBJECTS)
	$(CC) $(CFLAGS) -s -o $(BINDIR)/ossxmix $(OBJECTS) $(LIBRARIES) $(LDFLAGS) 


$(TMPDIR)/gtkvu.o:	gtkvu.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) gtkvu.c -o $(TMPDIR)/gtkvu.o $(LIBRARIES)

$(TMPDIR)/ossxmix.o:	ossxmix.c
	$(CC) -c $(CFLAGS) $(OSFLAGS) $(INCLUDES) ossxmix.c -o $(TMPDIR)/ossxmix.o $(LIBRARIES)

clean: clean_local

