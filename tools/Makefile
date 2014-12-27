TARGETS	= $(OBJDIR)dirtree $(OBJDIR)iconlist
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
BINDIR	= $(PREFIX)/bin
SBINDIR	= $(PREFIX)/sbin
CC	= cc
CPPFLAGSF=
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags gtk+-2.0`
CFLAGS	= -Wall -g -O2
LDFLAGSF= `pkg-config --libs gtk+-2.0`
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

dirtree_OBJS = $(OBJDIR)dirtree.o
dirtree_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
dirtree_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)dirtree: $(dirtree_OBJS)
	$(CC) -o $(OBJDIR)dirtree $(dirtree_OBJS) $(dirtree_LDFLAGS)

iconlist_OBJS = $(OBJDIR)iconlist.o
iconlist_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
iconlist_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)iconlist: $(iconlist_OBJS)
	$(CC) -o $(OBJDIR)iconlist $(iconlist_OBJS) $(iconlist_LDFLAGS)

$(OBJDIR)dirtree.o: dirtree.c
	$(CC) $(dirtree_CFLAGS) -o $(OBJDIR)dirtree.o -c dirtree.c

$(OBJDIR)iconlist.o: iconlist.c
	$(CC) $(iconlist_CFLAGS) -o $(OBJDIR)iconlist.o -c iconlist.c

clean:
	$(RM) -- $(dirtree_OBJS) $(iconlist_OBJS)

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)

uninstall:

.PHONY: all clean distclean install uninstall
