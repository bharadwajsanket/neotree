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
SRCS    := main.c tree.c fs.c utils.c cli.c
OBJS    := $(SRCS:.c=.o)
BIN     := neotree

# ---------------------------------------------------------------
# Flags
# ---------------------------------------------------------------
CFLAGS_BASE := -std=c99 -Wall -Wextra -Wpedantic \
               -Wshadow -Wformat=2 -Wstrict-prototypes \
               -Wmissing-prototypes

CFLAGS_REL  := -O2
CFLAGS_DBG  := -O0 -g -fsanitize=address,undefined

CFLAGS      ?= $(CFLAGS_BASE) $(CFLAGS_REL)

# ---------------------------------------------------------------
# Default target
# ---------------------------------------------------------------
.PHONY: all
all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ---------------------------------------------------------------
# Debug build
# ---------------------------------------------------------------
.PHONY: debug
debug: CFLAGS = $(CFLAGS_BASE) $(CFLAGS_DBG)
debug: $(BIN)

# ---------------------------------------------------------------
# Install / uninstall
# ---------------------------------------------------------------
.PHONY: install
install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@echo "Installed to $(PREFIX)/bin/$(BIN)"

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@echo "Uninstalled $(BIN)"

# ---------------------------------------------------------------
# Clean
# ---------------------------------------------------------------
.PHONY: clean
clean:
	rm -f $(OBJS) $(BIN)

# ---------------------------------------------------------------
# Dependency tracking (auto-generated .d files)
# ---------------------------------------------------------------
DEPS := $(SRCS:.c=.d)
-include $(DEPS)

%.d: %.c
	@$(CC) -MM $(CFLAGS) $< | sed 's|$*.o|$*.o $@|g' > $@
