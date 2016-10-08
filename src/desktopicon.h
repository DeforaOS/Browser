/* $Id$ */
/* Copyright (c) 2010-2016 Pierre Pronchery <khorben@defora.org> */
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



#ifndef BROWSER_DESKTOPICON_H
# define BROWSER_DESKTOPICON_H

# include <gtk/gtk.h>
# include "common.h"


/* DesktopIcon */
/* types */
typedef void (*DesktopIconCallback)(Desktop * desktop, gpointer data);


/* constants */
#define DESKTOPICON_ICON_SIZE	48
#define DESKTOPICON_MAX_HEIGHT	(DESKTOPICON_ICON_SIZE + 40)
#define DESKTOPICON_MAX_WIDTH	(DESKTOPICON_ICON_SIZE << 1)
#define DESKTOPICON_MIN_HEIGHT	DESKTOPICON_MAX_HEIGHT
#define DESKTOPICON_MIN_WIDTH	DESKTOPICON_MAX_WIDTH


/* functions */
DesktopIcon * desktopicon_new(Desktop * desktop, char const * name,
		char const * url);
DesktopIcon * desktopicon_new_application(Desktop * desktop, char const * path,
		char const * datadir);
DesktopIcon * desktopicon_new_category(Desktop * desktop, char const * name,
		char const * icon);
void desktopicon_delete(DesktopIcon * desktopicon);

/* accessors */
gboolean desktopicon_get_first(DesktopIcon * desktopicon);
GtkWidget * desktopicon_get_image(DesktopIcon * desktopicon);
gboolean desktopicon_get_immutable(DesktopIcon * desktopicon);
gboolean desktopicon_get_isdir(DesktopIcon * desktopicon);
GtkWidget * desktopicon_get_label(DesktopIcon * desktopicon);
char const * desktopicon_get_name(DesktopIcon * desktopicon);
char const * desktopicon_get_path(DesktopIcon * desktopicon);
gboolean desktopicon_get_selected(DesktopIcon * desktopicon);
gboolean desktopicon_get_updated(DesktopIcon * desktopicon);
GtkWidget * desktopicon_get_widget(DesktopIcon * desktopicon);

# if GTK_CHECK_VERSION(3, 0, 0)
void desktopicon_set_background(DesktopIcon * desktopicon, GdkRGBA * color);
# else
void desktopicon_set_background(DesktopIcon * desktopicon, GdkColor * color);
# endif
void desktopicon_set_callback(DesktopIcon * desktopicon,
		DesktopIconCallback callback, gpointer data);
void desktopicon_set_confirm(DesktopIcon * desktopicon, gboolean confirm);
void desktopicon_set_executable(DesktopIcon * desktopicon, gboolean executable);
void desktopicon_set_first(DesktopIcon * desktopicon, gboolean first);
void desktopicon_set_font(DesktopIcon * desktopicon,
		PangoFontDescription * font);
# if GTK_CHECK_VERSION(3, 0, 0)
void desktopicon_set_foreground(DesktopIcon * desktopicon, GdkRGBA * color);
# else
void desktopicon_set_foreground(DesktopIcon * desktopicon, GdkColor * color);
# endif
void desktopicon_set_icon(DesktopIcon * desktopicon, GdkPixbuf * icon);
void desktopicon_set_immutable(DesktopIcon * desktopicon, gboolean immutable);
void desktopicon_set_selected(DesktopIcon * desktopicon, gboolean selected);
void desktopicon_set_updated(DesktopIcon * desktopicon, gboolean updated);

#endif /* !BROWSER_DESKTOPICON_H */
