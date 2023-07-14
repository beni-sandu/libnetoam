STRICT_COMPILE = 1

CFLAGS = -Wall
LDFLAGS = -lpthread -lrt -lcap -lnet
OUTDIR = build

# Use VERBOSE=1 to echo all Makefile commands when running
VERBOSE ?= 0
ifneq ($(VERBOSE), 1)
Q=@
endif

# Use DEBUG_ENABLE=1 for a debug build
DEBUG_ENABLE ?= 0
ifeq ($(DEBUG_ENABLE), 1)
CFLAGS += -DDEBUG_ENABLE -g
endif

ifeq ($(STRICT_COMPILE),1)
CFLAGS += -O2 -W -Werror -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -Wmissing-declarations -Wold-style-definition -Wpointer-arith
CFLAGS += -Wcast-align -Wnested-externs -Wcast-qual
CFLAGS += -Wformat-security -Wundef -Wwrite-strings
CFLAGS += -Wbad-function-cast -Wformat-nonliteral -Wsuggest-attribute=format -Winline
CFLAGS += -std=gnu99
endif

TEST_BIN = oam_test

VERSION = $(shell grep LIBNETOAM_VERSION libnetoam.h | cut -d " " -f 3)

oam_test_FILES = libnetoam_test.c

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
	$(Q)$(CC) -c $(CFLAGS) -fpic libnetoam.c oam_session.c oam_frame.c eth-lb.c
	$(Q)$(CC) -shared -Wl,-soname,libnetoam.so.$(VERSION) -o $(OUTDIR)/libnetoam.so.$(VERSION) libnetoam.o oam_session.o oam_frame.o eth-lb.o $(LDFLAGS)
	$(Q)rm *.o

install:
	$(Q)mkdir -p $(PREFIX)/include/libnetoam
	$(Q)if [ ! -d $(PREFIX)/lib ] ; then \
			mkdir -p $(PREFIX)/lib ; \
		fi
	$(Q)cp -d $(OUTDIR)/libnetoam.so* $(PREFIX)/lib
	$(Q)cp *.h $(PREFIX)/include/libnetoam
	$(Q)ln -sf $(PREFIX)/lib/libnetoam.so.$(VERSION) $(PREFIX)/lib/libnetoam.so

uninstall:
	$(Q)rm -rf $(PREFIX)/include/libnetoam 2> /dev/null ||:
	$(Q)rm -rf $(PREFIX)/lib/libnetoam.so* 2> /dev/null ||:

test:
	$(Q)$(CC) $(CFLAGS) $(oam_test_FILES) -o $(TEST_BIN) -lnetoam

clean:
	$(Q)rm -rf $(OUTDIR) 2> /dev/null ||: