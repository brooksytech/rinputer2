DESTDIR ?= /
PREFIX ?= /usr/

default:
	$(CROSS_COMPILE)$(CC) main.c -o rinputer2 $(CFLAGS) -lpthread -Wall -Wextra

install:
	install -m 0644 Rinputer2.service $(DESTDIR)/etc/systemd/system/
	install -m 0755 rinputer2 $(DESTDIR)$(PREFIX)/bin/rinputer2
