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
} Favorites;


/* prototypes */
static Favorites * _favorites_init(BrowserPluginHelper * helper);
static void _favorites_destroy(Favorites * favorites);
static GtkWidget * _favorites_get_widget(Favorites * favorites);
static void _favorites_refresh(Favorites * favorites, GList * selection);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	"Favorites",
	NULL,
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
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeSelection * treesel;

	if((favorites = object_new(sizeof(*favorites))) == NULL)
		return NULL;
	favorites->helper = helper;
	favorites->widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(favorites->widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	favorites->store = gtk_list_store_new(2, GDK_TYPE_PIXBUF,
			G_TYPE_STRING);
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
	String * filename;
	char buf[512];
	size_t len;
	int c;
	GtkTreeIter iter;

	gtk_list_store_clear(favorites->store);
	if((home = getenv("HOME")) == NULL)
		home = g_get_home_dir();
	if((filename = string_new_append(home, "/.gtk-bookmarks", NULL))
			== NULL)
		return;
	fp = fopen(filename, "r");
	string_delete(filename);
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
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_list_store_insert_with_values(favorites->store, &iter, -1,
#else
		gtk_list_store_append(favorites->store, &iter);
		gtk_list_store_set(favorites->store, &iter,
#endif
				1, buf, -1);
	}
	fclose(fp);
}
