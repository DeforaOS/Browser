/* $Id$ */
/* Copyright (c) 2015-2020 Pierre Pronchery <khorben@defora.org> */
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



#include <gtk/gtk.h>
#include <System.h>
#include <Desktop.h>
#include "../browser/browser.h"
#include "../browser/callbacks.h"

#include "../browser/browser.c"
/* XXX */
#undef COMMON_CONFIG_FILENAME
#undef COMMON_DND
#undef COMMON_EXEC
#undef COMMON_GET_ABSOLUTE_PATH
#undef COMMON_SIZE
#include "../browser/callbacks.c"
#include "../browser/window.c"


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
	(void) name;

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
