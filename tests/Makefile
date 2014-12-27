TARGETS	= $(OBJDIR)plugins $(OBJDIR)tests.log
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

$(OBJDIR)tests.log: plugins tests.sh
	./tests.sh -P "$(PREFIX)" -- "$(OBJDIR)tests.log"

$(OBJDIR)plugins.o: plugins.c
	$(CC) $(plugins_CFLAGS) -o $(OBJDIR)plugins.o -c plugins.c

clean:
	$(RM) -- $(plugins_OBJS) $(tests.log_OBJS)
	./tests.sh -c -P "$(PREFIX)" -- "tests.log"

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)

uninstall:

.PHONY: all clean distclean install uninstall
