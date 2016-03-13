/* $Id$ */
/* Copyright (c) 2014-2015 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Browser */
/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice
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
/* TODO:
 * - count the files and directories selected */



#include <sys/stat.h>
#include <System.h>
#include <libintl.h>
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)

#define COMMON_SIZE
#include "../common.c"


/* Selection */
/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	GtkWidget * widget;
	GtkWidget * view;
	GtkListStore * store;
	GtkWidget * status;
} Selection;

typedef enum _SelectionCount
{
	SC_ICON = 0, SC_FILENAME, SC_FILENAME_DISPLAY, SC_SIZE, SC_SIZE_DISPLAY,
	SC_ELLIPSIZE
} SelectionCount;
#define SC_LAST SC_ELLIPSIZE
#define SC_COUNT (SC_LAST + 1)


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
	selection->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	selection->store = gtk_list_store_new(SC_COUNT,
			GDK_TYPE_PIXBUF,	/* SC_ICON */
			G_TYPE_STRING,		/* SC_FILENAME */
			G_TYPE_STRING,		/* SC_FILENAME_DISPLAY */
			G_TYPE_UINT64,		/* SC_SIZE */
			G_TYPE_STRING,		/* SC_SIZE_DISPLAY */
			G_TYPE_UINT);		/* SC_ELLIPSIZE */
	selection->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				selection->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(selection->view),
			TRUE);
	/* column: icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", SC_ICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(selection->view), column);
	/* column: filename */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"),
			renderer, "text", SC_FILENAME_DISPLAY,
			"ellipsize", SC_ELLIPSIZE, NULL);
#if GTK_CHECK_VERSION(2, 4, 0)
	gtk_tree_view_column_set_expand(column, TRUE);
#endif
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, SC_FILENAME_DISPLAY);
	gtk_tree_view_append_column(GTK_TREE_VIEW(selection->view), column);
	/* column: size */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Size"),
			renderer, "text", SC_SIZE_DISPLAY, NULL);
	gtk_tree_view_column_set_sort_column_id(column, SC_SIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(selection->view), column);
	gtk_container_add(GTK_CONTAINER(widget), selection->view);
	gtk_box_pack_start(GTK_BOX(selection->widget), widget, TRUE, TRUE, 0);
	selection->status = gtk_label_new(NULL);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(selection->status, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(selection->status), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(selection->widget), selection->status, FALSE,
			TRUE, 0);
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
	struct stat lst;
	struct stat st;
	struct stat * plst;
	struct stat * pst;
	size_t cnt;
	size_t size;
	char buf[64];

	gtk_list_store_clear(selection->store);
	for(l = selected, cnt = 0, size = 0; l != NULL; l = l->next, cnt++)
	{
		plst = (lstat(l->data, &lst) == 0) ? &lst : NULL;
		pst = (stat(l->data, &st) == 0) ? &st : NULL;
		pixbuf = helper->get_icon(helper->browser, l->data, NULL, plst,
				pst, 16);
		basename = g_path_get_basename(l->data);
		gtk_list_store_append(selection->store, &iter);
		gtk_list_store_set(selection->store, &iter, SC_ICON, pixbuf,
				SC_FILENAME, l->data,
				SC_FILENAME_DISPLAY, basename,
				SC_SIZE, lst.st_size,
				SC_SIZE_DISPLAY, _common_size(lst.st_size),
				SC_ELLIPSIZE, PANGO_ELLIPSIZE_END, -1);
		g_free(basename);
		if(plst != NULL)
			size += lst.st_size;
	}
	snprintf(buf, sizeof(buf), "%lu selected (%s)", cnt,
			_common_size(size));
	gtk_label_set_text(GTK_LABEL(selection->status), buf);
}
