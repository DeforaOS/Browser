/* $Id$ */
/* Copyright (c) 2015 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Browser */


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

static int _browser_set_property(BrowserWidget * browser, va_list ap);


/* public */
/* variables */
DesktopWidgetDefinition widget =
{
	"Browser",
	"file-manager",
	NULL,
	_browser_init,
	_browser_destroy,
	_browser_get_widget,
	_browser_set_property
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


/* browser_set_property */
static int _browser_set_property(BrowserWidget * browser, va_list ap)
{
	int ret = 0;
	String const * property;
	String const * s;
	unsigned int u;

	while((property = va_arg(ap, String const *)) != NULL)
		if(strcmp(property, "location") == 0)
		{
			s = va_arg(ap, String const *);
			ret = browser_set_location(browser->browser, s);
		}
		else if(strcmp(property, "view") == 0)
		{
			u = va_arg(ap, unsigned int);
			browser_set_view(browser->browser, u);
		}
	return ret;
}
