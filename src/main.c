/* $Id$ */
/* Copyright (c) 2006-2015 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <locale.h>
#include <libintl.h>
#include "window.h"
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME
# define PROGNAME	"browser"
#endif
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* private */
/* prototypes */
static int _browser(char const * filename, int view);

static int _error(char const * message, int ret);
static int _usage(void);


/* functions */
/* browser */
static int _browser(char const * filename, int view)
{
	BrowserWindow * window;
	Browser * browser;

	if((window = browserwindow_new(filename)) == NULL)
		return -1;
	if(view != -1)
	{
		browser = browserwindow_get_browser(window);
		browser_set_view(browser, view);
	}
	return 0;
}


/* error */
static int _error(char const * message, int ret)
{
	fputs(PROGNAME ": ", stderr);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
#if GTK_CHECK_VERSION(2, 6, 0)
	fprintf(stderr, _("Usage: %s [-D|-I|-L|-T] [directory...]\n"
"  -D	Start in detailed view\n"
"  -I	Start in icon view\n"
"  -L	Start in list view\n"
"  -T	Start in thumbnail view\n"), PROGNAME);
#else
	fprintf(stderr, _("Usage: %s [-D] [directory...]\n"
"  -D	Start in detailed view\n"), PROGNAME);
#endif
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;
	int i;
	int view = -1;

	if(setlocale(LC_ALL, "") == NULL)
		_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
#if GTK_CHECK_VERSION(2, 6, 0)
	while((o = getopt(argc, argv, "DILT")) != -1)
#else
	while((o = getopt(argc, argv, "D")) != -1)
#endif
		switch(o)
		{
			case 'D':
				view = BV_DETAILS;
				break;
#if GTK_CHECK_VERSION(2, 6, 0)
			case 'I':
				view = BV_ICONS;
				break;
			case 'L':
				view = BV_LIST;
				break;
			case 'T':
				view = BV_THUMBNAILS;
				break;
#endif
			default:
				return _usage();
		}
	if(optind == argc)
		_browser(NULL, view);
	else
		for(i = optind; i < argc; i++)
			_browser(argv[i], view);
	gtk_main();
	/* browser is automatically deleted */
	return 0;
}
