/* $Id$ */
/* Copyright (c) 2006-2016 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/param.h>
#ifndef __GNU__ /* XXX hurd portability */
# include <sys/mount.h>
# if defined(__linux__) || defined(__CYGWIN__) || defined(__sun)
#  define unmount(a, b) umount(a)
# endif
# ifndef unmount
#  define unmount unmount
# endif
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include "callbacks.h"
#include "browser.h"
#include "../../config.h"
#define _(string) gettext(string)

#define COMMON_SYMLINK
#include "../common.c"


/* public */
/* functions */
/* callbacks */
/* accelerators */
/* on_location */
void on_location(gpointer data)
{
	Browser * browser = data;

	browser_focus_location(browser);
}


/* on_new_window */
void on_new_window(gpointer data)
{
	Browser * browser = data;

	browser_new_copy(browser);
}


/* on_open_file */
void on_open_file(gpointer data)
{
	Browser * browser = data;

	browser_open(browser, NULL);
}


/* toolbar */
/* on_back */
void on_back(gpointer data)
{
	Browser * browser = data;

	browser_go_back(browser);
}


/* on_copy */
void on_copy(gpointer data)
{
	Browser * browser = data;

	browser_copy(browser);
}


/* on_cut */
void on_cut(gpointer data)
{
	Browser * browser = data;

	browser_cut(browser);
}


/* on_delete */
void on_delete(gpointer data)
{
	Browser * browser = data;

	browser_selection_delete(browser);
}


/* on_forward */
void on_forward(gpointer data)
{
	Browser * browser = data;

	browser_go_forward(browser);
}


/* on_home */
void on_home(gpointer data)
{
	Browser * browser = data;

	browser_go_home(browser);
}


/* on_new_folder */
void on_new_folder(gpointer data)
{
	Browser * browser = data;
	char const * newfolder = _("New folder");
	char const * location;
	size_t len;
	char * path;

	if((location = browser_get_location(browser)) == NULL)
		return;
	len = strlen(location) + strlen(newfolder) + 2;
	if((path = malloc(len)) == NULL)
	{
		browser_error(browser, strerror(errno), 1);
		return;
	}
	snprintf(path, len, "%s/%s", location, newfolder);
	if(mkdir(path, 0777) != 0)
		browser_error(browser, strerror(errno), 1);
	free(path);
}


/* on_new_symlink */
void on_new_symlink(gpointer data)
{
	Browser * browser = data;
	GtkWidget * window;
	char const * location;

	if((location = browser_get_location(browser)) == NULL)
		return;
	window = browser_get_window(browser);
	if(_common_symlink(window, location) != 0)
		browser_error(browser, strerror(errno), 1);
}


/* on_paste */
void on_paste(gpointer data)
{
	Browser * browser = data;

	browser_paste(browser);
}


/* on_preferences */
void on_preferences(gpointer data)
{
	Browser * browser = data;

	browser_show_preferences(browser, TRUE);
}


/* on_properties */
void on_properties(gpointer data)
{
	Browser * browser = data;

	browser_properties(browser);
}


/* on_refresh */
void on_refresh(gpointer data)
{
	Browser * browser = data;

	browser_refresh(browser);
}


/* on_updir */
void on_updir(gpointer data)
{
	Browser * browser = data;
	char const * location;
	char * dir;

	if((location = browser_get_location(browser)) == NULL)
		return;
	dir = g_path_get_dirname(location);
	browser_set_location(browser, dir);
	g_free(dir);
}


#if GTK_CHECK_VERSION(2, 6, 0)
/* on_view_as */
void on_view_as(gpointer data)
{
	Browser * browser = data;
	BrowserView view;

	view = browser_get_view(browser);
	switch(view)
	{
		case BV_DETAILS:
			browser_set_view(browser, BV_ICONS);
			break;
		case BV_LIST:
			browser_set_view(browser, BV_THUMBNAILS);
			break;
		case BV_ICONS:
			browser_set_view(browser, BV_LIST);
			break;
		case BV_THUMBNAILS:
			browser_set_view(browser, BV_DETAILS);
			break;
	}
}


/* on_view_details */
void on_view_details(gpointer data)
{
	Browser * browser = data;

	browser_set_view(browser, BV_DETAILS);
}


/* on_view_icons */
void on_view_icons(gpointer data)
{
	Browser * browser = data;

	browser_set_view(browser, BV_ICONS);
}


/* on_view_list */
void on_view_list(gpointer data)
{
	Browser * browser = data;

	browser_set_view(browser, BV_LIST);
}


/* on_view_thumbnails */
void on_view_thumbnails(gpointer data)
{
	Browser * browser = data;

	browser_set_view(browser, BV_THUMBNAILS);
}
#endif


/* address bar */
/* on_path_activate */
void on_path_activate(gpointer data)
{
	Browser * browser = data;
	gchar const * p;

	p = browser_get_path_entry(browser);
	browser_set_location(browser, p);
}
