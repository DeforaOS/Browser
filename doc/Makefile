TARGETS	= $(OBJDIR)browser.1 $(OBJDIR)browser.html $(OBJDIR)copy.1 $(OBJDIR)copy.html $(OBJDIR)delete.1 $(OBJDIR)delete.html $(OBJDIR)desktop.1 $(OBJDIR)desktop.html $(OBJDIR)desktopctl.1 $(OBJDIR)desktopctl.html $(OBJDIR)move.1 $(OBJDIR)move.html $(OBJDIR)open.1 $(OBJDIR)open.html $(OBJDIR)properties.1 $(OBJDIR)properties.html $(OBJDIR)view.1 $(OBJDIR)view.html
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

$(OBJDIR)browser.1: browser.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)browser.1"

$(OBJDIR)browser.html: browser.css.xml browser.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)browser.html"

$(OBJDIR)copy.1: copy.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)copy.1"

$(OBJDIR)copy.html: copy.css.xml copy.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)copy.html"

$(OBJDIR)delete.1: delete.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)delete.1"

$(OBJDIR)delete.html: delete.css.xml delete.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)delete.html"

$(OBJDIR)desktop.1: desktop.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)desktop.1"

$(OBJDIR)desktop.html: desktop.css.xml desktop.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)desktop.html"

$(OBJDIR)desktopctl.1: desktopctl.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)desktopctl.1"

$(OBJDIR)desktopctl.html: desktopctl.css.xml desktopctl.xml manual.css.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)desktopctl.html"

$(OBJDIR)move.1: move.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)move.1"

$(OBJDIR)move.html: manual.css.xml move.css.xml move.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)move.html"

$(OBJDIR)open.1: open.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)open.1"

$(OBJDIR)open.html: manual.css.xml open.css.xml open.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)open.html"

$(OBJDIR)properties.1: properties.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)properties.1"

$(OBJDIR)properties.html: manual.css.xml properties.css.xml properties.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)properties.html"

$(OBJDIR)view.1: view.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)view.1"

$(OBJDIR)view.html: manual.css.xml view.css.xml view.xml
	./docbook.sh -P "$(PREFIX)" -- "$(OBJDIR)view.html"

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
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)browser.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)browser.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)copy.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)copy.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)delete.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)delete.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)desktop.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)desktop.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)desktopctl.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)desktopctl.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)move.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)move.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)open.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)open.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)properties.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)properties.html"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)view.1"
	./docbook.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)view.html"

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
