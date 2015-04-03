TARGETS	= $(OBJDIR)plugins $(OBJDIR)vfs $(OBJDIR)tests.log
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
BINDIR	= $(PREFIX)/bin
SBINDIR	= $(PREFIX)/sbin
CC	= cc
CPPFLAGSF= -I ../include
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags libDesktop`
CFLAGS	= -Wall -g -O2
LDFLAGSF= -W `pkg-config --libs libDesktop`
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

plugins_OBJS = $(OBJDIR)plugins.o
plugins_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
plugins_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)plugins: $(plugins_OBJS)
	$(CC) -o $(OBJDIR)plugins $(plugins_OBJS) $(plugins_LDFLAGS)

vfs_OBJS = $(OBJDIR)vfs.o
vfs_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
vfs_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -L../src -Wl,-rpath,../src -lBrowser

$(OBJDIR)vfs: $(vfs_OBJS)
	$(CC) -o $(OBJDIR)vfs $(vfs_OBJS) $(vfs_LDFLAGS)

$(OBJDIR)tests.log: plugins vfs tests.sh
	./tests.sh -P "$(PREFIX)" -- "$(OBJDIR)tests.log"

$(OBJDIR)plugins.o: plugins.c
	$(CC) $(plugins_CFLAGS) -o $(OBJDIR)plugins.o -c plugins.c

$(OBJDIR)vfs.o: vfs.c
	$(CC) $(vfs_CFLAGS) -o $(OBJDIR)vfs.o -c vfs.c

clean:
	$(RM) -- $(plugins_OBJS) $(vfs_OBJS) $(tests.log_OBJS)
	./tests.sh -c -P "$(PREFIX)" -- "tests.log"

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)

uninstall:

.PHONY: all clean distclean install uninstall
