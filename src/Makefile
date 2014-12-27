TARGETS	= $(OBJDIR)libBrowser.a $(OBJDIR)libBrowser.so.0.0 $(OBJDIR)libBrowser.so.0 $(OBJDIR)libBrowser.so $(OBJDIR)browser $(OBJDIR)copy $(OBJDIR)delete $(OBJDIR)desktop $(OBJDIR)desktopctl $(OBJDIR)move $(OBJDIR)open $(OBJDIR)properties $(OBJDIR)view
OBJDIR	=
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


all: $(TARGETS)

libBrowser_OBJS = $(OBJDIR)vfs.o
libBrowser_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) -fPIC
libBrowser_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)libBrowser.a: $(libBrowser_OBJS)
	$(AR) -rc $(OBJDIR)libBrowser.a $(libBrowser_OBJS)
	$(RANLIB) $(OBJDIR)libBrowser.a

$(OBJDIR)libBrowser.so.0.0 $(OBJDIR)libBrowser.so.0 $(OBJDIR)libBrowser.so: $(libBrowser_OBJS)
	$(CCSHARED) -o $(OBJDIR)libBrowser.so.0.0 -Wl,-soname,libBrowser.so.0 $(libBrowser_OBJS) $(libBrowser_LDFLAGS)
	$(LN) -s -- libBrowser.so.0.0 $(OBJDIR)libBrowser.so.0
	$(LN) -s -- libBrowser.so.0.0 $(OBJDIR)libBrowser.so

browser_OBJS = $(OBJDIR)browser.o $(OBJDIR)callbacks.o $(OBJDIR)main.o
browser_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
browser_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -L . -Wl,-rpath,$(LIBDIR) -lBrowser

$(OBJDIR)browser: $(browser_OBJS) libBrowser.a
	$(CC) -o $(OBJDIR)browser $(browser_OBJS) $(browser_LDFLAGS)

copy_OBJS = $(OBJDIR)copy.o
copy_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
copy_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)copy: $(copy_OBJS)
	$(CC) -o $(OBJDIR)copy $(copy_OBJS) $(copy_LDFLAGS)

delete_OBJS = $(OBJDIR)delete.o
delete_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
delete_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)delete: $(delete_OBJS)
	$(CC) -o $(OBJDIR)delete $(delete_OBJS) $(delete_LDFLAGS)

desktop_OBJS = $(OBJDIR)desktop.o $(OBJDIR)desktopicon.o
desktop_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags x11 xrandr`
desktop_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs x11 xrandr` -L . -Wl,-rpath,$(LIBDIR) -lBrowser

$(OBJDIR)desktop: $(desktop_OBJS) libBrowser.a
	$(CC) -o $(OBJDIR)desktop $(desktop_OBJS) $(desktop_LDFLAGS)

desktopctl_OBJS = $(OBJDIR)desktopctl.o
desktopctl_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
desktopctl_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)desktopctl: $(desktopctl_OBJS)
	$(CC) -o $(OBJDIR)desktopctl $(desktopctl_OBJS) $(desktopctl_LDFLAGS)

move_OBJS = $(OBJDIR)move.o
move_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
move_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)move: $(move_OBJS)
	$(CC) -o $(OBJDIR)move $(move_OBJS) $(move_LDFLAGS)

open_OBJS = $(OBJDIR)open.o
open_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
open_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)open: $(open_OBJS)
	$(CC) -o $(OBJDIR)open $(open_OBJS) $(open_LDFLAGS)

properties_OBJS = $(OBJDIR)properties.o
properties_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
properties_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -L . -Wl,-rpath,$(LIBDIR) -lBrowser

$(OBJDIR)properties: $(properties_OBJS) libBrowser.a
	$(CC) -o $(OBJDIR)properties $(properties_OBJS) $(properties_LDFLAGS)

view_OBJS = $(OBJDIR)view.o
view_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
view_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)view: $(view_OBJS)
	$(CC) -o $(OBJDIR)view $(view_OBJS) $(view_LDFLAGS)

$(OBJDIR)vfs.o: vfs.c
	$(CC) $(libBrowser_CFLAGS) -o $(OBJDIR)vfs.o -c vfs.c

$(OBJDIR)browser.o: browser.c callbacks.h common.c browser.h ../config.h
	$(CC) -D PREFIX=\"$(PREFIX)\" $(browser_CFLAGS) -o $(OBJDIR)browser.o -c browser.c

$(OBJDIR)callbacks.o: callbacks.c browser.h callbacks.h common.c ../config.h
	$(CC) $(browser_CFLAGS) -o $(OBJDIR)callbacks.o -c callbacks.c

$(OBJDIR)main.o: main.c browser.h
	$(CC) $(browser_CFLAGS) -o $(OBJDIR)main.o -c main.c

$(OBJDIR)copy.o: copy.c
	$(CC) $(copy_CFLAGS) -o $(OBJDIR)copy.o -c copy.c

$(OBJDIR)delete.o: delete.c
	$(CC) $(delete_CFLAGS) -o $(OBJDIR)delete.o -c delete.c

$(OBJDIR)desktop.o: desktop.c desktopicon.h desktop.h common.c ../config.h
	$(CC) -D PREFIX=\"$(PREFIX)\" $(desktop_CFLAGS) -o $(OBJDIR)desktop.o -c desktop.c

$(OBJDIR)desktopicon.o: desktopicon.c desktop.h desktopicon.h ../config.h
	$(CC) -D PREFIX=\"$(PREFIX)\" $(desktop_CFLAGS) -o $(OBJDIR)desktopicon.o -c desktopicon.c

$(OBJDIR)desktopctl.o: desktopctl.c desktop.h ../config.h
	$(CC) $(desktopctl_CFLAGS) -o $(OBJDIR)desktopctl.o -c desktopctl.c

$(OBJDIR)move.o: move.c
	$(CC) $(move_CFLAGS) -o $(OBJDIR)move.o -c move.c

$(OBJDIR)open.o: open.c
	$(CC) $(open_CFLAGS) -o $(OBJDIR)open.o -c open.c

$(OBJDIR)properties.o: properties.c ../include/Browser.h browser.h common.c
	$(CC) -D PREFIX=\"$(PREFIX)\" $(properties_CFLAGS) -o $(OBJDIR)properties.o -c properties.c

$(OBJDIR)view.o: view.c
	$(CC) $(view_CFLAGS) -o $(OBJDIR)view.o -c view.c

clean:
	$(RM) -- $(libBrowser_OBJS) $(browser_OBJS) $(copy_OBJS) $(delete_OBJS) $(desktop_OBJS) $(desktopctl_OBJS) $(move_OBJS) $(open_OBJS) $(properties_OBJS) $(view_OBJS)

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	$(MKDIR) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0644 $(OBJDIR)libBrowser.a $(DESTDIR)$(LIBDIR)/libBrowser.a
	$(INSTALL) -m 0755 $(OBJDIR)libBrowser.so.0.0 $(DESTDIR)$(LIBDIR)/libBrowser.so.0.0
	$(LN) -s -- libBrowser.so.0.0 $(DESTDIR)$(LIBDIR)/libBrowser.so.0
	$(LN) -s -- libBrowser.so.0.0 $(DESTDIR)$(LIBDIR)/libBrowser.so
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)browser $(DESTDIR)$(BINDIR)/browser
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)copy $(DESTDIR)$(BINDIR)/copy
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)delete $(DESTDIR)$(BINDIR)/delete
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)desktop $(DESTDIR)$(BINDIR)/desktop
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)desktopctl $(DESTDIR)$(BINDIR)/desktopctl
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)move $(DESTDIR)$(BINDIR)/move
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)open $(DESTDIR)$(BINDIR)/open
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)properties $(DESTDIR)$(BINDIR)/properties
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)view $(DESTDIR)$(BINDIR)/view

uninstall:
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

.PHONY: all clean distclean install uninstall
