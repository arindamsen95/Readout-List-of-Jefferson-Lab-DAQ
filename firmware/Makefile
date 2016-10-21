#
# File:
#    Makefile
#
# Description:
#    Makefile for the VTP Firmware updating program
#
#

FWFILES	= $(shell cat .currentfw)

VTPLIB_PATH		= ../

ifdef BUILD64
CC			= gcc
else
CC			= gcc -m32
endif
AR                      = ar
RANLIB                  = ranlib
INCS			= -I. -I${VTPLIB_PATH}
CFLAGS			= -Wall -g ${INCS}\
			  -L. -L${VTPLIB_PATH}

PROGS			= vtpFirmwareLoad
SRC			= $(PROGS:%=%.c)
DEPS			= $(SRC:%.c=%.d)

all: $(PROGS)

clean:
	@rm -vf $(PROGS) $(DEPS) *~

%: %.c
	@echo "Building $@ from $<"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) -lrt -lvtp

%.d: %.c
	@echo ""
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: all clean
