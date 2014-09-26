/* $Id$ */
/* Copyright (c) 2011-2014 Pierre Pronchery <khorben@defora.org> */
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



#include <System.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#ifndef __GNU__ /* XXX hurd portability */
# include <sys/mount.h>
# if defined(__linux__) || defined(__CYGWIN__)
#  define unmount(a, b) umount(a)
# endif
# ifndef unmount
#  define unmount unmount
# endif
#endif
#if defined(__FreeBSD__)
# include <sys/param.h>
# include <sys/ucred.h>
#elif defined(__NetBSD__)
# include <sys/param.h>
# include <sys/types.h>
# include <sys/statvfs.h>
#endif
#include "Browser.h"
#include "../../config.h"
#define _(string) gettext(string)
#define N_(string) (string)

#ifndef PREFIX
# define PREFIX	"/usr/local"
#endif
#ifndef BINDIR
# define BINDIR	PREFIX "/bin"
#endif


/* Volumes */
/* private */
/* types */
enum _VolumesColumn
{
	DC_PIXBUF = 0,
	DC_DEVICE,
	DC_NAME,
	DC_ELLIPSIZE,
	DC_ELLIPSIZE_SET,
	DC_FILESYSTEM,
	DC_FLAGS,
	DC_MOUNTPOINT,
	DC_FREE,
	DC_FREE_DISPLAY,
	DC_UPDATED
};
#define DC_LAST DC_UPDATED
#define DC_COUNT (DC_LAST + 1)

typedef enum _VolumesFlag
{
	DF_READONLY = 1,
	DF_REMOVABLE
} VolumesFlag;

typedef enum _VolumesPixbuf
{
	DP_HARDDISK = 0,
	DP_CDROM,
	DP_REMOVABLE
} VolumesPixbuf;
#define DP_LAST DP_REMOVABLE
#define DP_COUNT (DP_LAST + 1)

typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;
	guint source;
	GtkWidget * window;
	GtkListStore * store;
	GtkWidget * view;

	/* icons */
	GdkPixbuf * icons[DP_COUNT];
	gint width;
	gint height;
} Volumes;


/* prototypes */
/* plug-in */
static Volumes * _volumes_init(BrowserPluginHelper * helper);
static void _volumes_destroy(Volumes * volumes);
static GtkWidget * _volumes_get_widget(Volumes * volumes);
static void _volumes_refresh(Volumes * volumes, GList * selection);

/* accessors */
static int _volumes_can_eject(unsigned int flags);

/* useful */
static void _volumes_list(Volumes * volumes);

/* callbacks */
static gboolean _volumes_on_timeout(gpointer data);
static gboolean _volumes_on_view_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data);
static gboolean _volumes_on_view_popup_menu(GtkWidget * widget, gpointer data);
static void _volumes_on_view_row_activated(GtkWidget * widget,
		GtkTreePath * path, GtkTreeViewColumn * column, gpointer data);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	N_("Volumes"),
	"drive-harddisk",
	NULL,
	_volumes_init,
	_volumes_destroy,
	_volumes_get_widget,
	_volumes_refresh
};


/* private */
/* functions */
/* plug-in */
/* volumes_init */
static Volumes * _volumes_init(BrowserPluginHelper * helper)
{
	Volumes * volumes;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeSelection * treesel;
	GtkIconTheme * icontheme;
	char const * icons[DP_COUNT] = { "drive-harddisk", "drive-cdrom",
		"drive-removable-media" };
	size_t i;

	if((volumes = object_new(sizeof(*volumes))) == NULL)
		return NULL;
	volumes->helper = helper;
	volumes->source = 0;
	volumes->window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(volumes->window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	volumes->store = gtk_list_store_new(DC_COUNT, GDK_TYPE_PIXBUF,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT,
			G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_UINT,
			G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING,
			G_TYPE_BOOLEAN);
	volumes->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				volumes->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(volumes->view), TRUE);
	g_signal_connect(volumes->view, "button-press-event", G_CALLBACK(
				_volumes_on_view_button_press), volumes);
	g_signal_connect(volumes->view, "popup-menu", G_CALLBACK(
				_volumes_on_view_popup_menu), volumes);
	/* icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", DC_PIXBUF, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(volumes->view), column);
	/* volume name */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Mountpoint"),
			renderer, "text", DC_NAME, "ellipsize", DC_ELLIPSIZE,
			"ellipsize-set", DC_ELLIPSIZE_SET, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(volumes->view), column);
	/* free space */
	renderer = gtk_cell_renderer_progress_new();
	column = gtk_tree_view_column_new_with_attributes(_("Used"), renderer,
			"text", DC_FREE_DISPLAY, "value", DC_FREE, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(volumes->view), column);
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(volumes->view));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
	g_signal_connect(volumes->view, "row-activated", G_CALLBACK(
				_volumes_on_view_row_activated), volumes);
	gtk_container_add(GTK_CONTAINER(volumes->window), volumes->view);
	/* icons */
	icontheme = gtk_icon_theme_get_default();
	volumes->width = 24;
	volumes->height = 24;
	gtk_icon_size_lookup(GTK_ICON_SIZE_BUTTON, &volumes->width,
			&volumes->height);
	for(i = 0; i < DP_COUNT; i++)
		volumes->icons[i] = gtk_icon_theme_load_icon(icontheme,
				icons[i], volumes->width,
				GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	gtk_widget_show_all(volumes->window);
	return volumes;
}


/* volumes_destroy */
static void _volumes_destroy(Volumes * volumes)
{
	size_t i;

	if(volumes->source != 0)
		g_source_remove(volumes->source);
	for(i = 0; i < DP_COUNT; i++)
		if(volumes->icons[i] != NULL)
			g_object_unref(volumes->icons[i]);
	object_delete(volumes);
}


/* volumes_get_widget */
static GtkWidget * _volumes_get_widget(Volumes * volumes)
{
	return volumes->window;
}


/* volumes_refresh */
static void _volumes_refresh(Volumes * volumes, GList * selection)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p)\n", __func__, (void *)selection);
#endif
	if(selection == NULL)
	{
		/* stop refreshing */
		if(volumes->source != 0)
			g_source_remove(volumes->source);
		volumes->source = 0;
	}
	else
	{
		_volumes_list(volumes);
		if(volumes->source == 0)
			/* refresh every 5 seconds */
			volumes->source = g_timeout_add(5000,
					_volumes_on_timeout, volumes);
	}
}


/* accessors */
/* volumes_can_eject */
static int _volumes_can_eject(unsigned int flags)
{
	return ((flags & DF_REMOVABLE) != 0) ? 1 : 0;
}


/* useful */
/* volumes_list */
static void _list_add(Volumes * volumes, char const * name, char const * device,
		char const * filesystem, unsigned int flags,
		char const * mountpoint, fsblkcnt_t free, fsblkcnt_t total);
static GdkPixbuf * _list_get_icon(Volumes * volumes, VolumesPixbuf dp,
		char const * mountpoint);
static void _list_get_iter(Volumes * volumes, GtkTreeIter * iter,
		char const * mountpoint);
static void _list_purge(Volumes * volumes);
static void _list_reset(Volumes * volumes);

static void _volumes_list(Volumes * volumes)
{
#if defined(ST_NOWAIT)
	struct statvfs * mnt;
	int res;
	int i;
	unsigned int flags;
#elif defined(MNT_NOWAIT)
	struct statfs * mnt;
	int res;
	int i;
	unsigned int flags;
#endif

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
#if defined(ST_NOWAIT)
	if((res = getmntinfo(&mnt, ST_NOWAIT)) <= 0)
		return;
	_list_reset(volumes);
	for(i = 0; i < res; i++)
	{
		flags = (mnt[i].f_flag & ST_RDONLY) ? DF_READONLY : 0;
		_list_add(volumes, (mnt[i].f_flag & ST_ROOTFS)
				? _("Root filesystem") : NULL,
				mnt[i].f_mntfromname, mnt[i].f_fstypename,
				flags, mnt[i].f_mntonname, mnt[i].f_bavail,
				mnt[i].f_blocks);
	}
#elif defined(MNT_NOWAIT)
	if((res = getmntinfo(&mnt, MNT_NOWAIT)) <= 0)
		return;
	_list_reset(volumes);
	for(i = 0; i < res; i++)
	{
		flags = (mnt[i].f_flags & MNT_RDONLY) ? DF_READONLY : 0;
		_list_add(volumes, (mnt[i].f_flags & MNT_ROOTFS)
				? _("Root filesystem") : NULL,
				mnt[i].f_mntfromname, mnt[i].f_fstypename,
				flags, mnt[i].f_mntonname, mnt[i].f_bavail,
				mnt[i].f_blocks);
	}
#else
	_list_reset(volumes);
	_list_add(volumes, _("Root filesystem"), NULL, NULL, 0, "/", 0, 0);
#endif
	_list_purge(volumes);
}

static void _list_add(Volumes * volumes, char const * name, char const * device,
		char const * filesystem, unsigned int flags,
		char const * mountpoint, fsblkcnt_t free, fsblkcnt_t total)
{
	GtkTreeIter iter;
	VolumesPixbuf dp = DP_HARDDISK;
	char const * ignore[] = { "kernfs", "proc", "procfs", "ptyfs" };
	char const * cdrom[] = { "/dev/cd" };
	char const * removable[] = { "/dev/sd" };
	size_t i;
	double fraction = 0.0;
	unsigned int f = 0;
	char buf[16] = "";

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\", \"%s\", \"%s\", %lu, %lu)\n",
			__func__, name, device, filesystem, mountpoint, free,
			total);
#endif
	for(i = 0; i < sizeof(ignore) / sizeof(*ignore); i++)
		if(strcmp(ignore[i], filesystem) == 0)
			return;
	for(i = 0; i < sizeof(cdrom) / sizeof(*cdrom); i++)
		if(strncmp(cdrom[i], device, strlen(cdrom[i])) == 0)
		{
			flags |= DF_REMOVABLE;
			dp = DP_CDROM;
			break;
		}
	for(i = 0; i < sizeof(removable) / sizeof(removable); i++)
		if(strncmp(removable[i], device, strlen(removable[i])) == 0)
		{
			flags |= DF_REMOVABLE;
			dp = DP_REMOVABLE;
			break;
		}
	if(name == NULL)
	{
		if(strcmp(mountpoint, "/") == 0)
			name = _("Root filesystem");
		else
			name = mountpoint;
	}
	if(dp != DP_CDROM && total != 0 && total >= free)
	{
		fraction = total - free;
		fraction = fraction / total;
		f = fraction * 100;
		snprintf(buf, sizeof(buf), "%.1lf%%", fraction * 100.0);
	}
	_list_get_iter(volumes, &iter, mountpoint);
	gtk_list_store_set(volumes->store, &iter, DC_DEVICE, device,
			DC_PIXBUF, _list_get_icon(volumes, dp, mountpoint),
			DC_NAME, name, DC_ELLIPSIZE, PANGO_ELLIPSIZE_END,
			DC_ELLIPSIZE_SET, TRUE, DC_FILESYSTEM, filesystem,
			DC_FLAGS, flags, DC_MOUNTPOINT, mountpoint, DC_FREE, f,
			DC_FREE_DISPLAY, buf, DC_UPDATED, TRUE, -1);
}

static GdkPixbuf * _list_get_icon(Volumes * volumes, VolumesPixbuf dp,
		char const * mountpoint)
{
	GdkPixbuf * ret = volumes->icons[dp];
	const char autorun[] = "/autorun.inf";
	const char icon[] = "icon=";
	String * s;
	FILE * fp;
	char buf[256];
	size_t len;
	GError * error = NULL;

	if(dp != DP_REMOVABLE)
		/* stick to the default icon */
		return ret;
	if((s = string_new_append(mountpoint, autorun, NULL)) == NULL)
		return ret;
	fp = fopen(s, "r");
	string_delete(s);
	if(fp == NULL)
		return ret;
	/* FIXME improve the parser (use the Config class?) */
	while(fgets(buf, sizeof(buf), fp) != NULL)
		if((len = strlen(buf)) == sizeof(buf) - 1)
			continue;
		else if(strncasecmp(icon, buf, sizeof(icon) - 1) == 0)
		{
			buf[len - 2] = '\0';
			if((s = string_new_append(mountpoint, "/",
							&buf[sizeof(icon) - 1],
							NULL)) == NULL)
				continue;
			if((ret = gdk_pixbuf_new_from_file_at_scale(s,
							volumes->width,
							volumes->height, TRUE,
							&error)) == NULL)
			{
				g_error_free(error);
				error = NULL;
				ret = volumes->icons[dp];
			}
			string_delete(s);
		}
	fclose(fp);
	return ret;
}

static void _list_get_iter(Volumes * volumes, GtkTreeIter * iter,
		char const * mountpoint)
{
	GtkTreeModel * model = GTK_TREE_MODEL(volumes->store);
	gboolean valid;
	gchar * p;
	int res;

	for(valid = gtk_tree_model_get_iter_first(model, iter); valid == TRUE;
		valid = gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, DC_MOUNTPOINT, &p, -1);
		res = strcmp(mountpoint, p);
		g_free(p);
		if(res == 0)
			return;
	}
	gtk_list_store_append(volumes->store, iter);
}

static void _list_purge(Volumes * volumes)
{
	GtkTreeModel * model = GTK_TREE_MODEL(volumes->store);
	GtkTreeIter iter;
	gboolean valid;
	gboolean updated;

	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;)
	{
		gtk_tree_model_get(model, &iter, DC_UPDATED, &updated, -1);
		valid = updated ? gtk_tree_model_iter_next(model, &iter)
			: gtk_list_store_remove(volumes->store, &iter);
	}
}

static void _list_reset(Volumes * volumes)
{
	GtkTreeModel * model = GTK_TREE_MODEL(volumes->store);
	GtkTreeIter iter;
	gboolean valid;

	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
		gtk_list_store_set(volumes->store, &iter, DC_UPDATED, FALSE,
				-1);
}


/* callbacks */
/* volumes_on_timeout */
static gboolean _volumes_on_timeout(gpointer data)
{
	Volumes * volumes = data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	_volumes_list(volumes);
	return TRUE;
}


/* volumes_on_view_button_press */
static void _volumes_on_eject(GtkWidget * widget, gpointer data);
static void _volumes_on_properties(GtkWidget * widget, gpointer data);
static void _volumes_on_unmount(GtkWidget * widget, gpointer data);

static gboolean _volumes_on_view_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data)
{
	Volumes * volumes = data;
	GtkTreeSelection * treesel;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gchar * device;
	unsigned int flags;
	gchar * mountpoint;
	GtkWidget * menu;

	if(event->type != GDK_BUTTON_PRESS
			|| (event->button != 3 && event->button != 0))
		return FALSE;
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	if(gtk_tree_selection_get_selected(treesel, &model, &iter) != TRUE)
		return FALSE;
	gtk_tree_model_get(model, &iter, DC_DEVICE, &device,
			DC_MOUNTPOINT, &mountpoint, DC_FLAGS, &flags, -1);
	if(mountpoint == NULL)
		return FALSE;
	menu = gtk_menu_new();
	/* unmount */
	widget = gtk_image_menu_item_new_with_label(_("Unmount"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
			gtk_image_new_from_icon_name("hdd_unmount",
				GTK_ICON_SIZE_MENU));
	g_object_set_data(G_OBJECT(widget), "mountpoint", mountpoint);
	g_signal_connect(widget, "activate", G_CALLBACK(
				_volumes_on_unmount), volumes);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	/* eject */
	if(_volumes_can_eject(flags))
	{
		widget = gtk_image_menu_item_new_with_label(_("Eject"));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
				gtk_image_new_from_icon_name("media-eject",
					GTK_ICON_SIZE_MENU));
		g_object_set_data(G_OBJECT(widget), "device", device);
		g_signal_connect(widget, "activate", G_CALLBACK(
					_volumes_on_eject), volumes);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	}
	/* properties */
	widget = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	widget = gtk_image_menu_item_new_from_stock(GTK_STOCK_PROPERTIES, NULL);
	g_object_set_data(G_OBJECT(widget), "mountpoint", mountpoint);
	g_signal_connect(widget, "activate", G_CALLBACK(_volumes_on_properties),
			volumes);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button,
			event->time);
#if 0 /* XXX memory leak (for g_object_set_data() above) */
	g_free(device);
	g_free(mountpoint);
#endif
	return TRUE;
}

static void _volumes_on_eject(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	BrowserPluginHelper * helper = volumes->helper;
	gchar * device;
	char * argv[] = { "eject", "--", NULL, NULL };
	const unsigned int flags = G_SPAWN_SEARCH_PATH;
	GError * error = NULL;

	device = g_object_get_data(G_OBJECT(widget), "device");
	/* FIXME use the device node instead */
	argv[2] = device;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
	}
	g_free(device);
}

static void _volumes_on_properties(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	BrowserPluginHelper * helper = volumes->helper;
	gchar * mountpoint;
	char * argv[] = { BINDIR "/properties", "properties", "--", NULL,
		NULL };
	const unsigned int flags = G_SPAWN_FILE_AND_ARGV_ZERO;
	GError * error = NULL;

	mountpoint = g_object_get_data(G_OBJECT(widget), "mountpoint");
	argv[2] = mountpoint;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
	}
	g_free(mountpoint);
}

static void _volumes_on_unmount(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	gchar * mountpoint;

	mountpoint = g_object_get_data(G_OBJECT(widget), "mountpoint");
#ifndef unmount
	errno = ENOSYS;
#else
	if(unmount(mountpoint, 0) != 0)
#endif
		volumes->helper->error(volumes->helper->browser,
				strerror(errno), 1);
	g_free(mountpoint);
}


/* volumes_on_view_popup_menu */
static gboolean _volumes_on_view_popup_menu(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	GdkEventButton event;

	memset(&event, 0, sizeof(event));
	event.type = GDK_BUTTON_PRESS;
	event.button = 0;
	event.time = gtk_get_current_event_time();
	return _volumes_on_view_button_press(widget, &event, volumes);
}


/* volumes_on_view_row_activated */
static void _volumes_on_view_row_activated(GtkWidget * widget,
		GtkTreePath * path, GtkTreeViewColumn * column, gpointer data)
{
	Volumes * volumes = data;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gchar * location;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
	if(gtk_tree_model_get_iter(model, &iter, path) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, DC_MOUNTPOINT, &location, -1);
	volumes->helper->set_location(volumes->helper->browser, location);
	g_free(location);
}
