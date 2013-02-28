PREFIX	= /usr/local
DESTDIR	=
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install
INCLUDEDIR= $(PREFIX)/include


all:

clean:

distclean: clean

install:
	$(MKDIR) $(DESTDIR)$(INCLUDEDIR)/Desktop/Browser
	$(INSTALL) -m 0644 -- desktop.h $(DESTDIR)$(INCLUDEDIR)/Desktop/Browser/desktop.h
	$(MKDIR) $(DESTDIR)$(INCLUDEDIR)/Desktop/Browser
	$(INSTALL) -m 0644 -- plugin.h $(DESTDIR)$(INCLUDEDIR)/Desktop/Browser/plugin.h

uninstall:
	$(RM) -- $(DESTDIR)$(INCLUDEDIR)/Desktop/Browser/desktop.h
	$(RM) -- $(DESTDIR)$(INCLUDEDIR)/Desktop/Browser/plugin.h

.PHONY: all clean distclean install uninstall
