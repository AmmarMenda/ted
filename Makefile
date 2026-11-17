CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
LDFLAGS ?=
LDLIBS  ?=
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
DESTDIR ?=

TARGET   := ted

TS_ROOT  := deps/tree-sitter
TSC_ROOT := deps/tree-sitter-c

TS_LIB   := $(TS_ROOT)/libtree-sitter.a
TSC_LIB  := $(TSC_ROOT)/libtree-sitter-c.a

TS_INC   := -I$(TS_ROOT)/lib/include
TSC_INC  := -I$(TSC_ROOT)/src

SRC := main.c common.c buffer.c undo.c file.c selection.c \
       copypaste.c prompt.c search.c render.c input.c syntax.c
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)

all: $(TARGET)

# Rebuild each grammar runtime on demand.
$(TS_LIB):
	$(MAKE) -C $(TS_ROOT)
$(TSC_LIB):
	$(MAKE) -C $(TSC_ROOT)

$(TARGET): $(OBJ) $(TS_LIB) $(TSC_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(TS_LIB) $(TSC_LIB) $(LDLIBS)

%.o: %.c ted.h syntax.h
	$(CC) $(CFLAGS) $(TS_INC) $(TSC_INC) -MMD -MP -c -o $@ $<

install: $(TARGET)
	install -d  $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET) $(OBJ) $(DEP)

distclean: clean
	$(MAKE) -C $(TS_ROOT)  clean || true
	$(MAKE) -C $(TSC_ROOT) clean || true

.PHONY: all install uninstall clean distclean

-include $(DEP)