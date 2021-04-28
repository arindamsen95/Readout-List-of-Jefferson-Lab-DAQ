#
# File:
#    Makefile
#
# Description:
#    Makefile for DiagGui server running on VTP
#

USE_IPC  ?= 0
USE_CODA ?= 0

# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG   ?= 1
QUIET	?= 1
#
#
ARCH=${shell uname -m}
KERNEL_VERSION=${shell uname -r}

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -DLinux_$(ARCH) -L. \
				-lpthread -lrt -lm

STATIC_LIBS		= ../libvtp.a
STATIC_LIBS		+= /usr/local/lib/libi2c.a

INCS			= -I. -I../

ifdef DEBUG
	CFLAGS		+= -Wall -g
else
	CFLAGS		+= -O2
endif

SRC			= $(wildcard *.c)
HDRS			= $(wildcard *.h)
OBJ			= $(SRC:.c=.o)
DEPS			= $(SRC:.c=.d)

ifeq ($(QUIET),1)
	Q = @
else
	Q =
endif


all: DiagGuiServer

%.o: %.c
	@echo " CC     $@"
	$(Q)$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

%.d: %.c
	@echo " DEP    $@"
	$(Q)set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

DiagGuiServer: $(OBJ) ${STATIC_LIBS}
	@echo " CC     $@"
	$(Q)$(CC) $(CFLAGS) $(INCS) -o $@ ${OBJ} ${STATIC_LIBS}


-include $(DEPS)

clean:
	$(Q)rm -vf ${OBJ} ${DEPS} ${DEPS}.* DiagGuiServer

realclean: clean
	$(Q)rm -vf *~

install: DiagGuiServer
	@echo " INST   $<"
	-$(Q)cp DiagGuiServer $(HOME)/Linux-$(ARCH)/bin/

echoarch:
	@echo "Make for $(ARCH)"

.PHONY: clean echoarch
