prefix = /usr/local

all:
	$(CC) -g -o cwatch cwatch.c

install:
	install cwatch $(DESTDIR)$(prefix)/bin
	install cwatch.1 $(DESTDIR)$(prefix)/share/man/man1

clean:
	rm -f cwatch
