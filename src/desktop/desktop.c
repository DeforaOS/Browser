/* $Id$ */
/* Copyright (c) 2007-2018 Pierre Pronchery <khorben@defora.org> */
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
/* TODO:
 * - let the user define the desktop folder (possibly default to FDO's)
 * - track multiple selection on delete/properties
 * - use the MimeHandler class from libDesktop */



#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
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
#include "Browser/desktop.h"
#include "Browser/vfs.h"
#include "desktopicon.h"
#include "desktopiconwindow.h"
#include "desktop.h"
#include "../../config.h"
#define _(string) gettext(string)
#define N_(string) string

#define COMMON_SYMLINK
#include "../common.c"


/* constants */
#ifndef PROGNAME_DESKTOP
# define PROGNAME_DESKTOP	"desktop"
#endif
#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
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
	unsigned int icons_size;
	DesktopIconWindow ** icons;
	size_t icons_cnt;

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
#if GTK_CHECK_VERSION(3, 0, 0)
	cairo_t * cairo;
#else
	GdkPixmap * pixmap;
#endif
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
static int _desktop_perror(Desktop * desktop, char const * message, int ret);
static int _desktop_serror(Desktop * desktop, char const * message, int ret);

/* accessors */
static Config * _desktop_get_config(Desktop * desktop);
static int _desktop_get_monitor_properties(Desktop * desktop, int monitor,
		GdkRectangle * geometry, GdkRectangle * workarea,
		int * scale, int * width_mm, int * height_mm);
static int _desktop_get_properties(Desktop * desktop, GdkRectangle * geometry,
		GdkRectangle * workarea, int * scale,
		int * width_mm, int * height_mm);
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
static void _new_window(Desktop * desktop, GdkEventMask * mask);
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
	desktop->prefs.window = -1;
	if(prefs != NULL)
		desktop->prefs = *prefs;
	desktop->font = NULL;
	/* workarea */
	desktop->screen = gdk_screen_get_default();
	desktop->display = gdk_screen_get_display(desktop->screen);
	desktop->root = gdk_screen_get_root_window(desktop->screen);
	/* icons */
	desktop->icons_size = DESKTOPICON_ICON_SIZE;
	/* common */
	if((desktop->home = getenv("HOME")) == NULL
			&& (desktop->home = g_get_home_dir()) == NULL)
		desktop->home = "/";
	/* internal */
	desktop->theme = gtk_icon_theme_get_default();
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
	_new_window(desktop, &mask);
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
		desktop->file = gtk_icon_theme_load_icon(desktop->theme, *p,
				desktop->icons_size, 0, NULL);
	for(p = folder; *p != NULL && desktop->folder == NULL; p++)
		desktop->folder = gtk_icon_theme_load_icon(desktop->theme, *p,
				desktop->icons_size, 0, NULL);
}

static void _new_window(Desktop * desktop, GdkEventMask * mask)
{
	Config * config;
	String const * p;

	if(desktop->prefs.window < 0
			&& (config = _desktop_get_config(desktop)) != NULL)
	{
		if((p = config_get(config, "background", "window")) != NULL)
			desktop->prefs.window = strtol(p, NULL, 10);
		config_delete(config);
	}
#if GTK_CHECK_VERSION(3, 0, 0)
	if(desktop->prefs.window != 0)
#else
	if(desktop->prefs.window > 0)
#endif
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
			*mask |= GDK_BUTTON_PRESS_MASK;
	}
}

static int _on_message(void * data, uint32_t value1, uint32_t value2,
		uint32_t value3)
{
	Desktop * desktop = data;
	DesktopMessage message;
	DesktopAlignment alignment;
	DesktopIcons icons;
	DesktopLayout layout;
	(void) value3;

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
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_New"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("document-new",
				GTK_ICON_SIZE_MENU));
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	/* submenu for new documents */
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Folder"));
	image = gtk_image_new_from_icon_name("folder-new", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_new_folder), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Symbolic link..."));
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_symlink), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Text file"));
	image = gtk_image_new_from_icon_name("stock_new-text",
			GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_new_text_file), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	/* edition */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Paste"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("edit-paste",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_popup_paste), desktop);
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	/* preferences */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(desktop->menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Preferences"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("gtk-preferences",
				GTK_ICON_SIZE_MENU));
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
		_desktop_perror(desktop, path, 0);
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
		_desktop_perror(desktop, path, 0);
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
		_desktop_perror(desktop, desktop->path, 0);
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
	for(i = 0; i < desktop->icons_cnt; i++)
		desktopiconwindow_delete(desktop->icons[i]);
	free(desktop->icons);
	if(desktop->mime != NULL)
		mime_delete(desktop->mime);
	g_slist_foreach(desktop->apps, (GFunc)mimehandler_delete, NULL);
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
	DesktopIcon * icon;
	size_t len;
	char const * path;
	unsigned char * p;

	seldata->format = 0;
	seldata->data = NULL;
	seldata->length = 0;
	for(i = 0; i < desktop->icons_cnt; i++)
	{
		icon = desktopiconwindow_get_icon(desktop->icons[i]);
		if(desktopicon_get_selected(icon) != TRUE)
			continue;
		if((path = desktopicon_get_path(icon)) == NULL)
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


/* desktop_get_icon_size */
void desktop_get_icon_size(Desktop * desktop, unsigned int * width,
		unsigned int * height, unsigned int * size)
{
	unsigned int h;

	if(width != NULL)
		*width = desktop->icons_size * 2;
	if(height != NULL)
	{
		if((h = pango_font_description_get_size(desktop->font)) > 0)
			*height = desktop->icons_size + ((h >> 10) * 3 + 8);
		else
			*height = desktop->icons_size * 2;
	}
	if(size != NULL)
		*size = desktop->icons_size;
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
	unsigned int x = desktop->workarea.x;
	unsigned int y = desktop->workarea.y;
	unsigned int width = x + desktop->workarea.width;
	unsigned int iwidth;
	unsigned int iheight;

	desktop_get_icon_size(desktop, &iwidth, &iheight, NULL);
	for(i = 0; i < desktop->icons_cnt; i++)
	{
		if(x + iwidth > width)
		{
			y += iheight;
			x = desktop->workarea.x;
		}
		desktopiconwindow_move(desktop->icons[i], x, y);
		x += iwidth;
	}
}

static void _alignment_vertical(Desktop * desktop)
{
	size_t i;
	unsigned int x = desktop->workarea.x;
	unsigned int y = desktop->workarea.y;
	unsigned int height = desktop->workarea.y + desktop->workarea.height;
	unsigned int iwidth;
	unsigned int iheight;

	desktop_get_icon_size(desktop, &iwidth, &iheight, NULL);
	for(i = 0; i < desktop->icons_cnt; i++)
	{
		if(y + iheight > height)
		{
			x += iwidth;
			y = desktop->workarea.y;
		}
		desktopiconwindow_move(desktop->icons[i], x, y);
		y += iheight;
	}
}


/* desktop_set_icons */
static int _icons_applications(Desktop * desktop);
static int _icons_categories(Desktop * desktop);
static int _icons_files(Desktop * desktop);
static int _icons_files_add_home(Desktop * desktop);
static int _icons_homescreen(Desktop * desktop);
static int _icons_homescreen_subdir(Desktop * desktop, char const * subdir,
		char const * name);
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
		return -_desktop_serror(desktop, _("Back"), 1);
	desktopicon_set_callback(desktopicon, _icons_set_categories, NULL);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	icon = gtk_icon_theme_load_icon(desktop->theme, "back",
			desktop->icons_size, 0, NULL);
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
		return -_desktop_serror(desktop, _("Back"), 1);
	desktopicon_set_callback(desktopicon, _icons_set_homescreen, NULL);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	icon = gtk_icon_theme_load_icon(desktop->theme, "back",
			desktop->icons_size, 0, NULL);
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
		return -_desktop_perror(NULL, NULL, 1);
	snprintf(desktop->path, desktop->path_cnt, "%s/%s", desktop->home,
			path);
	if(stat(desktop->path, &st) == 0)
	{
		if(!S_ISDIR(st.st_mode))
			return _desktop_error(NULL, desktop->path,
					strerror(ENOTDIR), 1);
	}
	else if(errno != ENOENT || mkdir(desktop->path, 0777) != 0)
		return _desktop_perror(NULL, desktop->path, 1);
	return 0;
}

static int _icons_files_add_home(Desktop * desktop)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;

	if((desktopicon = desktopicon_new(desktop, _("Home"), desktop->home))
			== NULL)
		return -_desktop_serror(desktop, _("Home"), 1);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	icon = gtk_icon_theme_load_icon(desktop->theme, "gnome-home",
			desktop->icons_size, 0, NULL);
	if(icon == NULL)
		icon = gtk_icon_theme_load_icon(desktop->theme, "gnome-fs-home",
				desktop->icons_size, 0, NULL);
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
	char const * paths[] =
	{
#ifdef EMBEDDED
		/* FIXME let this be configurable */
		"deforaos-phone-contacts.desktop",
		"deforaos-phone-dialer.desktop",
		"deforaos-phone-messages.desktop",
#endif
		NULL
	};
	char const ** p;

	if((desktopicon = desktopicon_new(desktop, _("Applications"), NULL))
			== NULL)
		return _desktop_serror(desktop, _("Applications"), 1);
	desktopicon_set_callback(desktopicon, _icons_set_categories, NULL);
	desktopicon_set_immutable(desktopicon, TRUE);
	icon = gtk_icon_theme_load_icon(desktop->theme, "gnome-applications",
			desktop->icons_size, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	_desktop_icon_add(desktop, desktopicon);
	for(p = paths; *p != NULL; p++)
		_icons_homescreen_subdir(desktop, DATADIR "/applications", *p);
	return 0;
}

static int _icons_homescreen_subdir(Desktop * desktop, char const * subdir,
		char const * name)
{
	DesktopIcon * desktopicon;
	String * q;

	if((q = string_new_append(subdir, "/", name, NULL)) == NULL)
		return -_desktop_error(NULL, name, error_get(NULL), 1);
	if(access(q, R_OK) == 0
			&& (desktopicon = desktopicon_new_application(desktop,
					q, DATADIR)) != NULL)
		_desktop_icon_add(desktop, desktopicon);
	string_delete(q);
	return 0;
}

static void _icons_reset(Desktop * desktop)
{
	size_t i;
	DesktopIcon * icon;

	if(desktop->path != NULL)
		free(desktop->path);
	desktop->path = NULL;
	desktop->path_cnt = 0;
	for(i = 0; i < desktop->icons_cnt; i++)
	{
		icon = desktopiconwindow_get_icon(desktop->icons[i]);
		desktopicon_set_immutable(icon, FALSE);
		desktopicon_set_updated(icon, FALSE);
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
int desktop_error(Desktop * desktop, char const * message, char const * error,
		int ret)
{
	return _desktop_error(desktop, message, error, ret);
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
	g_slist_foreach(desktop->apps, (GFunc)mimehandler_delete, NULL);
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
		_desktop_perror(NULL, desktop->path, 1);
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
static void _reset_icons_size(Desktop * desktop, Config * config);
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
	DesktopIcon * icon;
	int j;

	_reset_icons_colors(desktop, config);
	_reset_icons_font(desktop, config);
	_reset_icons_size(desktop, config);
	for(i = 0; i < desktop->icons_cnt; i++)
	{
		icon = desktopiconwindow_get_icon(desktop->icons[i]);
		desktopicon_set_background(icon, &desktop->background);
		desktopicon_set_font(icon, desktop->font);
		desktopicon_set_foreground(icon, &desktop->foreground);
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

static void _reset_icons_size(Desktop * desktop, Config * config)
{
	String const * p;
	char * q;
	int size;

	/* icons size */
	if((p = config_get(config, "icons", "size")) == NULL
			|| (size = strtol(p, &q, 10)) <= 0)
		size = DESKTOPICON_ICON_SIZE;
	desktop->icons_size = size;
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
	qsort(desktop->icons, desktop->icons_cnt, sizeof(void *),
			_align_compare);
	desktop_set_alignment(desktop, desktop->prefs.alignment);
}

static int _align_compare(const void * a, const void * b)
{
	DesktopIconWindow * wina = *(DesktopIconWindow**)a;
	DesktopIconWindow * winb = *(DesktopIconWindow**)b;
	DesktopIcon * icona;
	DesktopIcon * iconb;
	gboolean firsta;
	gboolean firstb;
	gboolean dira;
	gboolean dirb;

	icona = desktopiconwindow_get_icon(wina);
	iconb = desktopiconwindow_get_icon(winb);
	firsta = desktopicon_get_first(icona);
	firstb = desktopicon_get_first(iconb);
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
	DesktopIcon * icon;

	for(i = 0; i < desktop->icons_cnt; i++)
	{
		icon = desktopiconwindow_get_icon(desktop->icons[i]);
		desktopicon_set_selected(icon, TRUE);
	}
}


/* desktop_select_above */
void desktop_select_above(Desktop * desktop, DesktopIcon * icon)
	/* FIXME icons may be wrapped */
{
	size_t i;
	DesktopIcon * j;

	for(i = 1; i < desktop->icons_cnt; i++)
	{
		j = desktopiconwindow_get_icon(desktop->icons[i]);
		if(j == icon)
		{
			desktopicon_set_selected(j, TRUE);
			return;
		}
	}
}


/* desktop_select_under */
void desktop_select_under(Desktop * desktop, DesktopIcon * icon)
	/* FIXME icons may be wrapped */
{
	size_t i;
	DesktopIcon * j;

	for(i = 0; i < desktop->icons_cnt; i++)
	{
		j = desktopiconwindow_get_icon(desktop->icons[i]);
		if(j == icon && i + 1 < desktop->icons_cnt)
		{
			desktopicon_set_selected(j, TRUE);
			return;
		}
	}
}


/* desktop_unselect_all */
void desktop_unselect_all(Desktop * desktop)
{
	size_t i;
	DesktopIcon * icon;

	for(i = 0; i < desktop->icons_cnt; i++)
	{
		icon = desktopiconwindow_get_icon(desktop->icons[i]);
		desktopicon_set_selected(icon, FALSE);
	}
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
			GTK_BUTTONS_CLOSE, "%s%s%s",
			(message != NULL) ? message : "",
			(message != NULL && error != NULL) ? ": " : "",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s%s%s", (message != NULL) ? message : "",
			(message != NULL && error != NULL) ? ": " : "",
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
	fprintf(stderr, "%s: %s%s%s\n", PROGNAME_DESKTOP,
			(message != NULL) ? message : "",
			(message != NULL) ? ": " : "", error);
	return ret;
}


/* desktop_perror */
static int _desktop_perror(Desktop * desktop, char const * message, int ret)
{
	return _desktop_error(desktop, message, strerror(errno), ret);
}


/* desktop_serror */
static int _desktop_serror(Desktop * desktop, char const * message, int ret)
{
	return _desktop_error(desktop, message, error_get(NULL), ret);
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


/* desktop_get_monitor_properties */
#if !GTK_CHECK_VERSION(3, 22, 0)
static void _monitor_properties_workarea(Desktop * desktop, int monitor,
		GdkRectangle * workarea);
#endif

static int _desktop_get_monitor_properties(Desktop * desktop, int monitor,
		GdkRectangle * geometry, GdkRectangle * workarea,
		int * scale, int * width_mm, int * height_mm)
{
#if GTK_CHECK_VERSION(3, 22, 0)
	GdkMonitor * m;
#endif
	int s;

	if(monitor < 0)
	{
		_desktop_get_properties(desktop, geometry, workarea, scale,
				width_mm, height_mm);
		return 0;
	}
#if GTK_CHECK_VERSION(3, 22, 0)
	if((m = gdk_display_get_monitor(desktop->display, monitor)) == NULL)
		return -1;
	if(geometry != NULL)
		gdk_monitor_get_geometry(m, geometry);
	if(workarea != NULL)
		gdk_monitor_get_workarea(m, workarea);
	if(scale != NULL || geometry != NULL)
	{
		if((s = gdk_monitor_get_scale_factor(m)) != 1
				&& geometry != NULL)
		{
			geometry->x *= s;
			geometry->y *= s;
			geometry->width *= s;
			geometry->height *= s;
		}
		if(scale != NULL)
			*scale = s;
	}
	if(width_mm != NULL)
		*width_mm = gdk_monitor_get_width_mm(m);
	if(height_mm != NULL)
		*height_mm = gdk_monitor_get_height_mm(m);
#else
	if(geometry != NULL)
		gdk_screen_get_monitor_geometry(desktop->screen, monitor,
				geometry);
	if(workarea != NULL)
		_monitor_properties_workarea(desktop, monitor, workarea);
	if(scale != NULL)
		*scale = 1;
	if(width_mm != NULL)
		*width_mm = gdk_screen_get_monitor_width_mm(desktop->screen,
				monitor);
	if(height_mm != NULL)
		*height_mm = gdk_screen_get_monitor_height_mm(desktop->screen,
				monitor);
#endif
	return 0;
}

#if !GTK_CHECK_VERSION(3, 22, 0)
static void _monitor_properties_workarea(Desktop * desktop, int monitor,
		GdkRectangle * workarea)
{
	GdkRectangle d;

	_desktop_get_properties(desktop, NULL, &d, NULL, NULL, NULL);
	gdk_screen_get_monitor_geometry(desktop->screen, monitor, workarea);
	if(d.x >= workarea->x
			&& d.x + d.width <= workarea->x + workarea->width
			&& d.y >= workarea->y
			&& d.y + d.height <= workarea->y + workarea->height)
		*workarea = d;
# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() (%d, %d) %dx%d\n", __func__,
			workarea->x, workarea->y,
			workarea->width, workarea->height);
# endif
}
#endif


/* desktop_get_properties */
static void _properties_workarea(Desktop * desktop, GdkRectangle * workarea);

static int _desktop_get_properties(Desktop * desktop, GdkRectangle * geometry,
		GdkRectangle * workarea, int * scale,
		int * width_mm, int * height_mm)
{
	if(geometry != NULL)
	{
		geometry->x = 0;
		geometry->y = 0;
		geometry->width = gdk_screen_get_width(desktop->screen);
		geometry->height = gdk_screen_get_height(desktop->screen);
	}
	if(workarea != NULL)
		_properties_workarea(desktop, workarea);
	if(scale != NULL)
		*scale = 1;
	if(width_mm != NULL)
		*width_mm = gdk_screen_get_width_mm(desktop->screen);
	if(height_mm != NULL)
		*height_mm = gdk_screen_get_height_mm(desktop->screen);
	return 0;
}

static void _properties_workarea(Desktop * desktop, GdkRectangle * workarea)
{
	Atom atom;
	Atom type;
	int format;
	unsigned long cnt;
	unsigned long bytes;
	unsigned char * p = NULL;
	unsigned long * u;
	int res;

	atom = gdk_x11_get_xatom_by_name("_NET_WORKAREA");
	gdk_error_trap_push();
	res = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(desktop->display),
				GDK_WINDOW_XID(desktop->root), atom, 0,
				G_MAXLONG, False, XA_CARDINAL, &type, &format,
				&cnt, &bytes, &p);
	if(gdk_error_trap_pop() || res != Success || cnt < 4)
		gdk_screen_get_monitor_geometry(desktop->screen, 0, workarea);
	else
	{
		u = (unsigned long *)p;
		workarea->x = u[0];
		workarea->y = u[1];
		if((workarea->width = u[2]) == 0
				|| (workarea->height = u[3]) == 0)
			gdk_screen_get_monitor_geometry(desktop->screen, 0,
					workarea);
	}
	if(p != NULL)
		XFree(p);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() (%d, %d) %dx%d\n", __func__,
			workarea->x, workarea->y,
			workarea->width, workarea->height);
#endif
}


/* desktop_get_workarea */
static int _desktop_get_workarea(Desktop * desktop)
{
	_desktop_get_monitor_properties(desktop, desktop->prefs.monitor, NULL,
			&desktop->workarea, NULL, NULL, NULL);
	desktop_icons_align(desktop);
	return 0;
}


/* useful */
/* desktop_background */
static gboolean _background_how_centered(Desktop * desktop,
		GdkRectangle * window, char const * filename, GError ** error);
static gboolean _background_how_scaled(Desktop * desktop, GdkRectangle * window,
		char const * filename, GError ** error);
static gboolean _background_how_scaled_ratio(Desktop * desktop,
		GdkRectangle * window, char const * filename, GError ** error);
static gboolean _background_how_tiled(Desktop * desktop, GdkRectangle * window,
		char const * filename, GError ** error);
static void _background_monitor(Desktop * desktop, char const * filename,
		DesktopHows how, gboolean extend, GdkRectangle * window,
		int monitor);
static void _background_monitors(Desktop * desktop, char const * filename,
		DesktopHows how, gboolean extend, GdkRectangle * window);

#if GTK_CHECK_VERSION(3, 0, 0)
static void _desktop_draw_background(Desktop * desktop, GdkRGBA * color,
		char const * filename, DesktopHows how, gboolean extend)
#else
static void _desktop_draw_background(Desktop * desktop, GdkColor * color,
		char const * filename, DesktopHows how, gboolean extend)
#endif
{
#if !GTK_CHECK_VERSION(3, 0, 0)
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
	desktop->cairo = gdk_cairo_create(desktop->back != NULL
			? desktop->back : desktop->root);
	cairo_set_source_rgba(desktop->cairo, color->red, color->green,
			color->blue, 1.0);
	gdk_cairo_rectangle(desktop->cairo, &desktop->window);
	if(filename != NULL)
		/* draw the background */
		_background_monitors(desktop, filename, how, extend,
				&desktop->window);
	cairo_paint(desktop->cairo);
	cairo_destroy(desktop->cairo);
	desktop->cairo = NULL;
#else
	/* draw default color */
	desktop->pixmap = gdk_pixmap_new(desktop->back, desktop->window.width,
			desktop->window.height, -1);
	gc = gdk_gc_new(desktop->pixmap);
	gdk_gc_set_rgb_fg_color(gc, color);
	gdk_draw_rectangle(desktop->pixmap, gc, TRUE, 0, 0,
			desktop->window.width, desktop->window.height);
	if(filename != NULL)
		/* draw the background */
		_background_monitors(desktop, filename, how, extend,
				&desktop->window);
	if(desktop->desktop != NULL)
	{
		style = gtk_style_new();
		style->bg_pixmap[GTK_STATE_NORMAL] = desktop->pixmap;
		gtk_widget_set_style(desktop->desktop, style);
	}
	else
	{
		gdk_window_set_back_pixmap(desktop->back, desktop->pixmap,
				FALSE);
		gdk_window_clear(desktop->back);
		gdk_pixmap_unref(desktop->pixmap);
	}
	desktop->pixmap = NULL;
#endif
}

static gboolean _background_how_centered(Desktop * desktop,
		GdkRectangle * window, char const * filename, GError ** error)
{
	GdkPixbuf * background;
	gint w;
	gint h;
	gint x;
	gint y;

	if((background = gdk_pixbuf_new_from_file(filename, error)) == NULL)
		return FALSE;
	w = gdk_pixbuf_get_width(background);
	h = gdk_pixbuf_get_height(background);
	x = (window->width - w) / 2 + window->x;
	y = (window->height - h) / 2 + window->y;
#if GTK_CHECK_VERSION(3, 0, 0)
	gdk_cairo_set_source_pixbuf(desktop->cairo, background, x, y);
#else
	gdk_draw_pixbuf(desktop->pixmap, NULL, background, 0, 0, x, y, w, h,
			GDK_RGB_DITHER_NONE, 0, 0);
#endif
	g_object_unref(background);
	return TRUE;
}

static gboolean _background_how_scaled(Desktop * desktop, GdkRectangle * window,
		char const * filename, GError ** error)
{
	GdkPixbuf * background;
	gint w;
	gint h;
	gint x;
	gint y;

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
		return FALSE;
	w = gdk_pixbuf_get_width(background);
	h = gdk_pixbuf_get_height(background);
	x = (window->width - w) / 2 + window->x;
	y = (window->height - h) / 2 + window->y;
#if GTK_CHECK_VERSION(3, 0, 0)
	gdk_cairo_set_source_pixbuf(desktop->cairo, background, x, y);
#else
	gdk_draw_pixbuf(desktop->pixmap, NULL, background, 0, 0, x, y, w, h,
			GDK_RGB_DITHER_NONE, 0, 0);
#endif
	g_object_unref(background);
	return TRUE;
}

static gboolean _background_how_scaled_ratio(Desktop * desktop,
		GdkRectangle * window, char const * filename, GError ** error)
{
#if GTK_CHECK_VERSION(2, 4, 0)
	GdkPixbuf * background;
	gint w;
	gint h;
	gint x;
	gint y;

	background = gdk_pixbuf_new_from_file_at_size(filename, window->width,
			window->height, error);
	if(background == NULL)
		return FALSE;
	w = gdk_pixbuf_get_width(background);
	h = gdk_pixbuf_get_height(background);
	x =(window->width - w) / 2 + window->x;
	y = (window->height - h) / 2 + window->y;
#if GTK_CHECK_VERSION(3, 0, 0)
	gdk_cairo_set_source_pixbuf(desktop->cairo, background, x, y);
#else
	gdk_draw_pixbuf(desktop->pixmap, NULL, background, 0, 0, x, y, w, h,
			GDK_RGB_DITHER_NONE, 0, 0);
#endif
	g_object_unref(background);
	return TRUE;
#else
	return _background_how_scaled(desktop, window, filename, error);
#endif
}

static gboolean _background_how_tiled(Desktop * desktop, GdkRectangle * window,
		char const * filename, GError ** error)
{
	GdkPixbuf * background;
	gint w;
	gint h;
	gint i;
	gint j;

	if((background = gdk_pixbuf_new_from_file(filename, error)) == NULL)
		return FALSE;
	w = gdk_pixbuf_get_width(background);
	h = gdk_pixbuf_get_height(background);
	for(j = 0; j < window->height; j += h)
		for(i = 0; i < window->width; i += w)
#if GTK_CHECK_VERSION(3, 0, 0)
			gdk_cairo_set_source_pixbuf(desktop->cairo, background,
					i + window->x, j + window->y);
#else
			gdk_draw_pixbuf(desktop->pixmap, NULL, background, 0, 0,
					i + window->x, j + window->y, w, h,
					GDK_RGB_DITHER_NONE, 0, 0);
#endif
	g_object_unref(background);
	return TRUE;
}

static void _background_monitor(Desktop * desktop, char const * filename,
		DesktopHows how, gboolean extend, GdkRectangle * window,
		int monitor)
{
	GError * error = NULL;

	if(extend != TRUE)
		_desktop_get_monitor_properties(desktop, monitor, window, NULL,
				NULL, NULL, NULL);
	switch(how)
	{
		case DESKTOP_HOW_NONE:
			break;
		case DESKTOP_HOW_CENTERED:
			_background_how_centered(desktop, window, filename,
					&error);
			break;
		case DESKTOP_HOW_SCALED_RATIO:
			_background_how_scaled_ratio(desktop, window, filename,
					&error);
			break;
		case DESKTOP_HOW_TILED:
			_background_how_tiled(desktop, window, filename,
					&error);
			break;
		case DESKTOP_HOW_SCALED:
			_background_how_scaled(desktop, window, filename,
					&error);
			break;
	}
	if(error != NULL)
	{
		desktop_error(desktop, NULL, error->message, 1);
		g_error_free(error);
	}
}

static void _background_monitors(Desktop * desktop, char const * filename,
		DesktopHows how, gboolean extend, GdkRectangle * window)
{
	gint n;
	gint i;

	n = (extend != TRUE) ?
#if GTK_CHECK_VERSION(3, 22, 0)
		gdk_display_get_n_monitors(desktop->display)
#else
		gdk_screen_get_n_monitors(desktop->screen)
#endif
		: 1;
	for(i = 0; i < n; i++)
		_background_monitor(desktop, filename, how, extend, window, i);
}


/* desktop_icon_add */
static int _desktop_icon_add(Desktop * desktop, DesktopIcon * icon)
{
	DesktopIconWindow * window;
	DesktopIconWindow ** p;

	if((p = realloc(desktop->icons, sizeof(*p) * (desktop->icons_cnt + 1)))
			== NULL)
		return -_desktop_perror(desktop, desktopicon_get_name(icon), 1);
	desktop->icons = p;
	if((window = desktopiconwindow_new(icon)) == NULL)
		return -_desktop_serror(desktop, desktopicon_get_name(icon), 1);
	desktop->icons[desktop->icons_cnt++] = window;
	desktopicon_set_background(icon, &desktop->background);
	desktopicon_set_font(icon, desktop->font);
	desktopicon_set_foreground(icon, &desktop->foreground);
	desktopiconwindow_show(window);
	return 0;
}


/* desktop_icon_remove */
static int _desktop_icon_remove(Desktop * desktop, DesktopIcon * icon)
{
	size_t i;
	DesktopIcon * j;
	DesktopIconWindow ** p;

	for(i = 0; i < desktop->icons_cnt; i++)
	{
		j = desktopiconwindow_get_icon(desktop->icons[i]);
		if(j != icon)
			continue;
		desktopiconwindow_delete(desktop->icons[i]);
		for(desktop->icons_cnt--; i < desktop->icons_cnt; i++)
			desktop->icons[i] = desktop->icons[i + 1];
		if((p = realloc(desktop->icons, sizeof(*p)
						* (desktop->icons_cnt)))
				!= NULL)
			desktop->icons = p; /* we can ignore errors... */
		else if(desktop->icons_cnt == 0)
			desktop->icons = NULL; /* ...except when it's not one */
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

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 4);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Default color: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_color = gtk_color_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_color, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Wallpaper: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
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
	gtk_file_filter_add_mime_type(filter, "image/xpm");
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
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Position: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
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

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 4);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* icons */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Layout: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
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
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Monitor: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
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
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Background color: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_ibcolor = gtk_color_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_ibcolor, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* foreground color */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Foreground color: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_ifcolor = gtk_color_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_ifcolor, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* font */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Font: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
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
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 4);
	/* selector */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Monitor: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
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
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Resolution: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_monitors_res = gtk_label_new(NULL);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(desktop->pr_monitors_res, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(desktop->pr_monitors_res), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_monitors_res, TRUE, TRUE,
			0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* size */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Size: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	desktop->pr_monitors_size = gtk_label_new(NULL);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(desktop->pr_monitors_size, "halign", GTK_ALIGN_START,
			NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(desktop->pr_monitors_size), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(hbox), desktop->pr_monitors_size, TRUE, TRUE,
			0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	/* refresh */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
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
	if(active-- <= 0
			|| _desktop_get_monitor_properties(desktop, active,
				&geometry, NULL, NULL, &width, &height) != 0)
		_desktop_get_properties(desktop, &geometry, NULL, NULL,
				&width, &height);
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
	gchar * name;
	char buf[32];
# if GTK_CHECK_VERSION(3, 22, 0)
	GdkMonitor * monitor;
	char const * manufacturer;
	char const * model;
	char buf2[64];
# endif
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
# if GTK_CHECK_VERSION(3, 22, 0)
	n = gdk_display_get_n_monitors(desktop->display);
# else
	n = gdk_screen_get_n_monitors(desktop->screen);
# endif
	for(i = 0; i < n; i++)
	{
		snprintf(buf, sizeof(buf), _("Monitor %d"), i);
# if GTK_CHECK_VERSION(3, 22, 0)
		if((monitor = gdk_display_get_monitor(desktop->display, i))
				== NULL)
			continue;
		manufacturer = gdk_monitor_get_manufacturer(monitor);
		model = gdk_monitor_get_model(monitor);
		if(manufacturer != NULL || model != NULL)
		{
			snprintf(buf2, sizeof(buf2), "%s%s%s",
					(manufacturer != NULL)
					? manufacturer : "",
					(manufacturer != NULL && model != NULL)
					? " " : "",
					(model != NULL) ? model : "");
			name = g_strdup(buf2);
		}
		else
			name = NULL;
# else
		name = gdk_screen_get_monitor_plug_name(desktop->screen, i);
# endif
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
	(void) widget;

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
	int i;

	/* XXX not very efficient */
	desktop_reset(desktop);
	/* icons */
	desktop->prefs.icons = gtk_combo_box_get_active(
			GTK_COMBO_BOX(desktop->pr_ilayout));
	/* monitor */
	i = gtk_combo_box_get_active(GTK_COMBO_BOX(desktop->pr_imonitor));
	desktop->prefs.monitor = (i >= 0) ? i - 1 : i;
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

	gtk_widget_hide(desktop->pr_window);
	_on_preferences_response_apply(desktop);
	if((config = _desktop_get_config(desktop)) == NULL)
		return;
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
	if(desktop->prefs.icons >= 0
			&& desktop->prefs.icons < DESKTOP_ICONS_COUNT)
		config_set(config, "icons", "layout",
				_desktop_icons_config[desktop->prefs.icons]);
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
	/* monitor */
	snprintf(buf, sizeof(buf), "%d", desktop->prefs.monitor);
	config_set(config, "icons", "monitor", buf);
	config_set(config, "icons", "show_hidden", desktop->show_hidden
			? "1" : "0");
	/* XXX code duplication */
	if((p = string_new_append(desktop->home, "/" DESKTOPRC, NULL)) != NULL)
	{
		if(config_save(config, p) != 0)
			error_print(PROGNAME_DESKTOP);
		string_delete(p);
	}
	config_delete(config);
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
#ifdef DEBUG
			_desktop_error(NULL, NULL, error->message, 1);
#endif
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
	DesktopIcon * icon;

	for(i = 0; i < desktop->icons_cnt;)
	{
		icon = desktopiconwindow_get_icon(desktop->icons[i]);
		if(desktopicon_get_immutable(icon) == TRUE)
			i++;
		else if(desktopicon_get_updated(icon) != TRUE)
			_desktop_icon_remove(desktop, icon);
		else
		{
			desktopicon_set_updated(icon, FALSE);
			i++;
		}
	}
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
	MimeHandler * handler;
	DesktopCategory * dc = desktop->category;
	String const ** categories;
	size_t i;
	String const * filename;
	DesktopIcon * icon;

	for(p = desktop->apps; p != NULL; p = p->next)
	{
		handler = p->data;
		if(dc != NULL)
		{
			if((categories = mimehandler_get_categories(handler))
					== NULL || categories[0] == NULL)
				continue;
			for(i = 0; categories[i] != NULL; i++)
				if(string_compare(categories[i], dc->category)
						== 0)
					break;
			if(categories[i] == NULL)
				continue;
		}
		filename = mimehandler_get_filename(handler);
		/* FIXME keep track of the datadir */
		if((icon = desktopicon_new_application(desktop, filename, NULL))
				== NULL)
			continue;
		_desktop_icon_add(desktop, icon);
	}
}

static void _refresh_done_categories(Desktop * desktop)
{
	GSList * p;
	MimeHandler * handler;
	DesktopCategory * dc;
	String const ** categories;
	size_t i;
	size_t j;
	String const * filename;
	DesktopIcon * icon;

	for(p = desktop->apps; p != NULL; p = p->next)
	{
		handler = p->data;
		filename = mimehandler_get_filename(handler);
		if((categories = mimehandler_get_categories(handler)) == NULL
				|| categories[0] == NULL)
		{
			/* FIXME keep track of the datadir */
			if((icon = desktopicon_new_application(desktop,
							filename, NULL))
					!= NULL)
				_desktop_icon_add(desktop, icon);
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
			if((icon = desktopicon_new_application(desktop,
							filename, NULL))
					!= NULL)
				_desktop_icon_add(desktop, icon);
			continue;
		}
		if(dc->show == TRUE)
			continue;
		dc->show = TRUE;
		if((icon = desktopicon_new_category(desktop, _(dc->name),
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
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, _(dc->name));
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
		return _desktop_perror(NULL, desktop->path, FALSE);
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
	struct stat st;
	size_t alen;
	struct dirent * de;
	size_t len;
	const char ext[] = ".desktop";
	char * name = NULL;
	char * p;
	MimeHandler * handler;
	(void) path;

	if((dir = browser_vfs_opendir(apppath, &st)) == NULL)
	{
		if(errno != ENOENT)
			_desktop_perror(NULL, apppath, 1);
		return;
	}
	if(st.st_mtime > desktop->refresh_mti)
		desktop->refresh_mti = st.st_mtime;
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
			_desktop_perror(NULL, apppath, 1);
			continue;
		}
		name = p;
		snprintf(name, alen + len + 2, "%s/%s", apppath, de->d_name);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() name=\"%s\" path=\"%s\"\n",
				__func__, name, path);
#endif
		if((handler = mimehandler_new_load(name)) == NULL
				|| mimehandler_can_display(handler) == 0)
		{
			if(handler != NULL)
				mimehandler_delete(handler);
			else
				_desktop_serror(NULL, NULL, 1);
			continue;
		}
		desktop->apps = g_slist_insert_sorted(desktop->apps, handler,
				_categories_apps_compare);
	}
	free(name);
	browser_vfs_closedir(dir);
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
		path = "/usr/local/share:" DATADIR ":/usr/share";
	if((p = strdup(path)) == NULL)
	{
		_desktop_perror(NULL, NULL, 1);
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
		_desktop_perror(NULL, homedir, 1);
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
		_desktop_serror(NULL, path, 1);
	callback(desktop, path, apppath);
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

static int _refresh_loop_files(Desktop * desktop)
{
	const char ext[] = ".desktop";
	struct dirent * de;
	String * p;
	size_t len;
	gchar * q;
	DesktopIcon * desktopicon;
	GError * error = NULL;

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
	/* detect desktop entries */
	if((len = strlen(de->d_name)) > sizeof(ext)
			&& strcmp(&de->d_name[len - sizeof(ext) + 1], ext) == 0)
		desktopicon = desktopicon_new_application(desktop, p,
				desktop->path);
	/* XXX not relative to the current folder */
	else if((q = g_filename_to_utf8(de->d_name, -1, NULL, NULL, &error))
			!= NULL)
	{
		desktopicon = desktopicon_new(desktop, q, p);
		g_free(q);
	}
	else
	{
		desktop_error(NULL, NULL, error->message, 1);
		g_error_free(error);
		desktopicon = desktopicon_new(desktop, de->d_name, p);
	}
	if(desktopicon != NULL)
		desktop_icon_add(desktop, desktopicon);
	string_delete(p);
	return 0;
}

static int _refresh_loop_lookup(Desktop * desktop, char const * name)
{
	size_t i;
	DesktopIcon * icon;
	char const * p;

	for(i = 0; i < desktop->icons_cnt; i++)
	{
		icon = desktopiconwindow_get_icon(desktop->icons[i]);
		if(desktopicon_get_updated(icon) == TRUE)
			continue;
		if((p = desktopicon_get_path(icon)) == NULL
				|| (p = strrchr(p, '/')) == NULL)
			continue;
		if(strcmp(name, ++p) != 0)
			continue;
		desktopicon_set_updated(icon, TRUE);
		return 1;
	}
	return 0;
}
