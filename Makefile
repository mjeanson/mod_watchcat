PKGNAME= mod_watchcat
PKGCONF= 55_mod_watchcat.conf
PKGVERSION= 1.0

APXS= /usr/sbin/apxs2

all: mod_watchcat.so

mod_watchcat.so: mod_watchcat.c
	$(APXS) -c mod_watchcat.c -lwcat

install:
	$(APXS) -i -n mod_watchcat.so mod_watchcat.la

clean:
	rm -f *.o *.c~ *.h~ *.conf~ *.[0-9]~ mod_watchcat.so mod_watchcat.lo mod_watchcat.slo mod_watchcat.la
	rm -rf .libs ../mod_watchcat-$(PKGVERSION)
	rm -f COPYRIGHT~ TODO~ Makefile~

rpm: clean
	cp -a . ../$(PKGNAME)-$(PKGVERSION)
	bzip2 -9 $(PKGCONF)
	tar -jcC .. -f $(PKGNAME)-$(PKGVERSION).tar.bz2 $(PKGNAME)-$(PKGVERSION) --exclude CVS
	rpm -ta $(PKGNAME)-$(PKGVERSION).tar.bz2
	bunzip2 $(PKGCONF).bz2
	rm -rf $(PKGNAME)-$(PKGVERSION).tar.bz2 ../$(PKGNAME)-$(PKGVERSION)
