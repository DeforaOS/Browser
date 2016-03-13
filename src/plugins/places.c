/* $Id$ */
/* Copyright (c) 2015 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Browser */
/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice
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



#include <gtk/gtk.h>
#if !GTK_CHECK_VERSION(3, 10, 0)
# include <stdio.h>
# include <string.h>
# include <errno.h>
#endif
#include <libintl.h>
#include <System.h>
#include "Browser.h"
#define N_(string) (string)


/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;
	GtkWidget * widget;
} Places;


/* prototypes */
/* plug-in */
static Places * _places_init(BrowserPluginHelper * helper);
static void _places_destroy(Places * places);
static GtkWidget * _places_get_widget(Places * places);

#if GTK_CHECK_VERSION(3, 10, 0)
/* callbacks */
static void _places_on_open_location(GtkWidget * widget, GObject * location,
		GtkPlacesOpenFlags flags, gpointer data);
#endif


/* public */
/* variables */
/* plug-in */
BrowserPluginDefinition plugin =
{
	N_("Places"),
	"user-desktop",
	NULL,
	_places_init,
	_places_destroy,
	_places_get_widget,
	NULL
};


/* private */
/* functions */
/* places_init */
static Places * _places_init(BrowserPluginHelper * helper)
{
#if GTK_CHECK_VERSION(3, 10, 0)
	Places * places;

	if((places = object_new(sizeof(*places))) == NULL)
		return NULL;
	places->helper = helper;
	places->widget = gtk_places_sidebar_new();
# if GTK_CHECK_VERSION(3, 12, 0)
	gtk_places_sidebar_set_local_only(GTK_PLACES_SIDEBAR(
				places->widget), TRUE);
# endif
	g_signal_connect(places->widget, "open-location", G_CALLBACK(
				_places_on_open_location), places);
	return places;
#else
	error_set("%s: %s", plugin.name, strerror(ENOSYS));
	return NULL;
#endif
}


/* places_destroy */
static void _places_destroy(Places * places)
{
	object_delete(places);
}


/* places_get_widget */
static GtkWidget * _places_get_widget(Places * places)
{
	return places->widget;
}


#if GTK_CHECK_VERSION(3, 10, 0)
/* callbacks */
static void _places_on_open_location(GtkWidget * widget, GObject * location,
		GtkPlacesOpenFlags flags, gpointer data)
{
	Places * places = data;
	gchar * path;

	if((path = g_file_get_path(G_FILE(location))) == NULL)
		return;
	places->helper->set_location(places->helper->browser, path);
	g_free(path);
}
#endif
