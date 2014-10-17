#
# iniparser Makefile
#

# Compiler settings
CC      ?= gcc
CFLAGS	?= -O2
CFLAGS  += -fPIC -Wall -ansi -std=c99 -pedantic

# Ar settings to build the library
AR	    ?= ar
ARFLAGS = rcv

SHLD = ${CC} ${CFLAGS}
LDSHFLAGS = -shared -Wl,-Bsymbolic
LDFLAGS += -Wl,-rpath -Wl,/usr/lib -Wl,-rpath,/usr/lib

# Set RANLIB to ranlib on systems that require it (Sun OS < 4, Mac OSX)
# RANLIB  = ranlib
RANLIB = true

RM      ?= rm -f


# Implicit rules

SUFFIXES = .o .c .h .a .so .sl

COMPILE.c	?= $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

ifndef V
QUIET_AR	= @echo "AR	$@";
QUIET_CC	= @echo "CC	$@";
QUIET_LINK	= @echo "LINK	$@";
QUIET_RANLIB	= @echo "RANLIB	$@";
endif

.c.o:
	$(QUIET_CC)$(COMPILE.c) $(OUTPUT_OPTION) $<


SRCS = $(wildcard src/*.c)

OBJS = $(SRCS:.c=.o)

INCS = $(wildcard src/*.h)

default:	libastr.a libastr.so

libastr.a:	$(OBJS)
	$(QUIET_AR)$(AR) $(ARFLAGS) $@ $^
	$(QUIET_RANLIB)$(RANLIB) $@

libastr.so:	$(OBJS)
	$(QUIET_LINK)$(SHLD) $(LDSHFLAGS) $(LDFLAGS) -o $@.0 $(OBJS) \
		-Wl,-soname=`basename $@`.0

clean:
	$(RM) $(OBJS)
	$(RM) libastr.a libastr.so.0
	#@(cd test ; $(MAKE) clean)

veryclean:
	$(RM) $(OBJS) libastr.a libastr.so*
	rm -rf ./html ; mkdir html
	cd test ; $(MAKE) veryclean

check: default
	@(cd test ; $(MAKE))

install: default
	cp -f libastr.a $(PREFIX)/lib/
	cp -f libastr.so* $(PREFIX)/lib/
	cp -f $(INCS) $(PREFIX)/include/

uninstall:
	rm -f  $(PREFIX)/lib/libastr.a
	rm -f  $(PREFIX)/lib/libastr.so*
	rm -f $(patsubst src/%, $(PREFIX)/include/%, $(INCS))

.PHONY: default clean veryclean docs check

