/* $Id$ */
/* Copyright (c) 2011-2018 Pierre Pronchery <khorben@defora.org> */
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



#include <System.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#if defined(__FreeBSD__)
# include <sys/param.h>
# include <sys/ucred.h>
# include <fstab.h>
#elif defined(__NetBSD__)
# include <sys/param.h>
# include <sys/types.h>
# include <sys/statvfs.h>
# include <fstab.h>
#elif defined(__OpenBSD__)
# include <fstab.h>
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

#ifndef PROGNAME_BROWSER
# define PROGNAME_BROWSER	"browser"
#endif
#ifndef PROGNAME_PROPERTIES
# define PROGNAME_PROPERTIES	"properties"
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
	DC_SIZE,
	DC_SIZE_DISPLAY,
	DC_FREE,
	DC_FREE_DISPLAY,
	DC_UPDATED
};
#define DC_LAST DC_UPDATED
#define DC_COUNT (DC_LAST + 1)

typedef enum _VolumesFlag
{
	DF_MOUNTED	= 0x01,
	DF_NETWORK	= 0x02,
	DF_READONLY	= 0x04,
	DF_REMOVABLE	= 0x08,
	DF_SHARED	= 0x10
} VolumesFlag;

typedef enum _VolumesPixbuf
{
	DP_CDROM = 0,
	DP_HARDDISK,
	DP_NETWORK,
	DP_REMOVABLE
} VolumesPixbuf;
#define DP_LAST DP_REMOVABLE
#define DP_COUNT (DP_LAST + 1)

typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;
	guint source;
	GtkWidget * widget;
	GtkToolItem * tb_mount;
	GtkToolItem * tb_unmount;
	GtkToolItem * tb_eject;
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
static int _volumes_can_mount(unsigned int flags);
static int _volumes_can_unmount(unsigned int flags);

/* useful */
static int _volumes_eject(Volumes * volumes, char const * mountpoint);
static void _volumes_list(Volumes * volumes);
static int _volumes_mount(Volumes * volumes, char const * mountpoint);
static int _volumes_unmount(Volumes * volumes, char const * mountpoint);

/* callbacks */
static void _volumes_on_eject_selection(gpointer data);
static void _volumes_on_mount_selection(gpointer data);
static gboolean _volumes_on_timeout(gpointer data);
static void _volumes_on_unmount_selection(gpointer data);
static gboolean _volumes_on_view_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data);
static gboolean _volumes_on_view_popup_menu(GtkWidget * widget, gpointer data);
static void _volumes_on_view_row_activated(GtkWidget * widget,
		GtkTreePath * path, GtkTreeViewColumn * column, gpointer data);
static void _volumes_on_view_row_changed(GtkTreeSelection * treesel,
		gpointer data);


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
static int _init_sort(GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b,
		gpointer data);

static Volumes * _volumes_init(BrowserPluginHelper * helper)
{
	Volumes * volumes;
	GtkWidget * widget;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeSelection * treesel;
	GtkIconTheme * icontheme;
	char const * icons[DP_COUNT] = { "drive-cdrom", "drive-harddisk",
		"network-server", "drive-removable-media" };
	size_t i;

	if((volumes = object_new(sizeof(*volumes))) == NULL)
		return NULL;
	volumes->helper = helper;
	volumes->source = 0;
	volumes->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	widget = gtk_toolbar_new();
	/* mount */
#if GTK_CHECK_VERSION(2, 8, 0)
	volumes->tb_mount = gtk_tool_button_new(NULL, _("Mount"));
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(volumes->tb_mount),
			"hdd_unmount");
# if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(GTK_WIDGET(volumes->tb_mount),
			_("Mount the volume"));
# endif
#else
	volumes->tb_mount = gtk_tool_button_new(gtk_image_new_from_icon_name(
				"hdd_unmount", GTK_ICON_SIZE_SMALL_TOOLBAR),
			_("Mount"));
#endif
	gtk_widget_set_sensitive(GTK_WIDGET(volumes->tb_mount), FALSE);
	g_signal_connect_swapped(volumes->tb_mount, "clicked", G_CALLBACK(
				_volumes_on_mount_selection), volumes);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), volumes->tb_mount, -1);
	/* unmount */
#if GTK_CHECK_VERSION(2, 8, 0)
	volumes->tb_unmount = gtk_tool_button_new(NULL, _("Unmount"));
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(volumes->tb_unmount),
			"hdd_unmount");
# if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(GTK_WIDGET(volumes->tb_unmount),
			_("Unmount the volume"));
# endif
#else
	volumes->tb_unmount = gtk_tool_button_new(gtk_image_new_from_icon_name(
				"hdd_unmount", GTK_ICON_SIZE_SMALL_TOOLBAR),
			_("Unmount"));
#endif
	gtk_widget_set_sensitive(GTK_WIDGET(volumes->tb_unmount), FALSE);
	g_signal_connect_swapped(volumes->tb_unmount, "clicked", G_CALLBACK(
				_volumes_on_unmount_selection), volumes);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), volumes->tb_unmount, -1);
	/* eject */
#if GTK_CHECK_VERSION(2, 8, 0)
	volumes->tb_eject = gtk_tool_button_new(NULL, _("Eject"));
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(volumes->tb_eject),
			"media-eject");
# if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(GTK_WIDGET(volumes->tb_eject),
			_("Eject the volume"));
# endif
#else
	volumes->tb_eject = gtk_tool_button_new(gtk_image_new_from_icon_name(
				"media-eject", GTK_ICON_SIZE_SMALL_TOOLBAR),
			_("Eject"));
#endif
	gtk_widget_set_sensitive(GTK_WIDGET(volumes->tb_eject), FALSE);
	g_signal_connect_swapped(volumes->tb_eject, "clicked", G_CALLBACK(
				_volumes_on_eject_selection), volumes);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), volumes->tb_eject, -1);
	gtk_box_pack_start(GTK_BOX(volumes->widget), widget, FALSE, TRUE, 0);
	volumes->window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(volumes->window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	volumes->store = gtk_list_store_new(DC_COUNT, GDK_TYPE_PIXBUF,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT,
			G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_UINT,
			G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING,
			G_TYPE_UINT, G_TYPE_STRING, G_TYPE_BOOLEAN);
	gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(
				volumes->store), _init_sort, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(volumes->store),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING);
	volumes->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				volumes->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(volumes->view), TRUE);
	g_signal_connect(volumes->view, "button-press-event", G_CALLBACK(
				_volumes_on_view_button_press), volumes);
	g_signal_connect(volumes->view, "popup-menu", G_CALLBACK(
				_volumes_on_view_popup_menu), volumes);
	/* column: icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", DC_PIXBUF, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(volumes->view), column);
	/* column: volume name */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Mountpoint"),
			renderer, "text", DC_NAME, "ellipsize", DC_ELLIPSIZE,
			"ellipsize-set", DC_ELLIPSIZE_SET, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, DC_FILESYSTEM);
	gtk_tree_view_append_column(GTK_TREE_VIEW(volumes->view), column);
	/* column: size */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Size"), renderer,
			"text", DC_SIZE_DISPLAY, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, DC_SIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(volumes->view), column);
	/* column: free space */
	renderer = gtk_cell_renderer_progress_new();
	column = gtk_tree_view_column_new_with_attributes(_("Used"), renderer,
			"text", DC_FREE_DISPLAY, "value", DC_FREE, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, DC_FREE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(volumes->view), column);
	/* selection */
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(volumes->view));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
	g_signal_connect(treesel, "changed", G_CALLBACK(
				_volumes_on_view_row_changed), volumes);
	g_signal_connect(volumes->view, "row-activated", G_CALLBACK(
				_volumes_on_view_row_activated), volumes);
	gtk_container_add(GTK_CONTAINER(volumes->window), volumes->view);
	/* icons */
	icontheme = gtk_icon_theme_get_default();
	volumes->width = helper->get_icon_size(helper->browser,
			BROWSER_VIEW_DETAILS);
	volumes->height = volumes->width;
	gtk_icon_size_lookup(GTK_ICON_SIZE_BUTTON, &volumes->width,
			&volumes->height);
	for(i = 0; i < DP_COUNT; i++)
		volumes->icons[i] = gtk_icon_theme_load_icon(icontheme,
				icons[i], volumes->width,
				GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	gtk_box_pack_start(GTK_BOX(volumes->widget), volumes->window, TRUE,
			TRUE, 0);
	gtk_widget_show_all(volumes->widget);
	return volumes;
}

static int _init_sort(GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b,
		gpointer data)
{
	gchar * name_a;
	gchar * name_b;
	unsigned int flags_a;
	unsigned int flags_b;
	int ret = 0;
	(void) data;

	gtk_tree_model_get(model, a, DC_MOUNTPOINT, &name_a, DC_FLAGS, &flags_a,
			-1);
	gtk_tree_model_get(model, b, DC_MOUNTPOINT, &name_b, DC_FLAGS, &flags_b,
			-1);
	if((flags_a & DF_REMOVABLE) != 0 && (flags_b & DF_REMOVABLE) == 0)
		ret = 1;
	else if((flags_a & DF_REMOVABLE) == 0 && (flags_b & DF_REMOVABLE) != 0)
		ret = -1;
	if(ret == 0)
		ret = strcmp(name_a, name_b);
	g_free(name_a);
	g_free(name_b);
	return ret;
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
	return volumes->widget;
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


/* volumes_can_mount */
static int _volumes_can_mount(unsigned int flags)
{
	return ((flags & DF_REMOVABLE) != 0
			&& (flags & DF_MOUNTED) == 0) ? 1 : 0;
}


/* volumes_can_unmount */
static int _volumes_can_unmount(unsigned int flags)
{
	return ((flags & DF_REMOVABLE) != 0
			&& (flags & DF_MOUNTED) != 0) ? 1 : 0;
}


/* useful */
/* volumes_eject */
static int _volumes_eject(Volumes * volumes, char const * mountpoint)
{
	BrowserPluginHelper * helper = volumes->helper;

	if(browser_vfs_eject(mountpoint) != 0)
		return helper->error(helper->browser, error_get(NULL), 1);
	return 0;
}


/* volumes_list */
static void _list_add(Volumes * volumes, char const * name, char const * device,
		char const * filesystem, unsigned int flags,
		char const * mountpoint, unsigned long bsize,
		fsblkcnt_t free, fsblkcnt_t total);
static void _list_add_size(char * buf, size_t len, unsigned long bsize,
		fsblkcnt_t total);
static GdkPixbuf * _list_get_icon(Volumes * volumes, VolumesPixbuf dp,
		unsigned int flags, char const * mountpoint);
static GdkPixbuf * _list_get_icon_emblem(GdkPixbuf * pixbuf, int size,
		char const * emblem);
static GdkPixbuf * _list_get_icon_removable(Volumes * volumes, VolumesPixbuf dp,
		char const * mountpoint);
static void _list_get_iter(Volumes * volumes, GtkTreeIter * iter,
		char const * mountpoint);
static void _list_loop_mounted(Volumes * volumes, int reset);
static int _list_loop_unmounted(Volumes * volumes);
static void _list_purge(Volumes * volumes);
static void _list_reset(Volumes * volumes);

static void _volumes_list(Volumes * volumes)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	_list_loop_mounted(volumes,
			_list_loop_unmounted(volumes));
	_list_purge(volumes);
	_volumes_on_view_row_changed(gtk_tree_view_get_selection(
				GTK_TREE_VIEW(volumes->view)), volumes);
}

static void _list_add(Volumes * volumes, char const * name, char const * device,
		char const * filesystem, unsigned int flags,
		char const * mountpoint, unsigned long bsize,
		fsblkcnt_t free, fsblkcnt_t total)
{
	GtkTreeIter iter;
	VolumesPixbuf dp;
	GdkPixbuf * pixbuf;
	char const * ignore[] = { "kernfs", "proc", "procfs", "ptyfs" };
	char const * cdrom[] = { "/dev/cd" };
	char const * removable[] = { "/dev/ld", "/dev/sd" };
	size_t i;
	double fraction = 0.0;
	unsigned int f = 0;
	char buf[16] = "";
	char buf2[16];

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\", \"%s\", \"%s\", %lu, %lu)\n",
			__func__, name, device, filesystem, mountpoint, free,
			total);
#endif
	dp = (flags & DF_NETWORK) ? DP_NETWORK : DP_HARDDISK;
	if(filesystem != NULL)
		for(i = 0; i < sizeof(ignore) / sizeof(*ignore); i++)
			if(strcmp(ignore[i], filesystem) == 0)
				return;
	if(device != NULL)
	{
		for(i = 0; i < sizeof(cdrom) / sizeof(*cdrom); i++)
			if(strncmp(cdrom[i], device, strlen(cdrom[i])) == 0)
			{
				flags |= DF_REMOVABLE;
				dp = DP_CDROM;
				break;
			}
		for(i = 0; i < sizeof(removable) / sizeof(*removable); i++)
			if(strncmp(removable[i], device, strlen(removable[i]))
					== 0)
			{
				flags |= DF_REMOVABLE;
				dp = DP_REMOVABLE;
				break;
			}
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
		snprintf(buf, sizeof(buf), "%.1f%%", fraction * 100.0);
	}
	if(bsize > 0)
		_list_add_size(buf2, sizeof(buf2), bsize, total);
	else
		buf2[0] = '\0';
	_list_get_iter(volumes, &iter, mountpoint);
	pixbuf = _list_get_icon(volumes, dp, flags, mountpoint);
	gtk_list_store_set(volumes->store, &iter, DC_DEVICE, device,
			DC_PIXBUF, pixbuf, DC_NAME, name,
			DC_ELLIPSIZE, PANGO_ELLIPSIZE_END,
			DC_ELLIPSIZE_SET, TRUE, DC_FILESYSTEM, filesystem,
			DC_FLAGS, flags, DC_MOUNTPOINT, mountpoint, DC_FREE, f,
			DC_FREE_DISPLAY, buf, DC_SIZE, bsize * total,
			DC_SIZE_DISPLAY, buf2, DC_UPDATED, TRUE, -1);
}

static void _list_add_size(char * buf, size_t len, unsigned long bsize,
		fsblkcnt_t total)
{
	double sz = bsize * total;
	char * unit = _("bytes");
	char const * format = "%.1f %s";

	if(sz < 1024)
		format = "%.0f %s";
	else if((sz /= 1024) < 1024)
		unit = _("kB");
	else if((sz /= 1024) < 1024)
		unit = _("MB");
	else if((sz /= 1024) < 1024)
		unit = _("GB");
	else if((sz /= 1024) < 1024)
		unit = _("TB");
	else
	{
		sz /= 1024;
		unit = _("PB");
	}
	snprintf(buf, len, format, sz, unit);
}

static GdkPixbuf * _list_get_icon(Volumes * volumes, VolumesPixbuf dp,
		unsigned int flags, char const * mountpoint)
{
	GdkPixbuf * pixbuf = volumes->icons[dp];

	if(dp == DP_REMOVABLE)
		pixbuf = _list_get_icon_removable(volumes, dp, mountpoint);
	if(flags & DF_SHARED)
		return _list_get_icon_emblem(pixbuf, volumes->width,
				"emblem-shared");
	else if(flags & DF_READONLY)
		return _list_get_icon_emblem(pixbuf, volumes->width,
				"emblem-readonly");
	return pixbuf;
}

static GdkPixbuf * _list_get_icon_emblem(GdkPixbuf * pixbuf, int size,
		char const * emblem)
{
	/* FIXME duplicated from _mime_icon_emblem() in src/vfs.c */
	int esize;
	GdkPixbuf * epixbuf;
	GtkIconTheme * icontheme;
#if GTK_CHECK_VERSION(2, 14, 0)
	const int flags = GTK_ICON_LOOKUP_USE_BUILTIN
		| GTK_ICON_LOOKUP_FORCE_SIZE;
#else
	const int flags = GTK_ICON_LOOKUP_USE_BUILTIN;
#endif

	/* work on a copy */
	pixbuf = gdk_pixbuf_copy(pixbuf);
	/* determine the size of the emblem */
	if(size >= 96)
		esize = 32;
	else if(size >= 48)
		esize = 24;
	else
		esize = 12;
	/* obtain the emblem's icon */
	icontheme = gtk_icon_theme_get_default();
	if((epixbuf = gtk_icon_theme_load_icon(icontheme, emblem, esize, flags,
					NULL)) == NULL)
		return pixbuf;
	/* blit the emblem */
#if 0 /* XXX does not show anything (bottom right) */
	gdk_pixbuf_composite(epixbuf, pixbuf, size - esize, size - esize,
			esize, esize, 0, 0, 1.0, 1.0, GDK_INTERP_NEAREST,
			255);
#else /* blitting at the top left instead */
	gdk_pixbuf_composite(epixbuf, pixbuf, 0, 0, esize, esize, 0, 0,
			1.0, 1.0, GDK_INTERP_NEAREST, 255);
#endif
	g_object_unref(epixbuf);
	return pixbuf;
}

static GdkPixbuf * _list_get_icon_removable(Volumes * volumes, VolumesPixbuf dp,
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

static void _list_loop_mounted(Volumes * volumes, int reset)
{
#if defined(ST_NOWAIT)
	struct statvfs * mnt;
	int res;
	int i;
	unsigned int flags;

	if((res = getmntinfo(&mnt, ST_NOWAIT)) <= 0)
		return;
	if(reset)
		_list_reset(volumes);
	for(i = 0; i < res; i++)
	{
		flags = DF_MOUNTED;
		flags |= (mnt[i].f_flag & ST_LOCAL) ? 0 : DF_NETWORK;
		flags |= (mnt[i].f_flag & (ST_EXRDONLY | ST_EXPORTED))
			? DF_SHARED : 0;
		flags |= (mnt[i].f_flag & ST_RDONLY) ? DF_READONLY : 0;
		_list_add(volumes, (mnt[i].f_flag & ST_ROOTFS)
				? _("Root filesystem") : NULL,
				mnt[i].f_mntfromname, mnt[i].f_fstypename,
				flags, mnt[i].f_mntonname, mnt[i].f_frsize,
				mnt[i].f_bavail, mnt[i].f_blocks);
	}
#elif defined(MNT_NOWAIT)
	struct statfs * mnt;
	int res;
	int i;
	unsigned int flags;

	if((res = getmntinfo(&mnt, MNT_NOWAIT)) <= 0)
		return;
	if(reset)
		_list_reset(volumes);
	for(i = 0; i < res; i++)
	{
		flags = DF_MOUNTED;
		flags |= (mnt[i].f_flags & MNT_LOCAL) ? 0 : DF_NETWORK;
		flags |= (mnt[i].f_flags & MNT_RDONLY) ? DF_READONLY : 0;
		_list_add(volumes, (mnt[i].f_flags & MNT_ROOTFS)
				? _("Root filesystem") : NULL,
				mnt[i].f_mntfromname, mnt[i].f_fstypename,
				flags, mnt[i].f_mntonname, mnt[i].f_bsize,
				mnt[i].f_bavail, mnt[i].f_blocks);
	}
#else
	if(reset)
		_list_reset(volumes);
	_list_add(volumes, _("Root filesystem"), NULL, NULL, 0, "/", 0, 0, 0);
#endif
}

static int _list_loop_unmounted(Volumes * volumes)
{
#if defined(_PATH_FSTAB)
	struct fstab * f;
	unsigned int flags;

	if(setfsent() != 1)
		return -1;
	_list_reset(volumes);
	while((f = getfsent()) != NULL)
	{
# if defined(FSTAB_SW)
		if(strcmp(f->fs_type, FSTAB_SW) == 0)
			continue;
# endif
# if defined(FSTAB_XX)
		if(strcmp(f->fs_type, FSTAB_XX) == 0)
			continue;
# endif
		flags = (strcmp(f->fs_vfstype, "nfs") == 0
				|| strcmp(f->fs_vfstype, "smbfs") == 0)
			? DF_NETWORK : 0;
		flags |= (strcmp(f->fs_type, "ro") == 0) ? DF_READONLY : 0;
		_list_add(volumes, (strcmp(f->fs_file, "/") == 0)
				? _("Root filesystem") : NULL,
				f->fs_spec, f->fs_vfstype,
				flags, f->fs_file, 0, 0, 0);
	}
	return 0;
#else
	(void) volumes;

	return -1;
#endif
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


/* volumes_mount */
static int _volumes_mount(Volumes * volumes, char const * mountpoint)
{
	BrowserPluginHelper * helper = volumes->helper;

	if(browser_vfs_mount(mountpoint) != 0)
		return helper->error(helper->browser, error_get(NULL), 1);
	return 0;
}


/* volumes_unmount */
static int _volumes_unmount(Volumes * volumes, char const * mountpoint)
{
	BrowserPluginHelper * helper = volumes->helper;

	if(browser_vfs_unmount(mountpoint) != 0)
		return helper->error(helper->browser, error_get(NULL), 1);
	return 0;
}


/* callbacks */
/* volumes_on_eject_selection */
static void _volumes_on_eject_selection(gpointer data)
{
	Volumes * volumes = data;
	GtkTreeSelection * treesel;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gchar * device;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(volumes->view));
	if(gtk_tree_selection_get_selected(treesel, &model, &iter) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, DC_DEVICE, &device, -1);
	if(device == NULL)
		return;
	_volumes_eject(volumes, device);
	g_free(device);
}


/* volumes_on_mount_selection */
static void _volumes_on_mount_selection(gpointer data)
{
	Volumes * volumes = data;
	GtkTreeSelection * treesel;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gchar * mountpoint;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(volumes->view));
	if(gtk_tree_selection_get_selected(treesel, &model, &iter) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, DC_MOUNTPOINT, &mountpoint, -1);
	if(mountpoint == NULL)
		return;
	_volumes_mount(volumes, mountpoint);
	g_free(mountpoint);
}


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


/* volumes_on_unmount_selection */
static void _volumes_on_unmount_selection(gpointer data)
{
	Volumes * volumes = data;
	GtkTreeSelection * treesel;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gchar * mountpoint;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(volumes->view));
	if(gtk_tree_selection_get_selected(treesel, &model, &iter) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, DC_MOUNTPOINT, &mountpoint, -1);
	if(mountpoint == NULL)
		return;
	_volumes_unmount(volumes, mountpoint);
	g_free(mountpoint);
}


/* volumes_on_view_button_press */
static void _volumes_on_eject(GtkWidget * widget, gpointer data);
static void _volumes_on_open(GtkWidget * widget, gpointer data);
static void _volumes_on_open_new_window(GtkWidget * widget, gpointer data);
static void _volumes_on_properties(GtkWidget * widget, gpointer data);
static void _volumes_on_mount(GtkWidget * widget, gpointer data);
static void _volumes_on_unmount(GtkWidget * widget, gpointer data);

static gboolean _volumes_on_view_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data)
{
	Volumes * volumes = data;
	GtkTreeSelection * treesel;
	GtkTreeModel * model;
	GtkTreeIter iter;
	unsigned int flags;
	gchar * mountpoint;
	GtkWidget * menu;

	if(event->type != GDK_BUTTON_PRESS
			|| (event->button != 3 && event->button != 0))
		return FALSE;
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	if(gtk_tree_selection_get_selected(treesel, &model, &iter) != TRUE)
		return FALSE;
	gtk_tree_model_get(model, &iter, DC_MOUNTPOINT, &mountpoint,
			DC_FLAGS, &flags, -1);
	if(mountpoint == NULL)
		return FALSE;
	menu = gtk_menu_new();
	/* open */
	widget = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
	g_object_set_data(G_OBJECT(widget), "mountpoint", mountpoint);
	g_signal_connect(widget, "activate", G_CALLBACK(_volumes_on_open),
			volumes);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	widget = gtk_image_menu_item_new_with_mnemonic(
			_("Open in new _window"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
			gtk_image_new_from_icon_name("window-new",
				GTK_ICON_SIZE_MENU));
	g_object_set_data(G_OBJECT(widget), "mountpoint", mountpoint);
	g_signal_connect(widget, "activate", G_CALLBACK(
				_volumes_on_open_new_window), volumes);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	widget = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	/* unmount */
	if(_volumes_can_unmount(flags))
	{
		widget = gtk_image_menu_item_new_with_label(_("Unmount"));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
				gtk_image_new_from_icon_name("hdd_unmount",
					GTK_ICON_SIZE_MENU));
		g_object_set_data(G_OBJECT(widget), "mountpoint", mountpoint);
		g_signal_connect(widget, "activate", G_CALLBACK(
					_volumes_on_unmount), volumes);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	}
	/* mount */
	else if(_volumes_can_mount(flags))
	{
		widget = gtk_image_menu_item_new_with_label(_("Mount"));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
				gtk_image_new_from_icon_name("hdd_unmount",
					GTK_ICON_SIZE_MENU));
		g_object_set_data(G_OBJECT(widget), "mountpoint", mountpoint);
		g_signal_connect(widget, "activate", G_CALLBACK(
					_volumes_on_mount), volumes);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	}
	/* eject */
	if(_volumes_can_eject(flags))
	{
		widget = gtk_image_menu_item_new_with_label(_("Eject"));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
				gtk_image_new_from_icon_name("media-eject",
					GTK_ICON_SIZE_MENU));
		g_object_set_data(G_OBJECT(widget), "mountpoint", mountpoint);
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
	g_free(mountpoint);
#endif
	return TRUE;
}

static void _volumes_on_eject(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	BrowserPluginHelper * helper = volumes->helper;
	gchar * mountpoint;

	mountpoint = g_object_get_data(G_OBJECT(widget), "mountpoint");
	if(_volumes_eject(volumes, mountpoint) != 0)
		helper->error(helper->browser, error_get(NULL), 1);
	g_free(mountpoint);
}

static void _volumes_on_mount(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	BrowserPluginHelper * helper = volumes->helper;
	gchar * mountpoint;

	mountpoint = g_object_get_data(G_OBJECT(widget), "mountpoint");
	if(_volumes_mount(volumes, mountpoint) != 0)
		helper->error(helper->browser, error_get(NULL), 1);
	g_free(mountpoint);
}

static void _volumes_on_open(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	BrowserPluginHelper * helper = volumes->helper;
	gchar * mountpoint;

	mountpoint = g_object_get_data(G_OBJECT(widget), "mountpoint");
	helper->set_location(helper->browser, mountpoint);
	g_free(mountpoint);
}

static void _volumes_on_open_new_window(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	BrowserPluginHelper * helper = volumes->helper;
	gchar * mountpoint;
	char * argv[] = { BINDIR "/" PROGNAME_BROWSER, PROGNAME_BROWSER,
		"--", NULL, NULL };
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

static void _volumes_on_properties(GtkWidget * widget, gpointer data)
{
	Volumes * volumes = data;
	BrowserPluginHelper * helper = volumes->helper;
	gchar * mountpoint;
	char * argv[] = { BINDIR "/" PROGNAME_PROPERTIES, PROGNAME_PROPERTIES,
		"--", NULL, NULL };
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
	BrowserPluginHelper * helper = volumes->helper;
	gchar * mountpoint;

	mountpoint = g_object_get_data(G_OBJECT(widget), "mountpoint");
	if(_volumes_unmount(volumes, mountpoint) != 0)
		helper->error(helper->browser, error_get(NULL), 1);
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
	BrowserPluginHelper * helper = volumes->helper;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gchar * location;
	(void) column;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
	if(gtk_tree_model_get_iter(model, &iter, path) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, DC_MOUNTPOINT, &location, -1);
	helper->set_location(helper->browser, location);
	g_free(location);
}


/* volumes_on_view_row_changed */
static void _volumes_on_view_row_changed(GtkTreeSelection * treesel,
		gpointer data)
{
	Volumes * volumes = data;
	GtkTreeModel * model;
	GtkTreeIter iter;
	unsigned int flags = 0;
	gboolean sensitive = TRUE;

	if(gtk_tree_selection_get_selected(treesel, &model, &iter) != TRUE)
		sensitive = FALSE;
	else
		gtk_tree_model_get(model, &iter, DC_FLAGS, &flags, -1);
	gtk_widget_set_sensitive(GTK_WIDGET(volumes->tb_mount),
			sensitive && _volumes_can_mount(flags));
	gtk_widget_set_sensitive(GTK_WIDGET(volumes->tb_unmount),
			sensitive && _volumes_can_unmount(flags));
	gtk_widget_set_sensitive(GTK_WIDGET(volumes->tb_eject),
			sensitive && _volumes_can_eject(flags));
}
