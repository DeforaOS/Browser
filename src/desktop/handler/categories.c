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



/* DesktopHandlerCategories */
/* private */
/* variables */
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
static void _desktophandler_categories_init(DesktopHandler * handler);
static void _desktophandler_categories_destroy(DesktopHandler * handler);
static void _desktophandler_categories_popup(DesktopHandler * handler,
		XButtonEvent * xbev);
static void _desktophandler_categories_refresh(DesktopHandler * handler);


/* functions */
/* desktophandler_categories_init */
static void _categories_init_on_back(Desktop * desktop, gpointer data);

static void _desktophandler_categories_init(DesktopHandler * handler)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;
	unsigned int size;

	handler->u.categories.refresh_dir = NULL;
	handler->u.categories.refresh_mtime = 0;
	handler->u.categories.refresh_source = 0;
	handler->u.categories.apps = NULL;
	desktop_set_alignment(handler->desktop, DESKTOP_ALIGNMENT_HORIZONTAL);
	if((desktopicon = desktopicon_new(handler->desktop, _("Back"), NULL))
			== NULL)
	{
		desktop_serror(handler->desktop, _("Back"), 1);
		return;
	}
	desktopicon_set_callback(desktopicon, _categories_init_on_back, NULL);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	desktop_get_icon_size(handler->desktop, NULL, NULL, &size);
	icon = gtk_icon_theme_load_icon(desktop_get_theme(handler->desktop),
			"back", size, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	desktop_icon_add(handler->desktop, desktopicon, FALSE);
}

static void _categories_init_on_back(Desktop * desktop, gpointer data)
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
	desktop_icons_cleanup(handler->desktop, TRUE);
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
	DesktopHandler * handler;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, _(dc->name));
#endif
	handler = desktop_get_handler(desktop);
	desktophandler_set_icons(handler, DESKTOP_ICONS_APPLICATIONS);
	/* FIXME really need an extra argument for applications (category) */
	handler->u.applications.category = dc;
	desktop_refresh(desktop);
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
