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
/* FIXME:
 * - count the files and directories selected
 * - count the total size */



#include <System.h>
#include <libintl.h>
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)


/* Selection */
/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	GtkWidget * widget;
	GtkWidget * view;
	GtkListStore * store;
} Selection;


/* prototypes */
static Selection * _selection_init(BrowserPluginHelper * helper);
static void _selection_destroy(Selection * selection);
static GtkWidget * _selection_get_widget(Selection * selection);
static void _selection_refresh(Selection * selection, GList * selected);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	N_("Selection"),
	"stock_select-all",
	NULL,
	_selection_init,
	_selection_destroy,
	_selection_get_widget,
	_selection_refresh
};


/* private */
/* functions */
/* selection_init */
static Selection * _selection_init(BrowserPluginHelper * helper)
{
	Selection * selection;
	GtkWidget * widget;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	if((selection = object_new(sizeof(*selection))) == NULL)
		return NULL;
	selection->helper = helper;
#if GTK_CHECK_VERSION(3, 0, 0)
	selection->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	selection->widget = gtk_vbox_new(FALSE, 0);
#endif
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	selection->store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING,
			G_TYPE_STRING);
	selection->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				selection->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(selection->view),
			FALSE);
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(selection->view), column);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"),
			renderer, "text", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(selection->view), column);
	gtk_container_add(GTK_CONTAINER(widget), selection->view);
	gtk_box_pack_start(GTK_BOX(selection->widget), widget, TRUE, TRUE, 0);
	gtk_widget_show_all(selection->widget);
	return selection;
}


/* selection_destroy */
static void _selection_destroy(Selection * selection)
{
	/* FIXME implement */
	object_delete(selection);
}


/* selection_get_widget */
static GtkWidget * _selection_get_widget(Selection * selection)
{
	return selection->widget;
}


/* selection_refresh */
static void _selection_refresh(Selection * selection, GList * selected)
{
	BrowserPluginHelper * helper = selection->helper;
	GList * l;
	gchar * basename;
	GtkTreeIter iter;
	GdkPixbuf * pixbuf;

	gtk_list_store_clear(selection->store);
	for(l = selected; l != NULL; l = l->next)
	{
		pixbuf = helper->get_icon(helper->browser, l->data, NULL, NULL,
				NULL, 16);
		basename = g_path_get_basename(l->data);
		gtk_list_store_append(selection->store, &iter);
		gtk_list_store_set(selection->store, &iter, 0, pixbuf,
				1, l->data, 2, basename, -1);
		g_free(basename);
	}
}
