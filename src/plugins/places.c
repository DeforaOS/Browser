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
