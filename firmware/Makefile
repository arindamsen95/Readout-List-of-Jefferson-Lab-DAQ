#
# File:
#    Makefile
#
# Description:
#    Makefile for the VTP Firmware updating program
#
#

# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG   ?= 1
QUIET	?= 1

ifeq ($(QUIET),1)
	Q = @
	INSTALL = @install -b
else
	Q =
	INSTALL = install -b --verbose
endif


VTPLIB_PATH		= ../

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
INCS			= -I. -I${VTPLIB_PATH}
CFLAGS			= -Wall -g ${INCS}

STATIC_LIBS		= ../libvtp.a
STATIC_LIBS		+= /usr/local/lib/libi2c.a

PROGS			= vtpFirmwareLoad
SRC			= $(PROGS:%=%.c)
DEPS			= $(SRC:%.c=%.d)

all: $(PROGS)

clean:
	$(Q)rm -vf $(PROGS) $(DEPS) *~

%: %.c ${STATIC_LIBS}
	@echo " CC     $@"
	$(Q)$(CC) $(CFLAGS) -o $@ $(@:%=%.c) ${STATIC_LIBS} -lm -lrt -lpthread

%.d: %.c
	@echo " DEP    $@"
	$(Q)set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

install: ${PROGS}
	@echo " INST   $<"
	-${INSTALL} --mode=755 --directory /usr/local/vtp/
	-${INSTALL} --mode=755 ${PROGS} /usr/local/vtp/
	-${INSTALL} --mode=755 --directory /usr/local/vtp/systemd
	-${INSTALL} --mode=644 ../systemd/vtp_fpga_load.service /usr/local/vtp/systemd/

-include $(DEPS)

.PHONY: all clean
