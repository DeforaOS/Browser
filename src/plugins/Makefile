TARGETS	= $(OBJDIR)backup.so $(OBJDIR)cvs.so $(OBJDIR)dirtree.so $(OBJDIR)favorites.so $(OBJDIR)git.so $(OBJDIR)make.so $(OBJDIR)preview.so $(OBJDIR)properties.so $(OBJDIR)selection.so $(OBJDIR)subversion.so $(OBJDIR)template.so $(OBJDIR)trash.so $(OBJDIR)undelete.so $(OBJDIR)volumes.so
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
LIBDIR	= $(PREFIX)/lib
CC	= cc
CPPFLAGSF= -I ../../include
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags libDesktop`
CFLAGS	= -Wall -g -O2 -pedantic -fPIC
LDFLAGSF= `pkg-config --libs libDesktop`
AR	= ar
RANLIB	= ranlib
CCSHARED= $(CC) -shared
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

backup_OBJS = $(OBJDIR)backup.o
backup_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
backup_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)backup.so: $(backup_OBJS)
	$(CCSHARED) -o $(OBJDIR)backup.so $(backup_OBJS) $(backup_LDFLAGS)

cvs_OBJS = $(OBJDIR)cvs.o
cvs_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
cvs_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)cvs.so: $(cvs_OBJS)
	$(CCSHARED) -o $(OBJDIR)cvs.so $(cvs_OBJS) $(cvs_LDFLAGS)

dirtree_OBJS = $(OBJDIR)dirtree.o
dirtree_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
dirtree_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -L .. -Wl,-rpath,$(LIBDIR) -lBrowser

$(OBJDIR)dirtree.so: $(dirtree_OBJS)
	$(CCSHARED) -o $(OBJDIR)dirtree.so $(dirtree_OBJS) $(dirtree_LDFLAGS)

favorites_OBJS = $(OBJDIR)favorites.o
favorites_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
favorites_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)favorites.so: $(favorites_OBJS)
	$(CCSHARED) -o $(OBJDIR)favorites.so $(favorites_OBJS) $(favorites_LDFLAGS)

git_OBJS = $(OBJDIR)git.o
git_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
git_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)git.so: $(git_OBJS)
	$(CCSHARED) -o $(OBJDIR)git.so $(git_OBJS) $(git_LDFLAGS)

make_OBJS = $(OBJDIR)make.o
make_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
make_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)make.so: $(make_OBJS)
	$(CCSHARED) -o $(OBJDIR)make.so $(make_OBJS) $(make_LDFLAGS)

preview_OBJS = $(OBJDIR)preview.o
preview_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
preview_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)preview.so: $(preview_OBJS)
	$(CCSHARED) -o $(OBJDIR)preview.so $(preview_OBJS) $(preview_LDFLAGS)

properties_OBJS = $(OBJDIR)properties.o
properties_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
properties_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)properties.so: $(properties_OBJS)
	$(CCSHARED) -o $(OBJDIR)properties.so $(properties_OBJS) $(properties_LDFLAGS)

selection_OBJS = $(OBJDIR)selection.o
selection_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
selection_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)selection.so: $(selection_OBJS)
	$(CCSHARED) -o $(OBJDIR)selection.so $(selection_OBJS) $(selection_LDFLAGS)

subversion_OBJS = $(OBJDIR)subversion.o
subversion_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
subversion_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)subversion.so: $(subversion_OBJS)
	$(CCSHARED) -o $(OBJDIR)subversion.so $(subversion_OBJS) $(subversion_LDFLAGS)

template_OBJS = $(OBJDIR)template.o
template_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
template_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)template.so: $(template_OBJS)
	$(CCSHARED) -o $(OBJDIR)template.so $(template_OBJS) $(template_LDFLAGS)

trash_OBJS = $(OBJDIR)trash.o
trash_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
trash_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)trash.so: $(trash_OBJS)
	$(CCSHARED) -o $(OBJDIR)trash.so $(trash_OBJS) $(trash_LDFLAGS)

undelete_OBJS = $(OBJDIR)undelete.o
undelete_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
undelete_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)undelete.so: $(undelete_OBJS)
	$(CCSHARED) -o $(OBJDIR)undelete.so $(undelete_OBJS) $(undelete_LDFLAGS)

volumes_OBJS = $(OBJDIR)volumes.o
volumes_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
volumes_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)volumes.so: $(volumes_OBJS)
	$(CCSHARED) -o $(OBJDIR)volumes.so $(volumes_OBJS) $(volumes_LDFLAGS)

$(OBJDIR)backup.o: backup.c trash.c ../../include/Browser.h
	$(CC) $(backup_CFLAGS) -o $(OBJDIR)backup.o -c backup.c

$(OBJDIR)cvs.o: cvs.c common.c ../../include/Browser.h
	$(CC) $(cvs_CFLAGS) -o $(OBJDIR)cvs.o -c cvs.c

$(OBJDIR)dirtree.o: dirtree.c ../../include/Browser.h
	$(CC) $(dirtree_CFLAGS) -o $(OBJDIR)dirtree.o -c dirtree.c

$(OBJDIR)favorites.o: favorites.c ../../include/Browser.h
	$(CC) $(favorites_CFLAGS) -o $(OBJDIR)favorites.o -c favorites.c

$(OBJDIR)git.o: git.c common.c ../../include/Browser.h
	$(CC) $(git_CFLAGS) -o $(OBJDIR)git.o -c git.c

$(OBJDIR)make.o: make.c common.c ../../include/Browser.h
	$(CC) $(make_CFLAGS) -o $(OBJDIR)make.o -c make.c

$(OBJDIR)preview.o: preview.c ../../include/Browser.h
	$(CC) $(preview_CFLAGS) -o $(OBJDIR)preview.o -c preview.c

$(OBJDIR)properties.o: properties.c ../../include/Browser.h
	$(CC) $(properties_CFLAGS) -o $(OBJDIR)properties.o -c properties.c

$(OBJDIR)selection.o: selection.c ../../include/Browser.h
	$(CC) $(selection_CFLAGS) -o $(OBJDIR)selection.o -c selection.c

$(OBJDIR)subversion.o: subversion.c common.c ../../include/Browser.h
	$(CC) $(subversion_CFLAGS) -o $(OBJDIR)subversion.o -c subversion.c

$(OBJDIR)template.o: template.c ../../include/Browser.h
	$(CC) $(template_CFLAGS) -o $(OBJDIR)template.o -c template.c

$(OBJDIR)trash.o: trash.c ../../include/Browser.h
	$(CC) $(trash_CFLAGS) -o $(OBJDIR)trash.o -c trash.c

$(OBJDIR)undelete.o: undelete.c ../../include/Browser.h
	$(CC) $(undelete_CFLAGS) -o $(OBJDIR)undelete.o -c undelete.c

$(OBJDIR)volumes.o: volumes.c ../../include/Browser.h
	$(CC) $(volumes_CFLAGS) -o $(OBJDIR)volumes.o -c volumes.c

clean:
	$(RM) -- $(backup_OBJS) $(cvs_OBJS) $(dirtree_OBJS) $(favorites_OBJS) $(git_OBJS) $(make_OBJS) $(preview_OBJS) $(properties_OBJS) $(selection_OBJS) $(subversion_OBJS) $(template_OBJS) $(trash_OBJS) $(undelete_OBJS) $(volumes_OBJS)

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)cvs.so $(DESTDIR)$(LIBDIR)/Browser/plugins/cvs.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)dirtree.so $(DESTDIR)$(LIBDIR)/Browser/plugins/dirtree.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)favorites.so $(DESTDIR)$(LIBDIR)/Browser/plugins/favorites.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)git.so $(DESTDIR)$(LIBDIR)/Browser/plugins/git.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)make.so $(DESTDIR)$(LIBDIR)/Browser/plugins/make.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)preview.so $(DESTDIR)$(LIBDIR)/Browser/plugins/preview.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)properties.so $(DESTDIR)$(LIBDIR)/Browser/plugins/properties.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)selection.so $(DESTDIR)$(LIBDIR)/Browser/plugins/selection.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)subversion.so $(DESTDIR)$(LIBDIR)/Browser/plugins/subversion.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)trash.so $(DESTDIR)$(LIBDIR)/Browser/plugins/trash.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Browser/plugins
	$(INSTALL) -m 0755 $(OBJDIR)volumes.so $(DESTDIR)$(LIBDIR)/Browser/plugins/volumes.so

uninstall:
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/cvs.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/dirtree.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/favorites.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/git.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/make.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/preview.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/properties.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/selection.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/subversion.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/trash.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Browser/plugins/volumes.so

.PHONY: all clean distclean install uninstall
