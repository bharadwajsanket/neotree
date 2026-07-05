# Makefile — neotree
#
# Targets:
#   make           build release binary (./neotree)
#   make debug     build with -g -fsanitize=address,undefined
#   make clean     remove build artifacts
#   make install   install to PREFIX/bin (default: /usr/local/bin)
#   make uninstall remove installed binary
#
# Cross-platform notes:
#   Linux / macOS:  works as-is with gcc or clang.
#   Windows (MinGW): run from a MinGW shell; the #ifdef _WIN32 paths
#                   in fs.c are compiled automatically.

# ---------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------
CC      ?= gcc
PREFIX  ?= /usr/local

# ---------------------------------------------------------------
# Sources
# ---------------------------------------------------------------
SRCS    := main.c tree.c fs.c utils.c cli.c find.c largest.c
OBJS    := $(SRCS:.c=.o)
BIN     := neotree

# ---------------------------------------------------------------
# Flags
# ---------------------------------------------------------------
CFLAGS_BASE := -std=c99 -Wall -Wextra -Wpedantic \
               -Wshadow -Wformat=2 -Wstrict-prototypes \
               -Wmissing-prototypes -Wconversion -Wsign-conversion

CFLAGS_REL  := -O2
CFLAGS_DBG  := -O0 -g -fsanitize=address,undefined

CFLAGS      ?= $(CFLAGS_BASE) $(CFLAGS_REL)

# ---------------------------------------------------------------
# Default target
# ---------------------------------------------------------------
.PHONY: all
all: $(BIN) ntree

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

ntree: $(BIN)
	ln -sf $(BIN) ntree || cp $(BIN) ntree

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ---------------------------------------------------------------
# Debug build
# ---------------------------------------------------------------
.PHONY: debug
debug: CFLAGS = $(CFLAGS_BASE) $(CFLAGS_DBG)
debug: $(BIN) ntree

# ---------------------------------------------------------------
# Install / uninstall
# ---------------------------------------------------------------
.PHONY: install
install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	ln -sf $(BIN) $(DESTDIR)$(PREFIX)/bin/ntree || cp $(BIN) $(DESTDIR)$(PREFIX)/bin/ntree
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 644 man/neotree.1 $(DESTDIR)$(PREFIX)/share/man/man1/neotree.1
	ln -sf neotree.1 $(DESTDIR)$(PREFIX)/share/man/man1/ntree.1 || cp man/neotree.1 $(DESTDIR)$(PREFIX)/share/man/man1/ntree.1
	@echo "Installed to $(PREFIX)/bin/$(BIN)"

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	rm -f $(DESTDIR)$(PREFIX)/bin/ntree
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/neotree.1
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/ntree.1
	@echo "Uninstalled $(BIN)"

# ---------------------------------------------------------------
# Test
# ---------------------------------------------------------------
.PHONY: test
test: $(BIN)
	bash test.sh

# ---------------------------------------------------------------
# Clean
# ---------------------------------------------------------------
.PHONY: clean
clean:
	rm -f $(OBJS) $(BIN) $(DEPS) ntree

# ---------------------------------------------------------------
# Dependency tracking (auto-generated .d files)
# ---------------------------------------------------------------
DEPS := $(SRCS:.c=.d)
-include $(DEPS)

%.d: %.c
	@$(CC) -MM $(CFLAGS) $< | sed 's|$*.o|$*.o $@|g' > $@
