/* $Id$ */
/* Copyright (c) 2012-2013 Pierre Pronchery <khorben@defora.org> */
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



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <System.h>
#include "Browser.h"


/* Favorites */
/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	GtkWidget * widget;
	GtkListStore * store;
	GtkWidget * view;
	GdkPixbuf * folder;
} Favorites;


/* prototypes */
static Favorites * _favorites_init(BrowserPluginHelper * helper);
static void _favorites_destroy(Favorites * favorites);
static GtkWidget * _favorites_get_widget(Favorites * favorites);
static void _favorites_refresh(Favorites * favorites, GList * selection);

/* callbacks */
static void _favorites_on_row_activated(GtkTreeView * view, GtkTreePath * path,
		GtkTreeViewColumn * column, gpointer data);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	"Favorites",
	"user-bookmarks",
	NULL,
	_favorites_init,
	_favorites_destroy,
	_favorites_get_widget,
	_favorites_refresh
};


/* private */
/* functions */
/* favorites_init */
static Favorites * _favorites_init(BrowserPluginHelper * helper)
{
	Favorites * favorites;
	GtkIconTheme * icontheme;
	gint size;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeSelection * treesel;
	GError * error = NULL;

	if((favorites = object_new(sizeof(*favorites))) == NULL)
		return NULL;
	favorites->helper = helper;
	icontheme = gtk_icon_theme_get_default();
	gtk_icon_size_lookup(GTK_ICON_SIZE_BUTTON, &size, &size);
	favorites->folder = gtk_icon_theme_load_icon(icontheme, "stock_folder",
			size, GTK_ICON_LOOKUP_USE_BUILTIN, &error);
	if(favorites->folder == NULL)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
	}
	favorites->widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(favorites->widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	favorites->store = gtk_list_store_new(3, GDK_TYPE_PIXBUF,
			G_TYPE_STRING, G_TYPE_STRING);
	favorites->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				favorites->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(favorites->view),
			FALSE);
	/* columns */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", 0, NULL);
	gtk_tree_view_column_set_expand(column, FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(favorites->view), column);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(favorites->view), column);
	/* selection */
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(favorites->view));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
	/* signals */
	g_signal_connect(favorites->view, "row-activated", G_CALLBACK(
				_favorites_on_row_activated), favorites);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(
				favorites->widget), favorites->view);
	gtk_widget_show_all(favorites->widget);
	return favorites;
}


/* favorites_destroy */
static void _favorites_destroy(Favorites * favorites)
{
	/* FIXME implement */
	object_delete(favorites);
}


/* favorites_get_widget */
static GtkWidget * _favorites_get_widget(Favorites * favorites)
{
	return favorites->widget;
}


/* favorites_refresh */
static void _favorites_refresh(Favorites * favorites, GList * selection)
{
	FILE * fp;
	char const * home;
	gchar * filename;
	char buf[512];
	size_t len;
	int c;
	GtkTreeIter iter;

	gtk_list_store_clear(favorites->store);
	if((home = getenv("HOME")) == NULL)
		home = g_get_home_dir();
	if((filename = g_build_filename(home, ".gtk-bookmarks", NULL)) == NULL)
		return;
	fp = fopen(filename, "r");
	g_free(filename);
	if(fp == NULL)
		return;
	while(fgets(buf, sizeof(buf), fp) != NULL)
	{
		if((len = strlen(buf)) == 0)
			/* ignore empty lines */
			continue;
		else if(buf[len - 1] != '\n')
		{
			/* skip the rest of the current line */
			while((c = fgetc(fp)) != EOF && c != '\n');
			continue;
		}
		if(strncmp(buf, "file:///", 8) != 0)
			/* ignore anything but local file: URLs */
			continue;
		buf[len - 1] = '\0';
		memmove(buf, &buf[7], len - 7);
		filename = g_path_get_basename(buf);
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_list_store_insert_with_values(favorites->store, &iter, -1,
#else
		gtk_list_store_append(favorites->store, &iter);
		gtk_list_store_set(favorites->store, &iter,
#endif
				0, favorites->folder, 1, filename, 2, buf, -1);
		g_free(filename);
	}
	fclose(fp);
}


/* callbacks */
static void _favorites_on_row_activated(GtkTreeView * view, GtkTreePath * path,
	GtkTreeViewColumn * column, gpointer data)
{
	Favorites * favorites = data;
	GtkTreeModel * model = GTK_TREE_MODEL(favorites->store);
	GtkTreeIter iter;
	gchar * location;

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, 2, &location, -1);
	favorites->helper->set_location(favorites->helper->browser, location);
	g_free(location);
}
