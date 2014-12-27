PACKAGE	= Browser
VERSION	= 0.4.13
SUBDIRS	= data doc include po src src/plugins tests tools
RM	= rm -f
LN	= ln -f
TAR	= tar
MKDIR	= mkdir -m 0755 -p


all: subdirs

subdirs:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE)) || exit; done

clean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) clean) || exit; done

distclean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) distclean) || exit; done

dist:
	$(RM) -r -- $(OBJDIR)$(PACKAGE)-$(VERSION)
	$(LN) -s -- "$$PWD" $(OBJDIR)$(PACKAGE)-$(VERSION)
	@cd $(OBJDIR). && $(TAR) -czvf $(OBJDIR)$(PACKAGE)-$(VERSION).tar.gz -- \
		$(PACKAGE)-$(VERSION)/data/Makefile \
		$(PACKAGE)-$(VERSION)/data/Browser.pc.in \
		$(PACKAGE)-$(VERSION)/data/deforaos-browser.desktop \
		$(PACKAGE)-$(VERSION)/data/deforaos-desktop-settings.desktop \
		$(PACKAGE)-$(VERSION)/data/pkgconfig.sh \
		$(PACKAGE)-$(VERSION)/data/project.conf \
		$(PACKAGE)-$(VERSION)/data/16x16/Makefile \
		$(PACKAGE)-$(VERSION)/data/16x16/browser-view-details.png \
		$(PACKAGE)-$(VERSION)/data/16x16/browser-view-icons.png \
		$(PACKAGE)-$(VERSION)/data/16x16/browser-view-list.png \
		$(PACKAGE)-$(VERSION)/data/16x16/project.conf \
		$(PACKAGE)-$(VERSION)/doc/Makefile \
		$(PACKAGE)-$(VERSION)/doc/browser.css.xml \
		$(PACKAGE)-$(VERSION)/doc/browser.xml \
		$(PACKAGE)-$(VERSION)/doc/copy.css.xml \
		$(PACKAGE)-$(VERSION)/doc/copy.xml \
		$(PACKAGE)-$(VERSION)/doc/delete.css.xml \
		$(PACKAGE)-$(VERSION)/doc/delete.xml \
		$(PACKAGE)-$(VERSION)/doc/desktop.css.xml \
		$(PACKAGE)-$(VERSION)/doc/desktop.xml \
		$(PACKAGE)-$(VERSION)/doc/desktopctl.css.xml \
		$(PACKAGE)-$(VERSION)/doc/desktopctl.xml \
		$(PACKAGE)-$(VERSION)/doc/docbook.sh \
		$(PACKAGE)-$(VERSION)/doc/manual.css.xml \
		$(PACKAGE)-$(VERSION)/doc/mime.conf \
		$(PACKAGE)-$(VERSION)/doc/move.css.xml \
		$(PACKAGE)-$(VERSION)/doc/move.xml \
		$(PACKAGE)-$(VERSION)/doc/open.css.xml \
		$(PACKAGE)-$(VERSION)/doc/open.xml \
		$(PACKAGE)-$(VERSION)/doc/properties.css.xml \
		$(PACKAGE)-$(VERSION)/doc/properties.xml \
		$(PACKAGE)-$(VERSION)/doc/view.css.xml \
		$(PACKAGE)-$(VERSION)/doc/view.xml \
		$(PACKAGE)-$(VERSION)/doc/project.conf \
		$(PACKAGE)-$(VERSION)/include/Browser.h \
		$(PACKAGE)-$(VERSION)/include/Makefile \
		$(PACKAGE)-$(VERSION)/include/project.conf \
		$(PACKAGE)-$(VERSION)/include/Browser/desktop.h \
		$(PACKAGE)-$(VERSION)/include/Browser/plugin.h \
		$(PACKAGE)-$(VERSION)/include/Browser/vfs.h \
		$(PACKAGE)-$(VERSION)/include/Browser/Makefile \
		$(PACKAGE)-$(VERSION)/include/Browser/project.conf \
		$(PACKAGE)-$(VERSION)/po/Makefile \
		$(PACKAGE)-$(VERSION)/po/gettext.sh \
		$(PACKAGE)-$(VERSION)/po/POTFILES \
		$(PACKAGE)-$(VERSION)/po/de.po \
		$(PACKAGE)-$(VERSION)/po/es.po \
		$(PACKAGE)-$(VERSION)/po/fr.po \
		$(PACKAGE)-$(VERSION)/po/it.po \
		$(PACKAGE)-$(VERSION)/po/project.conf \
		$(PACKAGE)-$(VERSION)/src/vfs.c \
		$(PACKAGE)-$(VERSION)/src/browser.c \
		$(PACKAGE)-$(VERSION)/src/callbacks.c \
		$(PACKAGE)-$(VERSION)/src/main.c \
		$(PACKAGE)-$(VERSION)/src/copy.c \
		$(PACKAGE)-$(VERSION)/src/delete.c \
		$(PACKAGE)-$(VERSION)/src/desktop.c \
		$(PACKAGE)-$(VERSION)/src/desktopicon.c \
		$(PACKAGE)-$(VERSION)/src/desktopctl.c \
		$(PACKAGE)-$(VERSION)/src/move.c \
		$(PACKAGE)-$(VERSION)/src/open.c \
		$(PACKAGE)-$(VERSION)/src/properties.c \
		$(PACKAGE)-$(VERSION)/src/view.c \
		$(PACKAGE)-$(VERSION)/src/Makefile \
		$(PACKAGE)-$(VERSION)/src/browser.h \
		$(PACKAGE)-$(VERSION)/src/callbacks.h \
		$(PACKAGE)-$(VERSION)/src/common.c \
		$(PACKAGE)-$(VERSION)/src/desktop.h \
		$(PACKAGE)-$(VERSION)/src/desktopicon.h \
		$(PACKAGE)-$(VERSION)/src/project.conf \
		$(PACKAGE)-$(VERSION)/src/plugins/backup.c \
		$(PACKAGE)-$(VERSION)/src/plugins/cvs.c \
		$(PACKAGE)-$(VERSION)/src/plugins/dirtree.c \
		$(PACKAGE)-$(VERSION)/src/plugins/favorites.c \
		$(PACKAGE)-$(VERSION)/src/plugins/git.c \
		$(PACKAGE)-$(VERSION)/src/plugins/make.c \
		$(PACKAGE)-$(VERSION)/src/plugins/preview.c \
		$(PACKAGE)-$(VERSION)/src/plugins/properties.c \
		$(PACKAGE)-$(VERSION)/src/plugins/selection.c \
		$(PACKAGE)-$(VERSION)/src/plugins/subversion.c \
		$(PACKAGE)-$(VERSION)/src/plugins/template.c \
		$(PACKAGE)-$(VERSION)/src/plugins/trash.c \
		$(PACKAGE)-$(VERSION)/src/plugins/volumes.c \
		$(PACKAGE)-$(VERSION)/src/plugins/Makefile \
		$(PACKAGE)-$(VERSION)/src/plugins/common.c \
		$(PACKAGE)-$(VERSION)/src/plugins/project.conf \
		$(PACKAGE)-$(VERSION)/tests/plugins.c \
		$(PACKAGE)-$(VERSION)/tests/Makefile \
		$(PACKAGE)-$(VERSION)/tests/tests.sh \
		$(PACKAGE)-$(VERSION)/tests/project.conf \
		$(PACKAGE)-$(VERSION)/tools/dirtree.c \
		$(PACKAGE)-$(VERSION)/tools/iconlist.c \
		$(PACKAGE)-$(VERSION)/tools/Makefile \
		$(PACKAGE)-$(VERSION)/tools/project.conf \
		$(PACKAGE)-$(VERSION)/Makefile \
		$(PACKAGE)-$(VERSION)/COPYING \
		$(PACKAGE)-$(VERSION)/TODO \
		$(PACKAGE)-$(VERSION)/config.h \
		$(PACKAGE)-$(VERSION)/config.sh \
		$(PACKAGE)-$(VERSION)/project.conf
	$(RM) -- $(OBJDIR)$(PACKAGE)-$(VERSION)

distcheck: dist
	$(TAR) -xzvf $(OBJDIR)$(PACKAGE)-$(VERSION).tar.gz
	$(MKDIR) -- $(PACKAGE)-$(VERSION)/objdir
	$(MKDIR) -- $(PACKAGE)-$(VERSION)/destdir
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) OBJDIR="$$PWD/objdir/")
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) OBJDIR="$$PWD/objdir/" DESTDIR="$$PWD/destdir" install)
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) OBJDIR="$$PWD/objdir/" DESTDIR="$$PWD/destdir" uninstall)
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) OBJDIR="$$PWD/objdir/" distclean)
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) dist)
	$(RM) -r -- $(PACKAGE)-$(VERSION)

install:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) install) || exit; done

uninstall:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) uninstall) || exit; done

.PHONY: all subdirs clean distclean dist distcheck install uninstall
