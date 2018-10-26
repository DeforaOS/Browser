/* $Id$ */
/* Copyright (c) 2016-2018 Pierre Pronchery <khorben@defora.org> */
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



#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include "../../include/Browser/desktop.h"
#include "desktop.h"
#include "../../config.h"
#define _(string) gettext(string)


/* constants */
#ifndef PROGNAME
# define PROGNAME	"desktop"
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


/* Desktop */
/* private */
/* prototypes */
static int _error(char const * message, int ret);
static int _usage(void);


/* functions */
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
	fprintf(stderr, _("Usage: %s [-H|-V][-w|-W][-a|-c|-f|-h|-n][-m monitor][-N]\n"
"  -H	Place icons horizontally\n"
"  -V	Place icons vertically\n"
"  -W	Draw the desktop on the root window\n"
"  -a	Display the applications registered\n"
"  -c	Sort the applications registered by category\n"
"  -f	Display contents of the desktop folder (default)\n"
"  -h	Display the homescreen\n"
"  -m	Monitor where to display the desktop\n"
"  -n	Do not display icons on the desktop\n"
"  -N	Do not intercept mouse clicks on the desktop\n"
"  -w	Draw the desktop as a window\n"), PROGNAME);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;
	Desktop * desktop;
	DesktopPrefs prefs;
	char * p;

	if(setlocale(LC_ALL, "") == NULL)
		_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	prefs.alignment = -1;
	prefs.icons = -1;
	prefs.monitor = -1;
	prefs.popup = 1;
	prefs.window = -1;
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "HVWacfhm:nNw")) != -1)
		switch(o)
		{
			case 'H':
				prefs.alignment = DESKTOP_ALIGNMENT_HORIZONTAL;
				break;
			case 'V':
				prefs.alignment = DESKTOP_ALIGNMENT_VERTICAL;
				break;
			case 'W':
				prefs.window = 0;
				break;
			case 'a':
				prefs.icons = DESKTOP_ICONS_APPLICATIONS;
				break;
			case 'c':
				prefs.icons = DESKTOP_ICONS_CATEGORIES;
				break;
			case 'f':
				prefs.icons = DESKTOP_ICONS_FILES;
				break;
			case 'h':
				prefs.icons = DESKTOP_ICONS_HOMESCREEN;
				break;
			case 'm':
				prefs.monitor = strtol(optarg, &p, 0);
				if(optarg[0] == '\0' || *p != '\0')
					return _usage();
				break;
			case 'n':
				prefs.icons = DESKTOP_ICONS_NONE;
				break;
			case 'N':
				prefs.popup = 0;
				break;
			case 'w':
				prefs.window = 1;
				break;
			default:
				return _usage();
		}
	if(optind < argc)
		return _usage();
	if((desktop = desktop_new(&prefs)) == NULL)
	{
		gtk_main();
		return 2;
	}
	gtk_main();
	desktop_delete(desktop);
	return 0;
}
