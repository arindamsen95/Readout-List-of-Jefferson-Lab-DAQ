#
# File:
#    Makefile
#
# Description:
#    Makefile for the JLab VTP module running Linux on an ARMv7 processor
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG=1
#
#
BASENAME=vtp
ARCH=${shell uname -m}
KERNEL_VERSION=${shell uname -r}

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -L.
INCS			= -I.

LIBS			= lib${BASENAME}.a

ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif
SRC			= ${BASENAME}Lib.c
HDRS			= $(SRC:.c=.h)
OBJ			= ${BASENAME}Lib.o
DEPS			= $(SRC:.c=.d)

all: echoarch $(LIBS)

$(OBJ): $(SRC) $(HDRS)
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $(SRC)

$(LIBS): $(OBJ)
	$(CC) -fpic -shared $(CFLAGS) $(INCS) -o $(@:%.a=%.so) $(SRC)
	$(AR) ruv $@ $<
	$(RANLIB) $@

%.d: %.c
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

clean:
	@rm -vf ${BASENAME}Lib.{o,d,d.*} lib${BASENAME}.{a,so}

realclean: clean
	@rm -vf *~

echoarch:
	@echo "Make for $(ARCH)"

.PHONY: clean echoarch
