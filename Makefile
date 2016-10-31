#
# File:
#    Makefile
#
# Description:
#    Makefile for the JLab VTP module running Linux on an ARMv7 processor
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG   ?= 1
QUIET	?= 1
#
#
BASENAME=vtp
ARCH=${shell uname -m}
KERNEL_VERSION=${shell uname -r}

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -L. 
INCS			= -I. -I/usr/local/include

LIBS			= lib${BASENAME}.a

ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif
SRC			= vtpLib.c vtp-i2c.c vtp-spi.c
HDRS			= $(SRC:.c=.h)
OBJ			= $(SRC:.c=.o)
DEPS			= $(SRC:.c=.d)

ifeq ($(QUIET),1)
	Q = @
else
	Q =
endif


all: echoarch $(LIBS)

%.o: %.c
	@echo " CC     $@"
	$(Q)$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

$(LIBS): $(OBJ)
	@echo " CC     $(@:%.a=%.so)"
	$(Q)$(CC) -fpic -shared $(CFLAGS) $(INCS) -o $(@:%.a=%.so) $(SRC)
	@echo " AR     $(@)"
	$(Q)$(AR) r $@ $<
	@echo " RANLIB $(@)"
	$(Q)$(RANLIB) $@

%.d: %.c
	@echo " DEP    $@"
	$(Q)set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

clean:
	$(Q)rm -vf ${BASENAME}Lib.{o,d,d.*} lib${BASENAME}.{a,so}

realclean: clean
	$(Q)rm -vf *~

echoarch:
	@echo "Make for $(ARCH)"

.PHONY: clean echoarch
