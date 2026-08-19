.POSIX:

VERSION = 1.0
TARGET = 90s
MANPAGE = $(TARGET).1
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

CFLAGS += -std=c99 -pedantic -Wall -DVERSION=$(VERSION) -D_DEFAULT_SOURCE

SRC != find src -name "*.c"
OBJS = $(SRC:.c=.o)
INCLUDE = include

.c.o:
	$(CC) -o $@ $(CFLAGS) -I$(INCLUDE) -c $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

dist:
	mkdir -p $(TARGET)-$(VERSION)
	cp -R README.md $(MANPAGE) $(TARGET) $(TARGET)-$(VERSION)
	tar -czf $(TARGET)-$(VERSION).tar.gz $(TARGET)-$(VERSION)
	$(RM) -r $(TARGET)-$(VERSION)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)
	cp -p $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BINDIR)/$(TARGET)
	cp -p $(MANPAGE) $(DESTDIR)$(MANDIR)/$(MANPAGE)
	chmod 644 $(DESTDIR)$(MANDIR)/$(MANPAGE)

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(TARGET)
	$(RM) $(DESTDIR)$(MANDIR)/$(MANPAGE)

clean:
	$(RM) $(TARGET) src/*.o

all: $(TARGET)

.PHONY: all dist install uninstall clean
