SUBDIRS	= plugins
TARGETS	= libBrowser.a libBrowser.so.0.0 libBrowser.so.0 libBrowser.so browser copy delete desktop desktopctl move open properties view
PREFIX	= /usr/local
DESTDIR	=
LIBDIR	= $(PREFIX)/lib
CC	= cc
CPPFLAGSF=
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags libDesktop`
CFLAGS	= -Wall -g -O2 -pedantic
LDFLAGSF= `pkg-config --libs libDesktop` -lintl
AR	= ar
RANLIB	= ranlib
CCSHARED= $(CC) -shared
BINDIR	= $(PREFIX)/bin
SBINDIR	= $(PREFIX)/sbin
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: subdirs $(TARGETS)

subdirs:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE)) || exit; done

libBrowser_OBJS = vfs.o
libBrowser_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) -fPIC
libBrowser_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

libBrowser.a: $(libBrowser_OBJS)
	$(AR) -rc libBrowser.a $(libBrowser_OBJS)
	$(RANLIB) libBrowser.a

libBrowser.so.0.0 libBrowser.so.0 libBrowser.so: $(libBrowser_OBJS)
	$(CCSHARED) -o libBrowser.so.0.0 -Wl,-soname,libBrowser.so.0 $(libBrowser_OBJS) $(libBrowser_LDFLAGS)
	$(LN) -s -- libBrowser.so.0.0 libBrowser.so.0
	$(LN) -s -- libBrowser.so.0.0 libBrowser.so

browser_OBJS = browser.o callbacks.o main.o
browser_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
browser_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -L . -Wl,-rpath,$(LIBDIR) -lBrowser

browser: $(browser_OBJS) libBrowser.a
	$(CC) -o browser $(browser_OBJS) $(browser_LDFLAGS)

copy_OBJS = copy.o
copy_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
copy_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

copy: $(copy_OBJS)
	$(CC) -o copy $(copy_OBJS) $(copy_LDFLAGS)

delete_OBJS = delete.o
delete_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
delete_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

delete: $(delete_OBJS)
	$(CC) -o delete $(delete_OBJS) $(delete_LDFLAGS)

desktop_OBJS = desktop.o desktopicon.o
desktop_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags x11 xrandr`
desktop_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs x11 xrandr` -L . -Wl,-rpath,$(LIBDIR) -lBrowser

desktop: $(desktop_OBJS) libBrowser.a
	$(CC) -o desktop $(desktop_OBJS) $(desktop_LDFLAGS)

desktopctl_OBJS = desktopctl.o
desktopctl_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
desktopctl_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

desktopctl: $(desktopctl_OBJS)
	$(CC) -o desktopctl $(desktopctl_OBJS) $(desktopctl_LDFLAGS)

move_OBJS = move.o
move_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
move_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

move: $(move_OBJS)
	$(CC) -o move $(move_OBJS) $(move_LDFLAGS)

open_OBJS = open.o
open_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
open_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

open: $(open_OBJS)
	$(CC) -o open $(open_OBJS) $(open_LDFLAGS)

properties_OBJS = properties.o
properties_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
properties_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -L . -Wl,-rpath,$(LIBDIR) -lBrowser

properties: $(properties_OBJS) libBrowser.a
	$(CC) -o properties $(properties_OBJS) $(properties_LDFLAGS)

view_OBJS = view.o
view_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
view_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

view: $(view_OBJS)
	$(CC) -o view $(view_OBJS) $(view_LDFLAGS)

vfs.o: vfs.c
	$(CC) $(libBrowser_CFLAGS) -c vfs.c

browser.o: browser.c callbacks.h common.c browser.h ../config.h
	$(CC) -D PREFIX=\"$(PREFIX)\" $(browser_CFLAGS) -c browser.c

callbacks.o: callbacks.c browser.h callbacks.h common.c ../config.h
	$(CC) $(browser_CFLAGS) -c callbacks.c

main.o: main.c browser.h
	$(CC) $(browser_CFLAGS) -c main.c

copy.o: copy.c
	$(CC) $(copy_CFLAGS) -c copy.c

delete.o: delete.c
	$(CC) $(delete_CFLAGS) -c delete.c

desktop.o: desktop.c desktopicon.h desktop.h common.c ../config.h
	$(CC) -D PREFIX=\"$(PREFIX)\" $(desktop_CFLAGS) -c desktop.c

desktopicon.o: desktopicon.c desktop.h desktopicon.h ../config.h
	$(CC) -D PREFIX=\"$(PREFIX)\" $(desktop_CFLAGS) -c desktopicon.c

desktopctl.o: desktopctl.c desktop.h ../config.h
	$(CC) $(desktopctl_CFLAGS) -c desktopctl.c

move.o: move.c
	$(CC) $(move_CFLAGS) -c move.c

open.o: open.c
	$(CC) $(open_CFLAGS) -c open.c

properties.o: properties.c ../include/Browser.h browser.h common.c
	$(CC) -D PREFIX=\"$(PREFIX)\" $(properties_CFLAGS) -c properties.c

view.o: view.c
	$(CC) $(view_CFLAGS) -c view.c

clean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) clean) || exit; done
	$(RM) -- $(libBrowser_OBJS) $(browser_OBJS) $(copy_OBJS) $(delete_OBJS) $(desktop_OBJS) $(desktopctl_OBJS) $(move_OBJS) $(open_OBJS) $(properties_OBJS) $(view_OBJS)

distclean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) distclean) || exit; done
	$(RM) -- $(libBrowser_OBJS) $(browser_OBJS) $(copy_OBJS) $(delete_OBJS) $(desktop_OBJS) $(desktopctl_OBJS) $(move_OBJS) $(open_OBJS) $(properties_OBJS) $(view_OBJS)
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) install) || exit; done
	$(MKDIR) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0644 libBrowser.a $(DESTDIR)$(LIBDIR)/libBrowser.a
	$(INSTALL) -m 0755 libBrowser.so.0.0 $(DESTDIR)$(LIBDIR)/libBrowser.so.0.0
	$(LN) -s -- libBrowser.so.0.0 $(DESTDIR)$(LIBDIR)/libBrowser.so.0
	$(LN) -s -- libBrowser.so.0.0 $(DESTDIR)$(LIBDIR)/libBrowser.so
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 browser $(DESTDIR)$(BINDIR)/browser
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 copy $(DESTDIR)$(BINDIR)/copy
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 delete $(DESTDIR)$(BINDIR)/delete
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 desktop $(DESTDIR)$(BINDIR)/desktop
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 desktopctl $(DESTDIR)$(BINDIR)/desktopctl
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 move $(DESTDIR)$(BINDIR)/move
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 open $(DESTDIR)$(BINDIR)/open
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 properties $(DESTDIR)$(BINDIR)/properties
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 view $(DESTDIR)$(BINDIR)/view

uninstall:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) uninstall) || exit; done
	$(RM) -- $(DESTDIR)$(LIBDIR)/libBrowser.a
	$(RM) -- $(DESTDIR)$(LIBDIR)/libBrowser.so.0.0
	$(RM) -- $(DESTDIR)$(LIBDIR)/libBrowser.so.0
	$(RM) -- $(DESTDIR)$(LIBDIR)/libBrowser.so
	$(RM) -- $(DESTDIR)$(BINDIR)/browser
	$(RM) -- $(DESTDIR)$(BINDIR)/copy
	$(RM) -- $(DESTDIR)$(BINDIR)/delete
	$(RM) -- $(DESTDIR)$(BINDIR)/desktop
	$(RM) -- $(DESTDIR)$(BINDIR)/desktopctl
	$(RM) -- $(DESTDIR)$(BINDIR)/move
	$(RM) -- $(DESTDIR)$(BINDIR)/open
	$(RM) -- $(DESTDIR)$(BINDIR)/properties
	$(RM) -- $(DESTDIR)$(BINDIR)/view

.PHONY: all subdirs clean distclean install uninstall
