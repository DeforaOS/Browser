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
#include <System.h>
#include <Desktop.h>
#include "../browser.h"
#include "../callbacks.h"

#include "../browser.c"
/* XXX */
#undef COMMON_CONFIG_FILENAME
#undef COMMON_DND
#undef COMMON_EXEC
#undef COMMON_GET_ABSOLUTE_PATH
#undef COMMON_SIZE
#include "../callbacks.c"
#include "../window.c"


/* BrowserWidget */
/* private */
/* types */
typedef struct _DesktopWidgetPlugin
{
	Browser * browser;
} BrowserWidget;


/* prototypes */
static BrowserWidget * _browser_init(char const * name);
static void _browser_destroy(BrowserWidget * browser);

static GtkWidget * _browser_get_widget(BrowserWidget * browser);


/* public */
/* variables */
DesktopWidgetDefinition widget =
{
	"Browser",
	"file-manager",
	NULL,
	_browser_init,
	_browser_destroy,
	_browser_get_widget
};


/* private */
/* functions */
/* browser_init */
static BrowserWidget * _browser_init(char const * name)
{
	BrowserWidget * browser;

	if((browser = object_new(sizeof(*browser))) == NULL)
		return NULL;
	if((browser->browser = browser_new(NULL, NULL, NULL)) == NULL)
	{
		_browser_destroy(browser);
		return NULL;
	}
	return browser;
}


/* browser_destroy */
static void _browser_destroy(BrowserWidget * browser)
{
	if(browser->browser != NULL)
		browser_delete(browser->browser);
	object_delete(browser);
}


/* accessors */
/* browser_get_widget */
static GtkWidget * _browser_get_widget(BrowserWidget * browser)
{
	return browser_get_widget(browser->browser);
}
