PKGNAME= mod_watchcat
PKGCONF= 55_mod_watchcat.conf
PKGVERSION= 1.1.2

APXS= /usr/bin/apxs2

all: mod_watchcat.so

mod_watchcat.so: mod_watchcat.c
	$(APXS) -c mod_watchcat.c -lwcat

install:
	$(APXS) -i -n mod_watchcat.so mod_watchcat.la

clean:
	rm -f *.o *.c~ *.h~ *.conf~ *.[0-9]~ mod_watchcat.so mod_watchcat.lo mod_watchcat.slo mod_watchcat.la $(PKGNAME)-$(PKGVERSION).tar.bz2
	rm -rf .libs
	rm -f COPYRIGHT~ TODO~ Makefile~
	if [ -f $(PKGCONF).bz2 ]; then bunzip2 $(PKGCONF).bz2; fi
	if [ `basename ${PWD}` != "$(PKGNAME)-$(PKGVERSION)" ]; then \
	  rm -rf ../$(PKGNAME)-$(PKGVERSION); \
	fi

rpm: clean
	if [ `basename ${PWD}` != "$(PKGNAME)-$(PKGVERSION)" ]; then \
	  cp -a . ../$(PKGNAME)-$(PKGVERSION); \
	fi
	bzip2 -9 $(PKGCONF)
	tar -jcC .. --exclude CVS -f $(PKGNAME)-$(PKGVERSION).tar.bz2 $(PKGNAME)-$(PKGVERSION)
	rpm -ta $(PKGNAME)-$(PKGVERSION).tar.bz2
	bunzip2 $(PKGCONF).bz2
	rm -rf $(PKGNAME)-$(PKGVERSION).tar.bz2
	if [ `basename ${PWD}` != "$(PKGNAME)-$(PKGVERSION)" ]; then \
	  rm -rf ../$(PKGNAME)-$(PKGVERSION); \
	fi
