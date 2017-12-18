#
# File:
#    Makefile
#
# Description:
#    Makefile for the VTP test programs
#
#
# $Date$
# $Rev$
#

VTPLIB_PATH		= ../
ARCH			= ${shell uname -m}
KERNEL_VERSION		= ${shell uname -r}

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
INCS			= -I. -I${VTPLIB_PATH} -I/usr/local/include
CFLAGS			= -Wall -g ${INCS} \
			   -L/usr/local/lib -L. -L${VTPLIB_PATH} -lrt -lvtp -li2c -lm

PROGS			= vtpLibTest i2cvtpmon vtpSPItest vtpI2Ctest vtpDmaTest i2cvtpsetup vtpConfigTest vtpStatus
SRC			= $(PROGS:%=%.c)
DEPS			= $(SRC:%.c=%.d)

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~ 

%: %.c
	@echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c)

%.d: %.c
	@echo ""
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: all clean distclean
