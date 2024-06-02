/* $Id$ */
/* Copyright (c) 2006-2024 Pierre Pronchery <khorben@defora.org> */
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



#ifndef BROWSER_BROWSER_H
# define BROWSER_BROWSER_H

# include <dirent.h>
# include <gtk/gtk.h>
# include <System.h>
# include <Desktop.h>
# include "../../include/Browser.h"
# include "../../config.h"


/* Browser */
/* defaults */
# define BROWSER_CONFIG_FILE		"Browser.conf"
# define BROWSER_CONFIG_VENDOR		"DeforaOS/" VENDOR
# define BROWSER_ICON_SIZE_SMALL_ICONS	24
# define BROWSER_ICON_SIZE_ICONS	48
# define BROWSER_ICON_SIZE_THUMBNAILS	96
# define BROWSER_ICON_WRAP_WIDTH	96
# define BROWSER_LIST_WRAP_WIDTH	118
# define BROWSER_THUMBNAIL_WRAP_WIDTH	112


/* types */
typedef struct _BrowserPrefs
{
# if GTK_CHECK_VERSION(2, 6, 0)
	int default_view;
# endif
	gboolean alternate_rows;
	gboolean confirm_before_delete;
	gboolean sort_folders_first;
	gboolean show_hidden_files;
} BrowserPrefs;


/* functions */
Browser * browser_new(GtkWidget * window, GtkAccelGroup * group,
		String const * directory);
Browser * browser_new_copy(Browser * browser);
void browser_delete(Browser * browser);

/* accessors */
char const * browser_get_location(Browser * browser);
char const * browser_get_path_entry(Browser * browser);
BrowserView browser_get_view(Browser * browser);
GtkWidget * browser_get_widget(Browser * browser);
GtkWidget * browser_get_window(Browser * browser);

int browser_set_location(Browser * browser, char const * path);
void browser_set_view(Browser * browser, BrowserView view);

/* useful */
int browser_error(Browser * browser, char const * message, int ret);

int browser_config_load(Browser * browser);
int browser_config_save(Browser * browser);

/* clipboard */
void browser_copy(Browser * browser);
void browser_cut(Browser * browser);
void browser_paste(Browser * browser);

void browser_focus_location(Browser * browser);

void browser_go_back(Browser * browser);
void browser_go_forward(Browser * browser);
void browser_go_home(Browser * browser);

/* plug-ins */
int browser_load(Browser * browser, char const * plugin);
int browser_unload(Browser * browser, char const * plugin);

void browser_open(Browser * browser, char const * path);
void browser_open_with(Browser * browser, char const * path);

void browser_properties(Browser * browser);

void browser_refresh(Browser * browser);

/* selection */
void browser_select_all(Browser * browser);
GList * browser_selection_copy(Browser * browser);
void browser_selection_delete(Browser * browser);
void browser_selection_paste(Browser * browser);
void browser_unselect_all(Browser * browser);

/* interface */
void browser_show_about(Browser * browser, gboolean show);
void browser_show_preferences(Browser * browser, gboolean show);

#endif /* !BROWSER_BROWSER_H */
