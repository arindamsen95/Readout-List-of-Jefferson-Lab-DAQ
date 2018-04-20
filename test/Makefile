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

CC				= g++
AR				= ar
RANLIB		= ranlib
INCS			= -I. -I${VTPLIB_PATH} -I/usr/local/include -I$(CODA)/Linux_armv7l/include
CFLAGS		= -Wall -g ${INCS} \
            -L. -lrt -li2c -lm

#LIBNAMES	= $(CODA)/src/mysql4.1.20/$(OSTYPE_MACHINE)/lib/libmysql4.1.20.a
LIBNAMES  = $(CODA)/src/codautil/$(OSTYPE_MACHINE)/lib/libcodautil.a
LIBNAMES += /usr/local/lib/libactivemq-cpp.so
LIBNAMES += -lapr-1
LIBNAMES += -lcrypt -lssl -lcrypto -ldl -lz
LIBNAMES += -L/usr/kerberos/lib -lpthread -pthread -Wl,-rpath -Wl,/usr/local/lib

#LIBNAMES	= $(CODA)/src/codautil/Linux_armv7l/lib/libcodautil.a
LIBNAMES	+= $(CODA)/src/ipc/Linux_armv7l/lib/libipc.a
#LIBNAMES	+= /usr/local/lib/libactivemq-cpp.so
LIBNAMES	+= ../libvtp.a

PROGS			= vtpLibTest i2cvtpmon vtpSPItest vtpI2Ctest vtpDmaTest i2cvtpsetup vtpConfigTest vtpStatus
SRC			= $(PROGS:%=%.c)
DEPS			= $(SRC:%.c=%.d)

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~ 

%: %.c
	@echo "Making $@"
	$(CC) $(CFLAGS) ${LIBNAMES} -o $@ $(@:%=%.c)

%.d: %.c
	@echo ""
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared ${CFLAGS} $(LIBNAMES) $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: all clean distclean
