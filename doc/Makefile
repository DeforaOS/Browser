TARGETS	= browser.1 browser.html copy.1 copy.html delete.1 delete.html desktop.1 desktop.html desktopctl.1 desktopctl.html move.1 move.html open.1 open.html properties.1 properties.html view.1 view.html
PREFIX	= /usr/local
DESTDIR	=
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

browser.1: browser.xml
	./docbook.sh -P "$(PREFIX)" -- "browser.1"

browser.html: browser.css.xml browser.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "browser.html"

copy.1: copy.xml
	./docbook.sh -P "$(PREFIX)" -- "copy.1"

copy.html: copy.css.xml copy.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "copy.html"

delete.1: delete.xml
	./docbook.sh -P "$(PREFIX)" -- "delete.1"

delete.html: delete.css.xml delete.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "delete.html"

desktop.1: desktop.xml
	./docbook.sh -P "$(PREFIX)" -- "desktop.1"

desktop.html: desktop.css.xml desktop.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "desktop.html"

desktopctl.1: desktopctl.xml
	./docbook.sh -P "$(PREFIX)" -- "desktopctl.1"

desktopctl.html: desktopctl.css.xml desktopctl.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "desktopctl.html"

move.1: move.xml
	./docbook.sh -P "$(PREFIX)" -- "move.1"

move.html: manual.css.xml move.css.xml move.xml
	./docbook.sh -P "$(PREFIX)" -- "move.html"

open.1: open.xml
	./docbook.sh -P "$(PREFIX)" -- "open.1"

open.html: manual.css.xml open.css.xml open.xml
	./docbook.sh -P "$(PREFIX)" -- "open.html"

properties.1: properties.xml
	./docbook.sh -P "$(PREFIX)" -- "properties.1"

properties.html: manual.css.xml properties.css.xml properties.xml
	./docbook.sh -P "$(PREFIX)" -- "properties.html"

view.1: view.xml
	./docbook.sh -P "$(PREFIX)" -- "view.1"

view.html: manual.css.xml view.css.xml view.xml
	./docbook.sh -P "$(PREFIX)" -- "view.html"

clean:
	$(RM) -- $(browser.1_OBJS) $(browser.html_OBJS) $(copy.1_OBJS) $(copy.html_OBJS) $(delete.1_OBJS) $(delete.html_OBJS) $(desktop.1_OBJS) $(desktop.html_OBJS) $(desktopctl.1_OBJS) $(desktopctl.html_OBJS) $(move.1_OBJS) $(move.html_OBJS) $(open.1_OBJS) $(open.html_OBJS) $(properties.1_OBJS) $(properties.html_OBJS) $(view.1_OBJS) $(view.html_OBJS)
	./docbook.sh -c -P "$(PREFIX)" -- "browser.1"
	./docbook.sh -c -P "$(PREFIX)" -- "browser.html"
	./docbook.sh -c -P "$(PREFIX)" -- "copy.1"
	./docbook.sh -c -P "$(PREFIX)" -- "copy.html"
	./docbook.sh -c -P "$(PREFIX)" -- "delete.1"
	./docbook.sh -c -P "$(PREFIX)" -- "delete.html"
	./docbook.sh -c -P "$(PREFIX)" -- "desktop.1"
	./docbook.sh -c -P "$(PREFIX)" -- "desktop.html"
	./docbook.sh -c -P "$(PREFIX)" -- "desktopctl.1"
	./docbook.sh -c -P "$(PREFIX)" -- "desktopctl.html"
	./docbook.sh -c -P "$(PREFIX)" -- "move.1"
	./docbook.sh -c -P "$(PREFIX)" -- "move.html"
	./docbook.sh -c -P "$(PREFIX)" -- "open.1"
	./docbook.sh -c -P "$(PREFIX)" -- "open.html"
	./docbook.sh -c -P "$(PREFIX)" -- "properties.1"
	./docbook.sh -c -P "$(PREFIX)" -- "properties.html"
	./docbook.sh -c -P "$(PREFIX)" -- "view.1"
	./docbook.sh -c -P "$(PREFIX)" -- "view.html"

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "browser.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "browser.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "copy.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "copy.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "delete.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "delete.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "desktop.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "desktop.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "desktopctl.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "desktopctl.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "move.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "move.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "open.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "open.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "properties.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "properties.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "view.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "view.html"

uninstall:
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "browser.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "browser.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "copy.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "copy.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "delete.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "delete.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "desktop.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "desktop.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "desktopctl.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "desktopctl.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "move.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "move.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "open.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "open.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "properties.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "properties.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "view.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -u -- "view.html"

.PHONY: all clean distclean install uninstall
