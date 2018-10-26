/* $Id$ */
/* Copyright (c) 2008-2016 Pierre Pronchery <khorben@defora.org> */
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



#ifdef __sun
# include <sys/stat.h>
#endif
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <gtk/gtk.h>
#include "../config.h"

#ifndef PROGNAME
# define PROGNAME		"dirtree"
#endif
#ifndef PROGNAME_BROWSER
# define PROGNAME_BROWSER	"browser"
#endif
#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef BINDIR
# define BINDIR			PREFIX "/bin"
#endif


/* dirtree */
/* types */
typedef int Prefs;
#define PREFS_p		0x1
#define PREFS_u		0x2


/* variables */
static GdkPixbuf * _folder = NULL;


/* prototypes */
static int _dirtree_new(Prefs * prefs, char const * pathname);
static int _dirtree_error(char const * message, int ret);


/* functions */
/* dirtree_new */
static int _dirtree_add(GtkTreeStore * store, GtkTreeIter * iter);
/* callbacks */
static gboolean _on_dirtree_closex(gpointer data);
static void _on_dirtree_default(GtkTreeView * view, GtkTreePath * path,
		GtkTreeViewColumn * column, gpointer data);

static int _dirtree_new(Prefs * prefs, char const * pathname)
{
	GtkIconTheme * theme;
	GtkWidget * scrolled;
	GtkWidget * window;
	GtkTreeStore * store;
	GtkTreeIter iter;
	GtkWidget * treeview;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	char * p;

	if((p = strdup(pathname)) == NULL)
		return -1;
	if(_folder == NULL)
	{
		theme = gtk_icon_theme_get_default();
		_folder = gtk_icon_theme_load_icon(theme, "stock_folder", 16, 0,
				NULL);
	}
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Directory tree");
	gtk_window_set_default_size(GTK_WINDOW(window), 480, 640);
	g_signal_connect_swapped(G_OBJECT(window), "delete-event", G_CALLBACK(
				_on_dirtree_closex), window);
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	store = gtk_tree_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING,
			G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), 2,
			GTK_SORT_ASCENDING);
	gtk_tree_store_append(store, &iter, NULL);
	gtk_tree_store_set(store, &iter, 0, _folder, 1, pathname, 2,
			basename(p), -1);
	_dirtree_add(store, &iter);
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("Icon", renderer,
			"pixbuf", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Name", renderer,
			"text", 2, NULL);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(treeview), "row-activated", G_CALLBACK(
				_on_dirtree_default), prefs);
	gtk_container_add(GTK_CONTAINER(scrolled), treeview);
	gtk_container_add(GTK_CONTAINER(window), scrolled);
	gtk_widget_show_all(window);
	if(*prefs & PREFS_u)
		gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));
	free(p);
	return 0;
}

static int _dirtree_add(GtkTreeStore * store, GtkTreeIter * iter)
{
	GValue pathname;
	const char * p;
	size_t len;
	char * str;
	DIR * dir;
	struct dirent * de;
#ifdef __sun
	struct stat st;
#endif
	char * q;
	GtkTreeIter iter2;

	memset(&pathname, 0, sizeof(pathname));
	gtk_tree_model_get_value(GTK_TREE_MODEL(store), iter, 1, &pathname);
	p = g_value_get_string(&pathname);
	len = strlen(p) + 1;
	if((dir = opendir(p)) == NULL
			|| (str = malloc(len + 1)) == NULL)
	{
		perror(p);
		g_value_unset(&pathname);
		return 1;
	}
	snprintf(str, len + 1, "%s/", p);
	while((de = readdir(dir)) != NULL)
	{
#ifdef __sun
		if(strcmp(".", de->d_name) == 0
				|| strcmp("..", de->d_name) == 0)
			continue;
#else
		if(!(de->d_type & DT_DIR)
				|| strcmp(".", de->d_name) == 0
				|| strcmp("..", de->d_name) == 0)
			continue;
#endif
		if((q = realloc(str, len + strlen(de->d_name) + 1)) == NULL)
			continue;
		str = q;
		snprintf(&str[len], strlen(de->d_name) + 1, "%s", de->d_name);
#ifdef __sun
		if(lstat(str, &st) != 0 || (st.st_mode & S_IFDIR) == 0)
			continue;
#endif
		gtk_tree_store_append(store, &iter2, iter);
		if((q = g_filename_to_utf8(de->d_name, strlen(de->d_name), NULL,
						NULL, NULL)) == NULL)
			q = de->d_name;
		gtk_tree_store_set(store, &iter2, 0, _folder, 1, str, 2, q, -1);
		_dirtree_add(store, &iter2);
	}
	closedir(dir);
	free(str);
	g_value_unset(&pathname);
	return 0;
}

static gboolean _on_dirtree_closex(gpointer data)
{
	GtkWidget * window = data;

	gtk_widget_hide(window);
	gtk_main_quit();
	return FALSE;
}

static void _on_dirtree_default(GtkTreeView * view, GtkTreePath * path,
		GtkTreeViewColumn * column, gpointer data)
{
	Prefs * prefs = data;
	GtkTreeModel * model;
	GtkTreeIter iter;
	char * location;
	pid_t pid;
	(void) column;

	model = gtk_tree_view_get_model(view);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, 1, &location, -1);
	if(location == NULL)
		return; /* FIXME should not happen */
	if(*prefs & PREFS_p)
	{
		if((pid = fork()) == -1)
		{
			_dirtree_error("fork", 1);
			return;
		}
		else if(pid != 0)
			return;
	}
	execl(BINDIR "/" PROGNAME_BROWSER, PROGNAME_BROWSER, "--", location,
			NULL);
	perror(PROGNAME_BROWSER);
}


/* dirtree_error */
static int _dirtree_error(char const * message, int ret)
{
	fputs(PROGNAME ": ", stderr);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fputs("Usage: " PROGNAME " [-pu][pathname]\n"
"  -p\tKeep the directory tree opened when opening a folder\n"
"  -u\tExpand all entries\n", stderr);
	return 1;
}


/* main */
int main(int argc, char * argv[])
{
	Prefs prefs = 0;
	int o;
	char * root = "/";

	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "pu")) != -1)
		switch(o)
		{
			case 'p':
				prefs |= PREFS_p;
				break;
			case 'u':
				prefs |= PREFS_u;
				break;
			default:
				return _usage();
		}
	if(optind + 1 == argc)
		root = argv[optind];
	else if(optind != argc)
		return _usage();
	_dirtree_new(&prefs, root);
	gtk_main();
	return 0;
}
