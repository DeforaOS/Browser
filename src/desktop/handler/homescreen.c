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



/* DesktopHandlerHomescreen */
/* private */
/* prototypes */
static void _desktophandler_homescreen_init(DesktopHandler * handler);
static void _desktophandler_homescreen_destroy(DesktopHandler * handler);
static void _desktophandler_homescreen_popup(DesktopHandler * handler,
		XButtonEvent * xbev);
static void _desktophandler_homescreen_refresh(DesktopHandler * handler);


/* functions */
/* desktophandler_homescreen_init */
static int _homescreen_init_subdir(DesktopHandler * handler,
		char const * subdir, char const * name);
static void _homescreen_init_on_applications(Desktop * desktop, gpointer data);

static void _desktophandler_homescreen_init(DesktopHandler * handler)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;
	char const * paths[] =
	{
#ifdef EMBEDDED
		/* FIXME let this be configurable */
		"org.defora.phone-contacts",
		"org.defora.phone-dialer",
		"org.defora.phone-messages",
#endif
		NULL
	};
	char const ** p;
	unsigned int size;

	if((desktopicon = desktopicon_new(handler->desktop, _("Applications"),
					NULL)) == NULL)
	{
		desktop_serror(handler->desktop, _("Applications"), 1);
		return;
	}
	desktopicon_set_callback(desktopicon, _homescreen_init_on_applications,
			NULL);
	desktopicon_set_immutable(desktopicon, TRUE);
	desktop_get_icon_size(handler->desktop, NULL, NULL, &size);
	icon = gtk_icon_theme_load_icon(desktop_get_theme(handler->desktop),
			"gnome-applications", size, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	desktop_icon_add(handler->desktop, desktopicon, FALSE);
	for(p = paths; *p != NULL; p++)
		_homescreen_init_subdir(handler, DATADIR "/applications", *p);
}

static int _homescreen_init_subdir(DesktopHandler * handler,
		char const * subdir, char const * name)
{
	DesktopIcon * desktopicon;
	String * q;

	if((q = string_new_append(subdir, "/", name, ".desktop", NULL)) == NULL)
		return -desktop_error(NULL, name, error_get(NULL), 1);
	if(access(q, R_OK) == 0
			&& (desktopicon = desktopicon_new_application(
					handler->desktop, q, DATADIR)) != NULL)
		desktop_icon_add(handler->desktop, desktopicon, FALSE);
	string_delete(q);
	return 0;
}

static void _homescreen_init_on_applications(Desktop * desktop, gpointer data)
{
	(void) data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	desktop_set_icons(desktop, DESKTOP_ICONS_CATEGORIES);
}


/* desktophandler_homescreen_destroy */
static void _desktophandler_homescreen_destroy(DesktopHandler * handler)
{
	(void) handler;
}


/* desktophandler_homescreen_popup */
static void _desktophandler_homescreen_popup(DesktopHandler * handler,
		XButtonEvent * xbev)
{
	(void) handler;
	(void) xbev;
}


/* desktophandler_homescreen_refresh */
static void _desktophandler_homescreen_refresh(DesktopHandler * handler)
{
	(void) handler;
}
