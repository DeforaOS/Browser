/* $Id$ */
/* Copyright (c) 2011-2021 Pierre Pronchery <khorben@defora.org> */
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



#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <System.h>
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)


/* Dirtree */
/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;
	Mime * mime;
	GtkWidget * widget;
	guint source;
	gboolean expanding;
	GdkPixbuf * folder;
	GtkTreeStore * store;
	GtkTreeModel * sorted;
	GtkWidget * view;
} Dirtree;

typedef enum _DirtreeColumn
{
	DC_ICON = 0,
	DC_NAME,
	DC_PATH,
	DC_UPDATED
} DirtreeColumn;
#define DC_LAST DC_UPDATED
#define DC_COUNT (DC_LAST + 1)


/* prototypes */
static Dirtree * _dirtree_init(BrowserPluginHelper * helper);
static void _dirtree_destroy(Dirtree * dirtree);
static GtkWidget * _dirtree_get_widget(Dirtree * dirtree);
static void _dirtree_refresh(Dirtree * dirtree, GList * selection);

static gboolean _dirtree_refresh_folder(Dirtree * dirtree, GtkTreeIter * parent,
		char const * path, char const * basename, gboolean recurse);

/* callbacks */
static gboolean _dirtree_on_idle(gpointer data);
static void _dirtree_on_row_activated(GtkTreeView * view, GtkTreePath * path,
		GtkTreeViewColumn * column, gpointer data);
static void _dirtree_on_row_expanded(GtkTreeView * view, GtkTreeIter * iter,
		GtkTreePath * path, gpointer data);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	N_("Directory tree"),
	"stock_folder",
	NULL,
	_dirtree_init,
	_dirtree_destroy,
	_dirtree_get_widget,
	_dirtree_refresh
};


/* private */
/* functions */
/* dirtree_init */
static Dirtree * _dirtree_init(BrowserPluginHelper * helper)
{
	Dirtree * dirtree;
	GtkIconTheme * icontheme;
	GError * error = NULL;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeSelection * treesel;
	GtkTreeIter iter;
	gint size;
	GdkPixbuf * pixbuf;

	if((dirtree = object_new(sizeof(*dirtree))) == NULL)
		return NULL;
	dirtree->helper = helper;
	dirtree->mime = helper->get_mime(helper->browser);
	dirtree->source = 0;
	dirtree->expanding = FALSE;
	icontheme = gtk_icon_theme_get_default();
	gtk_icon_size_lookup(GTK_ICON_SIZE_BUTTON, &size, &size);
	dirtree->folder = gtk_icon_theme_load_icon(icontheme, "stock_folder",
			size, GTK_ICON_LOOKUP_USE_BUILTIN, &error);
	if(dirtree->folder == NULL)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
	}
	dirtree->widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dirtree->widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	/* backend store */
	dirtree->store = gtk_tree_store_new(DC_COUNT, GDK_TYPE_PIXBUF,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	gtk_tree_store_insert(dirtree->store, &iter, NULL, -1);
	if((pixbuf = browser_vfs_mime_icon(dirtree->mime, "/", NULL, NULL, NULL,
					size)) == NULL)
		pixbuf = dirtree->folder;
	gtk_tree_store_set(dirtree->store, &iter, DC_ICON, pixbuf, DC_NAME, "/",
			DC_PATH, "/", DC_UPDATED, TRUE, -1);
	/* sorted store */
	dirtree->sorted = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(
				dirtree->store));
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(dirtree->sorted),
			DC_NAME, GTK_SORT_ASCENDING);
	/* view */
	dirtree->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				dirtree->sorted));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dirtree->view), FALSE);
	/* columns */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", DC_ICON, NULL);
	gtk_tree_view_column_set_expand(column, FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(dirtree->view), column);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", DC_NAME, NULL);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, DC_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(dirtree->view), column);
	/* selection */
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dirtree->view));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
	/* signals */
	g_signal_connect(dirtree->view, "row-activated", G_CALLBACK(
				_dirtree_on_row_activated), dirtree);
	g_signal_connect(dirtree->view, "row-expanded", G_CALLBACK(
				_dirtree_on_row_expanded), dirtree);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_container_add(GTK_CONTAINER(dirtree->widget), dirtree->view);
#else
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(
				dirtree->widget), dirtree->view);
#endif
	gtk_widget_show_all(dirtree->widget);
	/* populate the root folder */
	dirtree->source = g_idle_add(_dirtree_on_idle, dirtree);
	return dirtree;
}


/* dirtree_destroy */
static void _dirtree_destroy(Dirtree * dirtree)
{
	if(dirtree->source != 0)
		g_source_remove(dirtree->source);
	g_object_unref(dirtree->folder);
	object_delete(dirtree);
}


/* dirtree_get_widget */
static GtkWidget * _dirtree_get_widget(Dirtree * dirtree)
{
	return dirtree->widget;
}


/* dirtree_refresh */
static void _dirtree_refresh(Dirtree * dirtree, GList * selection)
{
	char * path = (selection != NULL) ? selection->data : NULL;
	GtkTreeModel * model = GTK_TREE_MODEL(dirtree->store);
	GtkTreeIter iter;
	GtkTreeIter siter;
	GtkTreePath * q;
	char * p;
	gboolean valid;
	size_t i;
	size_t j;
	char c;

	/* only take care of the tree if this is the first invocation */
	if(dirtree->source == 0 || path == NULL || (p = strdup(path)) == NULL)
		return;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, path);
#endif
	g_source_remove(dirtree->source);
	dirtree->source = 0;
	valid = gtk_tree_model_iter_children(model, &iter, NULL);
	for(i = 0; valid == TRUE && p[i] != '\0'; i++)
	{
		if(p[i] != '/')
			continue;
		p[i] = '\0';
		for(j = i + 1; p[j] != '\0' && p[j] != '/'; j++);
		c = p[j];
		p[j] = '\0';
		valid = _dirtree_refresh_folder(dirtree, &iter, (i == 0)
				? "/" : p, &p[i + 1], TRUE);
		p[i] = '/';
		p[j] = c;
	}
	free(p);
	if(valid != TRUE)
		return;
	/* expand and scroll to the correct position */
	gtk_tree_model_sort_convert_child_iter_to_iter(GTK_TREE_MODEL_SORT(
				dirtree->sorted), &siter, &iter);
	q = gtk_tree_model_get_path(GTK_TREE_MODEL(dirtree->sorted), &siter);
	dirtree->expanding = TRUE;
	gtk_tree_view_expand_to_path(GTK_TREE_VIEW(dirtree->view), q);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(dirtree->view), q, NULL,
			TRUE, 1.0, 1.0);
	dirtree->expanding = FALSE;
	gtk_tree_path_free(q);
}


/* dirtree_refresh_folder */
static gboolean _dirtree_refresh_folder(Dirtree * dirtree, GtkTreeIter * parent,
		char const * path, char const * basename, gboolean recurse)
{
	gboolean ret = FALSE;
	gint size;
	DIR * dir;
	struct dirent * de;
	struct stat st;
	GtkTreeModel * model = GTK_TREE_MODEL(dirtree->store);
	GtkTreeIter iter;
	GtkTreePath * s;
	GtkTreeRowReference * t = NULL;
	gboolean valid;
	String * q;
	gchar * r;
	gboolean b;
	GdkPixbuf * pixbuf;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(parent, \"%s\", \"%s\", %s)\n", __func__,
			path, basename, recurse ? "TRUE" : "FALSE");
#endif
	/* consider all the current nodes obsolete */
	for(valid = gtk_tree_model_iter_children(model, &iter, parent);
			valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
		gtk_tree_store_set(dirtree->store, &iter, DC_UPDATED, FALSE,
				-1);
	if((dir = browser_vfs_opendir(path, NULL)) == NULL)
		return FALSE;
	if(strcmp(path, "/") == 0) /* XXX hack */
		path = "";
	gtk_icon_size_lookup(GTK_ICON_SIZE_BUTTON, &size, &size);
	while((de = browser_vfs_readdir(dir)) != NULL)
	{
		/* skip hidden folders except if we traverse it */
		if(basename != NULL && strcmp(de->d_name, basename) == 0)
			ret = TRUE;
		else if(de->d_name[0] == '.')
			continue;
		if((q = string_new_append(path, "/", de->d_name, NULL)) != NULL
				&& browser_vfs_lstat(q, &st) == 0)
		{
			if(!S_ISDIR(st.st_mode))
				continue;
			pixbuf = browser_vfs_mime_icon(dirtree->mime, q, NULL,
					&st, NULL, size);
		}
		else
			pixbuf = dirtree->folder;
		/* FIXME check if the node already exists */
		r = (q != NULL) ? g_filename_display_basename(q) : NULL;
#if GTK_CHECK_VERSION(2, 10, 0)
		gtk_tree_store_insert_with_values(dirtree->store, &iter, parent,
				-1,
#else
		gtk_tree_store_insert(dirtree->store, &iter, parent, -1);
		gtk_tree_store_set(dirtree->store, &iter,
#endif
				DC_ICON, pixbuf,
				DC_NAME, (r != NULL) ? r : de->d_name,
				DC_PATH, q, DC_UPDATED, TRUE, -1);
		if(recurse && q != NULL)
			_dirtree_refresh_folder(dirtree, &iter, q, NULL,
					(basename != NULL) ? TRUE : FALSE);
		g_free(r);
		string_delete(q);
		if(ret == TRUE && strcmp(de->d_name, basename) == 0)
		{
			s = gtk_tree_model_get_path(model, &iter);
			t = gtk_tree_row_reference_new(model, s);
			gtk_tree_path_free(s);
		}
	}
	browser_vfs_closedir(dir);
	/* remove all the obsolete nodes */
	for(valid = gtk_tree_model_iter_children(model, &iter, parent);
			valid == TRUE; valid = (b == TRUE)
			? gtk_tree_model_iter_next(model, &iter)
			: gtk_tree_store_remove(dirtree->store, &iter))
		gtk_tree_model_get(model, &iter, DC_UPDATED, &b, -1);
	/* return the parent if appropriate */
	if(t != NULL)
	{
		s = gtk_tree_row_reference_get_path(t);
		gtk_tree_model_get_iter(model, parent, s);
		gtk_tree_row_reference_free(t);
	}
	return ret;
}


/* callbacks */
/* dirtree_on_idle */
static gboolean _dirtree_on_idle(gpointer data)
{
	Dirtree * dirtree = data;
	GtkTreeModel * model = GTK_TREE_MODEL(dirtree->store);
	GtkTreeIter iter;

	dirtree->source = 0;
	gtk_tree_model_iter_children(model, &iter, NULL);
	_dirtree_refresh_folder(dirtree, &iter, "/", NULL, TRUE);
	return FALSE;
}


/* dirtree_on_row_activated */
static void _dirtree_on_row_activated(GtkTreeView * view, GtkTreePath * path,
		GtkTreeViewColumn * column, gpointer data)
{
	Dirtree * dirtree = data;
	GtkTreeModel * model = GTK_TREE_MODEL(dirtree->sorted);
	GtkTreeIter iter;
	gchar * location;
	(void) column;

	gtk_tree_view_expand_row(view, path, FALSE);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, DC_PATH, &location, -1);
	dirtree->helper->set_location(dirtree->helper->browser, location);
	g_free(location);
}


/* dirtree_on_row_expanded */
static void _dirtree_on_row_expanded(GtkTreeView * view, GtkTreeIter * iter,
		GtkTreePath * path, gpointer data)
{
	Dirtree * dirtree = data;
	GtkTreeModel * model = GTK_TREE_MODEL(dirtree->store);
	GtkTreeIter child;
	gchar * p;
	(void) view;
	(void) path;

	if(dirtree->expanding == TRUE)
		return;
	gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT(
				dirtree->sorted), &child, iter);
	gtk_tree_model_get(model, &child, DC_PATH, &p, -1);
	_dirtree_refresh_folder(dirtree, &child, p, NULL, TRUE);
	g_free(p);
}
