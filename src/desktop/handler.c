/* $Id$ */
/* Copyright (c) 2020 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Browser */
/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ITS AUTHORS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */



#include <sys/stat.h>
#include <fcntl.h>
#ifdef DEBUG
# include <stdio.h>
#endif
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <System.h>
#include "Browser/vfs.h"
#include "desktop.h"
#include "desktopicon.h"
#include "desktopiconwindow.h"
#include "handler.h"
#define _(string) gettext(string)
#define N_(string) string

#define COMMON_SYMLINK
#include "../common.c"


/* DesktopHandler */
/* private */
/* types */
typedef struct _DesktopCategory
{
	gboolean show;
	char const * category;
	char const * name;
	char const * icon;
} DesktopCategory;

struct _DesktopHandler
{
	Desktop * desktop;
	DesktopIcons icons;
	union
	{
		struct
		{
			char * path;
			DIR * refresh_dir;
			time_t refresh_mtime;
			guint refresh_source;
			gboolean show_hidden;
			GtkWidget * menu;
		} files;
		struct
		{
			DIR * refresh_dir;
			time_t refresh_mtime;
			guint refresh_source;
			DesktopCategory * category;
			GSList * apps;
		} applications;
		struct
		{
			DIR * refresh_dir;
			time_t refresh_mtime;
			guint refresh_source;
			GSList * apps;
		} categories;
	} u;
};


/* constants */
#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif

#define DESKTOP			".desktop"

#define IDLE_LOOP_ICON_CNT	16	/* number of icons added in a loop */

static DesktopCategory _desktop_categories[] =
{
	{ FALSE, "Audio",	N_("Audio"),	"gnome-mime-audio",	},
	{ FALSE, "Development",	N_("Development"),"applications-development"},
	{ FALSE, "Education",	N_("Education"),"applications-science"	},
	{ FALSE, "Game",	N_("Games"),	"applications-games"	},
	{ FALSE, "Graphics",	N_("Graphics"),	"applications-graphics"	},
	{ FALSE, "AudioVideo",	N_("Multimedia"),"applications-multimedia"},
	{ FALSE, "Network",	N_("Network"),	"applications-internet" },
	{ FALSE, "Office",	N_("Office"),	"applications-office"	},
	{ FALSE, "Settings",	N_("Settings"),	"gnome-settings"	},
	{ FALSE, "System",	N_("System"),	"applications-system"	},
	{ FALSE, "Utility",	N_("Utilities"),"applications-utilities"},
	{ FALSE, "Video",	N_("Video"),	"video"			}
};
static const size_t _desktop_categories_cnt = sizeof(_desktop_categories)
	/ sizeof(*_desktop_categories);


/* prototypes */
static void _desktophandler_applications_init(DesktopHandler * handler);
static void _desktophandler_applications_destroy(DesktopHandler * handler);
static void _desktophandler_applications_popup(DesktopHandler * handler,
		XButtonEvent * xbev);
static void _desktophandler_applications_refresh(DesktopHandler * handler);

static void _desktophandler_categories_init(DesktopHandler * handler);
static void _desktophandler_categories_destroy(DesktopHandler * handler);
static void _desktophandler_categories_popup(DesktopHandler * handler,
		XButtonEvent * xbev);
static void _desktophandler_categories_refresh(DesktopHandler * handler);

static void _desktophandler_files_init(DesktopHandler * handler);
static void _desktophandler_files_destroy(DesktopHandler * handler);
static void _desktophandler_files_popup(DesktopHandler * handler,
		XButtonEvent * xbev);
static void _desktophandler_files_refresh(DesktopHandler * handler);

static void _desktophandler_homescreen_init(DesktopHandler * handler);
static void _desktophandler_homescreen_destroy(DesktopHandler * handler);
static void _desktophandler_homescreen_popup(DesktopHandler * handler,
		XButtonEvent * xbev);
static void _desktophandler_homescreen_refresh(DesktopHandler * handler);


/* protected */
/* functions */
/* desktophandler_new */
DesktopHandler * desktophandler_new(Desktop * desktop, DesktopIcons icons)
{
	DesktopHandler * handler;

	if((handler = object_new(sizeof(*handler))) == NULL)
		return NULL;
	handler->desktop = desktop;
	handler->icons = DESKTOP_ICONS_NONE;
	desktophandler_set_icons(handler, icons);
	return handler;
}


/* desktophandler_delete */
void desktophandler_delete(DesktopHandler * handler)
{
	desktophandler_set_icons(handler, DESKTOP_ICONS_NONE);
	object_delete(handler);
}


/* accessors */
static void _set_icons_destroy(DesktopHandler * handler);
static void _set_icons_init(DesktopHandler * handler);

void desktophandler_set_icons(DesktopHandler * handler, DesktopIcons icons)
{
	if(handler->icons != icons)
	{
		desktop_cleanup(handler->desktop);
		_set_icons_destroy(handler);
		handler->icons = icons;
		_set_icons_init(handler);
	}
}

static void _set_icons_destroy(DesktopHandler * handler)
{
	switch(handler->icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_desktophandler_applications_destroy(handler);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_desktophandler_categories_destroy(handler);
			break;
		case DESKTOP_ICONS_FILES:
			_desktophandler_files_destroy(handler);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_desktophandler_homescreen_destroy(handler);
			break;
		case DESKTOP_ICONS_NONE:
			break;
	}
}

static void _set_icons_init(DesktopHandler * handler)
{
	switch(handler->icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_desktophandler_applications_init(handler);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_desktophandler_categories_init(handler);
			break;
		case DESKTOP_ICONS_FILES:
			_desktophandler_files_init(handler);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_desktophandler_homescreen_init(handler);
			break;
		case DESKTOP_ICONS_NONE:
			break;
	}
}


/* desktophandler_popup */
void desktophandler_popup(DesktopHandler * handler, XButtonEvent * xbev)
{
	switch(handler->icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_desktophandler_applications_popup(handler, xbev);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_desktophandler_categories_popup(handler, xbev);
			break;
		case DESKTOP_ICONS_FILES:
			_desktophandler_files_popup(handler, xbev);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_desktophandler_homescreen_popup(handler, xbev);
			break;
		case DESKTOP_ICONS_NONE:
			break;
	}
}


/* desktophandler_refresh */
void desktophandler_refresh(DesktopHandler * handler)
{
	switch(handler->icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_desktophandler_applications_refresh(handler);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_desktophandler_categories_refresh(handler);
			break;
		case DESKTOP_ICONS_FILES:
			_desktophandler_files_refresh(handler);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_desktophandler_homescreen_refresh(handler);
			break;
		case DESKTOP_ICONS_NONE:
			break;
	}
}


/* private */
/* functions */
/* desktophandler_applications_init */
static void _applications_init_on_back(Desktop * desktop, gpointer data);

static void _desktophandler_applications_init(DesktopHandler * handler)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;
	unsigned int size;

	handler->u.applications.refresh_dir = NULL;
	handler->u.applications.refresh_mtime = 0;
	handler->u.applications.refresh_source = 0;
	handler->u.applications.category = NULL;
	handler->u.applications.apps = NULL;
	if((desktopicon = desktopicon_new(handler->desktop, _("Back"), NULL))
			== NULL)
	{
		desktop_serror(handler->desktop, _("Back"), 1);
		return;
	}
	desktopicon_set_callback(desktopicon, _applications_init_on_back, NULL);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	desktop_get_icon_size(handler->desktop, NULL, NULL, &size);
	icon = gtk_icon_theme_load_icon(desktop_get_theme(handler->desktop),
			"back", size, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	desktop_icon_add(handler->desktop, desktopicon, FALSE);
}

static void _applications_init_on_back(Desktop * desktop, gpointer data)
{
	(void) data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	desktop_set_icons(desktop, DESKTOP_ICONS_HOMESCREEN);
}


/* desktophandler_applications_destroy */
static void _desktophandler_applications_destroy(DesktopHandler * handler)
{
	if(handler->u.applications.refresh_source != 0)
		g_source_remove(handler->u.applications.refresh_source);
	if(handler->u.applications.refresh_dir != NULL)
		browser_vfs_closedir(handler->u.applications.refresh_dir);
	g_slist_foreach(handler->u.applications.apps, (GFunc)mimehandler_delete,
			NULL);
	g_slist_free(handler->u.applications.apps);
}


/* desktophandler_applications_popup */
static void _desktophandler_applications_popup(DesktopHandler * handler,
		XButtonEvent * xbev)
{
	(void) handler;
	(void) xbev;
}


/* desktophandler_applications_refresh */
static gboolean _desktophandler_applications_on_refresh(gpointer data);
static gboolean _applications_on_refresh_done(DesktopHandler * handler);
static void _applications_on_refresh_done_applications(
		DesktopHandler * handler);
static int _applications_on_refresh_loop(DesktopHandler * handler);
static void _applications_on_refresh_loop_path(DesktopHandler * handler,
		char const * path, char const * apppath);
static void _applications_on_refresh_loop_xdg(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath));
static void _applications_on_refresh_loop_xdg_home(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath));
static void _applications_on_refresh_loop_xdg_path(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath), char const * path);
static gint _applications_apps_compare(gconstpointer a, gconstpointer b);

static void _desktophandler_applications_refresh(DesktopHandler * handler)
{
	g_slist_foreach(handler->u.applications.apps, (GFunc)mimehandler_delete,
			NULL);
	g_slist_free(handler->u.applications.apps);
	handler->u.applications.apps = NULL;
	handler->u.applications.refresh_source = g_idle_add(
			_desktophandler_applications_on_refresh, handler);
}

static gboolean _desktophandler_applications_on_refresh(gpointer data)
{
	DesktopHandler * handler = data;
	unsigned int i;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	/* FIXME upon deletion one icon too many is removed */
	for(i = 0; i < IDLE_LOOP_ICON_CNT
			&& _applications_on_refresh_loop(handler) == 0; i++);
	if(i == IDLE_LOOP_ICON_CNT)
		return TRUE;
	return _applications_on_refresh_done(handler);
}

static gboolean _applications_on_refresh_done(DesktopHandler * handler)
{
	handler->u.applications.refresh_source = 0;
	_applications_on_refresh_done_applications(handler);
	desktop_cleanup(handler->desktop);
	desktop_icons_align(handler->desktop);
	return FALSE;
}

static void _applications_on_refresh_done_applications(DesktopHandler * handler)
{
	GSList * p;
	MimeHandler * mime;
	DesktopCategory * dc = handler->u.applications.category;
	String const ** categories;
	size_t i;
	String const * filename;
	DesktopIcon * icon;

	for(p = handler->u.applications.apps; p != NULL; p = p->next)
	{
		mime = p->data;
		if(dc != NULL)
		{
			if((categories = mimehandler_get_categories(mime))
					== NULL || categories[0] == NULL)
				continue;
			for(i = 0; categories[i] != NULL; i++)
				if(string_compare(categories[i], dc->category)
						== 0)
					break;
			if(categories[i] == NULL)
				continue;
		}
		filename = mimehandler_get_filename(mime);
		/* FIXME keep track of the datadir */
		if((icon = desktopicon_new_application(handler->desktop,
						filename, NULL)) == NULL)
			continue;
		desktop_icon_add(handler->desktop, icon, FALSE);
	}
}

static int _applications_on_refresh_loop(DesktopHandler * handler)
{
	_applications_on_refresh_loop_xdg(handler,
			_applications_on_refresh_loop_path);
	return 1;
}

static void _applications_on_refresh_loop_path(DesktopHandler * handler,
		char const * path, char const * apppath)
{
	DIR * dir;
	struct stat st;
	size_t alen;
	struct dirent * de;
	size_t len;
	const char ext[] = ".desktop";
	char * name = NULL;
	char * p;
	MimeHandler * mime;
	(void) path;

	if((dir = browser_vfs_opendir(apppath, &st)) == NULL)
	{
		if(errno != ENOENT)
			desktop_perror(NULL, apppath, 1);
		return;
	}
	if(st.st_mtime > handler->u.applications.refresh_mtime)
		handler->u.applications.refresh_mtime = st.st_mtime;
	alen = strlen(apppath);
	while((de = browser_vfs_readdir(dir)) != NULL)
	{
		if(de->d_name[0] == '.')
			if(de->d_name[1] == '\0' || (de->d_name[1] == '.'
						&& de->d_name[2] == '\0'))
				continue;
		len = strlen(de->d_name);
		if(len < sizeof(ext))
			continue;
		if(strncmp(&de->d_name[len - sizeof(ext) + 1], ext,
					sizeof(ext)) != 0)
			continue;
		if((p = realloc(name, alen + len + 2)) == NULL)
		{
			desktop_perror(NULL, apppath, 1);
			continue;
		}
		name = p;
		snprintf(name, alen + len + 2, "%s/%s", apppath, de->d_name);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() name=\"%s\" path=\"%s\"\n",
				__func__, name, path);
#endif
		if((mime = mimehandler_new_load(name)) == NULL
				|| mimehandler_can_display(mime) == 0)
		{
			if(mime != NULL)
				mimehandler_delete(mime);
			else
				desktop_serror(NULL, NULL, 1);
			continue;
		}
		handler->u.applications.apps = g_slist_insert_sorted(
				handler->u.applications.apps, mime,
				_applications_apps_compare);
	}
	free(name);
	browser_vfs_closedir(dir);
}

static void _applications_on_refresh_loop_xdg(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath))
{
	char const * path;
	char * p;
	size_t i;
	size_t j;

	if((path = getenv("XDG_DATA_DIRS")) == NULL || strlen(path) == 0)
		/* XXX check this at build-time instead */
		path = (strcmp(DATADIR, "/usr/local/share") == 0)
			? DATADIR ":/usr/share"
			: "/usr/local/share:" DATADIR ":/usr/share";
	if((p = strdup(path)) == NULL)
	{
		desktop_perror(NULL, NULL, 1);
		return;
	}
	for(i = 0, j = 0;; i++)
		if(p[i] == '\0')
		{
			_applications_on_refresh_loop_xdg_path(handler, callback,
					&p[j]);
			break;
		}
		else if(p[i] == ':')
		{
			p[i] = '\0';
			_applications_on_refresh_loop_xdg_path(handler, callback,
					&p[j]);
			j = i + 1;
		}
	free(p);
	_applications_on_refresh_loop_xdg_home(handler, callback);
}

static void _applications_on_refresh_loop_xdg_home(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath))
{
	char const fallback[] = ".local/share";
	char const * path;
	char const * homedir;
	size_t len;
	char * p;

	/* use $XDG_DATA_HOME if set and not empty */
	if((path = getenv("XDG_DATA_HOME")) != NULL && strlen(path) > 0)
	{
		_applications_on_refresh_loop_xdg_path(handler, callback, path);
		return;
	}
	/* fallback to "$HOME/.local/share" */
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	len = strlen(homedir) + 1 + sizeof(fallback);
	if((p = malloc(len)) == NULL)
	{
		desktop_perror(NULL, homedir, 1);
		return;
	}
	snprintf(p, len, "%s/%s", homedir, fallback);
	_applications_on_refresh_loop_xdg_path(handler, callback, p);
	free(p);
}

static void _applications_on_refresh_loop_xdg_path(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath), char const * path)
{
	const char applications[] = "/applications";
	char * apppath;

	if((apppath = string_new_append(path, applications, NULL)) == NULL)
		desktop_serror(NULL, path, 1);
	callback(handler, path, apppath);
	string_delete(apppath);
}

static gint _applications_apps_compare(gconstpointer a, gconstpointer b)
{
	MimeHandler * mha = (MimeHandler *)a;
	MimeHandler * mhb = (MimeHandler *)b;
	String const * mhas;
	String const * mhbs;

	if((mhas = mimehandler_get_generic_name(mha, 1)) == NULL)
		mhas = mimehandler_get_name(mha, 1);
	if((mhbs = mimehandler_get_generic_name(mhb, 1)) == NULL)
		mhbs = mimehandler_get_name(mhb, 1);
	return string_compare(mhas, mhbs);
}


/* desktophandler_categories_init */
static void _categories_init_set_homescreen(Desktop * desktop, gpointer data);

static void _desktophandler_categories_init(DesktopHandler * handler)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;
	unsigned int size;

	handler->u.categories.refresh_dir = NULL;
	handler->u.categories.refresh_mtime = 0;
	handler->u.categories.refresh_source = 0;
	handler->u.categories.apps = NULL;
	if((desktopicon = desktopicon_new(handler->desktop, _("Back"), NULL))
			== NULL)
	{
		desktop_serror(handler->desktop, _("Back"), 1);
		return;
	}
	desktopicon_set_callback(desktopicon, _categories_init_set_homescreen,
			NULL);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	desktop_get_icon_size(handler->desktop, NULL, NULL, &size);
	icon = gtk_icon_theme_load_icon(desktop_get_theme(handler->desktop),
			"back", size, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	desktop_icon_add(handler->desktop, desktopicon, FALSE);
}

static void _categories_init_set_homescreen(Desktop * desktop, gpointer data)
{
	(void) data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	desktop_set_icons(desktop, DESKTOP_ICONS_HOMESCREEN);
}


/* desktophandler_categories_destroy */
static void _desktophandler_categories_destroy(DesktopHandler * handler)
{
	if(handler->u.categories.refresh_source != 0)
		g_source_remove(handler->u.categories.refresh_source);
	if(handler->u.categories.refresh_dir != NULL)
		browser_vfs_closedir(handler->u.categories.refresh_dir);
	g_slist_foreach(handler->u.categories.apps, (GFunc)mimehandler_delete,
			NULL);
	g_slist_free(handler->u.categories.apps);
}


/* desktophandler_categories_popup */
static void _desktophandler_categories_popup(DesktopHandler * handler,
		XButtonEvent * xbev)
{
	(void) handler;
	(void) xbev;
}


/* desktophandler_categories_refresh */
static gboolean _desktophandler_categories_on_refresh(gpointer data);
static gboolean _categories_on_refresh_done(DesktopHandler * handler);
static void _categories_on_refresh_done_categories(DesktopHandler * handler);
static void _categories_on_refresh_done_categories_open(Desktop * desktop,
		gpointer data);
static int _categories_on_refresh_loop(DesktopHandler * handler);
static void _categories_on_refresh_loop_path(DesktopHandler * handler,
		char const * path, char const * apppath);
static void _categories_on_refresh_loop_xdg(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath));
static void _categories_on_refresh_loop_xdg_home(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath));
static void _categories_on_refresh_loop_xdg_path(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath), char const * path);
static gint _categories_apps_compare(gconstpointer a, gconstpointer b);

static void _desktophandler_categories_refresh(DesktopHandler * handler)
{
	size_t i;

	for(i = 0; i < _desktop_categories_cnt; i++)
		_desktop_categories[i].show = FALSE;
	g_slist_foreach(handler->u.categories.apps, (GFunc)mimehandler_delete,
			NULL);
	g_slist_free(handler->u.categories.apps);
	handler->u.categories.apps = NULL;
	handler->u.categories.refresh_source = g_idle_add(
			_desktophandler_categories_on_refresh, handler);
}

static gboolean _desktophandler_categories_on_refresh(gpointer data)
{
	DesktopHandler * handler = data;
	unsigned int i;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	/* FIXME upon deletion one icon too many is removed */
	for(i = 0; i < IDLE_LOOP_ICON_CNT
			&& _categories_on_refresh_loop(handler) == 0; i++);
	if(i == IDLE_LOOP_ICON_CNT)
		return TRUE;
	return _categories_on_refresh_done(handler);
}

static gboolean _categories_on_refresh_done(DesktopHandler * handler)
{
	handler->u.categories.refresh_source = 0;
	_categories_on_refresh_done_categories(handler);
	desktop_cleanup(handler->desktop);
	desktop_icons_align(handler->desktop);
	return FALSE;
}

static void _categories_on_refresh_done_categories(DesktopHandler * handler)
{
	GSList * p;
	MimeHandler * mime;
	DesktopCategory * dc;
	String const ** categories;
	size_t i;
	size_t j;
	String const * filename;
	DesktopIcon * icon;

	for(p = handler->u.categories.apps; p != NULL; p = p->next)
	{
		mime = p->data;
		filename = mimehandler_get_filename(mime);
		if((categories = mimehandler_get_categories(mime)) == NULL
				|| categories[0] == NULL)
		{
			/* FIXME keep track of the datadir */
			if((icon = desktopicon_new_application(handler->desktop,
							filename, NULL))
					!= NULL)
				desktop_icon_add(handler->desktop, icon, FALSE);
			continue;
		}
		for(i = 0; i < _desktop_categories_cnt; i++)
		{
			dc = &_desktop_categories[i];
			for(j = 0; categories[j] != NULL; j++)
				if(string_compare(categories[j], dc->category)
						== 0)
					break;
			if(categories[j] != NULL)
				break;
		}
		if(i == _desktop_categories_cnt)
		{
			/* FIXME keep track of the datadir */
			if((icon = desktopicon_new_application(handler->desktop,
							filename, NULL))
					!= NULL)
				desktop_icon_add(handler->desktop, icon, FALSE);
			continue;
		}
		if(dc->show == TRUE)
			continue;
		dc->show = TRUE;
		if((icon = desktopicon_new_category(handler->desktop,
						_(dc->name), dc->icon)) == NULL)
			continue;
		desktopicon_set_callback(icon,
				_categories_on_refresh_done_categories_open,
				dc);
		desktop_icon_add(handler->desktop, icon, FALSE);
	}
}

static void _categories_on_refresh_done_categories_open(Desktop * desktop,
		gpointer data)
{
	DesktopCategory * dc = data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, _(dc->name));
#endif
	/* FIXME need an extra argument for applications (category) */
	desktop_set_icons(desktop, DESKTOP_ICONS_APPLICATIONS);
}

static int _categories_on_refresh_loop(DesktopHandler * handler)
{
	_categories_on_refresh_loop_xdg(handler,
			_categories_on_refresh_loop_path);
	return 1;
}

static void _categories_on_refresh_loop_path(DesktopHandler * handler,
		char const * path, char const * apppath)
{
	DIR * dir;
	struct stat st;
	size_t alen;
	struct dirent * de;
	size_t len;
	const char ext[] = ".desktop";
	char * name = NULL;
	char * p;
	MimeHandler * mime;
	(void) path;

	if((dir = browser_vfs_opendir(apppath, &st)) == NULL)
	{
		if(errno != ENOENT)
			desktop_perror(NULL, apppath, 1);
		return;
	}
	if(st.st_mtime > handler->u.categories.refresh_mtime)
		handler->u.categories.refresh_mtime = st.st_mtime;
	alen = strlen(apppath);
	while((de = browser_vfs_readdir(dir)) != NULL)
	{
		if(de->d_name[0] == '.')
			if(de->d_name[1] == '\0' || (de->d_name[1] == '.'
						&& de->d_name[2] == '\0'))
				continue;
		len = strlen(de->d_name);
		if(len < sizeof(ext))
			continue;
		if(strncmp(&de->d_name[len - sizeof(ext) + 1], ext,
					sizeof(ext)) != 0)
			continue;
		if((p = realloc(name, alen + len + 2)) == NULL)
		{
			desktop_perror(NULL, apppath, 1);
			continue;
		}
		name = p;
		snprintf(name, alen + len + 2, "%s/%s", apppath, de->d_name);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() name=\"%s\" path=\"%s\"\n",
				__func__, name, path);
#endif
		if((mime = mimehandler_new_load(name)) == NULL
				|| mimehandler_can_display(mime) == 0)
		{
			if(mime != NULL)
				mimehandler_delete(mime);
			else
				desktop_serror(NULL, NULL, 1);
			continue;
		}
		handler->u.categories.apps = g_slist_insert_sorted(
				handler->u.categories.apps, mime,
				_categories_apps_compare);
	}
	free(name);
	browser_vfs_closedir(dir);
}

static void _categories_on_refresh_loop_xdg(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath))
{
	char const * path;
	char * p;
	size_t i;
	size_t j;

	if((path = getenv("XDG_DATA_DIRS")) == NULL || strlen(path) == 0)
		/* XXX check this at build-time instead */
		path = (strcmp(DATADIR, "/usr/local/share") == 0)
			? DATADIR ":/usr/share"
			: "/usr/local/share:" DATADIR ":/usr/share";
	if((p = strdup(path)) == NULL)
	{
		desktop_perror(NULL, NULL, 1);
		return;
	}
	for(i = 0, j = 0;; i++)
		if(p[i] == '\0')
		{
			_categories_on_refresh_loop_xdg_path(handler, callback,
					&p[j]);
			break;
		}
		else if(p[i] == ':')
		{
			p[i] = '\0';
			_categories_on_refresh_loop_xdg_path(handler, callback,
					&p[j]);
			j = i + 1;
		}
	free(p);
	_categories_on_refresh_loop_xdg_home(handler, callback);
}

static void _categories_on_refresh_loop_xdg_home(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath))
{
	char const fallback[] = ".local/share";
	char const * path;
	char const * homedir;
	size_t len;
	char * p;

	/* use $XDG_DATA_HOME if set and not empty */
	if((path = getenv("XDG_DATA_HOME")) != NULL && strlen(path) > 0)
	{
		_categories_on_refresh_loop_xdg_path(handler, callback, path);
		return;
	}
	/* fallback to "$HOME/.local/share" */
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	len = strlen(homedir) + 1 + sizeof(fallback);
	if((p = malloc(len)) == NULL)
	{
		desktop_perror(NULL, homedir, 1);
		return;
	}
	snprintf(p, len, "%s/%s", homedir, fallback);
	_categories_on_refresh_loop_xdg_path(handler, callback, p);
	free(p);
}

static void _categories_on_refresh_loop_xdg_path(DesktopHandler * handler,
		void (*callback)(DesktopHandler * handler, char const * path,
			char const * apppath), char const * path)
{
	const char applications[] = "/applications";
	char * apppath;

	if((apppath = string_new_append(path, applications, NULL)) == NULL)
		desktop_serror(NULL, path, 1);
	callback(handler, path, apppath);
	string_delete(apppath);
}

static gint _categories_apps_compare(gconstpointer a, gconstpointer b)
{
	MimeHandler * mha = (MimeHandler *)a;
	MimeHandler * mhb = (MimeHandler *)b;
	String const * mhas;
	String const * mhbs;

	if((mhas = mimehandler_get_generic_name(mha, 1)) == NULL)
		mhas = mimehandler_get_name(mha, 1);
	if((mhbs = mimehandler_get_generic_name(mhb, 1)) == NULL)
		mhbs = mimehandler_get_name(mhb, 1);
	return string_compare(mhas, mhbs);
}


/* desktophandler_files_init */
static int _files_init_add_home(DesktopHandler * handler);

static void _desktophandler_files_init(DesktopHandler * handler)
{
	char const * path;
	struct stat st;

	if((path = getenv("XDG_DESKTOP_DIR")) != NULL)
		handler->u.files.path = string_new(path);
	else
		handler->u.files.path = string_new_append(
				desktop_get_home(handler->desktop), "/",
				DESKTOP, NULL);
	handler->u.files.refresh_dir = NULL;
	handler->u.files.refresh_mtime = 0;
	handler->u.files.refresh_source = 0;
	handler->u.files.menu = NULL;
	/* FIXME let it be configured again */
	handler->u.files.show_hidden = FALSE;
	/* check for errors */
	if(handler->u.files.path == NULL)
	{
		desktop_error(handler->desktop, NULL, error_get(NULL), 1);
		return;
	}
	_files_init_add_home(handler);
	if(browser_vfs_stat(handler->u.files.path, &st) == 0)
	{
		if(!S_ISDIR(st.st_mode))
		{
			desktop_error(NULL, handler->u.files.path,
					strerror(ENOTDIR), 1);
			return;
		}
	}
	else if(errno != ENOENT)
		desktop_perror(NULL, handler->u.files.path, 1);
}

static int _files_init_add_home(DesktopHandler * handler)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;
	unsigned int size;

	if((desktopicon = desktopicon_new(handler->desktop, _("Home"),
					desktop_get_home(handler->desktop)))
				== NULL)
		return -desktop_serror(handler->desktop, _("Home"), 1);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	desktop_get_icon_size(handler->desktop, NULL, NULL, &size);
	icon = gtk_icon_theme_load_icon(desktop_get_theme(handler->desktop),
			"gnome-home", size, 0, NULL);
	if(icon == NULL)
		icon = gtk_icon_theme_load_icon(desktop_get_theme(
					handler->desktop), "gnome-fs-home",
				size, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	/* XXX report errors */
	desktop_icon_add(handler->desktop, desktopicon, FALSE);
	return 0;
}


/* desktophandler_files_destroy */
static void _desktophandler_files_destroy(DesktopHandler * handler)
{
	if(handler->u.files.refresh_source != 0)
		g_source_remove(handler->u.files.refresh_source);
	if(handler->u.files.refresh_dir != NULL)
		browser_vfs_closedir(handler->u.files.refresh_dir);
	if(handler->u.files.menu != NULL)
		gtk_widget_destroy(handler->u.files.menu);
}


/* desktophandler_files_popup */
static void _on_popup_new_folder(gpointer data);
static void _on_popup_new_text_file(gpointer data);
static void _on_popup_paste(gpointer data);
static void _on_popup_preferences(gpointer data);
static void _on_popup_symlink(gpointer data);

static void _desktophandler_files_popup(DesktopHandler * handler,
		XButtonEvent * xbev)
{
	GtkWidget * menuitem;
	GtkWidget * submenu;
	GtkWidget * image;

	if((xbev == NULL || xbev->button != 3) && handler->u.files.menu != NULL)
	{
		gtk_widget_destroy(handler->u.files.menu);
		handler->u.files.menu = NULL;
	}
	handler->u.files.menu = gtk_menu_new();
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_New"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("document-new",
				GTK_ICON_SIZE_MENU));
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	/* submenu for new documents */
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Folder"));
	image = gtk_image_new_from_icon_name("folder-new", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_new_folder), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Symbolic link..."));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_symlink), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Text file"));
	image = gtk_image_new_from_icon_name("stock_new-text",
			GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_new_text_file), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	/* edition */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Paste"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("edit-paste",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_paste), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	/* preferences */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Preferences"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("gtk-preferences",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_preferences), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	gtk_widget_show_all(handler->u.files.menu);
	gtk_menu_popup(GTK_MENU(handler->u.files.menu), NULL, NULL, NULL, NULL,
			3, (xbev != NULL)
			? xbev->time : gtk_get_current_event_time());
}

static void _on_popup_new_folder(gpointer data)
{
	static char const newfolder[] = N_("New folder");
	DesktopHandler * handler = data;
	String * path;

	gtk_widget_destroy(handler->u.files.menu);
	handler->u.files.menu = NULL;
	if((path = string_new_append(handler->u.files.path, "/", newfolder,
					NULL)) == NULL)
	{
		desktop_serror(handler->desktop, newfolder, 0);
		return;
	}
	if(browser_vfs_mkdir(path, 0777) != 0)
		desktop_perror(handler->desktop, path, 0);
	string_delete(path);
}

static void _on_popup_new_text_file(gpointer data)
{
	static char const newtext[] = N_("New text file.txt");
	DesktopHandler * handler = data;
	String * path;
	int fd;

	gtk_widget_destroy(handler->u.files.menu);
	handler->u.files.menu = NULL;
	if((path = string_new_append(handler->u.files.path, "/", _(newtext),
					NULL)) == NULL)
	{
		desktop_serror(handler->desktop, _(newtext), 0);
		return;
	}
	if((fd = creat(path, 0666)) < 0)
		desktop_perror(handler->desktop, path, 0);
	else
		close(fd);
	string_delete(path);
}

static void _on_popup_paste(gpointer data)
{
	DesktopHandler * handler = data;

	/* FIXME implement */
	gtk_widget_destroy(handler->u.files.menu);
	handler->u.files.menu = NULL;
}

static void _on_popup_preferences(gpointer data)
{
	DesktopHandler * handler = data;

	desktop_show_preferences(handler->desktop);
}

static void _on_popup_symlink(gpointer data)
{
	DesktopHandler * handler = data;

	if(_common_symlink(desktop_get_window(handler->desktop),
				handler->u.files.path) != 0)
		desktop_perror(handler->desktop, handler->u.files.path, 0);
}


/* desktophandler_files_refresh */
static gboolean _desktophandler_files_on_refresh(gpointer data);
static gboolean _files_on_refresh_done(DesktopHandler * handler);
static gboolean _files_on_refresh_done_timeout(gpointer data);
static gboolean _files_on_refresh_loop(DesktopHandler * handler);
static int _files_on_refresh_loop_lookup(DesktopHandler * handler,
		char const * name);
static int _files_on_refresh_loop_opendir(DesktopHandler * handler);

static void _desktophandler_files_refresh(DesktopHandler * handler)
{
	if(handler->u.files.refresh_dir != NULL)
		browser_vfs_closedir(handler->u.files.refresh_dir);
	handler->u.files.refresh_dir = NULL;
	if(handler->u.files.path == NULL)
		return;
	handler->u.files.refresh_source = g_idle_add(
			_desktophandler_files_on_refresh, handler);
}

static gboolean _desktophandler_files_on_refresh(gpointer data)
{
	DesktopHandler * handler = data;
	unsigned int i;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	/* FIXME upon deletion one icon too many is removed */
	for(i = 0; i < IDLE_LOOP_ICON_CNT
			&& _files_on_refresh_loop(handler) == 0; i++);
	if(i == IDLE_LOOP_ICON_CNT)
		return TRUE;
	return _files_on_refresh_done(handler);
}

static gboolean _files_on_refresh_done(DesktopHandler * handler)
{
	desktop_cleanup(handler->desktop);
	if(handler->u.files.refresh_dir != NULL)
		browser_vfs_closedir(handler->u.files.refresh_dir);
	handler->u.files.refresh_dir = NULL;
	desktop_icons_align(handler->desktop);
	handler->u.files.refresh_source = g_timeout_add(1000,
			_files_on_refresh_done_timeout, handler);
	return FALSE;
}

static gboolean _files_on_refresh_done_timeout(gpointer data)
{
	DesktopHandler * handler = data;
	struct stat st;

	handler->u.files.refresh_source = 0;
	if(handler->u.files.path == NULL)
		return FALSE;
	if(browser_vfs_stat(handler->u.files.path, &st) != 0)
		return desktop_perror(NULL, handler->u.files.path, FALSE);
	if(st.st_mtime == handler->u.files.refresh_mtime)
		return TRUE;
	desktop_refresh(handler->desktop);
	return FALSE;
}

static int _files_on_refresh_loop(DesktopHandler * handler)
{
	const char ext[] = ".desktop";
	struct dirent * de;
	String * p;
	size_t len;
	gchar * q;
	DesktopIcon * desktopicon;
	GError * error = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(handler->u.files.refresh_dir == NULL
			&& _files_on_refresh_loop_opendir(handler) != 0)
		return -1;
	while((de = browser_vfs_readdir(handler->u.files.refresh_dir)) != NULL)
	{
		if(de->d_name[0] == '.')
		{
			if(de->d_name[1] == '\0')
				continue;
			if(de->d_name[1] == '.' && de->d_name[2] == '\0')
				continue;
			if(handler->u.files.show_hidden == 0)
				continue;
		}
		if(_files_on_refresh_loop_lookup(handler, de->d_name) == 1)
			continue;
		break;
	}
	if(de == NULL)
		return -1;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, de->d_name);
#endif
	if((p = string_new_append(handler->u.files.path, "/", de->d_name, NULL))
			== NULL)
		return -desktop_serror(handler->desktop, de->d_name, 1);
	/* detect desktop entries */
	if((len = strlen(de->d_name)) > sizeof(ext)
			&& strcmp(&de->d_name[len - sizeof(ext) + 1], ext) == 0)
		desktopicon = desktopicon_new_application(handler->desktop, p,
				handler->u.files.path);
	/* XXX not relative to the current folder */
	else if((q = g_filename_to_utf8(de->d_name, -1, NULL, NULL, &error))
			!= NULL)
	{
		desktopicon = desktopicon_new(handler->desktop, q, p);
		g_free(q);
	}
	else
	{
		desktop_error(NULL, NULL, error->message, 1);
		g_error_free(error);
		desktopicon = desktopicon_new(handler->desktop, de->d_name, p);
	}
	if(desktopicon != NULL)
		desktop_icon_add(handler->desktop, desktopicon, FALSE);
	string_delete(p);
	return 0;
}

static int _files_on_refresh_loop_lookup(DesktopHandler * handler,
		char const * name)
{
	DesktopIcon * icon;

	if((icon = desktop_icons_lookup(handler->desktop, name)) == NULL)
		return 0;
	desktopicon_set_updated(icon, TRUE);
	return 1;
}

static int _files_on_refresh_loop_opendir(DesktopHandler * handler)
{
	struct stat st;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__,
			handler->u.files.path);
#endif
	if((handler->u.files.refresh_dir = browser_vfs_opendir(
					handler->u.files.path, &st)) == NULL)
	{
		desktop_perror(NULL, handler->u.files.path, 1);
		handler->u.files.refresh_source = 0;
		return -1;
	}
	handler->u.files.refresh_mtime = st.st_mtime;
	return 0;
}


/* desktophandler_homescreen_init */
static int _homescreen_init_subdir(DesktopHandler * handler,
		char const * subdir, char const * name);
static void _homescreen_init_on_applications(Desktop * desktop, gpointer data);

static void _desktophandler_homescreen_init(DesktopHandler * handler)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;
	char const * paths[] =
	{
#ifdef EMBEDDED
		/* FIXME let this be configurable */
		"deforaos-phone-contacts",
		"deforaos-phone-dialer",
		"deforaos-phone-messages",
#endif
		NULL
	};
	char const ** p;
	unsigned int size;

	if((desktopicon = desktopicon_new(handler->desktop, _("Applications"),
					NULL)) == NULL)
	{
		desktop_serror(handler->desktop, _("Applications"), 1);
		return;
	}
	desktopicon_set_callback(desktopicon, _homescreen_init_on_applications,
			NULL);
	desktopicon_set_immutable(desktopicon, TRUE);
	desktop_get_icon_size(handler->desktop, NULL, NULL, &size);
	icon = gtk_icon_theme_load_icon(desktop_get_theme(handler->desktop),
			"gnome-applications", size, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	desktop_icon_add(handler->desktop, desktopicon, FALSE);
	for(p = paths; *p != NULL; p++)
		_homescreen_init_subdir(handler, DATADIR "/applications", *p);
}

static int _homescreen_init_subdir(DesktopHandler * handler,
		char const * subdir, char const * name)
{
	DesktopIcon * desktopicon;
	String * q;

	if((q = string_new_append(subdir, "/", name, ".desktop", NULL)) == NULL)
		return -desktop_error(NULL, name, error_get(NULL), 1);
	if(access(q, R_OK) == 0
			&& (desktopicon = desktopicon_new_application(
					handler->desktop, q, DATADIR)) != NULL)
		desktop_icon_add(handler->desktop, desktopicon, FALSE);
	string_delete(q);
	return 0;
}

static void _homescreen_init_on_applications(Desktop * desktop, gpointer data)
{
	(void) data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	desktop_set_icons(desktop, DESKTOP_ICONS_CATEGORIES);
}


/* desktophandler_homescreen_destroy */
static void _desktophandler_homescreen_destroy(DesktopHandler * handler)
{
	(void) handler;
}


/* desktophandler_homescreen_popup */
static void _desktophandler_homescreen_popup(DesktopHandler * handler,
		XButtonEvent * xbev)
{
	(void) handler;
	(void) xbev;
}


/* desktophandler_homescreen_refresh */
static void _desktophandler_homescreen_refresh(DesktopHandler * handler)
{
	(void) handler;
}
