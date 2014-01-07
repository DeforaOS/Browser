/* $Id$ */
/* Copyright (c) 2014 Pierre Pronchery <khorben@defora.org> */
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



#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <System.h>
#include <libintl.h>
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)


/* Trash */
/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	guint source;

	/* widgets */
	GtkWidget * widget;
	GtkWidget * view;
	GtkListStore * store;
} Trash;


/* prototypes */
static Trash * _trash_init(BrowserPluginHelper * helper);
static void _trash_destroy(Trash * trash);
static GtkWidget * _trash_get_widget(Trash * trash);
static void _trash_refresh(Trash * trash, GList * selection);

static void _trash_refresh_trash(Trash * trash);

/* callbacks */
static void _trash_on_select_all(gpointer data);
static gboolean _trash_on_timeout(gpointer data);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	N_("Trash"),
	"user-trash",
	NULL,
	_trash_init,
	_trash_destroy,
	_trash_get_widget,
	_trash_refresh
};


/* private */
/* functions */
/* trash_init */
static Trash * _trash_init(BrowserPluginHelper * helper)
{
	Trash * trash;
	GtkWidget * widget;
	GtkToolItem * toolitem;
	GtkTreeSelection * treesel;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	if((trash = object_new(sizeof(*trash))) == NULL)
		return NULL;
	trash->helper = helper;
	trash->source = 0;
#if GTK_CHECK_VERSION(3, 0, 0)
	trash->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	trash->widget = gtk_vbox_new(FALSE, 0);
#endif
	widget = gtk_toolbar_new();
	/* FIXME handle sensitiveness */
	toolitem = gtk_tool_button_new(NULL, _("Move to trash"));
#if GTK_CHECK_VERSION(2, 8, 0)
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toolitem), "user-trash");
#else
	/* FIXME implement */
#endif
	/* FIXME handle the signal */
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	toolitem = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_SELECT_ALL);
	g_signal_connect_swapped(toolitem, "clicked", G_CALLBACK(
				_trash_on_select_all), trash);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_DELETE);
	/* FIXME handle the signal */
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	gtk_box_pack_start(GTK_BOX(trash->widget), widget, FALSE, TRUE, 0);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	trash->store = gtk_list_store_new(5, GDK_TYPE_PIXBUF, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING);
	trash->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				trash->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(trash->view), TRUE);
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(trash->view));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_MULTIPLE);
	/* icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer,
			"pixbuf", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(trash->view), column);
	/* path to the original file */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"),
			renderer, "text", 2, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(trash->view), column);
	/* timestamp */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Deleted"),
			renderer, "text", 4, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(trash->view), column);
	gtk_container_add(GTK_CONTAINER(widget), trash->view);
	gtk_box_pack_start(GTK_BOX(trash->widget), widget, TRUE, TRUE, 0);
	gtk_widget_show_all(trash->widget);
	trash->source = g_idle_add(_trash_on_timeout, trash);
	return trash;
}


/* trash_destroy */
static void _trash_destroy(Trash * trash)
{
	if(trash->source != 0)
		g_source_remove(trash->source);
	object_delete(trash);
}


/* trash_get_widget */
static GtkWidget * _trash_get_widget(Trash * trash)
{
	return trash->widget;
}


/* trash_refresh */
static void _trash_refresh(Trash * trash, GList * selection)
{
	/* FIXME implement (save a copy of the selection) */
}


/* trash_refresh_trash */
static char * _refresh_path(void);

static void _trash_refresh_trash(Trash * trash)
{
	const char ext[] = ".trashinfo";
	const char section[] = "Trash Info";
	BrowserPluginHelper * helper = trash->helper;
	Config * config;
	char * path;
	DIR * dir;
	struct dirent * de;
	size_t len;
	GtkTreeIter iter;
	GdkPixbuf * pixbuf;
	char * p;
	char const * q;
	struct tm tm;
	time_t t;
	char const * u;

	/* FIXME report errors */
	if((path = _refresh_path()) == NULL)
		return;
	if((config = config_new()) == NULL)
	{
		free(path);
		return;
	}
	/* FIXME should try to create the directory */
	if((dir = opendir(path)) == NULL)
	{
		config_delete(config);
		free(path);
		return;
	}
	gtk_list_store_clear(trash->store);
	while((de = readdir(dir)) != NULL)
	{
		if((len = strlen(de->d_name)) <= sizeof(ext))
			continue;
		if(strncmp(&de->d_name[len - sizeof(ext) + 1], ext,
					sizeof(ext)) != 0)
			continue;
		config_reset(config);
		p = g_strdup_printf("%s/%s", path, de->d_name);
		if(config_load(config, p) != 0
				|| (q = config_get(config, section, "Path"))
				== NULL)
		{
			g_free(p);
			continue;
		}
		pixbuf = helper->get_icon(helper->browser, q, NULL, NULL, NULL,
				24);
		t = -1;
		if((u = config_get(config, section, "DeletionDate")) != NULL
				&& strptime(u, "%Y-%m-%dT%H:%M:%S", &tm)
				!= NULL)
			/* XXX also format u in a nicer way */
			t = mktime(&tm);
		else
			u = "";
		gtk_list_store_append(trash->store, &iter);
		gtk_list_store_set(trash->store, &iter, 0, pixbuf, 1, p, 2, q,
				3, t, 4, u, -1);
		g_free(p);
	}
	config_delete(config);
	free(path);
}

static char * _refresh_path(void)
{
	const char fallback[] = ".local/share";
	const char trash[] = "Trash/info";
	char * ret;
	char const * homedir;
	size_t len;

	/* FIXME check $XDG_DATA_HOME first */
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	len = strlen(homedir) + 1 + sizeof(fallback) + 1 + sizeof(trash);
	if((ret = malloc(len)) == NULL)
		return NULL;
	snprintf(ret, len, "%s/%s/%s", homedir, fallback, trash);
	return ret;
}


/* callbacks */
/* trash_on_select_all */
static void _trash_on_select_all(gpointer data)
{
	Trash * trash = data;
	GtkTreeSelection * treesel;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(trash->view));
	gtk_tree_selection_select_all(treesel);
}


/* trash_on_timeout */
static gboolean _trash_on_timeout(gpointer data)
{
	Trash * trash = data;

	trash->source = 0;
	_trash_refresh_trash(trash);
	/* FIXME refresh only if necessary */
	trash->source = g_timeout_add(5000, _trash_on_timeout, trash);
	return FALSE;
}
