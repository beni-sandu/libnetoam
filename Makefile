STRICT_COMPILE = 1

CFLAGS = -Wall
LDFLAGS = -lpthread -lrt -lcap -lnet
OUTDIR = $(shell pwd)/build
TESTDIR = tests
SRCDIR = library
INCLDIR = include

# Use V=1 to echo all Makefile commands when running
V ?= 0
ifneq ($(V), 1)
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

VERSION = $(shell grep LIBNETOAM_VERSION $(INCLDIR)/libnetoam.h | cut -d " " -f 3)

# Use SDK environment if available
CC = $(shell echo $$CC)
ifeq ($(CC),)
	CC = $(shell which gcc)
endif

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

# Pass on vars to submakes
export

libs:
	$(Q)rm -rf $(OUTDIR) 2> /dev/null ||:
	$(Q)mkdir $(OUTDIR)
	$(Q)$(CC) -c $(CFLAGS) -fpic $(SRCDIR)/libnetoam.c $(SRCDIR)/oam_session.c $(SRCDIR)/oam_frame.c $(SRCDIR)/eth_lb.c
	$(Q)$(CC) -shared -Wl,-soname,libnetoam.so.$(VERSION) -o $(OUTDIR)/libnetoam.so.$(VERSION) libnetoam.o oam_session.o oam_frame.o eth_lb.o $(LDFLAGS)
	$(Q)ln -sf $(OUTDIR)/libnetoam.so.$(VERSION) $(OUTDIR)/libnetoam.so
	$(Q)rm *.o

install:
	$(Q)mkdir -p $(PREFIX)/include/libnetoam
	$(Q)mkdir -p $(PREFIX)/lib
	$(Q)cp -d $(OUTDIR)/libnetoam.so* $(PREFIX)/lib
	$(Q)cp $(INCLDIR)/*.h $(PREFIX)/include/libnetoam
	$(Q)ln -sf $(PREFIX)/lib/libnetoam.so.$(VERSION) $(PREFIX)/lib/libnetoam.so

uninstall:
	$(Q)rm -rf $(PREFIX)/include/libnetoam 2> /dev/null ||:
	$(Q)rm -rf $(PREFIX)/lib/libnetoam.so* 2> /dev/null ||:

test:
	$(Q)$(MAKE) -s -C $(TESTDIR) bins

test-run: test
	$(Q)cd $(TESTDIR) ; \
	./run.sh

clean:
	$(Q)rm -rf $(OUTDIR) 2> /dev/null ||:
	$(Q)$(MAKE) -s -C $(TESTDIR) clean
