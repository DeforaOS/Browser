/* $Id$ */
/* Copyright (c) 2007-2015 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Browser */
/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/* TODO:
 * - let the user define the desktop folder (possibly default to FDO's)
 * - track multiple selection on delete/properties
 * - avoid code duplication with DeforaOS Panel ("main" applet) */



#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <System.h>
#include "../include/Browser/desktop.h"
#include "../include/Browser/vfs.h"
#include "desktop.h"
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) string

#define COMMON_SYMLINK
#include "common.c"


/* constants */
#ifndef PROGNAME
# define PROGNAME	"desktop"
#endif
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* Desktop */
/* private */
/* types */
typedef struct _DesktopCategory DesktopCategory;

struct _Desktop
{
	DesktopPrefs prefs;
	PangoFontDescription * font;
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkRGBA background;
	GdkRGBA foreground;
#else
	GdkColor background;
	GdkColor foreground;
#endif

	/* workarea */
	GdkRectangle window;
	GdkRectangle workarea;

	/* icons */
	DesktopIcon ** icon;
	size_t icon_cnt;

	/* common */
	char * path;
	size_t path_cnt;
	DIR * refresh_dir;
	time_t refresh_mti;
	guint refresh_source;
	/* files */
	Mime * mime;
	char const * home;
	GdkPixbuf * file;
	GdkPixbuf * folder;
	gboolean show_hidden;
	/* applications */
	DesktopCategory * category;
	/* categories */
	GSList * apps;

	/* preferences */
	GtkWidget * pr_window;
	GtkWidget * pr_color;
	GtkWidget * pr_background;
	GtkWidget * pr_background_how;
	GtkWidget * pr_background_extend;
	GtkWidget * pr_ilayout;
	GtkWidget * pr_imonitor;
	GtkWidget * pr_ibcolor;
	GtkWidget * pr_ifcolor;
	GtkWidget * pr_ifont;
	GtkWidget * pr_monitors;
	GtkWidget * pr_monitors_res;
	GtkWidget * pr_monitors_size;

	/* internal */
	GdkScreen * screen;
	GdkDisplay * display;
	GdkWindow * root;
	GtkWidget * desktop;
	GdkWindow * back;
	GtkIconTheme * theme;
	GtkWidget * menu;
};

struct _DesktopCategory
{
	gboolean show;
	char const * category;
	char const * name;
	char const * icon;
};

typedef enum _DesktopHows
{
	DESKTOP_HOW_NONE = 0,
	DESKTOP_HOW_CENTERED,
	DESKTOP_HOW_SCALED,
	DESKTOP_HOW_SCALED_RATIO,
	DESKTOP_HOW_TILED
} DesktopHows;
#define DESKTOP_HOW_LAST	DESKTOP_HOW_TILED
#define DESKTOP_HOW_COUNT	(DESKTOP_HOW_LAST + 1)


/* constants */
#define DESKTOP			".desktop"
#define DESKTOPRC		".desktoprc"

static DesktopCategory _desktop_categories[] =
{
	{ FALSE, "Audio",	"Audio",	"gnome-mime-audio",	},
	{ FALSE, "Development",	"Development",	"applications-development"},
	{ FALSE, "Education",	"Education",	"applications-science"	},
	{ FALSE, "Game",	"Games",	"applications-games"	},
	{ FALSE, "Graphics",	"Graphics",	"applications-graphics"	},
	{ FALSE, "AudioVideo",	"Multimedia",	"applications-multimedia"},
	{ FALSE, "Network",	"Network",	"applications-internet" },
	{ FALSE, "Office",	"Office",	"applications-office"	},
	{ FALSE, "Settings",	"Settings",	"gnome-settings"	},
	{ FALSE, "System",	"System",	"applications-system"	},
	{ FALSE, "Utility",	"Utilities",	"applications-utilities"},
	{ FALSE, "Video",	"Video",	"video"			}
};
static const size_t _desktop_categories_cnt = sizeof(_desktop_categories)
	/ sizeof(*_desktop_categories);

static const char * _desktop_hows[DESKTOP_HOW_COUNT] =
{
	"none",
	"centered",
	"scaled",
	"scaled_ratio",
	"tiled"
};

static const char * _desktop_icons_config[DESKTOP_ICONS_COUNT] =
{
	"none", "applications", "categories", "files", "homescreen"
};

static const char * _desktop_icons[DESKTOP_ICONS_COUNT] =
{
	N_("Do not draw icons"),
	N_("Applications"),
	N_("Categories"),
	N_("Desktop contents"),
	N_("Home screen")
};


/* prototypes */
static int _desktop_error(Desktop * desktop, char const * message,
		char const * error, int ret);
static int _desktop_serror(Desktop * desktop, char const * message, int ret);

/* accessors */
static Config * _desktop_get_config(Desktop * desktop);
static int _desktop_get_workarea(Desktop * desktop);

/* useful */
#if GTK_CHECK_VERSION(3, 0, 0)
static void _desktop_draw_background(Desktop * desktop, GdkRGBA * color,
		char const * filename, DesktopHows how, gboolean extend);
#else
static void _desktop_draw_background(Desktop * desktop, GdkColor * color,
		char const * filename, DesktopHows how, gboolean extend);
#endif

static int _desktop_icon_add(Desktop * desktop, DesktopIcon * icon);
static int _desktop_icon_remove(Desktop * desktop, DesktopIcon * icon);

static void _desktop_show_preferences(Desktop * desktop);

/* callbacks */
static gboolean _desktop_on_refresh(gpointer data);


/* public */
/* functions */
/* desktop_new */
/* callbacks */
static void _new_events(Desktop * desktop, GdkWindow * window,
		GdkEventMask mask);
static void _new_icons(Desktop * desktop);
static int _on_message(void * data, uint32_t value1, uint32_t value2,
		uint32_t value3);
static void _on_popup(gpointer data);
static void _on_popup_event(gpointer data, XButtonEvent * xbev);
static void _on_realize(gpointer data);
static GdkFilterReturn _on_root_event(GdkXEvent * xevent, GdkEvent * event,
		gpointer data);

Desktop * desktop_new(DesktopPrefs * prefs)
{
	Desktop * desktop;
#if !GTK_CHECK_VERSION(2, 24, 0)
	gint depth;
#endif
	GdkEventMask mask = GDK_PROPERTY_CHANGE_MASK;

	if((desktop = object_new(sizeof(*desktop))) == NULL)
		return NULL;
	memset(desktop, 0, sizeof(*desktop));
	/* set default foreground to white */
	memset(&desktop->foreground, 0xff, sizeof(desktop->foreground));
	desktop->prefs.alignment = DESKTOP_ALIGNMENT_VERTICAL;
	desktop->prefs.icons = DESKTOP_ICONS_FILES;
	desktop->prefs.monitor = -1;
	if(prefs != NULL)
		desktop->prefs = *prefs;
	desktop->font = NULL;
	/* workarea */
	desktop->screen = gdk_screen_get_default();
	desktop->display = gdk_screen_get_display(desktop->screen);
	desktop->root = gdk_screen_get_root_window(desktop->screen);
	desktop->theme = gtk_icon_theme_get_default();
	desktop->menu = NULL;
	if((desktop->home = getenv("HOME")) == NULL
			&& (desktop->home = g_get_home_dir()) == NULL)
		desktop->home = "/";
	desktop_message_register(NULL, DESKTOP_CLIENT_MESSAGE, _on_message,
			desktop);
	/* query the root window */
#if GTK_CHECK_VERSION(2, 24, 0)
	gdk_window_get_position(desktop->root, &desktop->window.x,
			&desktop->window.y);
	desktop->window.width = gdk_window_get_width(desktop->root);
	desktop->window.height = gdk_window_get_height(desktop->root);
#else
	gdk_window_get_geometry(desktop->root, &desktop->window.x,
			&desktop->window.y, &desktop->window.width,
			&desktop->window.height, &depth);
#endif
	if(desktop->prefs.window)
	{
		/* create the desktop window */
		desktop->desktop = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_default_size(GTK_WINDOW(desktop->desktop),
				desktop->window.width, desktop->window.height);
		gtk_window_set_type_hint(GTK_WINDOW(desktop->desktop),
				GDK_WINDOW_TYPE_HINT_DESKTOP);
		/* support pop-up menus on the desktop window if enabled */
		if(desktop->prefs.popup)
			g_signal_connect_swapped(desktop->desktop, "popup-menu",
					G_CALLBACK(_on_popup), desktop);
		/* draw the icons and background when realized */
		g_signal_connect_swapped(desktop->desktop, "realize",
				G_CALLBACK(_on_realize), desktop);
		gtk_window_move(GTK_WINDOW(desktop->desktop), desktop->window.x,
				desktop->window.y);
		gtk_widget_show(desktop->desktop);
	}
	else
	{
		desktop->desktop = NULL;
		desktop->back = desktop->root;
		/* draw the icons and background when idle */
		desktop_reset(desktop);
		/* support pop-up menus on the root window if enabled */
		if(desktop->prefs.popup)
			mask |= GDK_BUTTON_PRESS_MASK;
	}
	/* manage events on the root window */
	_new_events(desktop, desktop->root, mask);
	/* load the default icons */
	_new_icons(desktop);
	return desktop;
}

static void _new_events(Desktop * desktop, GdkWindow * window,
		GdkEventMask mask)
{
	mask = gdk_window_get_events(window) | mask;
	gdk_window_set_events(window, mask);
	gdk_window_add_filter(window, _on_root_event, desktop);
}

static void _new_icons(Desktop * desktop)
{
	const char * file[] = { "gnome-fs-regular",
#if GTK_CHECK_VERSION(2, 6, 0)
		GTK_STOCK_FILE,
#endif
		GTK_STOCK_MISSING_IMAGE, NULL };
	const char * folder[] = { "gnome-fs-directory",
#if GTK_CHECK_VERSION(2, 6, 0)
		GTK_STOCK_DIRECTORY,
#endif
		GTK_STOCK_MISSING_IMAGE, NULL };
	char const ** p;

	for(p = file; *p != NULL && desktop->file == NULL; p++)
		desktop->file = gtk_icon_theme_load_icon(desktop->theme,
				*p, DESKTOPICON_ICON_SIZE, 0, NULL);
	for(p = folder; *p != NULL && desktop->folder == NULL; p++)
		desktop->folder = gtk_icon_theme_load_icon(desktop->theme, *p,
				DESKTOPICON_ICON_SIZE, 0, NULL);
}

static int _on_message(void * data, uint32_t value1, uint32_t value2,
		uint32_t value3)
{
	Desktop * desktop = data;
	DesktopMessage message;
	DesktopAlignment alignment;
	DesktopIcons icons;
	DesktopLayout layout;

	switch((message = value1))
	{
		case DESKTOP_MESSAGE_SET_ALIGNMENT:
			alignment = value2;
			desktop_set_alignment(desktop, alignment);
			break;
		case DESKTOP_MESSAGE_SET_ICONS:
			icons = value2;
			desktop_set_icons(desktop, icons);
			break;
		case DESKTOP_MESSAGE_SET_LAYOUT:
			layout = value2;
			desktop_set_layout(desktop, layout);
			break;
		case DESKTOP_MESSAGE_SHOW:
			if(value2 == DESKTOP_SHOW_SETTINGS)
				_desktop_show_preferences(desktop);
			break;
	}
	return GDK_FILTER_CONTINUE;
}

static void _on_popup_new_folder(gpointer data);
static void _on_popup_new_text_file(gpointer data);
static void _on_popup_paste(gpointer data);
static void _on_popup_preferences(gpointer data);
static void _on_popup_symlink(gpointer data);

static void _on_popup(gpointer data)
{
	Desktop * desktop = data;

	_on_popup_event(desktop, NULL);
}

static void _on_popup_event(gpointer data, XButtonEvent * xbev)
{
	Desktop * desktop = data;
	GtkWidget * menuitem;
	GtkWidget * submenu;
	GtkWidget * image;

	desktop->menu = gtk_menu_new();
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_NEW, NULL);
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	/* submenu for new documents */
	menuitem = gtk_image_menu_item_new_with_label(_("Folder"));
	image = gtk_image_new_from_icon_name("folder-new", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_new_folder), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_label(_("Symbolic link..."));
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_symlink), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_label(_("Text file"));
	image = gtk_image_new_from_icon_name("stock_new-text",
			GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_new_text_file), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	/* edition */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_paste), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	/* preferences */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES,
			NULL);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_preferences), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	gtk_widget_show_all(desktop->menu);
	gtk_menu_popup(GTK_MENU(desktop->menu), NULL, NULL, NULL, NULL, 3,
			(xbev != NULL)
			? xbev->time : gtk_get_current_event_time());
}

static void _on_popup_new_folder(gpointer data)
{
	static char const newfolder[] = N_("New folder");
	Desktop * desktop = data;
	String * path;

	gtk_widget_destroy(desktop->menu);
	desktop->menu = NULL;
	if((path = string_new_append(desktop->path, "/", newfolder, NULL))
			== NULL)
	{
		_desktop_serror(desktop, newfolder, 0);
		return;
	}
	if(mkdir(path, 0777) != 0)
		desktop_error(desktop, path, 0);
	string_delete(path);
}

static void _on_popup_new_text_file(gpointer data)
{
	static char const newtext[] = N_("New text file.txt");
	Desktop * desktop = data;
	String * path;
	int fd;

	gtk_widget_destroy(desktop->menu);
	desktop->menu = NULL;
	if((path = string_new_append(desktop->path, "/", _(newtext), NULL))
			== NULL)
	{
		_desktop_serror(desktop, _(newtext), 0);
		return;
	}
	if((fd = creat(path, 0666)) < 0)
		desktop_error(desktop, path, 0);
	else
		close(fd);
	string_delete(path);
}

static void _on_popup_paste(gpointer data)
{
	Desktop * desktop = data;

	/* FIXME implement */
	gtk_widget_destroy(desktop->menu);
	desktop->menu = NULL;
}

static void _on_popup_preferences(gpointer data)
{
	Desktop * desktop = data;

	_desktop_show_preferences(desktop);
}

static void _on_popup_symlink(gpointer data)
{
	Desktop * desktop = data;

	if(_common_symlink(NULL, desktop->path) != 0)
		desktop_error(desktop, "symlink", 0);
}

static void _on_realize(gpointer data)
{
	Desktop * desktop = data;
	GdkEventMask mask = desktop->prefs.popup ? GDK_BUTTON_PRESS_MASK : 0;

#if GTK_CHECK_VERSION(2, 14, 0)
	desktop->back = gtk_widget_get_window(desktop->desktop);
#else
	desktop->back = desktop->desktop->window;
#endif
	/* support pop-up menus on the desktop window if enabled */
	if(mask != 0)
		_new_events(desktop, desktop->back, mask);
	desktop_reset(desktop);
}

static GdkFilterReturn _event_button_press(XButtonEvent * xbev,
		Desktop * desktop);
static GdkFilterReturn _event_configure(XConfigureEvent * xevent,
		Desktop * desktop);
static GdkFilterReturn _event_property(XPropertyEvent * xevent,
		Desktop * desktop);

static GdkFilterReturn _on_root_event(GdkXEvent * xevent, GdkEvent * event,
		gpointer data)
{
	Desktop * desktop = data;
	XEvent * xev = xevent;

	if(xev->type == ButtonPress)
		return _event_button_press(xevent, desktop);
	else if(xev->type == ConfigureNotify)
		return _event_configure(xevent, desktop);
	else if(xev->type == PropertyNotify)
		return _event_property(xevent, desktop);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %d\n", __func__, xev->type);
#endif
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn _event_button_press(XButtonEvent * xbev,
		Desktop * desktop)
{
	if(xbev->button != 3 || desktop->menu != NULL)
	{
		if(desktop->menu != NULL)
		{
			gtk_widget_destroy(desktop->menu);
			desktop->menu = NULL;
		}
		return GDK_FILTER_CONTINUE;
	}
	/* ignore if not managing files */
	if(desktop->prefs.icons != DESKTOP_ICONS_FILES)
		return GDK_FILTER_CONTINUE;
	_on_popup_event(desktop, xbev);
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn _event_configure(XConfigureEvent * xevent,
		Desktop * desktop)
{
	desktop->window.x = xevent->x;
	desktop->window.y = xevent->y;
	desktop->window.width = xevent->width;
	desktop->window.height = xevent->height;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() (%dx%d) @ (%d,%d))\n", __func__,
			desktop->window.width, desktop->window.height,
			desktop->window.x, desktop->window.y);
#endif
	/* FIXME run it directly? */
	desktop_reset(desktop);
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn _event_property(XPropertyEvent * xevent,
		Desktop * desktop)
{
	Atom atom;

	atom = gdk_x11_get_xatom_by_name("_NET_WORKAREA");
	if(xevent->atom != atom)
		return GDK_FILTER_CONTINUE;
	_desktop_get_workarea(desktop);
	return GDK_FILTER_CONTINUE;
}


/* desktop_delete */
void desktop_delete(Desktop * desktop)
{
	size_t i;

	if(desktop->desktop != NULL)
		gtk_widget_destroy(desktop->desktop);
	if(desktop->refresh_source != 0)
		g_source_remove(desktop->refresh_source);
	for(i = 0; i < desktop->icon_cnt; i++)
		desktopicon_delete(desktop->icon[i]);
	free(desktop->icon);
	if(desktop->mime != NULL)
		mime_delete(desktop->mime);
	g_slist_foreach(desktop->apps, (GFunc)config_delete, NULL);
	g_slist_free(desktop->apps);
	free(desktop->path);
	if(desktop->font != NULL)
		pango_font_description_free(desktop->font);
	object_delete(desktop);
}


/* accessors */
/* desktop_get_drag_data */
int desktop_get_drag_data(Desktop * desktop, GtkSelectionData * seldata)
{
#if !GTK_CHECK_VERSION(3, 0, 0)
	int ret = 0;
	size_t i;
	size_t len;
	char const * path;
	unsigned char * p;

	seldata->format = 0;
	seldata->data = NULL;
	seldata->length = 0;
	for(i = 0; i < desktop->icon_cnt; i++)
	{
		if(desktopicon_get_selected(desktop->icon[i]) != TRUE)
			continue;
		if((path = desktopicon_get_path(desktop->icon[i])) == NULL)
			continue;
		len = strlen(path + 1);
		if((p = realloc(seldata->data, seldata->length + len)) == NULL)
		{
			ret = -error_set_code(1, "%s", strerror(errno));
			continue;
		}
		seldata->data = p;
		memcpy(&p[seldata->length], path, len);
		seldata->length += len;
	}
	return ret;
#else
	return -1;
#endif
}


/* desktop_get_file */
GdkPixbuf * desktop_get_file(Desktop * desktop)
{
	g_object_ref(desktop->file);
	return desktop->file;
}


/* desktop_get_folder */
GdkPixbuf * desktop_get_folder(Desktop * desktop)
{
	g_object_ref(desktop->folder);
	return desktop->folder;
}


/* desktop_get_mime */
Mime * desktop_get_mime(Desktop * desktop)
{
	return desktop->mime;
}


/* desktop_get_theme */
GtkIconTheme * desktop_get_theme(Desktop * desktop)
{
	return desktop->theme;
}


/* desktop_set_alignment */
static void _alignment_horizontal(Desktop * desktop);
static void _alignment_vertical(Desktop * desktop);

void desktop_set_alignment(Desktop * desktop, DesktopAlignment alignment)
{
	switch(alignment)
	{
		case DESKTOP_ALIGNMENT_VERTICAL:
			_alignment_vertical(desktop);
			break;
		case DESKTOP_ALIGNMENT_HORIZONTAL:
			_alignment_horizontal(desktop);
			break;
	}
}

static void _alignment_horizontal(Desktop * desktop)
{
	size_t i;
	int x = desktop->workarea.x;
	int y = desktop->workarea.y;
	int width = x + desktop->workarea.width;

	for(i = 0; i < desktop->icon_cnt; i++)
	{
		if(x + DESKTOPICON_MAX_WIDTH > width)
		{
			y += DESKTOPICON_MAX_HEIGHT;
			x = desktop->workarea.x;
		}
		desktopicon_move(desktop->icon[i], x, y);
		x += DESKTOPICON_MAX_WIDTH;
	}
}

static void _alignment_vertical(Desktop * desktop)
{
	size_t i;
	int x = desktop->workarea.x;
	int y = desktop->workarea.y;
	int height = desktop->workarea.y + desktop->workarea.height;

	for(i = 0; i < desktop->icon_cnt; i++)
	{
		if(y + DESKTOPICON_MAX_HEIGHT > height)
		{
			x += DESKTOPICON_MAX_WIDTH;
			y = desktop->workarea.y;
		}
		desktopicon_move(desktop->icon[i], x, y);
		y += DESKTOPICON_MAX_HEIGHT;
	}
}


/* desktop_set_icons */
static int _icons_applications(Desktop * desktop);
static int _icons_categories(Desktop * desktop);
static int _icons_files(Desktop * desktop);
static int _icons_files_add_home(Desktop * desktop);
static int _icons_homescreen(Desktop * desktop);
static void _icons_reset(Desktop * desktop);
static void _icons_set_categories(Desktop * desktop, gpointer data);
static void _icons_set_homescreen(Desktop * desktop, gpointer data);

void desktop_set_icons(Desktop * desktop, DesktopIcons icons)
{
	_icons_reset(desktop);
	desktop->prefs.icons = icons;
	switch(icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_icons_applications(desktop);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_icons_categories(desktop);
			break;
		case DESKTOP_ICONS_FILES:
			_icons_files(desktop);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_icons_homescreen(desktop);
			break;
		case DESKTOP_ICONS_NONE:
			/* nothing to do */
			break;
	}
	desktop_refresh(desktop);
}

static int _icons_applications(Desktop * desktop)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;

	if(desktop->category == NULL)
		return 0;
	if((desktopicon = desktopicon_new(desktop, _("Back"), NULL)) == NULL)
		return -_desktop_serror(desktop, "Back", 1);
	desktopicon_set_callback(desktopicon, _icons_set_categories, NULL);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	icon = gtk_icon_theme_load_icon(desktop->theme, "back",
			DESKTOPICON_ICON_SIZE, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	if(_desktop_icon_add(desktop, desktopicon) != 0)
	{
		desktopicon_delete(desktopicon);
		return -1;
	}
	return 0;
}

static int _icons_categories(Desktop * desktop)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;

	desktop->category = NULL;
	if((desktopicon = desktopicon_new(desktop, _("Back"), NULL)) == NULL)
		return -_desktop_serror(desktop, "Back", 1);
	desktopicon_set_callback(desktopicon, _icons_set_homescreen, NULL);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	icon = gtk_icon_theme_load_icon(desktop->theme, "back",
			DESKTOPICON_ICON_SIZE, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	if(_desktop_icon_add(desktop, desktopicon) != 0)
	{
		desktopicon_delete(desktopicon);
		return -1;
	}
	return 0;
}

static int _icons_files(Desktop * desktop)
{
	const char path[] = "/" DESKTOP;
	struct stat st;

	if(desktop->mime == NULL)
		desktop->mime = mime_new(NULL);
	_icons_files_add_home(desktop);
	desktop->path_cnt = strlen(desktop->home) + 1 + sizeof(path);
	if((desktop->path = malloc(desktop->path_cnt)) == NULL)
		return -desktop_error(NULL, "malloc", 1);
	snprintf(desktop->path, desktop->path_cnt, "%s/%s", desktop->home,
			path);
	if(stat(desktop->path, &st) == 0)
	{
		if(!S_ISDIR(st.st_mode))
			return _desktop_error(NULL, desktop->path,
					strerror(ENOTDIR), 1);
	}
	else if(errno != ENOENT || mkdir(desktop->path, 0777) != 0)
		return desktop_error(NULL, desktop->path, 1);
	return 0;
}

static int _icons_files_add_home(Desktop * desktop)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;

	if((desktopicon = desktopicon_new(desktop, _("Home"), desktop->home))
			== NULL)
		return -_desktop_serror(desktop, "Home", 1);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	icon = gtk_icon_theme_load_icon(desktop->theme, "gnome-home",
			DESKTOPICON_ICON_SIZE, 0, NULL);
	if(icon == NULL)
		icon = gtk_icon_theme_load_icon(desktop->theme, "gnome-fs-home",
				DESKTOPICON_ICON_SIZE, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	if(_desktop_icon_add(desktop, desktopicon) != 0)
	{
		desktopicon_delete(desktopicon);
		return -1;
	}
	return 0;
}

static int _icons_homescreen(Desktop * desktop)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;
#ifdef EMBEDDED
	char const * paths[] =
	{
		DATADIR "/applications/deforaos-phone-contacts.desktop",
		DATADIR "/applications/deforaos-phone-dialer.desktop",
		DATADIR "/applications/deforaos-phone-messages.desktop",
		NULL
	};
	char const ** p;
#endif

	if((desktopicon = desktopicon_new(desktop, _("Applications"), NULL))
			== NULL)
		return _desktop_serror(desktop, "Applications", 1);
	desktopicon_set_callback(desktopicon, _icons_set_categories, NULL);
	desktopicon_set_immutable(desktopicon, TRUE);
	icon = gtk_icon_theme_load_icon(desktop->theme, "gnome-applications",
			DESKTOPICON_ICON_SIZE, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	_desktop_icon_add(desktop, desktopicon);
#ifdef EMBEDDED
	for(p = paths; *p != NULL; p++)
		if(access(*p, R_OK) == 0
				&& (desktopicon = desktopicon_new_application(
						desktop, *p, DATADIR)) != NULL)
			_desktop_icon_add(desktop, desktopicon);
#endif
	return 0;
}

static void _icons_reset(Desktop * desktop)
{
	size_t i;

	if(desktop->path != NULL)
		free(desktop->path);
	desktop->path = NULL;
	desktop->path_cnt = 0;
	for(i = 0; i < desktop->icon_cnt; i++)
	{
		desktopicon_set_immutable(desktop->icon[i], FALSE);
		desktopicon_set_updated(desktop->icon[i], FALSE);
	}
	for(i = 0; i < _desktop_categories_cnt; i++)
		_desktop_categories[i].show = FALSE;
}

static void _icons_set_categories(Desktop * desktop, gpointer data)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	desktop_set_icons(desktop, DESKTOP_ICONS_CATEGORIES);
}

static void _icons_set_homescreen(Desktop * desktop, gpointer data)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	desktop_set_icons(desktop, DESKTOP_ICONS_HOMESCREEN);
}


/* desktop_set_layout */
int desktop_set_layout(Desktop * desktop, DesktopLayout layout)
{
	XRRScreenConfiguration * sc;
	Rotation r;
	SizeID size;

	sc = XRRGetScreenInfo(GDK_DISPLAY_XDISPLAY(desktop->display),
			GDK_WINDOW_XID(desktop->root));
	size = XRRConfigCurrentConfiguration(sc, &r);
	switch(layout)
	{
		case DESKTOP_LAYOUT_NORMAL:
		case DESKTOP_LAYOUT_LANDSCAPE:
			r = RR_Rotate_0;
			break;
		case DESKTOP_LAYOUT_PORTRAIT:
			r = RR_Rotate_90;
			break;
		case DESKTOP_LAYOUT_ROTATE:
			r <<= 1;
			r = (r > 16) ? 1 : r;
			break;
		case DESKTOP_LAYOUT_TOGGLE:
			r = (r != RR_Rotate_0) ? RR_Rotate_0 : RR_Rotate_90;
			break;
	}
	gdk_error_trap_push();
	XRRSetScreenConfig(GDK_DISPLAY_XDISPLAY(desktop->display), sc,
			GDK_WINDOW_XID(desktop->root), size, r, CurrentTime);
	return gdk_error_trap_pop();
}


/* useful */
/* desktop_error */
int desktop_error(Desktop * desktop, char const * message, int ret)
{
	/* FIXME no longer assume errno is properly set */
	return _desktop_error(desktop, message, strerror(errno), ret);
}


/* desktop_refresh */
static void _refresh_categories(Desktop * desktop);
static void _refresh_files(Desktop * desktop);
static void _refresh_homescreen(Desktop * desktop);

void desktop_refresh(Desktop * desktop)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(desktop->refresh_source != 0)
		g_source_remove(desktop->refresh_source);
	switch(desktop->prefs.icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
		case DESKTOP_ICONS_CATEGORIES:
			_refresh_categories(desktop);
			break;
		case DESKTOP_ICONS_FILES:
			_refresh_files(desktop);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
		case DESKTOP_ICONS_NONE:
			_refresh_homescreen(desktop);
			break;
	}
}

static void _refresh_categories(Desktop * desktop)
{
	g_slist_foreach(desktop->apps, (GFunc)config_delete, NULL);
	g_slist_free(desktop->apps);
	desktop->apps = NULL;
	desktop->refresh_source = g_idle_add(_desktop_on_refresh, desktop);
}

static void _refresh_files(Desktop * desktop)
{
	struct stat st;

	if(desktop->path == NULL)
		return;
	if((desktop->refresh_dir = browser_vfs_opendir(desktop->path, &st))
			== NULL)
	{
		desktop_error(NULL, desktop->path, 1);
		desktop->refresh_source = 0;
		return;
	}
	desktop->refresh_mti = st.st_mtime;
	desktop->refresh_source = g_idle_add(_desktop_on_refresh, desktop);
}

static void _refresh_homescreen(Desktop * desktop)
{
	/* for cleanup */
	desktop->refresh_source = g_idle_add(_desktop_on_refresh, desktop);
}


/* desktop_reset */
static void _reset_background(Desktop * desktop, Config * config);
static void _reset_icons(Desktop * desktop, Config * config);
static void _reset_icons_colors(Desktop * desktop, Config * config);
static void _reset_icons_font(Desktop * desktop, Config * config);
static void _reset_icons_monitor(Desktop * desktop, Config * config);
/* callbacks */
static gboolean _reset_on_idle(gpointer data);

void desktop_reset(Desktop * desktop)
{
	if(desktop->refresh_source != 0)
		g_source_remove(desktop->refresh_source);
	desktop->refresh_source = g_idle_add(_reset_on_idle, desktop);
}

static void _reset_background(Desktop * desktop, Config * config)
{
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkRGBA color = { 0.0, 0.0, 0.0, 0.0 };
#else
	GdkColor color = { 0, 0, 0, 0 };
#endif
	char const * filename;
	DesktopHows how = DESKTOP_HOW_SCALED;
	gboolean extend = FALSE;
	size_t i;
	char const * p;

	if((p = config_get(config, "background", "color")) != NULL)
#if GTK_CHECK_VERSION(3, 0, 0)
		gdk_rgba_parse(&color, p);
#else
		gdk_color_parse(p, &color);
#endif
	filename = config_get(config, "background", "wallpaper");
	if((p = config_get(config, "background", "how")) != NULL)
		for(i = 0; i < DESKTOP_HOW_COUNT; i++)
			if(strcmp(_desktop_hows[i], p) == 0)
				how = i;
	if((p = config_get(config, "background", "extend")) != NULL)
		extend = strtol(p, NULL, 10) ? TRUE : FALSE;
	_desktop_draw_background(desktop, &color, filename, how, extend);
}

static void _reset_icons(Desktop * desktop, Config * config)
{
	String const * p;
	String * q;
	size_t i;
	int j;

	_reset_icons_colors(desktop, config);
	_reset_icons_font(desktop, config);
	for(i = 0; i < desktop->icon_cnt; i++)
	{
		desktopicon_set_background(desktop->icon[i],
				&desktop->background);
		desktopicon_set_font(desktop->icon[i], desktop->font);
		desktopicon_set_foreground(desktop->icon[i],
				&desktop->foreground);
	}
	_reset_icons_monitor(desktop, config);
	/* icons layout */
	if(desktop->prefs.icons < 0
			&& (p = config_get(config, "icons", "layout")) != NULL)
	{
		for(i = 0; i < DESKTOP_ICONS_COUNT; i++)
			if(strcmp(_desktop_icons_config[i], p) == 0)
			{
				desktop->prefs.icons = i;
				break;
			}
	}
	if(desktop->prefs.icons < 0
			|| desktop->prefs.icons >= DESKTOP_ICONS_COUNT)
		desktop->prefs.icons = DESKTOP_ICONS_FILES;
	/* icons alignment */
	if(desktop->prefs.alignment < 0)
		desktop->prefs.alignment = (desktop->prefs.icons
				== DESKTOP_ICONS_FILES)
			? DESKTOP_ALIGNMENT_VERTICAL
			: DESKTOP_ALIGNMENT_HORIZONTAL;
	/* show hidden */
	if((p = config_get(config, "icons", "show_hidden")) != NULL)
	{
		j = strtol(p, &q, 10);
		if(p[0] == '\0' || *q != '\0' || j < 0)
			j = 0;
		desktop->show_hidden = j ? 1 : 0;
	}
}

static void _reset_icons_colors(Desktop * desktop, Config * config)
{
	String const * p;
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkRGBA color;
#else
	GdkColor color;
#endif

	if((p = config_get(config, "icons", "background")) != NULL)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		gdk_rgba_parse(&color, p);
#else
		gdk_color_parse(p, &color);
#endif
		desktop->background = color;
	}
	if((p = config_get(config, "icons", "foreground")) != NULL)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		gdk_rgba_parse(&color, p);
#else
		gdk_color_parse(p, &color);
#endif
		desktop->foreground = color;
	}
}

static void _reset_icons_font(Desktop * desktop, Config * config)
{
	String const * p;

	if(desktop->font != NULL)
		pango_font_description_free(desktop->font);
	if((p = config_get(config, "icons", "font")) != NULL)
		desktop->font = pango_font_description_from_string(p);
	else
	{
		desktop->font = pango_font_description_new();
		pango_font_description_set_weight(desktop->font,
				PANGO_WEIGHT_BOLD);
	}
}

static void _reset_icons_monitor(Desktop * desktop, Config * config)
{
	String const * p;
	char * q;

	/* icons monitor */
	if(desktop->prefs.monitor < 0
			&& (p = config_get(config, "icons", "monitor")) != NULL)
	{
		desktop->prefs.monitor = strtol(p, &q, 10);
		if(p[0] == '\0' || *q != '\0')
			desktop->prefs.monitor = -1;
	}
}

/* callbacks */
static gboolean _reset_on_idle(gpointer data)
{
	Desktop * desktop = data;
	Config * config;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	desktop->refresh_source = 0;
	if((config = _desktop_get_config(desktop)) == NULL)
		return FALSE;
	_reset_background(desktop, config);
	_reset_icons(desktop, config);
	config_delete(config);
	_desktop_get_workarea(desktop);
	desktop_set_icons(desktop, desktop->prefs.icons);
	return FALSE;
}


/* desktop_icon_add */
void desktop_icon_add(Desktop * desktop, DesktopIcon * icon)
{
	if(_desktop_icon_add(desktop, icon) == 0)
		desktop_icons_align(desktop);
}


/* desktop_icon_remove */
void desktop_icon_remove(Desktop * desktop, DesktopIcon * icon)
{
	if(_desktop_icon_remove(desktop, icon) == 0)
		desktop_icons_align(desktop);
}


/* desktop_icons_align */
static int _align_compare(const void * a, const void * b);

void desktop_icons_align(Desktop * desktop)
{
	qsort(desktop->icon, desktop->icon_cnt, sizeof(void *), _align_compare);
	desktop_set_alignment(desktop, desktop->prefs.alignment);
}

static int _align_compare(const void * a, const void * b)
{
	DesktopIcon * icona = *(DesktopIcon**)a;
	DesktopIcon * iconb = *(DesktopIcon**)b;
	gboolean firsta = desktopicon_get_first(icona);
	gboolean firstb = desktopicon_get_first(iconb);
	gboolean dira;
	gboolean dirb;

	if(firsta && !firstb)
		return -1;
	else if(!firsta && firstb)
		return 1;
	dira = desktopicon_get_isdir(icona);
	dirb = desktopicon_get_isdir(iconb);
	if(dira && !dirb)
		return -1;
	else if(!dira && dirb)
		return 1;
	return strcmp(desktopicon_get_name(icona), desktopicon_get_name(iconb));
}


/* desktop_select_all */
void desktop_select_all(Desktop * desktop)
{
	size_t i;

	for(i = 0; i < desktop->icon_cnt; i++)
		desktopicon_set_selected(desktop->icon[i], TRUE);
}


/* desktop_select_above */
void desktop_select_above(Desktop * desktop, DesktopIcon * icon)
	/* FIXME icons may be wrapped */
{
	size_t i;

	for(i = 1; i < desktop->icon_cnt; i++)
		if(desktop->icon[i] == icon)
		{
			desktopicon_set_selected(desktop->icon[i], TRUE);
			return;
		}
}


/* desktop_select_under */
void desktop_select_under(Desktop * desktop, DesktopIcon * icon)
	/* FIXME icons may be wrapped */
{
	size_t i;

	for(i = 0; i < desktop->icon_cnt; i++)
		if(desktop->icon[i] == icon && i + 1 < desktop->icon_cnt)
		{
			desktopicon_set_selected(desktop->icon[i], TRUE);
			return;
		}
}


/* desktop_unselect_all */
void desktop_unselect_all(Desktop * desktop)
{
	size_t i;

	for(i = 0; i < desktop->icon_cnt; i++)
		desktopicon_set_selected(desktop->icon[i], FALSE);
}


/* private */
/* functions */
/* desktop_error */
static int _error_text(char const * message, char const * error, int ret);

static int _desktop_error(Desktop * desktop, char const * message,
		char const * error, int ret)
{
	GtkWidget * dialog;

	if(desktop == NULL)
		return _error_text(message, error, ret);
	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s: %s", message,
#endif
			error);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	if(ret < 0)
	{
		g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(
					gtk_main_quit), NULL);
		ret = -ret;
	}
	else
		g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(
					gtk_widget_destroy), NULL);
	gtk_widget_show(dialog);
	return ret;
}

static int _error_text(char const * message, char const * error, int ret)
{
	fprintf(stderr, "%s: %s%s%s\n", PROGNAME,
			(message != NULL) ? message : "",
			(message != NULL) ? ": " : "", error);
	return ret;
}


/* desktop_serror */
static int _desktop_serror(Desktop * desktop, char const * message, int ret)
{
	return _desktop_error(desktop, message, error_get(), ret);
}


/* desktop_get_config */
static Config * _desktop_get_config(Desktop * desktop)
{
	Config * config;
	String * pathname = NULL;

	if((config = config_new()) == NULL
			|| (pathname = string_new_append(desktop->home,
					"/" DESKTOPRC, NULL)) == NULL)
	{
		if(config != NULL)
			config_delete(config);
		if(pathname != NULL)
			object_delete(pathname);
		_desktop_serror(NULL, _("Could not load preferences"), FALSE);
		return NULL;
	}
	config_load(config, pathname); /* XXX ignore errors */
	return config;
}


/* desktop_get_workarea */
static int _desktop_get_workarea(Desktop * desktop)
{
	Atom atom;
	Atom type;
	int format;
	unsigned long cnt;
	unsigned long bytes;
	unsigned char * p = NULL;
	unsigned long * u;
	int res;

	if(desktop->prefs.monitor >= 0 && desktop->prefs.monitor
			< gdk_screen_get_n_monitors(desktop->screen))
	{
		gdk_screen_get_monitor_geometry(desktop->screen,
				desktop->prefs.monitor, &desktop->workarea);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() (%d, %d) %dx%d\n", __func__,
				desktop->workarea.x, desktop->workarea.y,
				desktop->workarea.width,
				desktop->workarea.height);
#endif
		desktop_icons_align(desktop);
		return 0;
	}
	atom = gdk_x11_get_xatom_by_name("_NET_WORKAREA");
	gdk_error_trap_push();
	res = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(desktop->display),
				GDK_WINDOW_XID(desktop->root), atom, 0,
				G_MAXLONG, False, XA_CARDINAL, &type, &format,
				&cnt, &bytes, &p);
	if(gdk_error_trap_pop() || res != Success || cnt < 4)
		gdk_screen_get_monitor_geometry(desktop->screen, 0,
				&desktop->workarea);
	else
	{
		u = (unsigned long *)p;
		desktop->workarea.x = u[0];
		desktop->workarea.y = u[1];
		if((desktop->workarea.width = u[2]) == 0
				|| (desktop->workarea.height = u[3]) == 0)
			gdk_screen_get_monitor_geometry(desktop->screen, 0,
					&desktop->workarea);
	}
	if(p != NULL)
		XFree(p);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() (%d, %d) %dx%d\n", __func__,
			desktop->workarea.x, desktop->workarea.y,
			desktop->workarea.width, desktop->workarea.height);
#endif
	desktop_icons_align(desktop);
	return 0;
}


/* useful */
/* desktop_background */
#if !GTK_CHECK_VERSION(3, 0, 0)
static void _background_how_centered(GdkRectangle * window, GdkPixmap * pixmap,
		char const * filename, GError ** error);
static void _background_how_scaled(GdkRectangle * window, GdkPixmap * pixmap,
		char const * filename, GError ** error);
static void _background_how_scaled_ratio(GdkRectangle * window,
		GdkPixmap * pixmap, char const * filename, GError ** error);
static void _background_how_tiled(GdkRectangle * window, GdkPixmap * pixmap,
		char const * filename, GError ** error);
static void _background_monitor(Desktop * desktop, char const * filename,
		DesktopHows how, gboolean extend, GdkPixmap * pixmap,
		GdkRectangle * window, int monitor);
static void _background_monitors(Desktop * desktop, char const * filename,
		DesktopHows how, gboolean extend, GdkPixmap * pixmap,
		GdkRectangle * window);
#endif

#if GTK_CHECK_VERSION(3, 0, 0)
static void _desktop_draw_background(Desktop * desktop, GdkRGBA * color,
		char const * filename, DesktopHows how, gboolean extend)
#else
static void _desktop_draw_background(Desktop * desktop, GdkColor * color,
		char const * filename, DesktopHows how, gboolean extend)
#endif
{
	GdkRectangle window = desktop->window;
#if GTK_CHECK_VERSION(3, 0, 0)
	cairo_t * cairo;
#else
	GdkPixmap * pixmap;
	GdkGC * gc;
	GtkStyle * style;
#endif

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %u, %s)\n", __func__, filename, how,
			extend ? "TRUE" : "FALSE");
#endif
	if(how == DESKTOP_HOW_NONE)
		return;
#if GTK_CHECK_VERSION(3, 0, 0)
	/* FIXME really implement */
	cairo = gdk_cairo_create(NULL);
	cairo_set_source_rgba(cairo, color->red, color->green, color->blue,
			1.0);
	gdk_cairo_rectangle(cairo, &window);
	cairo_destroy(cairo);
#else
	/* draw default color */
	pixmap = gdk_pixmap_new(desktop->back, window.width, window.height, -1);
	gc = gdk_gc_new(pixmap);
	gdk_gc_set_rgb_fg_color(gc, color);
	gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, window.width, window.height);
	if(filename != NULL)
		/* draw the background */
		_background_monitors(desktop, filename, how, extend, pixmap,
				&window);
	if(desktop->desktop != NULL)
	{
		style = gtk_style_new();
		style->bg_pixmap[GTK_STATE_NORMAL] = pixmap;
		gtk_widget_set_style(desktop->desktop, style);
	}
	else
	{
		gdk_window_set_back_pixmap(desktop->back, pixmap, FALSE);
		gdk_window_clear(desktop->back);
		gdk_pixmap_unref(pixmap);
	}
#endif
}

#if !GTK_CHECK_VERSION(3, 0, 0)
static void _background_how_centered(GdkRectangle * window, GdkPixmap * pixmap,
		char const * filename, GError ** error)
{
	GdkPixbuf * background;
	gint w;
	gint h;

	if((background = gdk_pixbuf_new_from_file(filename, error)) == NULL)
		return;
	w = gdk_pixbuf_get_width(background);
	h = gdk_pixbuf_get_height(background);
	gdk_draw_pixbuf(pixmap, NULL, background, 0, 0,
			(window->width - w) / 2 + window->x,
			(window->height - h) / 2 + window->y, w, h,
			GDK_RGB_DITHER_NONE, 0, 0);
	g_object_unref(background);
}

static void _background_how_scaled(GdkRectangle * window, GdkPixmap * pixmap,
		char const * filename, GError ** error)
{
	GdkPixbuf * background;
	gint w;
	gint h;

#if GTK_CHECK_VERSION(2, 6, 0)
	background = gdk_pixbuf_new_from_file_at_scale(filename, window->width,
			window->height, FALSE, error);
#elif GTK_CHECK_VERSION(2, 4, 0)
	background = gdk_pixbuf_new_from_file_at_size(filename, window->width,
			window->height, error);
#else
	background = gdk_pixbuf_new_from_file(filename, error);
#endif
	if(background == NULL)
		return;
	w = gdk_pixbuf_get_width(background);
	h = gdk_pixbuf_get_height(background);
	gdk_draw_pixbuf(pixmap, NULL, background, 0, 0,
			(window->width - w) / 2 + window->x,
			(window->height - h) / 2 + window->y, w, h,
			GDK_RGB_DITHER_NONE, 0, 0);
	g_object_unref(background);
}

static void _background_how_scaled_ratio(GdkRectangle * window,
		GdkPixmap * pixmap, char const * filename, GError ** error)
{
#if GTK_CHECK_VERSION(2, 4, 0)
	GdkPixbuf * background;
	gint w;
	gint h;

	background = gdk_pixbuf_new_from_file_at_size(filename, window->width,
			window->height, error);
	if(background == NULL)
		return; /* XXX report error */
	w = gdk_pixbuf_get_width(background);
	h = gdk_pixbuf_get_height(background);
	gdk_draw_pixbuf(pixmap, NULL, background, 0, 0,
			(window->width - w) / 2 + window->x,
			(window->height - h) / 2 + window->y, w, h,
			GDK_RGB_DITHER_NONE, 0, 0);
	g_object_unref(background);
#else
	_background_how_scaled(window, pixmap, filename, error);
#endif
}

static void _background_how_tiled(GdkRectangle * window, GdkPixmap * pixmap,
		char const * filename, GError ** error)
{
	GdkPixbuf * background;
	gint w;
	gint h;
	gint i;
	gint j;

	if((background = gdk_pixbuf_new_from_file(filename, error)) == NULL)
		return; /* XXX report error */
	w = gdk_pixbuf_get_width(background);
	h = gdk_pixbuf_get_height(background);
	for(j = 0; j < window->height; j += h)
		for(i = 0; i < window->width; i += w)
			gdk_draw_pixbuf(pixmap, NULL, background, 0, 0,
					i + window->x, j + window->y, w, h,
					GDK_RGB_DITHER_NONE, 0, 0);
	g_object_unref(background);
}

static void _background_monitor(Desktop * desktop, char const * filename,
		DesktopHows how, gboolean extend, GdkPixmap * pixmap,
		GdkRectangle * window, int monitor)
{
	GError * error = NULL;

	if(extend != TRUE)
		gdk_screen_get_monitor_geometry(desktop->screen, monitor,
				window);
	switch(how)
	{
		case DESKTOP_HOW_NONE:
			break;
		case DESKTOP_HOW_CENTERED:
			_background_how_centered(window, pixmap, filename,
					&error);
			break;
		case DESKTOP_HOW_SCALED_RATIO:
			_background_how_scaled_ratio(window, pixmap, filename,
					&error);
			break;
		case DESKTOP_HOW_TILED:
			_background_how_tiled(window, pixmap, filename, &error);
			break;
		case DESKTOP_HOW_SCALED:
			_background_how_scaled(window, pixmap, filename,
					&error);
			break;
	}
	if(error != NULL)
	{
		desktop_error(desktop, error->message, 1);
		g_error_free(error);
	}
}

static void _background_monitors(Desktop * desktop, char const * filename,
		DesktopHows how, gboolean extend, GdkPixmap * pixmap,
		GdkRectangle * window)
{
	gint n;
	gint i;

	n = (extend != TRUE) ? gdk_screen_get_n_monitors(desktop->screen) : 1;
	for(i = 0; i < n; i++)
		_background_monitor(desktop, filename, how, extend, pixmap,
				window, i);
}
#endif


/* desktop_icon_add */
static int _desktop_icon_add(Desktop * desktop, DesktopIcon * icon)
{
	DesktopIcon ** p;

	if((p = realloc(desktop->icon, sizeof(*p) * (desktop->icon_cnt + 1)))
			== NULL)
		return -desktop_error(desktop, desktopicon_get_name(icon), 1);
	desktop->icon = p;
	desktop->icon[desktop->icon_cnt++] = icon;
	desktopicon_set_background(icon, &desktop->background);
	desktopicon_set_font(icon, desktop->font);
	desktopicon_set_foreground(icon, &desktop->foreground);
	desktopicon_show(icon);
	return 0;
}


/* desktop_icon_remove */
static int _desktop_icon_remove(Desktop * desktop, DesktopIcon * icon)
{
	size_t i;
	DesktopIcon ** p;

	for(i = 0; i < desktop->icon_cnt; i++)
	{
		if(desktop->icon[i] != icon)
			continue;
		desktopicon_delete(icon);
		for(desktop->icon_cnt--; i < desktop->icon_cnt; i++)
			desktop->icon[i] = desktop->icon[i + 1];
		if((p = realloc(desktop->icon, sizeof(*p)
						* (desktop->icon_cnt))) != NULL)
			desktop->icon = p; /* we can ignore errors... */
		else if(desktop->icon_cnt == 0)
			desktop->icon = NULL; /* ...except when it's not one */
		return 0;
	}
	return 1;
}


/* desktop_show_preferences */
static void _preferences_background(Desktop * desktop, GtkWidget * notebook);
static void _preferences_icons(Desktop * desktop, GtkWidget * notebook);
static void _preferences_monitors(Desktop * desktop, GtkWidget * notebook);
static void _preferences_set(Desktop * desktop);
static void _preferences_set_color(Config * config, char const * section,
		char const * variable, char const * fallback,
		GtkWidget * widget);
static gboolean _on_preferences_closex(gpointer data);
static void _on_preferences_monitors_changed(gpointer data);
static void _on_preferences_monitors_refresh(gpointer data);
static void _on_preferences_response(GtkWidget * widget, gint response,
		gpointer data);
static void _on_preferences_response_apply(gpointer data);
static void _on_preferences_response_cancel(gpointer data);
static void _on_preferences_response_ok(gpointer data);
static void _on_preferences_update_preview(gpointer data);

static void _desktop_show_preferences(Desktop * desktop)
{
	GtkWidget * vbox;
	GtkWidget * notebook;

	if(desktop->menu != NULL)
		gtk_widget_destroy(desktop->menu);
	desktop->menu = NULL;
	if(desktop->pr_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(desktop->pr_window));
		return;
	}
	/* window */
	desktop->pr_window = gtk_dialog_new_with_buttons(
			_("Desktop preferences"), NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
			GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_window_set_position(GTK_WINDOW(desktop->pr_window),
			GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(desktop->pr_window), FALSE);
	g_signal_connect_swapped(G_OBJECT(desktop->pr_window), "delete-event",
			G_CALLBACK(_on_preferences_closex), desktop);
	g_signal_connect(G_OBJECT(desktop->pr_window), "response", G_CALLBACK(
				_on_preferences_response), desktop);
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(desktop->pr_window));
#else
	vbox = GTK_DIALOG(desktop->pr_window)->vbox;
#endif
	/* notebook */
	notebook = gtk_notebook_new();
	_preferences_background(desktop, notebook);
	_preferences_icons(desktop, notebook);
	_preferences_monitors(desktop, notebook);
	_on_preferences_monitors_refresh(desktop);
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
	/* container */
	_preferences_set(desktop);
	gtk_widget_show_all(desktop->pr_window);
}

static void _preferences_background(Desktop * desktop, GtkWidget * notebook)
{
	GtkSizeGroup * group;
	GtkWidget * vbox2;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkFileFilter * filter;

#if GTK_CHECK_VERSION(3, 0, 0)
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox2 = gtk_vbox_new(FALSE, 4);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 4);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Default color: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_color = gtk_color_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_color, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Wallpaper: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_background = gtk_file_chooser_button_new(_("Background"),
			GTK_FILE_CHOOSER_ACTION_OPEN);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Picture files"));
	gtk_file_filter_add_mime_type(filter, "image/bmp");
	gtk_file_filter_add_mime_type(filter, "image/gif");
	gtk_file_filter_add_mime_type(filter, "image/jpeg");
	gtk_file_filter_add_mime_type(filter, "image/pbm");
	gtk_file_filter_add_mime_type(filter, "image/png");
	gtk_file_filter_add_mime_type(filter, "image/svg+xml");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(desktop->pr_background),
			filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(desktop->pr_background),
			filter);
	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(
				desktop->pr_background), gtk_image_new());
	g_signal_connect_swapped(desktop->pr_background, "update-preview",
			G_CALLBACK(_on_preferences_update_preview), desktop);
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_background, TRUE, TRUE,
			0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Position: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
#if GTK_CHECK_VERSION(2, 24, 0)
	desktop->pr_background_how = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(
				desktop->pr_background_how), _("Do not draw"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(
				desktop->pr_background_how), _("Centered"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(
				desktop->pr_background_how), _("Scaled"));
	gtk_combo_box_text_append_text(
			GTK_COMBO_BOX_TEXT(desktop->pr_background_how),
			_("Scaled (keep ratio)"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(
				desktop->pr_background_how), _("Tiled"));
#else
	desktop->pr_background_how = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_background_how),
			_("Do not draw"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_background_how),
			_("Centered"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_background_how),
			_("Scaled"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_background_how),
			_("Scaled (keep ratio)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_background_how),
			_("Tiled"));
#endif
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_background_how, TRUE,
			TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	desktop->pr_background_extend = gtk_check_button_new_with_mnemonic(
			_("E_xtend background to all monitors"));
	gtk_box_pack_start(GTK_BOX(vbox2), desktop->pr_background_extend, FALSE,
			TRUE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(
				_("Background")));
}

static void _preferences_icons(Desktop * desktop, GtkWidget * notebook)
{
	GtkSizeGroup * group;
	GtkWidget * vbox2;
	GtkWidget * hbox;
	GtkWidget * label;
	size_t i;

#if GTK_CHECK_VERSION(3, 0, 0)
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox2 = gtk_vbox_new(FALSE, 4);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 4);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* icons */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Layout: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
#if GTK_CHECK_VERSION(2, 24, 0)
	desktop->pr_ilayout = gtk_combo_box_text_new();
#else
	desktop->pr_ilayout = gtk_combo_box_new_text();
#endif
	for(i = 0; i < sizeof(_desktop_icons) / sizeof(*_desktop_icons);
			i++)
#if GTK_CHECK_VERSION(2, 24, 0)
		gtk_combo_box_text_append_text(
				GTK_COMBO_BOX_TEXT(desktop->pr_ilayout),
				_(_desktop_icons[i]));
#else
		gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_ilayout),
				_(_desktop_icons[i]));
#endif
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_ilayout, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* monitor */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Monitor: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
#if GTK_CHECK_VERSION(2, 24, 0)
	desktop->pr_imonitor = gtk_combo_box_text_new();
#else
	desktop->pr_imonitor = gtk_combo_box_new_text();
#endif
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_imonitor, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* background color */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Background color: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_ibcolor = gtk_color_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_ibcolor, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* foreground color */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Foreground color: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_ifcolor = gtk_color_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_ifcolor, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* font */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Font: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_ifont = gtk_font_button_new();
	gtk_font_button_set_use_font(GTK_FONT_BUTTON(desktop->pr_ifont), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_ifont, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(
				_("Icons")));
}

static void _preferences_monitors(Desktop * desktop, GtkWidget * notebook)
{
	GtkSizeGroup * group;
	GtkWidget * vbox2;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * widget;

	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox2 = gtk_vbox_new(FALSE, 4);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 4);
	/* selector */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Monitor: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
#if GTK_CHECK_VERSION(2, 24, 0)
	desktop->pr_monitors = gtk_combo_box_text_new();
#else
	desktop->pr_monitors = gtk_combo_box_new_text();
#endif
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_monitors, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* geometry */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Resolution: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_monitors_res = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(desktop->pr_monitors_res), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_monitors_res, TRUE, TRUE,
			0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* size */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(_("Size: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_monitors_size = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(desktop->pr_monitors_size), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_monitors_size, TRUE, TRUE,
			0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* refresh */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	label = gtk_label_new(NULL);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	widget = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	g_signal_connect_swapped(widget, "clicked", G_CALLBACK(
				_on_preferences_monitors_refresh), desktop);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* updates */
	g_signal_connect_swapped(desktop->pr_monitors, "changed", G_CALLBACK(
				_on_preferences_monitors_changed), desktop);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(
				_("Monitors")));
}

static gboolean _on_preferences_closex(gpointer data)
{
	_on_preferences_response_cancel(data);
	return TRUE;
}

static void _on_preferences_monitors_changed(gpointer data)
{
	Desktop * desktop = data;
	gint active;
	GdkRectangle geometry;
	gint width;
	gint height;
	char buf[64];

	active = gtk_combo_box_get_active(GTK_COMBO_BOX(desktop->pr_monitors));
	geometry.x = 0;
	geometry.y = 0;
	geometry.width = gdk_screen_get_width(desktop->screen);
	geometry.height = gdk_screen_get_height(desktop->screen);
	width = gdk_screen_get_width_mm(desktop->screen);
	height = gdk_screen_get_height_mm(desktop->screen);
#if GTK_CHECK_VERSION(2, 14, 0)
	if(active-- > 0)
	{
		gdk_screen_get_monitor_geometry(desktop->screen, active,
				&geometry);
		width = gdk_screen_get_monitor_width_mm(desktop->screen,
				active);
		height = gdk_screen_get_monitor_height_mm(desktop->screen,
				active);
	}
#endif
	if(width < 0 || height < 0)
		snprintf(buf, sizeof(buf), "%s", _("Unknown size"));
	else
		snprintf(buf, sizeof(buf), _("%dx%d (at %d,%d)"),
				geometry.width, geometry.height,
				geometry.x, geometry.y);
	gtk_label_set_text(GTK_LABEL(desktop->pr_monitors_res), buf);
	if(width < 0 || height < 0)
		snprintf(buf, sizeof(buf), "%s", _("Unknown resolution"));
	else
		snprintf(buf, sizeof(buf), _("%dx%d mm (%.0fx%.0f DPI)"),
				width, height, geometry.width * 25.4 / width,
				geometry.height * 25.4 / height);
	gtk_label_set_text(GTK_LABEL(desktop->pr_monitors_size), buf);
}

static void _on_preferences_monitors_refresh(gpointer data)
{
	Desktop * desktop = data;
	GtkTreeModel * model1;
	GtkTreeModel * model2;
	gint active;
#if GTK_CHECK_VERSION(2, 14, 0)
	gint n;
	gint i;
	char * name;
	char buf[32];
#endif

	active = gtk_combo_box_get_active(GTK_COMBO_BOX(desktop->pr_imonitor));
	model1 = gtk_combo_box_get_model(GTK_COMBO_BOX(desktop->pr_imonitor));
	model2 = gtk_combo_box_get_model(GTK_COMBO_BOX(desktop->pr_monitors));
	gtk_list_store_clear(GTK_LIST_STORE(model1));
	gtk_list_store_clear(GTK_LIST_STORE(model2));
#if GTK_CHECK_VERSION(2, 24, 0)
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(desktop->pr_imonitor),
			_("Default monitor"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(desktop->pr_monitors),
			_("Whole screen"));
#else
	gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_imonitor),
			_("Default monitor"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_monitors),
			_("Whole screen"));
#endif
#if GTK_CHECK_VERSION(2, 14, 0)
	n = gdk_screen_get_n_monitors(desktop->screen);
	for(i = 0; i < n; i++)
	{
		snprintf(buf, sizeof(buf), _("Monitor %d"), i);
		name = gdk_screen_get_monitor_plug_name(desktop->screen, i);
# if GTK_CHECK_VERSION(2, 24, 0)
		gtk_combo_box_text_append_text(
				GTK_COMBO_BOX_TEXT(desktop->pr_imonitor),
				(name != NULL) ? name : buf);
		gtk_combo_box_text_append_text(
				GTK_COMBO_BOX_TEXT(desktop->pr_monitors),
				(name != NULL) ? name : buf);
# else
		gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_imonitor),
				(name != NULL) ? name : buf);
		gtk_combo_box_append_text(GTK_COMBO_BOX(desktop->pr_monitors),
				(name != NULL) ? name : buf);
# endif
		g_free(name);
	}
#endif
	gtk_combo_box_set_active(GTK_COMBO_BOX(desktop->pr_imonitor), active);
	gtk_combo_box_set_active(GTK_COMBO_BOX(desktop->pr_monitors), 0);
}

static void _on_preferences_response(GtkWidget * widget, gint response,
		gpointer data)
{
	Desktop * desktop = data;

	if(response == GTK_RESPONSE_OK)
		_on_preferences_response_ok(desktop);
	else if(response == GTK_RESPONSE_APPLY)
		_on_preferences_response_apply(desktop);
	else if(response == GTK_RESPONSE_CANCEL)
		_on_preferences_response_cancel(desktop);
}

static void _on_preferences_response_apply(gpointer data)
{
	Desktop * desktop = data;
	Config * config;
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkRGBA color;
#else
	GdkColor color;
#endif
	char * p;
	char const * q;
	int i;
	char buf[12];

	if((config = _desktop_get_config(desktop)) == NULL)
		return;
	/* XXX not very efficient */
	desktop_reset(desktop);
	/* background */
	p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
				desktop->pr_background));
	config_set(config, "background", "wallpaper", p);
	g_free(p);
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(desktop->pr_color),
			&color);
	p = gdk_rgba_to_string(&color);
#elif GTK_CHECK_VERSION(3, 0, 0)
	gtk_color_button_get_rgba(GTK_COLOR_BUTTON(desktop->pr_color), &color);
	p = gdk_rgba_to_string(&color);
#else
	gtk_color_button_get_color(GTK_COLOR_BUTTON(desktop->pr_color), &color);
	p = gdk_color_to_string(&color);
#endif
	config_set(config, "background", "color", p);
	g_free(p);
	i = gtk_combo_box_get_active(GTK_COMBO_BOX(desktop->pr_background_how));
	if(i >= 0 && i < DESKTOP_HOW_COUNT)
		config_set(config, "background", "how", _desktop_hows[i]);
	p = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				desktop->pr_background_extend)) ? "1" : "0";
	config_set(config, "background", "extend", p);
	/* icons */
	i = gtk_combo_box_get_active(GTK_COMBO_BOX(desktop->pr_ilayout));
	if(i >= 0 && i < DESKTOP_ICONS_COUNT)
		config_set(config, "icons", "layout", _desktop_icons_config[i]);
	desktop->prefs.icons = i; /* applied by _new_idle() */
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(desktop->pr_ibcolor),
			&color);
	p = gdk_rgba_to_string(&color);
#elif GTK_CHECK_VERSION(3, 0, 0)
	gtk_color_button_get_rgba(GTK_COLOR_BUTTON(desktop->pr_ibcolor),
			&color);
	p = gdk_rgba_to_string(&color);
#else
	gtk_color_button_get_color(GTK_COLOR_BUTTON(desktop->pr_ibcolor),
			&color);
	p = gdk_color_to_string(&color);
#endif
	config_set(config, "icons", "background", p);
	g_free(p);
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(desktop->pr_ifcolor),
			&color);
	p = gdk_rgba_to_string(&color);
#elif GTK_CHECK_VERSION(3, 0, 0)
	gtk_color_button_get_rgba(GTK_COLOR_BUTTON(desktop->pr_ifcolor),
			&color);
	p = gdk_rgba_to_string(&color);
#else
	gtk_color_button_get_color(GTK_COLOR_BUTTON(desktop->pr_ifcolor),
			&color);
	p = gdk_color_to_string(&color);
#endif
	config_set(config, "icons", "foreground", p);
	g_free(p);
	q = gtk_font_button_get_font_name(GTK_FONT_BUTTON(desktop->pr_ifont));
	config_set(config, "icons", "font", q);
	i = gtk_combo_box_get_active(GTK_COMBO_BOX(desktop->pr_imonitor));
	desktop->prefs.monitor = (i >= 0) ? i - 1 : i;
	snprintf(buf, sizeof(buf), "%d", desktop->prefs.monitor);
	config_set(config, "icons", "monitor", buf);
	config_set(config, "icons", "show_hidden", desktop->show_hidden
			? "1" : "0");
	/* XXX code duplication */
	if((p = string_new_append(desktop->home, "/" DESKTOPRC, NULL)) != NULL)
	{
		/* FIXME save configuration in _on_preferences_ok() instead */
		if(config_save(config, p) != 0)
			error_print(PROGNAME);
		string_delete(p);
	}
	config_delete(config);
}

static void _on_preferences_response_cancel(gpointer data)
{
	Desktop * desktop = data;

	gtk_widget_hide(desktop->pr_window);
	_preferences_set(desktop);
}

static void _on_preferences_response_ok(gpointer data)
{
	Desktop * desktop = data;

	gtk_widget_hide(desktop->pr_window);
	_on_preferences_response_apply(desktop);
}

static void _on_preferences_update_preview(gpointer data)
{
	Desktop * desktop = data;
#if !GTK_CHECK_VERSION(2, 6, 0)
	gint ratio = desktop->window.width / desktop->window.height;
#endif
	GtkFileChooser * chooser = GTK_FILE_CHOOSER(desktop->pr_background);
	GtkWidget * widget;
	char * filename;
	GdkPixbuf * pixbuf;
	gboolean active = FALSE;
	GError * error = NULL;

	widget = gtk_file_chooser_get_preview_widget(chooser);
	if((filename = gtk_file_chooser_get_preview_filename(chooser)) != NULL)
	{
#if GTK_CHECK_VERSION(2, 6, 0)
		pixbuf = gdk_pixbuf_new_from_file_at_scale(filename, 96, -1,
				TRUE, &error);
#else
		pixbuf = gdk_pixbuf_new_from_file_at_size(filename, 96,
				96 / ratio, &error);
#endif
		if(error != NULL)
		{
			_desktop_error(NULL, NULL, error->message, 1);
			g_error_free(error);
		}
		if(pixbuf != NULL)
		{
			gtk_image_set_from_pixbuf(GTK_IMAGE(widget), pixbuf);
			g_object_unref(pixbuf);
			active = TRUE;
		}
	}
	g_free(filename);
	gtk_file_chooser_set_preview_widget_active(chooser, active);
}

static void _preferences_set(Desktop * desktop)
{
	Config * config;
	String const * p;
	String * q;
	String const * filename = NULL;
	const char black[] = "#000000000000";
	const char white[] = "#ffffffffffff";
	int how;
	gboolean extend = FALSE;
	size_t i;
	int j;

	/* FIXME:
	 * - cache the current configuration within the Desktop class
	 * - apply the configuration values to the widgets */
	if((config = _desktop_get_config(desktop)) != NULL)
	{
		/* background */
		filename = config_get(config, "background", "wallpaper");
		_preferences_set_color(config, "background", "color", NULL,
				desktop->pr_color);
		how = 0;
		if((p = config_get(config, "background", "how")) != NULL)
			for(i = 0; i < DESKTOP_HOW_COUNT; i++)
				if(strcmp(_desktop_hows[i], p) == 0)
				{
					how = i;
					break;
				}
		gtk_combo_box_set_active(GTK_COMBO_BOX(
					desktop->pr_background_how), how);
		if((p = config_get(config, "background", "extend")) != NULL)
			extend = strtol(p, NULL, 10) ? TRUE : FALSE;
		/* icons */
		how = DESKTOP_ICONS_FILES;
		if((p = config_get(config, "icons", "layout")) != NULL)
			for(i = 0; i < DESKTOP_ICONS_COUNT; i++)
				if(strcmp(_desktop_icons_config[i], p) == 0)
				{
					how = i;
					break;
				}
		gtk_combo_box_set_active(GTK_COMBO_BOX(desktop->pr_ilayout),
				how);
		_preferences_set_color(config, "icons", "background", black,
				desktop->pr_ibcolor);
		_preferences_set_color(config, "icons", "foreground", white,
				desktop->pr_ifcolor);
		if((p = config_get(config, "icons", "font")) != NULL)
			gtk_font_button_set_font_name(GTK_FONT_BUTTON(
						desktop->pr_ifont), p);
		else if((q = pango_font_description_to_string(desktop->font))
				!= NULL)
		{
			gtk_font_button_set_font_name(GTK_FONT_BUTTON(
						desktop->pr_ifont), q);
			g_free(q);
		}
		if((p = config_get(config, "icons", "monitor")) != NULL)
		{
			j = strtol(p, &q, 10);
			if(p[0] == '\0' || *q != '\0' || j < 0)
				j = -1;
			gtk_combo_box_set_active(GTK_COMBO_BOX(
						desktop->pr_imonitor), j + 1);
		}
		config_delete(config);
	}
	else
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(
					desktop->pr_background_how), 0);
		gtk_combo_box_set_active(GTK_COMBO_BOX(desktop->pr_ilayout),
				DESKTOP_ICONS_FILES);
		gtk_combo_box_set_active(GTK_COMBO_BOX(desktop->pr_imonitor),
				0);
	}
	if(filename != NULL)
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(
					desktop->pr_background), filename);
	else
		gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(
					desktop->pr_background));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
				desktop->pr_background_extend), extend);
}

static void _preferences_set_color(Config * config, char const * section,
		char const * variable, char const * fallback,
		GtkWidget * widget)
{
	char const * p;
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkRGBA color = { 0.0, 0.0, 0.0, 0.0 };
#else
	GdkColor color = { 0, 0, 0, 0 };
#endif

	if((p = config_get(config, section, variable)) == NULL)
		p = fallback;
#if GTK_CHECK_VERSION(3, 4, 0)
	if(p != NULL && gdk_rgba_parse(&color, p) == TRUE)
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(widget), &color);
#elif GTK_CHECK_VERSION(3, 0, 0)
	if(p != NULL && gdk_rgba_parse(&color, p) == TRUE)
		gtk_color_button_set_rgba(GTK_COLOR_BUTTON(widget), &color);
#else
	if(p != NULL && gdk_color_parse(p, &color) == TRUE)
		gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), &color);
#endif
}


/* callbacks */
/* desktop_on_refresh */
static void _refresh_cleanup(Desktop * desktop);
static void _refresh_done_applications(Desktop * desktop);
static void _refresh_done_categories(Desktop * desktop);
static void _refresh_done_categories_open(Desktop * desktop, gpointer data);
static gboolean _refresh_done_timeout(gpointer data);
static int _refresh_loop(Desktop * desktop);
static gint _categories_apps_compare(gconstpointer a, gconstpointer b);
static int _refresh_loop_categories(Desktop * desktop);
static void _refresh_loop_categories_path(Desktop * desktop, char const * path,
		char const * apppath);
static void _refresh_loop_categories_xdg(Desktop * desktop,
		void (*callback)(Desktop * desktop, char const * path,
			char const * apppath));
static void _refresh_loop_categories_xdg_home(Desktop * desktop,
		void (*callback)(Desktop * desktop, char const * path,
			char const * apppath));
static void _refresh_loop_categories_xdg_path(Desktop * desktop,
		void (*callback)(Desktop * desktop, char const * path,
			char const * apppath), char const * path);
static int _refresh_loop_files(Desktop * desktop);
static int _refresh_loop_lookup(Desktop * desktop, char const * name);
static gboolean _refresh_done(Desktop * desktop);

static gboolean _desktop_on_refresh(gpointer data)
{
	Desktop * desktop = data;
	unsigned int i;

	for(i = 0; i < 16 && _refresh_loop(desktop) == 0; i++);
	if(i == 16)
		return TRUE;
	return _refresh_done(desktop);
}

static void _refresh_cleanup(Desktop * desktop)
{
	size_t i;

	for(i = 0; i < desktop->icon_cnt;)
		if(desktopicon_get_immutable(desktop->icon[i]) == TRUE)
			i++;
		else if(desktopicon_get_updated(desktop->icon[i]) != TRUE)
			_desktop_icon_remove(desktop, desktop->icon[i]);
		else
			desktopicon_set_updated(desktop->icon[i++], FALSE);
}

static gboolean _refresh_done(Desktop * desktop)
{
	switch(desktop->prefs.icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_refresh_done_applications(desktop);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_refresh_done_categories(desktop);
			break;
		default:
			break;
	}
	_refresh_cleanup(desktop);
	if(desktop->refresh_dir != NULL)
		browser_vfs_closedir(desktop->refresh_dir);
	desktop->refresh_dir = NULL;
	desktop_icons_align(desktop);
	desktop->refresh_source = g_timeout_add(1000, _refresh_done_timeout,
			desktop);
	return FALSE;
}

static void _refresh_done_applications(Desktop * desktop)
{
	GSList * p;
	Config * config;
	const char section[] = "Desktop Entry";
	char const * q;
	char const * r;
	DesktopCategory * dc = desktop->category;
	char const * path;
	DesktopIcon * icon;

	for(p = desktop->apps; p != NULL; p = p->next)
	{
		config = p->data;
		if(dc != NULL)
		{
			if((q = config_get(config, section, "Categories"))
					== NULL)
				continue;
			if((r = string_find(q, dc->category)) == NULL)
				continue;
			r += string_length(dc->category);
			if(*r != '\0' && *r != ';')
				continue;
		}
		path = config_get(config, NULL, "path");
		q = config_get(config, NULL, "datadir");
		if((icon = desktopicon_new_application(desktop, path, q))
				== NULL)
			continue;
		_desktop_icon_add(desktop, icon);
	}
}

static void _refresh_done_categories(Desktop * desktop)
{
	GSList * p;
	Config * config;
	const char section[] = "Desktop Entry";
	char const * q;
	char const * r;
	size_t i;
	DesktopCategory * dc;
	char const * path;
	DesktopIcon * icon;

	for(p = desktop->apps; p != NULL; p = p->next)
	{
		config = p->data;
		path = config_get(config, NULL, "path");
		if((q = config_get(config, section, "Categories")) == NULL)
		{
			if((icon = desktopicon_new_application(desktop, path,
							NULL)) != NULL)
				_desktop_icon_add(desktop, icon);
			continue;
		}
		for(i = 0; i < _desktop_categories_cnt; i++)
		{
			dc = &_desktop_categories[i];
			if((r = string_find(q, dc->category)) == NULL)
				continue;
			r += string_length(dc->category);
			if(*r == '\0' || *r == ';')
				break;
		}
		if(i == _desktop_categories_cnt)
		{
			if((icon = desktopicon_new_application(desktop, path,
					NULL)) != NULL)
				_desktop_icon_add(desktop, icon);
			continue;
		}
		if(dc->show == TRUE)
			continue;
		dc->show = TRUE;
		if((icon = desktopicon_new_category(desktop, dc->name,
						dc->icon)) == NULL)
			continue;
		desktopicon_set_callback(icon, _refresh_done_categories_open,
				dc);
		_desktop_icon_add(desktop, icon);
	}
}

static void _refresh_done_categories_open(Desktop * desktop, gpointer data)
{
	DesktopCategory * dc = data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, dc->name);
#endif
	desktop->category = dc;
	desktop_set_icons(desktop, DESKTOP_ICONS_APPLICATIONS);
}

static gboolean _refresh_done_timeout(gpointer data)
{
	Desktop * desktop = data;
	struct stat st;

	desktop->refresh_source = 0;
	if(desktop->path == NULL)
		return FALSE;
	if(stat(desktop->path, &st) != 0)
		return desktop_error(NULL, desktop->path, FALSE);
	if(st.st_mtime == desktop->refresh_mti)
		return TRUE;
	desktop_refresh(desktop);
	return FALSE;
}

static int _refresh_loop(Desktop * desktop)
{
	switch(desktop->prefs.icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
		case DESKTOP_ICONS_CATEGORIES:
			return _refresh_loop_categories(desktop);
		case DESKTOP_ICONS_FILES:
			return _refresh_loop_files(desktop);
		case DESKTOP_ICONS_HOMESCREEN:
		case DESKTOP_ICONS_NONE:
			return 1; /* nothing to do */
	}
	return -1;
}

static int _refresh_loop_categories(Desktop * desktop)
{
	_refresh_loop_categories_xdg(desktop, _refresh_loop_categories_path);
	return 1;
}

static void _refresh_loop_categories_path(Desktop * desktop, char const * path,
		char const * apppath)
{
	DIR * dir;
	int fd;
	struct stat st;
	struct dirent * de;
	size_t len;
	const char ext[] = ".desktop";
	const char section[] = "Desktop Entry";
	char * name = NULL;
	char * p;
	Config * config = NULL;
	String const * q;
	String const * r;

#if defined(__sun)
	if((fd = open(apppath, O_RDONLY)) < 0
			|| fstat(fd, &st) != 0
			|| (dir = fdopendir(fd)) == NULL)
#else
	if((dir = opendir(apppath)) == NULL
			|| (fd = dirfd(dir)) < 0
			|| fstat(fd, &st) != 0)
#endif
	{
		if(errno != ENOENT)
			desktop_error(NULL, apppath, 1);
		return;
	}
	if(st.st_mtime > desktop->refresh_mti)
		desktop->refresh_mti = st.st_mtime;
	while((de = readdir(dir)) != NULL)
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
		if((p = realloc(name, strlen(apppath) + len + 2)) == NULL)
		{
			desktop_error(NULL, apppath, 1);
			continue;
		}
		name = p;
		snprintf(name, strlen(apppath) + len + 2, "%s/%s", apppath,
				de->d_name);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() name=\"%s\" path=\"%s\"\n",
				__func__, name, path);
#endif
		if(config == NULL)
			config = config_new();
		else
			config_reset(config);
		if(config == NULL || config_load(config, name) != 0)
		{
			desktop_error(NULL, NULL, 1); /* XXX */
			continue;
		}
		q = config_get(config, section, "Name");
		r = config_get(config, section, "Type");
		if(q == NULL || r == NULL)
			continue;
		/* remember the path */
		config_set(config, NULL, "path", name);
		config_set(config, NULL, "datadir", path);
		desktop->apps = g_slist_insert_sorted(desktop->apps, config,
				_categories_apps_compare);
		config = NULL;
	}
	free(name);
	closedir(dir);
	if(config != NULL)
		config_delete(config);
}

static void _refresh_loop_categories_xdg(Desktop * desktop,
		void (*callback)(Desktop * desktop, char const * path,
			char const * apppath))
{
	char const * path;
	char * p;
	size_t i;
	size_t j;

	if((path = getenv("XDG_DATA_DIRS")) == NULL || strlen(path) == 0)
		/* FIXME use DATADIR */
		path = "/usr/local/share:/usr/share";
	if((p = strdup(path)) == NULL)
	{
		desktop_error(NULL, "strdup", 1);
		return;
	}
	for(i = 0, j = 0;; i++)
		if(p[i] == '\0')
		{
			_refresh_loop_categories_xdg_path(desktop, callback,
					&p[j]);
			break;
		}
		else if(p[i] == ':')
		{
			p[i] = '\0';
			_refresh_loop_categories_xdg_path(desktop, callback,
					&p[j]);
			j = i + 1;
		}
	free(p);
	_refresh_loop_categories_xdg_home(desktop, callback);
}

static void _refresh_loop_categories_xdg_home(Desktop * desktop,
		void (*callback)(Desktop * desktop, char const * path,
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
		_refresh_loop_categories_xdg_path(desktop, callback, path);
		return;
	}
	/* fallback to "$HOME/.local/share" */
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	len = strlen(homedir) + 1 + sizeof(fallback);
	if((p = malloc(len)) == NULL)
	{
		desktop_error(NULL, homedir, 1);
		return;
	}
	snprintf(p, len, "%s/%s", homedir, fallback);
	_refresh_loop_categories_xdg_path(desktop, callback, p);
	free(p);
}

static void _refresh_loop_categories_xdg_path(Desktop * desktop,
		void (*callback)(Desktop * desktop, char const * path,
			char const * apppath), char const * path)
{
	const char applications[] = "/applications";
	char * apppath;

	if((apppath = string_new_append(path, applications, NULL)) == NULL)
		desktop_error(NULL, path, 1);
	callback(desktop, path, apppath);
	string_delete(apppath);
}

static gint _categories_apps_compare(gconstpointer a, gconstpointer b)
{
	Config * ca = (Config *)a;
	Config * cb = (Config *)b;
	char const * cap;
	char const * cbp;
	const char section[] = "Desktop Entry";
	const char variable[] = "Name";

	/* these should not fail */
	cap = config_get(ca, section, variable);
	cbp = config_get(cb, section, variable);
	return string_compare(cap, cbp);
}

static int _refresh_loop_files(Desktop * desktop)
{
	struct dirent * de;
	String * p;
	DesktopIcon * desktopicon;

	while((de = browser_vfs_readdir(desktop->refresh_dir)) != NULL)
	{
		if(de->d_name[0] == '.')
		{
			if(de->d_name[1] == '\0')
				continue;
			if(de->d_name[1] == '.' && de->d_name[2] == '\0')
				continue;
			if(desktop->show_hidden == 0)
				continue;
		}
		if(_refresh_loop_lookup(desktop, de->d_name) == 1)
			continue;
		break;
	}
	if(de == NULL)
		return -1;
	if((p = string_new_append(desktop->path, "/", de->d_name, NULL))
			== NULL)
		return -_desktop_serror(desktop, de->d_name, 1);
	if((desktopicon = desktopicon_new(desktop, de->d_name, p)) != NULL)
		desktop_icon_add(desktop, desktopicon);
	string_delete(p);
	return 0;
}

static int _refresh_loop_lookup(Desktop * desktop, char const * name)
{
	size_t i;
	char const * p;

	for(i = 0; i < desktop->icon_cnt; i++)
	{
		if(desktopicon_get_updated(desktop->icon[i]) == TRUE)
			continue;
		if((p = desktopicon_get_path(desktop->icon[i])) == NULL
				|| (p = strrchr(p, '/')) == NULL)
			continue;
		if(strcmp(name, ++p) != 0)
			continue;
		desktopicon_set_updated(desktop->icon[i], TRUE);
		return 1;
	}
	return 0;
}


/* error */
static int _error(char const * message, int ret)
{
	fputs("desktop: ", stderr);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-H|-V][-a|-c|-f|-h|-n][-m monitor][-N][-w]\n"
"  -H	Place icons horizontally\n"
"  -V	Place icons vertically\n"
"  -a	Display the applications registered\n"
"  -c	Sort the applications registered by category\n"
"  -f	Display contents of the desktop folder (default)\n"
"  -h	Display the homescreen\n"
"  -m	Monitor where to display the desktop\n"
"  -n	Do not display icons on the desktop\n"
"  -N	Do not intercept mouse clicks on the desktop\n"
"  -w	Draw the desktop as a window\n"), PROGNAME);
	return 1;
}


/* main */
int main(int argc, char * argv[])
{
	int o;
	Desktop * desktop;
	DesktopPrefs prefs;
	char * p;

	if(setlocale(LC_ALL, "") == NULL)
		_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	prefs.alignment = -1;
	prefs.icons = -1;
	prefs.monitor = -1;
	prefs.popup = 1;
	prefs.window = 0;
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "HVacfhm:nNw")) != -1)
		switch(o)
		{
			case 'H':
				prefs.alignment = DESKTOP_ALIGNMENT_HORIZONTAL;
				break;
			case 'V':
				prefs.alignment = DESKTOP_ALIGNMENT_VERTICAL;
				break;
			case 'a':
				prefs.icons = DESKTOP_ICONS_APPLICATIONS;
				break;
			case 'c':
				prefs.icons = DESKTOP_ICONS_CATEGORIES;
				break;
			case 'f':
				prefs.icons = DESKTOP_ICONS_FILES;
				break;
			case 'h':
				prefs.icons = DESKTOP_ICONS_HOMESCREEN;
				break;
			case 'm':
				prefs.monitor = strtol(optarg, &p, 0);
				if(optarg[0] == '\0' || *p != '\0')
					return _usage();
				break;
			case 'n':
				prefs.icons = DESKTOP_ICONS_NONE;
				break;
			case 'N':
				prefs.popup = 0;
				break;
			case 'w':
				prefs.window = 1;
				break;
			default:
				return _usage();
		}
	if(optind < argc)
		return _usage();
	if((desktop = desktop_new(&prefs)) == NULL)
	{
		gtk_main();
		return 2;
	}
	gtk_main();
	desktop_delete(desktop);
	return 0;
}
