/* $Id$ */
/* Copyright (c) 2015 Pierre Pronchery <khorben@defora.org> */
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



#ifndef BROWSER_WINDOW_H
# define BROWSER_WINDOW_H

# include "browser.h"


/* BrowserWindow */
/* public */
/* types */
typedef struct _BrowserWindow BrowserWindow;


/* functions */
BrowserWindow * browserwindow_new(String const * directory);
void browserwindow_delete(BrowserWindow * browser);

/* accessors */
Browser * browserwindow_get_browser(BrowserWindow * browser);
GtkWidget * browserwindow_get_window(BrowserWindow * browser);

/* useful */
/* interface */
void browserwindow_show_about(BrowserWindow * browser, gboolean show);
void browserwindow_show_preferences(BrowserWindow * browser, gboolean show);

#endif /* !BROWSER_WINDOW_H */
