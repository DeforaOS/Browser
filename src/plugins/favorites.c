/* $Id$ */
/* Copyright (c) 2012-2014 Pierre Pronchery <khorben@defora.org> */
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
/* FIXME:
 * - no longer skip unknown elements (they are lost on add/remove)
 * - store the complete line within the list store */



#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <System.h>
#include "Browser.h"
#define _(string) gettext(string)


/* Favorites */
/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	GList * selection;

	/* widgets */
	GtkWidget * widget;
	GtkListStore * store;
	GtkWidget * view;
	GdkPixbuf * folder;
} Favorites;

typedef enum _FavoritesColumn
{
	FC_ICON = 0,
	FC_NAME,
	FC_PATH
} FavoritesColumn;
#define FC_LAST FC_PATH
#define FC_COUNT (FC_LAST + 1)


/* prototypes */
/* plug-in */
static Favorites * _favorites_init(BrowserPluginHelper * helper);
static void _favorites_destroy(Favorites * favorites);
static GtkWidget * _favorites_get_widget(Favorites * favorites);
static void _favorites_refresh(Favorites * favorites, GList * selection);

/* accessors */
static gchar * _favorites_get_filename(void);

/* useful */
static int _favorites_save(Favorites * favorites);

/* callbacks */
static void _favorites_on_add(gpointer data);
static void _favorites_on_remove(gpointer data);
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
	GtkWidget * widget;
	GtkToolItem * toolitem;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeSelection * treesel;
	GError * error = NULL;

	if((favorites = object_new(sizeof(*favorites))) == NULL)
		return NULL;
	favorites->helper = helper;
	favorites->selection = NULL;
	icontheme = gtk_icon_theme_get_default();
	gtk_icon_size_lookup(GTK_ICON_SIZE_BUTTON, &size, &size);
	favorites->folder = gtk_icon_theme_load_icon(icontheme, "stock_folder",
			size, GTK_ICON_LOOKUP_USE_BUILTIN, &error);
	if(favorites->folder == NULL)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
	}
#if GTK_CHECK_VERSION(3, 0, 0)
	favorites->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	favorites->widget = gtk_vbox_new(FALSE, 0);
#endif
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	favorites->store = gtk_list_store_new(FC_COUNT, GDK_TYPE_PIXBUF,
			G_TYPE_STRING, G_TYPE_STRING);
	favorites->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				favorites->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(favorites->view),
			FALSE);
	/* columns */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", FC_ICON, NULL);
	gtk_tree_view_column_set_expand(column, FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(favorites->view), column);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", FC_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(favorites->view), column);
	/* selection */
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(favorites->view));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
	/* signals */
	g_signal_connect(favorites->view, "row-activated", G_CALLBACK(
				_favorites_on_row_activated), favorites);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(widget),
			favorites->view);
	gtk_box_pack_start(GTK_BOX(favorites->widget), widget, TRUE, TRUE, 0);
	/* lower toolbar */
	widget = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(widget), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(widget), GTK_TOOLBAR_ICONS);
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_ADD);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(GTK_WIDGET(toolitem),
			_("Add to bookmarks"));
#endif
	g_signal_connect_swapped(toolitem, "clicked", G_CALLBACK(
				_favorites_on_add), favorites);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_REMOVE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(GTK_WIDGET(toolitem),
			_("Remove from bookmarks"));
#endif
	g_signal_connect_swapped(toolitem, "clicked", G_CALLBACK(
				_favorites_on_remove), favorites);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	gtk_box_pack_start(GTK_BOX(favorites->widget), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(favorites->widget);
	return favorites;
}


/* favorites_destroy */
static void _favorites_destroy(Favorites * favorites)
{
	g_list_foreach(favorites->selection, (GFunc)g_free, NULL);
	g_list_free(favorites->selection);
	object_delete(favorites);
}


/* favorites_get_widget */
static GtkWidget * _favorites_get_widget(Favorites * favorites)
{
	return favorites->widget;
}


/* favorites_refresh */
static void _refresh_copy(gchar const * pathname, gpointer data);

static void _favorites_refresh(Favorites * favorites, GList * selection)
{
	FILE * fp;
	gchar * filename;
	char buf[512];
	size_t len;
	int c;
	GtkTreeIter iter;
	char const scheme[] = "file:///";

	/* obtain the current selection */
	g_list_foreach(favorites->selection, (GFunc)g_free, NULL);
	g_list_free(favorites->selection);
	favorites->selection = NULL;
	g_list_foreach(selection, (GFunc)_refresh_copy, favorites);
	/* refresh the bookmarks */
	gtk_list_store_clear(favorites->store);
	if((filename = _favorites_get_filename()) == NULL)
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
		if(strncmp(buf, scheme, sizeof(scheme) - 1) != 0)
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
				FC_ICON, favorites->folder, FC_NAME, filename,
				FC_PATH, buf, -1);
		g_free(filename);
	}
	fclose(fp);
}

static void _refresh_copy(gchar const * pathname, gpointer data)
{
	Favorites * favorites = data;

	favorites->selection = g_list_append(favorites->selection,
			g_strdup(pathname));
}


/* accessors */
/* favorites_get_filename */
static gchar * _favorites_get_filename(void)
{
	char const * home;

	if((home = getenv("HOME")) == NULL)
		home = g_get_home_dir();
#if GTK_CHECK_VERSION(3, 0, 0)
	/* FIXME may depend on the environment (XDG_CONFIG_HOME) */
	return g_build_filename(home, ".config", "gtk-3.0", "bookmarks", NULL);
#else
	return g_build_filename(home, ".gtk-bookmarks", NULL);
#endif
}


/* useful */
/* favorites_save */
static int _favorites_save(Favorites * favorites)
{
	GtkTreeModel * model = GTK_TREE_MODEL(favorites->store);
	GtkTreeIter iter;
	gchar * p;
	FILE * fp;
	gboolean valid;

	if((p = _favorites_get_filename()) == NULL)
		return -1;
	fp = fopen(p, "w");
	g_free(p);
	if(fp == NULL)
		return -1;
	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, FC_PATH, &p, -1);
		if(p == NULL)
			continue;
		fprintf(fp, "%s%s\n", "file://", p);
		g_free(p);
	}
	return fclose(fp);
}


/* callbacks */
/* favorites_on_add */
static void _on_add_filename(gchar const * pathname, gpointer data);

static void _favorites_on_add(gpointer data)
{
	Favorites * favorites = data;

	g_list_foreach(favorites->selection, (GFunc)_on_add_filename,
			favorites);
}

static void _on_add_filename(gchar const * pathname, gpointer data)
{
	Favorites * favorites = data;
	GtkTreeIter iter;
	struct stat st;
	gchar * filename;

	/* XXX ignore non-directories */
	if(stat(pathname, &st) != 0 || !S_ISDIR(st.st_mode))
		return;
	if((filename = g_path_get_basename(pathname)) == NULL)
		return;
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_list_store_insert_with_values(favorites->store, &iter, -1,
#else
	gtk_list_store_append(favorites->store, &iter);
	gtk_list_store_set(favorites->store, &iter,
#endif
			FC_ICON, favorites->folder, FC_NAME, filename,
			FC_PATH, pathname, -1);
	g_free(filename);
	_favorites_save(favorites);
}


/* favorites_on_remove */
static void _favorites_on_remove(gpointer data)
{
	Favorites * favorites = data;
	GtkTreeModel * model = GTK_TREE_MODEL(favorites->store);
	GtkTreeSelection * treesel;
	GtkTreeIter iter;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(favorites->view));
	if(gtk_tree_selection_get_selected(treesel, &model, &iter) != TRUE)
		return;
	gtk_list_store_remove(favorites->store, &iter);
	_favorites_save(favorites);
}


/* favorites_on_row_activated */
static void _favorites_on_row_activated(GtkTreeView * view, GtkTreePath * path,
	GtkTreeViewColumn * column, gpointer data)
{
	Favorites * favorites = data;
	GtkTreeModel * model = GTK_TREE_MODEL(favorites->store);
	GtkTreeIter iter;
	gchar * location;

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, FC_PATH, &location, -1);
	favorites->helper->set_location(favorites->helper->browser, location);
	g_free(location);
}
