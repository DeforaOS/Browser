/* $Id$ */
/* Copyright (c) 2010-2016 Pierre Pronchery <khorben@defora.org> */
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
 * 3. Neither the name of the authors nor the names of the contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <System.h>
#include "../../include/Browser/desktop.h"
#include "../../include/Browser/vfs.h"
#include "desktop.h"
#include "desktopicon.h"
#include "../../config.h"
#define _(string) gettext(string)

#define COMMON_DND
#define COMMON_EXEC
#include "../common.c"


/* constants */
#ifndef PROGNAME
# define PROGNAME		"desktop"
#endif
#ifndef PROGNAME_BROWSER
# define PROGNAME_BROWSER	"browser"
#endif
#ifndef PROGNAME_DELETE
# define PROGNAME_DELETE	"delete"
#endif
#ifndef PROGNAME_HTMLAPP
# define PROGNAME_HTMLAPP	"htmlapp"
#endif
#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef BINDIR
# define BINDIR			PREFIX "/bin"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif


/* DesktopIcon */
/* types */
struct _DesktopIcon
{
	Desktop * desktop;
	char * path;
	char * name;
	gboolean isfirst;
	gboolean isdir;
	gboolean isexec;
	char const * mimetype;

	/* applications */
	Config * config;

	/* callback */
	DesktopIconCallback callback;
	gpointer data;

	gboolean confirm;
	gboolean immutable;		/* cannot be deleted */
	gboolean selected;
	gboolean updated;		/* XXX for desktop refresh */

	GtkWidget * image;
	GtkWidget * event;
	GtkWidget * label;
};


/* constants */
static const char _desktop_type_application[] = "Application";
static const char _desktop_type_directory[] = "Directory";
static const char _desktop_type_url[] = "URL";


/* prototypes */
static DesktopIcon * _desktopicon_new_do(Desktop * desktop, GdkPixbuf * image,
		char const * name);

static void _desktopicon_set_icon(DesktopIcon * desktopicon, GdkPixbuf * icon);
static int _desktopicon_set_name(DesktopIcon * desktopicon, char const * name);

/* callbacks */
static gboolean _on_icon_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data);
static gboolean _on_icon_key_press(GtkWidget * widget, GdkEventKey * event,
		gpointer data);
static void _on_icon_drag_data_get(GtkWidget * widget, GdkDragContext * context,
		GtkSelectionData * seldata, guint info, guint time,
		gpointer data);
static void _on_icon_drag_data_received(GtkWidget * widget,
		GdkDragContext * context, gint x, gint y,
		GtkSelectionData * seldata, guint info, guint time,
		gpointer data);
static void _on_icon_open(gpointer data);
static void _on_icon_edit(gpointer data);
static void _on_icon_run(gpointer data);
static void _on_icon_open_with(gpointer data);
static void _on_icon_rename(gpointer data);
static void _on_icon_delete(gpointer data);
static void _on_icon_properties(gpointer data);


/* public */
/* functions */
/* desktopicon_new */
DesktopIcon * desktopicon_new(Desktop * desktop, char const * name,
		char const * path)
{
	DesktopIcon * desktopicon;
	struct stat st;
	struct stat lst;
	struct stat * s = &lst;
	Mime * mime;
	char const * mimetype = NULL;
	gboolean isdir = FALSE;
	gboolean isexec = FALSE;
	GdkPixbuf * image = NULL;
	GError * error = NULL;
	char * p;
	GtkTargetEntry targets[] = { { "deforaos_browser_dnd", 0, 0 } };
	size_t targets_cnt = sizeof(targets) / sizeof(*targets);

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p, \"%s\", \"%s\")\n", __func__,
			(void *)desktop, name, path);
#endif
	if(path != NULL && lstat(path, &lst) == 0)
	{
		if(S_ISLNK(lst.st_mode) && stat(path, &st) == 0)
			s = &st;
		mime = desktop_get_mime(desktop);
		isdir = S_ISDIR(s->st_mode) ? TRUE : FALSE;
		isexec = (isdir == FALSE) && (s->st_mode & S_IXUSR)
			? TRUE : FALSE;
		mimetype = browser_vfs_mime_type(mime, path, s->st_mode);
		image = browser_vfs_mime_icon(mime, path, mimetype, &lst, NULL,
				DESKTOPICON_ICON_SIZE);
	}
	if(name == NULL)
	{
		if((p = g_filename_to_utf8(path, -1, NULL, NULL, &error))
				== NULL)
		{
			fprintf(stderr, "%s%s\n", "desktop: ", error->message);
			name = path;
		}
		else
			name = p;
		if((name = strrchr(name, '/')) != NULL)
			name++;
	}
	if((desktopicon = _desktopicon_new_do(desktop, image, name)) == NULL)
		return NULL;
	if(image != NULL)
		g_object_unref(image);
	gtk_drag_source_set(desktopicon->event, GDK_BUTTON1_MASK, targets,
			targets_cnt, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set(desktopicon->event, GTK_DEST_DEFAULT_ALL, targets,
			targets_cnt, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(desktopicon->event), "drag-data-get",
			G_CALLBACK(_on_icon_drag_data_get), desktopicon);
	g_signal_connect(G_OBJECT(desktopicon->event), "drag-data-received",
			G_CALLBACK(_on_icon_drag_data_received), desktopicon);
	desktopicon->isdir = isdir;
	desktopicon_set_executable(desktopicon, isexec);
	desktopicon->mimetype = mimetype;
	desktopicon->config = NULL;
	if(path != NULL && (desktopicon->path = strdup(path)) == NULL)
	{
		desktopicon_delete(desktopicon);
		return NULL;
	}
	return desktopicon;
}


/* desktopicon_new_application */
static int _new_application_access(char const * path, int mode);
static int _new_application_access_path(char const * path,
		char const * filename, int mode);
static GdkPixbuf * _new_application_icon(Desktop * desktop, char const * icon,
		char const * datadir);

DesktopIcon * desktopicon_new_application(Desktop * desktop, char const * path,
		char const * datadir)
{
	DesktopIcon * desktopicon;
	Config * config;
	const char section[] = "Desktop Entry";
	char const * name;
	char const * icon;
	char const * p;
	GdkPixbuf * image = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p, \"%s\")\n", __func__, (void *)desktop,
			path);
#endif
	if((config = config_new()) == NULL)
		return NULL;
	if(config_load(config, path) != 0
			|| ((p = config_get(config, section, "Hidden")) != NULL
				&& strcmp(p, "true") == 0)
			|| (p = config_get(config, section, "Type")) == NULL
			|| (strcmp(p, _desktop_type_application) != 0
				&& strcmp(p, _desktop_type_directory) != 0
				&& strcmp(p, _desktop_type_url) != 0)
			|| (name = config_get(config, section, "Name")) == NULL
			|| ((p = config_get(config, section, "NoDisplay"))
				!= NULL && strcmp(p, "true") == 0)
			|| ((p = config_get(config, section, "TryExec")) != NULL
				&& _new_application_access(p, X_OK) != 0))
	{
		config_delete(config);
		return NULL;
	}
	if((icon = config_get(config, section, "Icon")) == NULL)
		icon = "application-x-executable";
	image = _new_application_icon(desktop, icon, datadir);
	desktopicon = _desktopicon_new_do(desktop, image, name);
	if(image != NULL)
		g_object_unref(image);
	if(desktopicon == NULL)
	{
		config_delete(config);
		return NULL;
	}
	desktopicon->config = config;
	desktopicon_set_confirm(desktopicon, FALSE);
	desktopicon_set_executable(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	return desktopicon;
}

static int _new_application_access(char const * path, int mode)
{
	int ret = -1;
	char const * p;
	char * q;
	size_t i;
	size_t j;

	if(path[0] == '/')
		return access(path, mode);
	if((p = getenv("PATH")) == NULL)
		return 0;
	if((q = strdup(p)) == NULL)
		return 0;
	errno = ENOENT;
	for(i = 0, j = 0;; i++)
		if(q[i] == '\0')
		{
			ret = _new_application_access_path(&q[j], path, mode);
			break;
		}
		else if(q[i] == ':')
		{
			q[i] = '\0';
			if((ret = _new_application_access_path(&q[j], path,
							mode)) == 0)
				break;
			j = i + 1;
		}
	free(q);
	return ret;
}

static int _new_application_access_path(char const * path,
		char const * filename, int mode)
{
	int ret;
	char * p;
	size_t len;

	len = strlen(path) + 1 + strlen(filename) + 1;
	if((p = malloc(len)) == NULL)
		return -1;
	snprintf(p, len, "%s/%s", path, filename);
	ret = access(p, mode);
	free(p);
	return ret;
}

static GdkPixbuf * _new_application_icon(Desktop * desktop, char const * icon,
		char const * datadir)
{
	const char pixmaps[] = "/pixmaps/";
	const int width = DESKTOPICON_ICON_SIZE;
	const int height = DESKTOPICON_ICON_SIZE;
	String * buf;
	GdkPixbuf * pixbuf = NULL;
	GError * error = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\")\n", __func__, icon, datadir);
#endif
	if(icon[0] == '/')
		pixbuf = gdk_pixbuf_new_from_file_at_size(icon, width, height,
				&error);
	else if(strchr(icon, '.') != NULL)
	{
		if(datadir == NULL)
			datadir = DATADIR;
		if((buf = string_new_append(datadir, pixmaps, icon, NULL))
				!= NULL)
		{
			pixbuf = gdk_pixbuf_new_from_file_at_size(buf, width,
					height, &error);
			string_delete(buf);
		}
	}
	if(error != NULL)
	{
		desktop_error(NULL, NULL, error->message, 1); /* XXX */
		g_error_free(error);
	}
	if(pixbuf == NULL)
		pixbuf = gtk_icon_theme_load_icon(desktop_get_theme(desktop),
				icon, DESKTOPICON_ICON_SIZE, 0, NULL);
	if(pixbuf == NULL)
		pixbuf = desktop_get_file(desktop);
	return pixbuf;
}


/* desktopicon_new_category */
DesktopIcon * desktopicon_new_category(Desktop * desktop, char const * name,
		char const * icon)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * image;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p, \"%s\", \"%s\")\n", __func__,
			(void *)desktop, name, icon);
#endif
	image = gtk_icon_theme_load_icon(desktop_get_theme(desktop), icon,
			DESKTOPICON_ICON_SIZE, 0, NULL);
	if((desktopicon = _desktopicon_new_do(desktop, image, name)) == NULL)
		return NULL;
	desktopicon_set_immutable(desktopicon, TRUE);
	return desktopicon;
}


/* desktopicon_delete */
void desktopicon_delete(DesktopIcon * desktopicon)
{
	if(desktopicon->config != NULL)
		config_delete(desktopicon->config);
	free(desktopicon->name);
	free(desktopicon->path);
	gtk_widget_destroy(desktopicon->event);
	object_delete(desktopicon);
}


/* accessors */
/* desktopicon_get_first */
gboolean desktopicon_get_first(DesktopIcon * desktopicon)
{
	return desktopicon->isfirst;
}


/* desktopicon_get_immutable */
gboolean desktopicon_get_immutable(DesktopIcon * desktopicon)
{
	return desktopicon->immutable;
}


/* desktopicon_get_isdir */
gboolean desktopicon_get_isdir(DesktopIcon * desktopicon)
{
	return desktopicon->isdir;
}


/* desktopicon_get_name */
char const * desktopicon_get_name(DesktopIcon * desktopicon)
{
	return desktopicon->name;
}


/* desktopicon_get_path */
char const * desktopicon_get_path(DesktopIcon * desktopicon)
{
	return desktopicon->path;
}


/* desktopicon_get_selected */
gboolean desktopicon_get_selected(DesktopIcon * desktopicon)
{
	return desktopicon->selected;
}


/* desktopicon_get_updated */
gboolean desktopicon_get_updated(DesktopIcon * desktopicon)
{
	return desktopicon->updated;
}


/* desktopicon_get_widget */
GtkWidget * desktopicon_get_widget(DesktopIcon * desktopicon)
{
	return desktopicon->event;
}


/* desktopicon_set_background */
#if GTK_CHECK_VERSION(3, 0, 0)
void desktopicon_set_background(DesktopIcon * desktopicon, GdkRGBA * color)
#else
void desktopicon_set_background(DesktopIcon * desktopicon, GdkColor * color)
#endif
{
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_background_color(desktopicon->event,
			GTK_STATE_NORMAL, color);
#else
	gtk_widget_modify_bg(desktopicon->event, GTK_STATE_NORMAL, color);
#endif
}


/* desktopicon_set_callback */
void desktopicon_set_callback(DesktopIcon * desktopicon,
		DesktopIconCallback callback, gpointer data)
{
	desktopicon->callback = callback;
	desktopicon->data = data;
}


/* desktopicon_set_confirm */
void desktopicon_set_confirm(DesktopIcon * desktopicon, gboolean confirm)
{
	desktopicon->confirm = confirm;
}


/* desktopicon_set_executable */
void desktopicon_set_executable(DesktopIcon * desktopicon, gboolean executable)
{
	desktopicon->isexec = executable;
}


/* desktopicon_set_first */
void desktopicon_set_first(DesktopIcon * desktopicon, gboolean first)
{
	desktopicon->isfirst = first;
}


/* desktopicon_set_font */
void desktopicon_set_font(DesktopIcon * desktopicon,
		PangoFontDescription * font)
{
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(desktopicon->label, font);
#else
	gtk_widget_modify_font(desktopicon->label, font);
#endif
}


/* desktopicon_set_foreground */
#if GTK_CHECK_VERSION(3, 0, 0)
void desktopicon_set_foreground(DesktopIcon * desktopicon, GdkRGBA * color)
#else
void desktopicon_set_foreground(DesktopIcon * desktopicon, GdkColor * color)
#endif
{
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_color(desktopicon->event, GTK_STATE_NORMAL, color);
#else
	gtk_widget_modify_fg(desktopicon->label, GTK_STATE_NORMAL, color);
#endif
}


/* desktopicon_set_icon */
void desktopicon_set_icon(DesktopIcon * desktopicon, GdkPixbuf * icon)
{
	_desktopicon_set_icon(desktopicon, icon);
#if 0
	_desktopicon_update_transparency(desktopicon);
#endif
}


/* desktopicon_set_immutable */
void desktopicon_set_immutable(DesktopIcon * desktopicon, gboolean immutable)
{
	desktopicon->immutable = immutable;
}


/* desktopicon_set_name */
int desktopicon_set_name(DesktopIcon * desktopicon, char const * name)
{
	if(_desktopicon_set_name(desktopicon, name) != 0)
		return 1;
#if 0
	_desktopicon_update_transparency(desktopicon);
#endif
	return 0;
}


/* desktopicon_set_selected */
void desktopicon_set_selected(DesktopIcon * desktopicon, gboolean selected)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %p is %s\n", (void*)desktopicon,
			selected ? "selected" : "deselected");
#endif
	desktopicon->selected = selected;
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_set_state_flags(desktopicon->event, selected
			? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_NORMAL,
			FALSE);
#else
	gtk_widget_set_state(desktopicon->event, selected
			? GTK_STATE_SELECTED : GTK_STATE_NORMAL);
#endif
}


/* desktopicon_set_updated */
void desktopicon_set_updated(DesktopIcon * desktopicon, gboolean updated)
{
	desktopicon->updated = updated;
}


/* private */
/* desktopicon_new_do */
static DesktopIcon * _desktopicon_new_do(Desktop * desktop, GdkPixbuf * image,
		char const * name)
{
	DesktopIcon * desktopicon;
	GtkWidget * vbox;

	if((desktopicon = object_new(sizeof(*desktopicon))) == NULL)
		return NULL;
	memset(desktopicon, 0, sizeof(*desktopicon));
	desktopicon->desktop = desktop;
	desktopicon->confirm = TRUE;
	desktopicon->updated = TRUE;
	/* widget */
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox = gtk_vbox_new(FALSE, 4);
#endif
	/* event */
	desktopicon->event = gtk_event_box_new();
	g_signal_connect(G_OBJECT(desktopicon->event), "button-press-event",
			G_CALLBACK(_on_icon_button_press), desktopicon);
	g_signal_connect(G_OBJECT(desktopicon->event), "key-press-event",
			G_CALLBACK(_on_icon_key_press), desktopicon);
	/* image */
	desktopicon->image = gtk_image_new();
	gtk_widget_set_size_request(desktopicon->image, DESKTOPICON_ICON_SIZE,
			DESKTOPICON_ICON_SIZE);
	gtk_box_pack_start(GTK_BOX(vbox), desktopicon->image, FALSE, TRUE, 0);
	/* label */
	desktopicon->label = gtk_label_new(NULL);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(desktopicon->label, "valign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(desktopicon->label), 0.5, 0.0);
#endif
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_label_set_line_wrap_mode(GTK_LABEL(desktopicon->label),
			PANGO_WRAP_WORD_CHAR);
#endif
	gtk_label_set_line_wrap(GTK_LABEL(desktopicon->label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), desktopicon->label, TRUE, TRUE, 4);
	gtk_container_add(GTK_CONTAINER(desktopicon->event), vbox);
	if(image == NULL)
	{
		image = desktop_get_file(desktop);
		_desktopicon_set_icon(desktopicon, image);
		g_object_unref(image);
	}
	else
		_desktopicon_set_icon(desktopicon, image);
	_desktopicon_set_name(desktopicon, name);
#if 0
	_desktopicon_update_transparency(desktopicon);
#endif
	return desktopicon;
}


/* desktopicon_set_icon */
static void _desktopicon_set_icon(DesktopIcon * desktopicon, GdkPixbuf * icon)
{
	GdkPixbuf * i = NULL;
#ifdef EMBEDDED
	const GdkInterpType interp = GDK_INTERP_NEAREST;
#else
	const GdkInterpType interp = GDK_INTERP_HYPER;
#endif

	if(icon == NULL)
		return;
	if(gdk_pixbuf_get_width(icon) != DESKTOPICON_ICON_SIZE
			&& gdk_pixbuf_get_height(icon) != DESKTOPICON_ICON_SIZE
			&& (i = gdk_pixbuf_scale_simple(icon,
					DESKTOPICON_ICON_SIZE,
					DESKTOPICON_ICON_SIZE, interp)) != NULL)
		icon = i;
	gtk_image_set_from_pixbuf(GTK_IMAGE(desktopicon->image), icon);
	if(i != NULL)
		g_object_unref(i);
}


/* desktopicon_set_name */
static int _desktopicon_set_name(DesktopIcon * desktopicon, char const * name)
{
	char * p;

	if((p = strdup(name)) == NULL)
		return 1;
	free(desktopicon->name);
	desktopicon->name = p;
	gtk_label_set_text(GTK_LABEL(desktopicon->label), p);
	return 0;
}


/* callbacks */
/* FIXME some code is duplicated from callbacks.c */
/* on_icon_button_press */
static void _popup_directory(GtkWidget * menu, DesktopIcon * desktopicon);
static void _popup_callback(GtkWidget * menu, DesktopIcon * desktopicon);
static void _popup_file(GtkWidget * menu, DesktopIcon * desktopicon);
static void _popup_mime(Mime * mime, char const * mimetype, char const * action,
		char const * label, GCallback callback, DesktopIcon * icon,
		GtkWidget * menu);

static gboolean _on_icon_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data)
{
	DesktopIcon * desktopicon = data;
	GtkWidget * menu;
	GtkWidget * menuitem;
	(void) widget;

	if(event->state & GDK_CONTROL_MASK)
		desktopicon_set_selected(desktopicon, !desktopicon_get_selected(
					desktopicon));
	else
	{
		desktop_unselect_all(desktopicon->desktop);
		desktopicon_set_selected(desktopicon, TRUE);
	}
	/* single click open for applications */
	if(desktopicon->path == NULL && event->type == GDK_BUTTON_PRESS
			&& event->button == 1)
	{
		if(desktopicon->isexec == TRUE)
			_on_icon_run(desktopicon);
		else if(desktopicon->callback != NULL)
			_on_icon_open(desktopicon);
		return FALSE;
	}
	if(event->type == GDK_2BUTTON_PRESS && event->button == 1)
	{
		if(desktopicon->isexec == TRUE) /* XXX slightly ugly */
			_on_icon_run(desktopicon);
		else
			_on_icon_open(desktopicon);
		return FALSE;
	}
	if(event->type != GDK_BUTTON_PRESS || event->button != 3)
		return FALSE;
	/* popup menu */
	menu = gtk_menu_new();
	if(desktopicon->isdir == TRUE)
		_popup_directory(menu, desktopicon);
	else if(desktopicon->callback != NULL)
		_popup_callback(menu, desktopicon);
	else
		_popup_file(menu, desktopicon);
	if(desktopicon->immutable == FALSE)
	{
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE,
				NULL);
		g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
				G_CALLBACK(_on_icon_delete), desktopicon);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	if(desktopicon->path != NULL)
	{
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		menuitem = gtk_image_menu_item_new_from_stock(
				GTK_STOCK_PROPERTIES, NULL);
		g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
				G_CALLBACK(_on_icon_properties), desktopicon);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);
	return TRUE;
}

static void _popup_directory(GtkWidget * menu, DesktopIcon * desktopicon)
{
	GtkWidget * menuitem;

	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_icon_open), desktopicon);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	if(desktopicon->immutable == FALSE)
	{
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		menuitem = gtk_menu_item_new_with_mnemonic(_("_Rename..."));
		g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
				G_CALLBACK(_on_icon_rename), desktopicon);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
}

static void _popup_callback(GtkWidget * menu, DesktopIcon * desktopicon)
{
	GtkWidget * menuitem;

	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			G_CALLBACK(_on_icon_open), desktopicon);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

static void _popup_file(GtkWidget * menu, DesktopIcon * desktopicon)
{
	Mime * mime;
	GtkWidget * menuitem;

	mime = desktop_get_mime(desktopicon->desktop);
	_popup_mime(mime, desktopicon->mimetype, "open", GTK_STOCK_OPEN,
			G_CALLBACK(_on_icon_open), desktopicon, menu);
	_popup_mime(mime, desktopicon->mimetype, "edit",
#if GTK_CHECK_VERSION(2, 6, 0)
			GTK_STOCK_EDIT,
#else
			_("_Edit"),
#endif
			G_CALLBACK(_on_icon_edit), desktopicon, menu);
	if(desktopicon->isexec == TRUE)
	{
		menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_EXECUTE,
				NULL);
		g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
				G_CALLBACK(_on_icon_run), desktopicon);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	if(desktopicon->path != NULL && desktopicon->path[0] == '/')
	{
		menuitem = gtk_menu_item_new_with_mnemonic(_("Open _with..."));
		g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
				G_CALLBACK(_on_icon_open_with), desktopicon);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		if(desktopicon->immutable == FALSE)
		{
			menuitem = gtk_separator_menu_item_new();
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
			menuitem = gtk_menu_item_new_with_mnemonic(
					_("_Rename..."));
			g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
					G_CALLBACK(_on_icon_rename),
					desktopicon);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		}
	}
}

static void _popup_mime(Mime * mime, char const * mimetype, char const * action,
		char const * label, GCallback callback,
		DesktopIcon * desktopicon, GtkWidget * menu)
{
	GtkWidget * menuitem;

	if(mime_get_handler(mime, mimetype, action) == NULL)
		return;
	if(strncmp(label, "gtk-", 4) == 0)
		menuitem = gtk_image_menu_item_new_from_stock(label, NULL);
	else
		menuitem = gtk_menu_item_new_with_mnemonic(label);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", callback,
			desktopicon);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}


/* on_icon_open */
static void _on_icon_open(gpointer data)
{
	DesktopIcon * desktopicon = data;
	Mime * mime;
	char * argv[] = { BINDIR "/" PROGNAME_BROWSER, PROGNAME_BROWSER, "--",
		NULL, NULL };
	const unsigned int flags = G_SPAWN_FILE_AND_ARGV_ZERO;
	GError * error = NULL;

	if(desktopicon->path == NULL && desktopicon->callback != NULL)
	{
		desktopicon->callback(desktopicon->desktop, desktopicon->data);
		return;
	}
	if(desktopicon->isdir == FALSE)
	{
		mime = desktop_get_mime(desktopicon->desktop);
		if(mime != NULL) /* XXX ugly */
			if(mime_action(mime, "open", desktopicon->path) != 0)
				_on_icon_open_with(desktopicon);
		return;
	}
	argv[3] = desktopicon->path;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		desktop_error(desktopicon->desktop, NULL, error->message, 1);
		g_error_free(error);
	}
}


/* on_icon_edit */
static void _on_icon_edit(gpointer data)
{
	DesktopIcon * desktopicon = data;
	Mime * mime;

	mime = desktop_get_mime(desktopicon->desktop);
	mime_action(mime, "edit", desktopicon->path);
}


/* on_icon_run */
static void _run_application(DesktopIcon * desktopicon);
static void _run_binary(DesktopIcon * desktopicon);
static gboolean _run_confirm(void);
static void _run_directory(DesktopIcon * desktopicon);
static void _run_url(DesktopIcon * desktopicon);

static void _on_icon_run(gpointer data)
{
	DesktopIcon * desktopicon = data;
	const char section[] = "Desktop Entry";
	char const * p;

	if(desktopicon->confirm != FALSE && _run_confirm() != TRUE)
		return;
	if(desktopicon->config == NULL)
		_run_binary(desktopicon);
	else if((p = config_get(desktopicon->config, section, "Type")) == NULL)
		return;
	else if(strcmp(p, _desktop_type_application) == 0)
		_run_application(desktopicon);
	else if(strcmp(p, _desktop_type_directory) == 0)
		_run_directory(desktopicon);
	else if(strcmp(p, _desktop_type_url) == 0)
		_run_url(desktopicon);
}

static void _run_application(DesktopIcon * desktopicon)
{
	/* XXX code duplicated from DeforaOS Panel */
	const char section[] = "Desktop Entry";
	char * program;
	char * p;
	char const * q;
	pid_t pid;
	GError * error = NULL;

	if((q = config_get(desktopicon->config, section, "Exec")) == NULL)
		return;
	if((program = strdup(q)) == NULL)
		return; /* XXX report error */
	/* XXX crude way to ignore %f, %F, %u and %U */
	if((p = strchr(program, '%')) != NULL)
		*p = '\0';
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"", __func__, program);
#endif
	if((q = config_get(desktopicon->config, section, "Path")) == NULL)
	{
		/* execute the program directly */
		if(g_spawn_command_line_async(program, &error) != TRUE)
		{
			desktop_error(desktopicon->desktop, NULL,
					error->message, 1);
			g_error_free(error);
		}
	}
	else if((pid = fork()) == 0)
	{
		/* change the current working directory */
		if(chdir(q) != 0)
			desktop_error(desktopicon->desktop, NULL,
					strerror(errno), 1);
		else if(g_spawn_command_line_async(program, &error) != TRUE)
		{
			desktop_error(desktopicon->desktop, NULL,
					error->message, 1);
			g_error_free(error);
		}
		exit(0);
	}
	else if(pid < 0)
		desktop_error(desktopicon->desktop, NULL, strerror(errno), 1);
	free(program);
}

static void _run_binary(DesktopIcon * desktopicon)
{
	char * argv[] = { NULL, NULL };
	const unsigned int flags = 0;
	GError * error = NULL;

	argv[0] = desktopicon->path;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		desktop_error(desktopicon->desktop, NULL, error->message, 1);
		g_error_free(error);
	}
}

static gboolean _run_confirm(void)
{
	GtkWidget * dialog;
	int res;

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Warning"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s",
#endif
			_("Are you sure you want to execute this file?"));
	gtk_window_set_title(GTK_WINDOW(dialog), _("Warning"));
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return (res == GTK_RESPONSE_YES) ? TRUE : FALSE;
}

static void _run_directory(DesktopIcon * desktopicon)
{
	const char section[] = "Desktop Entry";
	char const * directory;
	/* XXX open with the default file manager instead */
	char * argv[] = { BINDIR "/" PROGNAME_BROWSER, PROGNAME_BROWSER, "--",
		NULL, NULL };
	const unsigned int flags = G_SPAWN_FILE_AND_ARGV_ZERO;
	GError * error = NULL;

	/* XXX this may not might the correct key */
	if((directory = config_get(desktopicon->config, section, "Path"))
			== NULL)
		return;
	if((argv[2] = strdup(directory)) == NULL)
		desktop_error(desktopicon->desktop, NULL, strerror(errno), 1);
	else if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		desktop_error(desktopicon->desktop, NULL, error->message, 1);
		g_error_free(error);
	}
	free(argv[2]);
}

static void _run_url(DesktopIcon * desktopicon)
{
	const char section[] = "Desktop Entry";
	char const * url;
	/* XXX open with the default web browser instead */
	char * argv[] = { PROGNAME_HTMLAPP, "--", NULL, NULL };
	const unsigned int flags = G_SPAWN_SEARCH_PATH;
	GError * error = NULL;

	if((url = config_get(desktopicon->config, section, _desktop_type_url))
			== NULL)
		return;
	if((argv[2] = strdup(url)) == NULL)
		desktop_error(desktopicon->desktop, NULL, strerror(errno), 1);
	else if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL,
				&error) != TRUE)
	{
		desktop_error(desktopicon->desktop, NULL, error->message, 1);
		g_error_free(error);
	}
	free(argv[2]);
}


/* on_icon_open_with */
static void _on_icon_open_with(gpointer data)
{
	DesktopIcon * desktopicon = data;
	GtkWidget * dialog;
	char * filename = NULL;
	char * argv[] = { NULL, NULL, NULL, NULL };
	const unsigned int flags = G_SPAWN_SEARCH_PATH
		| G_SPAWN_FILE_AND_ARGV_ZERO;
	GError * error = NULL;

	dialog = gtk_file_chooser_dialog_new(_("Open with..."), NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
					dialog));
	gtk_widget_destroy(dialog);
	if(filename == NULL)
		return;
	argv[0] = filename;
	argv[1] = filename;
	argv[2] = desktopicon->path;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		desktop_error(desktopicon->desktop, NULL, error->message, 1);
		g_error_free(error);
	}
	g_free(filename);
}


/* on_icon_rename */
static void _on_icon_rename(gpointer data)
{
	DesktopIcon * desktopicon = data;
	GtkWidget * dialog;
	GtkSizeGroup * group;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * widget;
	int res;
	char * p;
	char * q;
	char * r;

	dialog = gtk_dialog_new_with_buttons(_("Rename"), NULL, 0,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, _("Rename"),
			GTK_RESPONSE_ACCEPT, NULL);
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	widget = gtk_label_new(_("Rename: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	widget = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(widget), FALSE);
	gtk_entry_set_text(GTK_ENTRY(widget), desktopicon->name);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	/* entry */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	widget = gtk_label_new(_("To: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	widget = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(widget), desktopicon->name);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show_all(vbox);
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_hide(dialog);
	if(res != GTK_RESPONSE_ACCEPT)
	{
		gtk_widget_destroy(dialog);
		return;
	}
	/* FIXME check errors */
	p = string_new(desktopicon->path);
	q = string_new(gtk_entry_get_text(GTK_ENTRY(widget)));
	/* FIXME convert entry from UTF-8 to filesystem's charset */
	if(q[0] == '/')
		r = string_new(q);
	else
		r = string_new_append(dirname(p), "/", q, NULL);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() rename(\"%s\", \"%s\")\n", __func__,
			desktopicon->path, r);
#else
	if(rename(desktopicon->path, r) != 0)
		desktop_error(desktopicon->desktop, NULL, strerror(errno), 1);
#endif
	string_delete(p);
	string_delete(q);
	string_delete(r);
	gtk_widget_destroy(dialog);
}


/* on_icon_delete */
static void _on_icon_delete(gpointer data)
{
	DesktopIcon * desktopicon = data;
	GtkWidget * dialog;
	unsigned long cnt = 1; /* FIXME implement */
	int res;
	GList * selection = NULL;

	/* FIXME duplicated from callbacks.c */
	dialog = gtk_message_dialog_new(NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "%s",
			_("Warning"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(
				dialog), "%s%lu%s",
			_("Are you sure you want to delete "), cnt,
			_(" file(s)?"));
	gtk_window_set_title(GTK_WINDOW(dialog), _("Warning"));
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if(res == GTK_RESPONSE_YES)
	{
		/* FIXME check if needs UTF-8 conversion */
		selection = g_list_append(selection, desktopicon->path);
		if(_common_exec(PROGNAME_DELETE, "-ir", selection) != 0)
			desktop_error(desktopicon->desktop, PROGNAME_DELETE,
					strerror(errno), 1);
		g_list_free(selection);
	}
}


/* on_icon_properties */
static void _on_icon_properties(gpointer data)
{
	DesktopIcon * desktopicon = data;
	char * argv[] = { BINDIR "/properties", "properties", "--", NULL,
		NULL };
	const unsigned int flags = G_SPAWN_FILE_AND_ARGV_ZERO;
	GError * error = NULL;

	argv[3] = desktopicon->path;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		desktop_error(desktopicon->desktop, NULL, error->message, 1);
		g_error_free(error);
	}
}


/* on_icon_key_press */
static gboolean _on_icon_key_press(GtkWidget * widget, GdkEventKey * event,
		gpointer data)
	/* FIXME handle shift and control */
{
	DesktopIcon * desktopicon = data;
	(void) widget;

	if(event->type != GDK_KEY_PRESS)
		return FALSE;
	if(event->keyval == GDK_KEY_uparrow)
	{
		desktop_unselect_all(desktopicon->desktop);
		desktop_select_above(desktopicon->desktop, desktopicon);
	}
	else if(event->keyval == GDK_KEY_downarrow)
	{
		desktop_unselect_all(desktopicon->desktop);
		desktop_select_under(desktopicon->desktop, desktopicon);
	}
	else /* not handling it */
		return FALSE;
	return TRUE;
}


/* on_icon_drag_data_get */
static void _on_icon_drag_data_get(GtkWidget * widget, GdkDragContext * context,
		GtkSelectionData * seldata, guint info, guint time,
		gpointer data)
{
	DesktopIcon * desktopicon = data;
	(void) widget;
	(void) context;
	(void) info;
	(void) time;

	desktop_get_drag_data(desktopicon->desktop, seldata);
}


/* on_icon_drag_data_received */
static void _on_icon_drag_data_received(GtkWidget * widget,
		GdkDragContext * context, gint x, gint y,
		GtkSelectionData * seldata, guint info, guint time,
		gpointer data)
{
	DesktopIcon * desktopicon = data;
	(void) widget;
	(void) x;
	(void) y;
	(void) info;
	(void) time;

	if(_common_drag_data_received(context, seldata, desktopicon->path) != 0)
		desktop_error(desktopicon->desktop, NULL, strerror(errno), 1);
}