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

ARCH=${shell uname -m}
KERNEL_VERSION=${shell uname -r}

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -Wall -g -I. -I../ \
			  -L. -L../ -lrt -lvtp

PROGS			= 

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~ 

%: %.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c)

.PHONY: all clean distclean
