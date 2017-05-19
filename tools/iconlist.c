/* $Id$ */
/* Copyright (c) 2007-2017 Pierre Pronchery <khorben@defora.org> */
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



#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#ifndef PROGNAME
# define PROGNAME	"iconlist"
#endif


/* types */
typedef struct _Prefs
{
	int flags;
	char * theme;
} Prefs;
#define PREFS_l 1

enum { COL_NAME = 0, COL_PIXBUF, COL_COUNT };


/* iconlist */
static int _iconlist_list(GtkIconTheme * icontheme);
static int _iconlist_do(GtkIconTheme * icontheme);

static int _iconlist(Prefs * prefs)
{
	GtkIconTheme * icontheme;

	if(prefs->theme == NULL)
		icontheme = gtk_icon_theme_get_default();
	else
	{
		icontheme = gtk_icon_theme_new();
		gtk_icon_theme_set_custom_theme(icontheme, prefs->theme);
	}
	if(prefs->flags & PREFS_l)
		return _iconlist_list(icontheme);
	return _iconlist_do(icontheme);
}

static int _iconlist_list(GtkIconTheme * icontheme)
{
	GList * list;
	GList * p;
	gint * sizes;
	gint * q;

	if((list = gtk_icon_theme_list_icons(icontheme, NULL)) == NULL)
		return 1;
	for(p = list; p != NULL; p = p->next)
	{
		printf("%s:", (char*)p->data);
		if((sizes = gtk_icon_theme_get_icon_sizes(icontheme, p->data))
				== NULL)
		{
			puts(" unknown");
			continue;
		}
		for(q = sizes; *q != 0; q++)
			printf(" %d", *q);
		putchar('\n');
		g_free(sizes);
	}
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
	return 0;
}

/* iconlist_do */
static void _do_iconview(GtkWidget * iconview, GtkIconTheme * icontheme);
/* callbacks */
static gboolean _on_closex(GtkWidget * widget);
static void _on_theme_activate(GtkWidget * widget, gpointer data);

static int _iconlist_do(GtkIconTheme * icontheme)
{
	GtkWidget * window;
	GtkWidget * vbox;
	GtkWidget * toolbar;
	GtkToolItem * toolitem;
	GtkWidget * label;
	GtkWidget * entry;
	GtkWidget * scrolled;
	GtkListStore * store;
	GtkWidget * iconview;

	/* window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Icon list");
	gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
	g_signal_connect(window, "delete-event", G_CALLBACK(_on_closex), NULL);
	/* vbox */
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	vbox = gtk_vbox_new(FALSE, 0);
#endif
	/* toolbar */
	toolbar = gtk_toolbar_new();
	toolitem = gtk_tool_item_new();
	label = gtk_label_new("Theme: ");
	gtk_container_add(GTK_CONTAINER(toolitem), label);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	toolitem = gtk_tool_item_new();
	entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(toolitem), entry);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 0);
	/* scrolled window */
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	/* icon view */
	store = gtk_list_store_new(COL_COUNT, G_TYPE_STRING, GDK_TYPE_PIXBUF);
	iconview = gtk_icon_view_new_with_model(GTK_TREE_MODEL(store));
	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(
				_on_theme_activate), iconview); /* late */
	g_object_unref(store);
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(iconview),
			GTK_SELECTION_MULTIPLE);
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(iconview), 0);
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(iconview), 1);
	_do_iconview(iconview, icontheme);
	gtk_container_add(GTK_CONTAINER(scrolled), iconview);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show_all(window);
	gtk_main();
	return 0;
}

static gboolean _on_closex(GtkWidget * widget)
{
	gtk_widget_hide(widget);
	gtk_main_quit();
	return FALSE;
}

static void _on_theme_activate(GtkWidget * widget, gpointer data)
{
	GtkWidget * iconview = data;
	GtkIconTheme * icontheme;
	char const * theme;

	if((theme = gtk_entry_get_text(GTK_ENTRY(widget))) == NULL
			|| strlen(theme) == 0)
		icontheme = gtk_icon_theme_get_default();
	else
	{
		icontheme = gtk_icon_theme_new();
		gtk_icon_theme_set_custom_theme(icontheme, theme);
	}
	_do_iconview(iconview, icontheme);
}

static void _do_iconview(GtkWidget * iconview, GtkIconTheme * icontheme)
{
	GList * list;
	GList * p;
	GtkListStore * store;
	GtkTreeIter iter;
	GdkPixbuf * pixbuf;

	if((list = gtk_icon_theme_list_icons(icontheme, NULL)) == NULL)
		return;
	store = GTK_LIST_STORE(gtk_icon_view_get_model(GTK_ICON_VIEW(
					iconview)));
	gtk_list_store_clear(store);
	for(p = list; p != NULL; p = p->next)
	{
		pixbuf = gtk_icon_theme_load_icon(icontheme, p->data, 48, 0,
				NULL);
		gtk_list_store_insert_with_values(store, &iter, -1,
				COL_NAME, p->data, COL_PIXBUF, pixbuf, -1);
		g_free(p->data);
	}
	g_list_free(list);
}


/* usage */
static int _usage(void)
{
	fputs("Usage: " PROGNAME " [-t theme]\n"
"       " PROGNAME " -l [-t theme]\n", stderr);
	return 1;
}


/* main */
int main(int argc, char * argv[])
{
	Prefs prefs;
	int o;

	gtk_init(&argc, &argv);
	memset(&prefs, 0, sizeof(prefs));
	while((o = getopt(argc, argv, "lt:")) != -1)
		switch(o)
		{
			case 'l':
				prefs.flags |= PREFS_l;
				break;
			case 't':
				prefs.theme = optarg;
				break;
			default:
				return _usage();
		}
	if(optind != argc)
		return _usage();
	return _iconlist(&prefs) == 0 ? 0 : 2;
}
