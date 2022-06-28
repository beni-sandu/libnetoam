STRICT_COMPILE = 0

CFLAGS = -Wall
LDFLAGS = -lnetcfm -lpthread -lrt -lcap -lnet
OUTDIR = build

# Use VERBOSE=1 to echo all Makefile commands when running
VERBOSE ?= 0
ifneq ($(VERBOSE), 1)
Q=@
endif

# Use DEBUG_ENABLE=1 for a debug build
DEBUG_ENABLE ?= 1
ifeq ($(DEBUG_ENABLE), 1)
CFLAGS += -DDEBUG_ENABLE
endif

ifeq ($(STRICT_COMPILE),1)
CFLAGS += -O2 -W -Werror -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -Wmissing-declarations -Wold-style-definition -Wpointer-arith
CFLAGS += -Wcast-align -Wnested-externs -Wcast-qual
CFLAGS += -Wformat-security -Wundef -Wwrite-strings
CFLAGS += -Wbad-function-cast -Wformat-nonliteral -Wsuggest-attribute=format -Winline
CFLAGS += -std=gnu99
endif

TEST_BIN = cfm_test

VERSION = $(shell grep LIBNETCFM_VERSION libnetcfm.h | cut -d " " -f 3)

cfm_test_FILES = libnetcfm_test.c

# Use SDK environment if available
CC = $(shell echo $$CC)
ifeq ($(CC),)
	CC = $(shell which gcc)
endif

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

libs:
	$(Q)rm -rf $(OUTDIR) 2> /dev/null ||:
	$(Q)mkdir $(OUTDIR)
	$(Q)$(CC) -c $(CFLAGS) -fpic libnetcfm.c cfm_session.c
	$(Q)$(CC) -shared -Wl,-soname,libnetcfm.so.$(VERSION) -o $(OUTDIR)/libnetcfm.so.$(VERSION) libnetcfm.o cfm_session.o
	$(Q)rm *.o

install:
	$(Q)mkdir -p $(PREFIX)/include/libnetcfm
	$(Q)cp -d $(OUTDIR)/libnetcfm.so* $(PREFIX)/lib
	$(Q)cp *.h $(PREFIX)/include/libnetcfm
	$(Q)ln -sf $(PREFIX)/lib/libnetcfm.so.$(VERSION) $(PREFIX)/lib/libnetcfm.so

uninstall:
	$(Q)rm -rf $(PREFIX)/include/libnetcfm 2> /dev/null ||:
	$(Q)rm -rf $(PREFIX)/lib/libnetcfm.so* 2> /dev/null ||:

test:
	$(Q)$(CC) $(CFLAGS) $(cfm_test_FILES) -o $(TEST_BIN) $(LDFLAGS)

clean:
	$(Q)rm -rf $(OUTDIR) 2> /dev/null ||: