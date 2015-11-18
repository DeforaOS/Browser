/* $Id$ */
/* Copyright (c) 2006-2015 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Browser */
/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */



#ifndef BROWSER_BROWSER_H
# define BROWSER_BROWSER_H

# include <dirent.h>
# include <gtk/gtk.h>
# include <System.h>
# include <Desktop.h>
# include "../include/Browser.h"


/* Browser */
/* defaults */
# define BROWSER_CONFIG_FILE		".browser"
# define BROWSER_ICON_WRAP_WIDTH	96
# define BROWSER_LIST_WRAP_WIDTH	118
# define BROWSER_THUMBNAIL_WRAP_WIDTH	112


/* types */
typedef enum _BrowserView
{
	BV_DETAILS = 0,
# if GTK_CHECK_VERSION(2, 6, 0)
	BV_ICONS,
	BV_LIST,
	BV_THUMBNAILS
} BrowserView;
#  define BV_FIRST BV_DETAILS
#  define BV_LAST BV_THUMBNAILS
# else
} BrowserView;
#  define BV_FIRST BV_DETAILS
#  define BV_LAST BV_DETAILS
# endif
# define BV_COUNT (BV_LAST + 1)

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
