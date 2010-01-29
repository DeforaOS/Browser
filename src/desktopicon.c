/* $Id$ */
/* Copyright (c) 2010 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <System.h>
#include "mime.h"
#include "desktopicon.h"
#define PACKAGE "desktop"

#define COMMON_DND
#define COMMON_EXEC
#include "common.c"


/* DesktopIcon */
/* types */
struct _DesktopIcon
{
	Desktop * desktop;
	char * path;
	int isdir;
	int isexec;
	char const * mimetype;

	gboolean immutable;		/* cannot be deleted */
	gboolean selected;
	gboolean updated;		/* XXX for desktop refresh */

	GtkWidget * window;
	GtkWidget * image;
	GtkWidget * event;
	GtkWidget * label;
};


/* prototypes */
static void _desktopicon_update_transparency(DesktopIcon * desktopicon,
		GdkPixbuf * icon);


/* public */
/* functions */
/* desktopicon_new */
/* callbacks */
static gboolean _on_desktopicon_closex(GtkWidget * widget, GdkEvent * event,
		gpointer data);
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

DesktopIcon * desktopicon_new(Desktop * desktop, char const * name,
		char const * path)
{
	DesktopIcon * desktopicon;
	GtkWindow * window;
	Mime * mime;
	struct stat st;
	GdkGeometry geometry;
	GtkWidget * vbox;
	GtkWidget * eventbox;
	GtkTargetEntry targets[] = { { "deforaos_browser_dnd", 0, 0 } };
	size_t targets_cnt = sizeof(targets) / sizeof(*targets);
	GdkPixbuf * icon = NULL;
	char * p;
	GtkLabel * label;

	if((desktopicon = malloc(sizeof(*desktopicon))) == NULL)
		return NULL;
	if((desktopicon->path = strdup(path)) == NULL)
	{
		free(desktopicon);
		return NULL;
	}
	desktopicon->desktop = desktop;
	desktopicon->isdir = 0;
	desktopicon->isexec = 0;
	desktopicon->mimetype = NULL;
	desktopicon->immutable = FALSE;
	desktopicon->selected = FALSE;
	desktopicon->updated = TRUE;
	desktopicon->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	window = GTK_WINDOW(desktopicon->window);
	gtk_window_set_type_hint(window, GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_resizable(window, FALSE);
	gtk_window_set_decorated(window, FALSE);
	gtk_window_set_keep_below(window, TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_focus_on_map(window, FALSE);
#endif
	g_signal_connect(G_OBJECT(desktopicon->window), "delete-event",
			G_CALLBACK(_on_desktopicon_closex), desktopicon);
	vbox = gtk_vbox_new(FALSE, 4);
	geometry.min_width = DESKTOPICON_MIN_WIDTH;
	geometry.min_height = DESKTOPICON_MIN_HEIGHT;
	geometry.max_width = DESKTOPICON_MAX_WIDTH;
	geometry.max_height = DESKTOPICON_MAX_HEIGHT;
	geometry.base_width = DESKTOPICON_MIN_WIDTH;
	geometry.base_height = DESKTOPICON_MIN_HEIGHT;
	gtk_window_set_geometry_hints(window, vbox, &geometry,
			GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE
			| GDK_HINT_BASE_SIZE);
	/* icon */
	if(stat(path, &st) == 0)
	{
		mime = desktop_get_mime(desktop);
		if(S_ISDIR(st.st_mode))
		{
			desktopicon->isdir = 1;
			icon = desktop_get_folder(desktop);
		}
		else if(st.st_mode & S_IXUSR)
		{
			desktopicon->isexec = 1;
			mime_icons(desktop_get_mime(desktop),
					desktop_get_theme(desktop),
					"application/x-executable",
					DESKTOPICON_ICON_SIZE,
					&icon, -1);
		}
		else if((desktopicon->mimetype = mime_type(mime, path)) != NULL)
			mime_icons(mime, desktop_get_theme(desktop),
					desktopicon->mimetype,
					DESKTOPICON_ICON_SIZE, &icon, -1);
	}
	if(icon == NULL)
		icon = desktop_get_file(desktop);
	eventbox = gtk_event_box_new();
	gtk_drag_source_set(eventbox, GDK_BUTTON1_MASK, targets, targets_cnt,
			GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set(eventbox, GTK_DEST_DEFAULT_ALL, targets, targets_cnt,
			GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(eventbox), "button-press-event",
			G_CALLBACK(_on_icon_button_press), desktopicon);
	g_signal_connect(G_OBJECT(eventbox), "key-press-event",
			G_CALLBACK(_on_icon_key_press), desktopicon);
	g_signal_connect(G_OBJECT(eventbox), "drag-data-get",
			G_CALLBACK(_on_icon_drag_data_get), desktopicon);
	g_signal_connect(G_OBJECT(eventbox), "drag-data-received",
			G_CALLBACK(_on_icon_drag_data_received), desktopicon);
	desktopicon->event = eventbox;
	desktopicon->image = gtk_image_new_from_pixbuf(icon);
	gtk_widget_set_size_request(desktopicon->image, DESKTOPICON_MIN_WIDTH,
			DESKTOPICON_ICON_SIZE);
	gtk_box_pack_start(GTK_BOX(vbox), desktopicon->image, FALSE, TRUE, 4);
	if((p = g_filename_to_utf8(name, -1, NULL, NULL, NULL)) != NULL)
		name = p;
	desktopicon->label = gtk_label_new(name);
	label = GTK_LABEL(desktopicon->label);
	gtk_label_set_justify(label, GTK_JUSTIFY_CENTER);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_label_set_line_wrap_mode(label, PANGO_WRAP_WORD_CHAR);
#endif
	gtk_label_set_line_wrap(label, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), desktopicon->label, TRUE, FALSE, 4);
	gtk_container_add(GTK_CONTAINER(eventbox), vbox);
	gtk_container_add(GTK_CONTAINER(desktopicon->window), eventbox);
	_desktopicon_update_transparency(desktopicon, icon);
	return desktopicon;
}

/* callbacks */
static gboolean _on_desktopicon_closex(GtkWidget * widget, GdkEvent * event,
		gpointer data)
{
	DesktopIcon * di = data;

	gtk_widget_hide(widget);
	desktopicon_delete(di);
	return TRUE;
}

/* FIXME some code is duplicated from callbacks.c */
/* types */
static void _popup_directory(GtkWidget * menu, DesktopIcon * desktopicon);
static void _popup_file(GtkWidget * menu, DesktopIcon * desktopicon);
static void _popup_mime(Mime * mime, char const * mimetype, char const * action,
		char const * label, GCallback callback, DesktopIcon * icon,
		GtkWidget * menu);
/* callbacks */
static void _on_icon_open(gpointer data);
static void _on_icon_edit(gpointer data);
static void _on_icon_run(gpointer data);
static void _on_icon_open_with(gpointer data);
static void _on_icon_delete(gpointer data);
static void _on_icon_properties(gpointer data);

static gboolean _on_icon_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data)
{
	DesktopIcon * desktopicon = data;
	GtkWidget * menu;
	GtkWidget * menuitem;

	if(event->state & GDK_CONTROL_MASK)
		desktopicon_set_selected(desktopicon, !desktopicon_get_selected(
					desktopicon));
	else
	{
		desktop_unselect_all(desktopicon->desktop);
		desktopicon_set_selected(desktopicon, TRUE);
	}
	if(event->type == GDK_2BUTTON_PRESS && event->button == 1)
	{
		_on_icon_open(desktopicon);
		return FALSE;
	}
	if(event->type != GDK_BUTTON_PRESS || event->button != 3)
		return FALSE;
	menu = gtk_menu_new();
	if(desktopicon->isdir)
		_popup_directory(menu, desktopicon);
	else
		_popup_file(menu, desktopicon);
	if(desktopicon->immutable != TRUE)
	{
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE,
				NULL);
		g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
				G_CALLBACK(_on_icon_delete), desktopicon);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(
			GTK_STOCK_PROPERTIES, NULL);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_icon_properties), desktopicon);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
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
			"_Edit",
#endif
			G_CALLBACK(_on_icon_edit), desktopicon, menu);
	if(desktopicon->isexec)
	{
		menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_EXECUTE,
				NULL);
		g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
				G_CALLBACK(_on_icon_run), desktopicon);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	menuitem = gtk_menu_item_new_with_mnemonic("Open _with...");
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(
				_on_icon_open_with), desktopicon);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
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

static void _on_icon_open(gpointer data)
{
	DesktopIcon * desktopicon = data;
	Mime * mime;
	pid_t pid;

	if(desktopicon->isdir == 0)
	{
		mime = desktop_get_mime(desktopicon->desktop);
		if(mime != NULL) /* XXX ugly */
			if(mime_action(mime, "open", desktopicon->path) != 0)
				_on_icon_open_with(desktopicon);
		return;
	}
	if((pid = fork()) == -1)
	{
		desktop_error(desktopicon->desktop, "fork", 0);
		return;
	}
	if(pid != 0)
		return;
	execlp("browser", "browser", "--", desktopicon->path, NULL);
	fprintf(stderr, "%s%s\n", "desktop: browser: ", strerror(errno));
	exit(127);
}

static void _on_icon_edit(gpointer data)
{
	DesktopIcon * desktopicon = data;
	Mime * mime;

	mime = desktop_get_mime(desktopicon->desktop);
	mime_action(mime, "edit", desktopicon->path);
}

static void _on_icon_run(gpointer data)
{
	DesktopIcon * desktopicon = data;
	GtkWidget * dialog;
	int res;
	pid_t pid;

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "%s",
			"Warning");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s", "Are you sure you want to execute this file?");
	gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if(res != GTK_RESPONSE_YES)
		return;
	if((pid = fork()) == -1)
		desktop_error(desktopicon->desktop, "fork", 0);
	else if(pid == 0)
	{
		execl(desktopicon->path, desktopicon->path, NULL);
		desktop_error(NULL, desktopicon->path, 0);
		exit(127);
	}
}

static void _on_icon_open_with(gpointer data)
{
	DesktopIcon * desktopicon = data;
	GtkWidget * dialog;
	char * filename = NULL;
	pid_t pid;

	dialog = gtk_file_chooser_dialog_new("Open with...",
			GTK_WINDOW(desktopicon->window),
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
					dialog));
	gtk_widget_destroy(dialog);
	if(filename == NULL)
		return;
	if((pid = fork()) == -1)
		desktop_error(desktopicon->desktop, "fork", 0);
	else if(pid == 0)
	{
		execlp(filename, filename, desktopicon->path, NULL);
		desktop_error(NULL, filename, 0);
		exit(127);
	}
	g_free(filename);
}

static void _on_icon_delete(gpointer data)
{
	DesktopIcon * desktopicon = data;
	GtkWidget * dialog;
	unsigned long cnt = 1; /* FIXME implement */
	int res;
	GList * selection = NULL;

	/* FIXME duplicated from callbacks.c */
	dialog = gtk_message_dialog_new(GTK_WINDOW(desktopicon->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "%s",
			"Warning");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(
				dialog), "%s%lu%s",
			"Are you sure you want to delete ", cnt, " file(s)?");
	gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if(res == GTK_RESPONSE_YES)
	{
		/* FIXME check if needs UTF-8 conversion */
		selection = g_list_append(selection, desktopicon->path);
		if(_common_exec("delete", "-ir", selection) != 0)
			desktop_error(desktopicon->desktop, "fork", 0);
		g_list_free(selection);
	}
}

static void _on_icon_properties(gpointer data)
{
	DesktopIcon * desktopicon = data;
	pid_t pid;

	if((pid = fork()) == -1)
	{
		desktop_error(desktopicon->desktop, "fork", 0);
		return;
	}
	else if(pid != 0)
		return;
	execlp("properties", "properties", "--", desktopicon->path, NULL);
	desktop_error(NULL, "properties", 0);
	exit(127);
}

static gboolean _on_icon_key_press(GtkWidget * widget, GdkEventKey * event,
		gpointer data)
	/* FIXME handle shift and control */
{
	DesktopIcon * desktopicon = data;

	if(event->type != GDK_KEY_PRESS)
		return FALSE;
	if(event->keyval == GDK_uparrow)
	{
		desktop_unselect_all(desktopicon->desktop);
		desktop_select_above(desktopicon->desktop, desktopicon);
	}
	else if(event->keyval == GDK_downarrow)
	{
		desktop_unselect_all(desktopicon->desktop);
		desktop_select_under(desktopicon->desktop, desktopicon);
	}
	else /* not handling it */
		return FALSE;
	return TRUE;
}

static void _on_icon_drag_data_get(GtkWidget * widget, GdkDragContext * context,
		GtkSelectionData * seldata, guint info, guint time,
		gpointer data)
{
	DesktopIcon * desktopicon = data;

	desktop_get_drag_data(desktopicon->desktop, seldata);
}

static void _on_icon_drag_data_received(GtkWidget * widget,
		GdkDragContext * context, gint x, gint y,
		GtkSelectionData * seldata, guint info, guint time,
		gpointer data)
{
	DesktopIcon * desktopicon = data;

	if(_common_drag_data_received(context, seldata, desktopicon->path) != 0)
		desktop_error(desktopicon->desktop, "fork", 0);
}


/* desktopicon_delete */
void desktopicon_delete(DesktopIcon * desktopicon)
{
	free(desktopicon->path);
	gtk_widget_destroy(desktopicon->window);
	free(desktopicon);
}


/* accessors */
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


/* desktopicon_set_icon */
void desktopicon_set_icon(DesktopIcon * desktopicon, GdkPixbuf * icon)
{
	gtk_image_set_from_pixbuf(GTK_IMAGE(desktopicon->image), icon);
	_desktopicon_update_transparency(desktopicon, icon);
}


/* desktopicon_set_immutable */
void desktopicon_set_immutable(DesktopIcon * desktopicon, gboolean immutable)
{
	desktopicon->immutable = immutable;
}


/* desktopicon_set_selected */
void desktopicon_set_selected(DesktopIcon * desktopicon, gboolean selected)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %p is %s\n", desktopicon, selected ? "selected"
			: "deselected");
#endif
	desktopicon->selected = selected;
	gtk_widget_set_state(desktopicon->event, selected
			? GTK_STATE_SELECTED : GTK_STATE_NORMAL);
}


/* desktopicon_set_updated */
void desktopicon_set_updated(DesktopIcon * desktopicon, gboolean updated)
{
	desktopicon->updated = updated;
}


/* useful */
/* desktopicon_move */
void desktopicon_move(DesktopIcon * desktopicon, int x, int y)
{
	gtk_window_move(GTK_WINDOW(desktopicon->window), x, y);
}


/* desktopicon_show */
void desktopicon_show(DesktopIcon * desktopicon)
{
	gtk_widget_show_all(desktopicon->window);
}


/* private */
/* desktopicon_update_transparency */
static void _desktopicon_update_transparency(DesktopIcon * desktopicon,
		GdkPixbuf * icon)
{
	int width;
	int height;
	int iwidth;
	int iheight;
	GdkBitmap * mask;
	GdkBitmap * iconmask;
	GdkGC * gc;
	GdkColor black = { 0, 0, 0, 0 };
	GdkColor white = { 0xffffffff, 0xffff, 0xffff, 0xffff };
	GtkRequisition req;
	int offset;

	gtk_window_get_size(GTK_WINDOW(desktopicon->window), &width, &height);
	iwidth = gdk_pixbuf_get_width(icon);
	iheight = gdk_pixbuf_get_height(icon);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%s) window is %dx%d\n", __func__,
			desktopicon->path, width, height);
#endif
	mask = gdk_pixmap_new(NULL, width, height, 1);
	gdk_pixbuf_render_pixmap_and_mask(icon, NULL, &iconmask, 255);
	gc = gdk_gc_new(mask);
	gdk_gc_set_foreground(gc, &black);
	gdk_draw_rectangle(mask, gc, TRUE, 0, 0, width, height);
	gdk_draw_drawable(mask, gc, iconmask, 0, 0,
			(width - iwidth) / 2,
			(DESKTOPICON_ICON_SIZE + 8 - iheight) / 2, -1, -1);
	gdk_gc_set_foreground(gc, &white);
	gtk_widget_size_request(desktopicon->label, &req);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%s) label is %dx%d\n", __func__,
			desktopicon->path, req.width, req.height);
#endif
	offset = DESKTOPICON_ICON_SIZE + 8;
	gdk_draw_rectangle(mask, gc, TRUE, (width - req.width - 8) / 2,
			offset + ((height - offset - req.height - 8)
				/ 2), req.width + 8, req.height + 8);
	gtk_widget_shape_combine_mask(desktopicon->window, mask, 0, 0);
	g_object_unref(gc);
	g_object_unref(iconmask);
	g_object_unref(mask);
}
