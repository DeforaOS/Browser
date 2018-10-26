/* $Id$ */
/* Copyright (c) 2016-2018 Pierre Pronchery <khorben@defora.org> */
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



#ifdef DEBUG
# include <stdio.h>
#endif
#include <string.h>
#include <gtk/gtk.h>
#include <System.h>
#include "desktopicon.h"
#include "desktopiconwindow.h"


/* private */
/* types */
struct _DesktopIconWindow
{
	DesktopIcon * icon;

	/* widgets */
	GtkWidget * widget;
};


/* public */
/* functions */
/* desktopiconwindow_new */
static gboolean _on_desktopiconwindow_closex(void);

DesktopIconWindow * desktopiconwindow_new(DesktopIcon * icon)
{
	DesktopIconWindow * window;
	GtkWindow * w;
	GtkWidget * widget;
	unsigned int size;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p)\n", __func__, icon);
#endif
	if((window = object_new(sizeof(*window))) == NULL)
		return NULL;
	/* window */
	window->widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	w = GTK_WINDOW(window->widget);
	gtk_window_set_decorated(w, FALSE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_focus_on_map(w, FALSE);
#endif
	gtk_window_set_keep_below(w, TRUE);
	gtk_window_set_skip_pager_hint(w, TRUE);
#ifdef EMBEDDED
	gtk_window_set_type_hint(w, GDK_WINDOW_TYPE_HINT_UTILITY);
#else
	gtk_window_set_type_hint(w, GDK_WINDOW_TYPE_HINT_DOCK);
#endif
	gtk_window_set_default_size(w, desktopicon_get_width(icon),
			desktopicon_get_height(icon));
	g_signal_connect(window->widget, "delete-event", G_CALLBACK(
				_on_desktopiconwindow_closex), NULL);
	/* icon */
	widget = desktopicon_get_widget(icon);
	gtk_container_add(GTK_CONTAINER(window->widget), widget);
	window->icon = icon;
	desktopicon_set_window(icon, window->widget);
	return window;
}

static gboolean _on_desktopiconwindow_closex(void)
{
	return TRUE;
}


/* desktopiconwindow_delete */
void desktopiconwindow_delete(DesktopIconWindow * window)
{
	desktopicon_delete(window->icon);
	object_delete(window);
}


/* accessors */
/* desktopiconwindow_get_icon */
DesktopIcon * desktopiconwindow_get_icon(DesktopIconWindow * window)
{
	return window->icon;
}


/* useful */
/* desktopiconwindow_move */
void desktopiconwindow_move(DesktopIconWindow * window, int x, int y)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%d, %d)\n", __func__, x, y);
#endif
	gtk_window_move(GTK_WINDOW(window->widget), x, y);
}


/* desktopiconwindow_show */
void desktopiconwindow_show(DesktopIconWindow * window)
{
	gtk_widget_show_all(window->widget);
}
