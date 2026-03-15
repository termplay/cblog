CC      ?= cc
CFLAGS  ?= -std=c17 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS ?= -lpthread
INCDIR  := include
SRCDIR  := src
OBJDIR  := build
BINDIR  := bin
TARGET  := $(BINDIR)/cblog

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: $(TARGET)
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/cblog
	@echo "Installed cblog to /usr/local/bin/cblog"
	@echo "Installing default theme..."
	install -d /usr/local/share/cblog/themes/default
	cp -r themes/default/* /usr/local/share/cblog/themes/default/
	@echo "Done."
