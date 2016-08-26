/* $Id$ */
/* Copyright (c) 2011-2015 Pierre Pronchery <khorben@defora.org> */
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



#ifndef BROWSER_DESKTOP_H
# define BROWSER_DESKTOP_H

# include <gtk/gtk.h>
# include <Desktop.h>
# include "../include/Desktop.h"
# include "desktopicon.h"


/* Desktop */
/* types */
# ifndef DesktopIcon
#  define DesktopIcon DesktopIcon
typedef struct _DesktopIcon DesktopIcon;
# endif

# ifndef Desktop
#  define Desktop Desktop
typedef struct _Desktop Desktop;
# endif

typedef struct _DesktopPrefs
{
	int alignment;
	int icons;
	int monitor;
	int popup;
	int window;
} DesktopPrefs;


/* functions */
Desktop * desktop_new(DesktopPrefs * prefs);
void desktop_delete(Desktop * desktop);

/* accessors */
/* XXX most of these accessors expose internal structures somehow */
int desktop_get_drag_data(Desktop * desktop, GtkSelectionData * seldata);
GdkPixbuf * desktop_get_file(Desktop * desktop);
GdkPixbuf * desktop_get_folder(Desktop * desktop);
Mime * desktop_get_mime(Desktop * desktop);
GtkIconTheme * desktop_get_theme(Desktop * desktop);

void desktop_set_alignment(Desktop * desktop, DesktopAlignment alignment);
void desktop_set_icons(Desktop * desktop, DesktopIcons icons);
int desktop_set_layout(Desktop * desktop, DesktopLayout layout);

/* useful */
int desktop_error(Desktop * desktop, char const * message, char const * error,
		int ret);

void desktop_refresh(Desktop * desktop);
void desktop_reset(Desktop * desktop);

void desktop_icon_add(Desktop * desktop, DesktopIcon * icon);
void desktop_icon_remove(Desktop * desktop, DesktopIcon * icon);

void desktop_icons_align(Desktop * desktop);
void desktop_icons_sort(Desktop * desktop);

void desktop_select_all(Desktop * desktop);
void desktop_select_above(Desktop * desktop, DesktopIcon * icon);
void desktop_select_under(Desktop * desktop, DesktopIcon * icon);
void desktop_unselect_all(Desktop * desktop);

#endif /* !BROWSER_DESKTOP_H */
