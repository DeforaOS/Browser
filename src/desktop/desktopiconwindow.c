/* $Id$ */
/* Copyright (c) 2016 Pierre Pronchery <khorben@defora.org> */
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


/* prototypes */
static void _desktopiconwindow_update_transparency(DesktopIconWindow * window);


/* public */
/* functions */
/* desktopiconwindow_new */
static gboolean _on_desktopiconwindow_closex(void);

DesktopIconWindow * desktopiconwindow_new(DesktopIcon * icon)
{
	DesktopIconWindow * window;
	GtkWindow * w;
	GdkGeometry geometry;
	/* XXX check */
	const unsigned int hints = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE
		| GDK_HINT_BASE_SIZE;
	GtkWidget * widget;

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
	gtk_window_set_resizable(w, FALSE);
	gtk_window_set_skip_pager_hint(w, TRUE);
#ifdef EMBEDDED
	gtk_window_set_type_hint(w, GDK_WINDOW_TYPE_HINT_UTILITY);
#else
	gtk_window_set_type_hint(w, GDK_WINDOW_TYPE_HINT_DOCK);
#endif
	g_signal_connect(window->widget, "delete-event", G_CALLBACK(
				_on_desktopiconwindow_closex), NULL);
	memset(&geometry, 0, sizeof(geometry));
	geometry.min_width = DESKTOPICON_MIN_WIDTH;
	geometry.min_height = DESKTOPICON_MIN_HEIGHT;
	geometry.max_width = DESKTOPICON_MAX_WIDTH;
	geometry.max_height = DESKTOPICON_MAX_HEIGHT;
	geometry.base_width = DESKTOPICON_MIN_WIDTH;
	geometry.base_height = DESKTOPICON_MIN_HEIGHT;
	gtk_window_set_geometry_hints(w, NULL, &geometry, hints);
	/* icon */
	widget = desktopicon_get_widget(icon);
	gtk_container_add(GTK_CONTAINER(window->widget), widget);
	window->icon = icon;
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


/* private */
/* functions */
/* desktopiconwindow_update_transparency */
static void _desktopiconwindow_update_transparency(DesktopIconWindow * window)
{
	DesktopIcon * icon;
	GtkWidget * widget;
	GdkPixbuf * pixbuf;
	int width;
	int height;
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkRGBA black = { 0.0, 0.0, 0.0, 1.0 };
	GdkRGBA white = { 1.0, 1.0, 1.0, 1.0 };
#else
	int iwidth;
	int iheight;
	GdkBitmap * mask;
	GdkBitmap * iconmask;
	GdkGC * gc;
	GdkColor black = { 0, 0, 0, 0 };
	GdkColor white = { 0xffffffff, 0xffff, 0xffff, 0xffff };
	int offset;
#endif
	GtkRequisition req;

	icon = desktopiconwindow_get_icon(window);
	widget = desktopicon_get_image(icon);
	if((pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(widget))) == NULL)
		return; /* XXX report error */
	gtk_window_get_size(GTK_WINDOW(window->widget), &width, &height);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\") window is %dx%d\n", __func__,
			desktopicon->name, width, height);
#endif
#if GTK_CHECK_VERSION(3, 0, 0)
	/* FIXME re-implement */
	widget = desktopicon_get_label(icon);
	gtk_widget_get_preferred_size(widget, NULL, &req);
#else
	iwidth = gdk_pixbuf_get_width(pixbuf);
	iheight = gdk_pixbuf_get_height(pixbuf);
	mask = gdk_pixmap_new(NULL, width, height, 1);
	gdk_pixbuf_render_pixmap_and_mask(pixbuf, NULL, &iconmask, 255);
	gc = gdk_gc_new(mask);
	gdk_gc_set_foreground(gc, &black);
	gdk_draw_rectangle(mask, gc, TRUE, 0, 0, width, height);
	gdk_draw_drawable(mask, gc, iconmask, 0, 0, (width - iwidth) / 2,
			(DESKTOPICON_ICON_SIZE - iheight) / 2, -1, -1);
	gdk_gc_set_foreground(gc, &white);
	widget = desktopicon_get_label(icon);
	gtk_widget_size_request(widget, &req);
# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\") label is %dx%d\n", __func__,
			desktopicon->name, req.width, req.height);
# endif
	offset = DESKTOPICON_ICON_SIZE + 4;
	gdk_draw_rectangle(mask, gc, TRUE, (width - req.width - 8) / 2,
			offset /* + ((height - offset - req.height - 8)
				/ 2) */, req.width + 8, req.height + 8);
	widget = desktopicon_get_widget(icon);
	gtk_widget_shape_combine_mask(widget, mask, 0, 0);
	g_object_unref(gc);
	g_object_unref(iconmask);
	g_object_unref(mask);
#endif
}
