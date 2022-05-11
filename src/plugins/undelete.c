/* $Id$ */
/* Copyright (c) 2015-2022 Pierre Pronchery <khorben@defora.org> */
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



#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <System.h>
#include <libintl.h>
#if defined(__APPLE__)
# include <errno.h>
#elif defined(__linux__)
# include <sys/syscall.h>
# include <unistd.h>
# include <errno.h>
#endif
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)


/* Undelete */
/* private */
/* types */
#ifdef __linux__
typedef struct _UndeleteDirent
{
	unsigned long d_ino;
	unsigned long d_off;
	unsigned short d_reclen;
	char d_name[0];
} UndeleteDirent;
#else
typedef struct dirent UndeleteDirent;
#endif

enum _UndeleteColumn
{
	UC_ICON,
	UC_FILENAME
};
#define UC_LAST UC_FILENAME
#define UC_COUNT (UC_LAST + 1)

typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	guint source;

	/* refresh */
	char * selection;
	int fd;

	/* widgets */
	GtkWidget * window;
	GtkListStore * store;
	GtkWidget * view;
} Undelete;


/* prototypes */
/* plug-in */
static Undelete * _undelete_init(BrowserPluginHelper * helper);
static void _undelete_destroy(Undelete * undelete);
static GtkWidget * _undelete_get_widget(Undelete * undelete);
static void _undelete_refresh(Undelete * undelete, GList * selection);

/* useful */
#if defined(__APPLE__) || defined(__linux__)
static int getdents(int fd, char * buf, unsigned int count);
#endif

/* callbacks */
static gboolean _undelete_on_idle(gpointer data);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	N_("Undelete"),
	"image-missing",
	NULL,
	_undelete_init,
	_undelete_destroy,
	_undelete_get_widget,
	_undelete_refresh
};


/* private */
/* functions */
/* undelete_init */
static Undelete * _undelete_init(BrowserPluginHelper * helper)
{
	Undelete * undelete;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	if((undelete = object_new(sizeof(*undelete))) == NULL)
		return NULL;
	undelete->helper = helper;
	undelete->source = 0;
	undelete->selection = NULL;
	undelete->fd = -1;
	undelete->window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(undelete->window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	undelete->store = gtk_list_store_new(UC_COUNT,
			GDK_TYPE_PIXBUF,	/* UC_ICON */
			G_TYPE_STRING);		/* UC_FILENAME */
	undelete->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				undelete->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(undelete->view), TRUE);
	/* icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", UC_ICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(undelete->view), column);
	/* filename */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"),
			renderer, "text", UC_FILENAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(undelete->view), column);
	gtk_container_add(GTK_CONTAINER(undelete->window), undelete->view);
	gtk_widget_show_all(undelete->window);
	return undelete;
}


/* undelete_destroy */
static void _undelete_destroy(Undelete * undelete)
{
	if(undelete->source != 0)
		g_source_remove(undelete->source);
	if(undelete->fd >= 0)
		close(undelete->fd);
	string_delete(undelete->selection);
	object_delete(undelete);
}


/* undelete_get_widget */
static GtkWidget * _undelete_get_widget(Undelete * undelete)
{
	return undelete->window;
}


/* undelete_refresh */
static void _undelete_refresh(Undelete * undelete, GList * selection)
{
	if(undelete->source != 0)
		g_source_remove(undelete->source);
	if(undelete->fd >= 0)
		close(undelete->fd);
	undelete->fd = -1;
	string_delete(undelete->selection);
	undelete->selection = NULL;
	if(selection == NULL
			|| selection->data == NULL
			|| (undelete->selection = string_new(selection->data))
			== NULL)
		undelete->source = 0;
	else
		undelete->source = g_idle_add(_undelete_on_idle, undelete);
}


/* useful */
#if defined(__APPLE__)
static int getdents(int fd, char * buf, unsigned int count)
{
	(void) fd;
	(void) buf;
	(void) count;

	errno = ENOSYS;
	return -1;
}
#elif defined(__linux__)
static int getdents(int fd, char * buf, unsigned int count)
{
	return syscall(__NR_getdents64, fd, buf, count);
}
#endif


/* callbacks */
/* undelete_on_idle */
static gboolean _undelete_on_idle(gpointer data)
{
	Undelete * undelete = data;
	char buf[65536];
	int i;
	int j;
	UndeleteDirent de;
	size_t namlen;

	if(undelete->fd < 0)
	{
		if((undelete->fd = open(undelete->selection, O_RDONLY)) < 0)
		{
			undelete->helper->error(undelete->helper->browser,
					undelete->selection, 1);
			undelete->source = 0;
			return FALSE;
		}
		return TRUE;
	}
	if((i = getdents(undelete->fd, buf, sizeof(buf))) < 0)
		undelete->helper->error(undelete->helper->browser,
				undelete->selection, 1);
	else if(i > 0)
	{
		for(j = 0; j + (int)sizeof(de) < i; j += de.d_reclen)
		{
			memcpy(&de, &buf[j], sizeof(de));
#ifdef __linux__
			namlen = de.d_reclen - 2
				- offsetof(UndeleteDirent, d_name);
#else
			namlen = de.d_namlen;
#endif
			if(strncmp(de.d_name, ".", 1) == 0 && namlen == 1)
				continue;
			if(strncmp(de.d_name, "..", 2) == 0 && namlen == 2)
				continue;
			/* FIXME implement */
		}
		return TRUE;
	}
	close(undelete->fd);
	string_delete(undelete->selection);
	undelete->selection = NULL;
	undelete->source = 0;
	return FALSE;
}
