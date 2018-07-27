#
# File:
#    Makefile
#
# Description:
#    Makefile for the JLab VTP module running Linux on an ARMv7 processor
#
# CLAS stuff
#MAIN = vtp
#include $(CODA)/src/Makefile.include
#
#
#

USE_IPC  ?= 1
USE_CODA ?= 0

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
INCS			= -I. -I/usr/local/include -I$(CODA)/src/ipc/ipc.s -I$(CODA)/src/codautil/codautil.s

LIBS			= lib${BASENAME}.a

ifeq ($(USE_CODA),1)
LIBNAMES        = $(CODA)/src/codautil/Linux_armv7l/lib/libcodautil.a
endif
ifeq ($(USE_IPC),1)
LIBNAMES        = $(CODA)/src/ipc/Linux_armv7l/lib/libipc.a
endif

ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif
ifeq ($(USE_IPC),1)
CFLAGS			+= -DIPC
endif

SRC			= vtpLib.c vtpConfig.c vtp-i2c.c vtp-spi.c si5341_cfg.c
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
	$(Q)$(CC) -fpic -shared $(CFLAGS) $(LIBNAMES) $(INCS) -o $(@:%.a=%.so) $(SRC)
	@echo " AR     $(@)"
	$(Q)$(AR) r $@ $(OBJ)
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
	$(Q)rm -vf ${OBJ} ${LIBS} $(LIBS:.a=.so) ${DEPS} ${DEPS}.*

realclean: clean
	$(Q)rm -vf *~

install:
	-cp *.a $(CODA)/Linux_armv7l/lib/
	-cp *.so $(CODA)/Linux_armv7l/lib/
	-cp *.h $(CODA)/Linux_armv7l/include/

echoarch:
	@echo "Make for $(ARCH)"

.PHONY: clean echoarch
