#
# File:
#    Makefile
#
# Description:
#    Makefile for the VTP Firmware updating program
#
#

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
	@rm -vf $(PROGS) $(DEPS) *~

%: %.c ${STATIC_LIBS}
	@echo "Building $@ from $<"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) ${STATIC_LIBS} -lm -lrt -lpthread

%.d: %.c
	@echo ""
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: all clean
