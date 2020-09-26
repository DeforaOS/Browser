/* $Id$ */
/* Copyright (c) 2020 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/stat.h>
#include <fcntl.h>
#ifdef DEBUG
# include <stdio.h>
#endif
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <System.h>
#include "Browser/vfs.h"
#include "desktop.h"
#include "desktopicon.h"
#include "desktopiconwindow.h"
#include "handler.h"
#define _(string) gettext(string)
#define N_(string) string

#define COMMON_SYMLINK
#include "../common.c"


/* DesktopHandler */
/* private */
/* types */
typedef struct _DesktopCategory
{
	gboolean show;
	char const * category;
	char const * name;
	char const * icon;
} DesktopCategory;

typedef struct _DesktopHandlerApplications
{
	DIR * refresh_dir;
	time_t refresh_mtime;
	guint refresh_source;
	GSList * apps;
	DesktopCategory * category;
} DesktopHandlerApplications;

typedef struct _DesktopHandlerCategories
{
	DIR * refresh_dir;
	time_t refresh_mtime;
	guint refresh_source;
	GSList * apps;
} DesktopHandlerCategories;

typedef struct _DesktopHandlerFiles
{
	char * path;
	DIR * refresh_dir;
	time_t refresh_mtime;
	guint refresh_source;
	gboolean show_hidden;
	GtkWidget * menu;
} DesktopHandlerFiles;

struct _DesktopHandler
{
	Desktop * desktop;
	DesktopIcons icons;
	union
	{
		DesktopHandlerApplications applications;
		DesktopHandlerCategories categories;
		DesktopHandlerFiles files;
	} u;
};


/* constants */
#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif

#define DESKTOP			".desktop"

#define IDLE_LOOP_ICON_CNT	16	/* number of icons added in a loop */


#include "handler/applications.c"
#include "handler/categories.c"
#include "handler/files.c"
#include "handler/homescreen.c"


/* protected */
/* functions */
/* desktophandler_new */
DesktopHandler * desktophandler_new(Desktop * desktop, DesktopIcons icons)
{
	DesktopHandler * handler;

	if((handler = object_new(sizeof(*handler))) == NULL)
		return NULL;
	handler->desktop = desktop;
	handler->icons = DESKTOP_ICONS_NONE;
	desktophandler_set_icons(handler, icons);
	return handler;
}


/* desktophandler_delete */
void desktophandler_delete(DesktopHandler * handler)
{
	desktophandler_set_icons(handler, DESKTOP_ICONS_NONE);
	object_delete(handler);
}


/* accessors */
static void _set_icons_destroy(DesktopHandler * handler);
static void _set_icons_init(DesktopHandler * handler);

void desktophandler_set_icons(DesktopHandler * handler, DesktopIcons icons)
{
	if(handler->icons != icons)
	{
		desktop_icons_remove_all(handler->desktop);
		_set_icons_destroy(handler);
		handler->icons = icons;
		_set_icons_init(handler);
	}
}

static void _set_icons_destroy(DesktopHandler * handler)
{
	switch(handler->icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_desktophandler_applications_destroy(handler);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_desktophandler_categories_destroy(handler);
			break;
		case DESKTOP_ICONS_FILES:
			_desktophandler_files_destroy(handler);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_desktophandler_homescreen_destroy(handler);
			break;
		case DESKTOP_ICONS_NONE:
			break;
	}
}

static void _set_icons_init(DesktopHandler * handler)
{
	switch(handler->icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_desktophandler_applications_init(handler);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_desktophandler_categories_init(handler);
			break;
		case DESKTOP_ICONS_FILES:
			_desktophandler_files_init(handler);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_desktophandler_homescreen_init(handler);
			break;
		case DESKTOP_ICONS_NONE:
			break;
	}
}


/* desktophandler_popup */
void desktophandler_popup(DesktopHandler * handler, XButtonEvent * xbev)
{
	switch(handler->icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_desktophandler_applications_popup(handler, xbev);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_desktophandler_categories_popup(handler, xbev);
			break;
		case DESKTOP_ICONS_FILES:
			_desktophandler_files_popup(handler, xbev);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_desktophandler_homescreen_popup(handler, xbev);
			break;
		case DESKTOP_ICONS_NONE:
			break;
	}
}


/* desktophandler_refresh */
void desktophandler_refresh(DesktopHandler * handler)
{
	switch(handler->icons)
	{
		case DESKTOP_ICONS_APPLICATIONS:
			_desktophandler_applications_refresh(handler);
			break;
		case DESKTOP_ICONS_CATEGORIES:
			_desktophandler_categories_refresh(handler);
			break;
		case DESKTOP_ICONS_FILES:
			_desktophandler_files_refresh(handler);
			break;
		case DESKTOP_ICONS_HOMESCREEN:
			_desktophandler_homescreen_refresh(handler);
			break;
		case DESKTOP_ICONS_NONE:
			break;
	}
}
