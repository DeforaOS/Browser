/* $Id$ */
/* Copyright (c) 2011-2016 Pierre Pronchery <khorben@defora.org> */
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



#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <Desktop.h>
#include "../../include/Browser/desktop.h"
#include "../../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME_DESKTOPCTL
# define PROGNAME_DESKTOPCTL	"desktopctl"
#endif
#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR		DATADIR "/locale"
#endif


/* desktopctl */
/* private */
/* prototypes */
static int _desktopctl(int action, int what);
static int _desktopctl_error(char const * message, int ret);

static int _usage(void);


/* functions */
/* desktopctl */
static int _desktopctl(int action, int what)
{
	desktop_message_send(DESKTOP_CLIENT_MESSAGE, action, what, TRUE);
	return 0;
}


/* desktopctl_error */
static int _desktopctl_error(char const * message, int ret)
{
	fprintf(stderr, "%s: ", PROGNAME_DESKTOPCTL);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-H|-L|-P|-R|-S|-T|-V|-a|-c|-f|-h|-n]\n"
"  -H	Place icons horizontally\n"
"  -L	Rotate screen to landscape mode\n"
"  -P	Rotate screen to portrait mode\n"
"  -R	Rotate screen\n"
"  -S	Display or change settings\n"
"  -T	Toggle screen rotation\n"
"  -V	Place icons vertically\n"
"  -a	Display the applications registered\n"
"  -c	Sort the applications registered by category\n"
"  -f	Display contents of the desktop folder\n"
"  -h	Display the homescreen\n"
"  -n	Do not display icons on the desktop\n"), PROGNAME_DESKTOPCTL);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;
	int action = -1;
	int what = 0;

	if(setlocale(LC_ALL, "") == NULL)
		_desktopctl_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "HLPRSVacfhn")) != -1)
		switch(o)
		{
			case 'H':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_ALIGNMENT;
				what = DESKTOP_ALIGNMENT_HORIZONTAL;
				break;
			case 'L':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_LAYOUT;
				what = DESKTOP_LAYOUT_LANDSCAPE;
				break;
			case 'P':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_LAYOUT;
				what = DESKTOP_LAYOUT_PORTRAIT;
				break;
			case 'R':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_LAYOUT;
				what = DESKTOP_LAYOUT_ROTATE;
				break;
			case 'S':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SHOW;
				what = DESKTOP_SHOW_SETTINGS;
				break;
			case 'T':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_LAYOUT;
				what = DESKTOP_LAYOUT_TOGGLE;
				break;
			case 'V':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_ALIGNMENT;
				what = DESKTOP_ALIGNMENT_VERTICAL;
				break;
			case 'a':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_ICONS;
				what = DESKTOP_ICONS_APPLICATIONS;
				break;
			case 'c':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_ICONS;
				what = DESKTOP_ICONS_CATEGORIES;
				break;
			case 'f':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_ICONS;
				what = DESKTOP_ICONS_FILES;
				break;
			case 'h':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_ICONS;
				what = DESKTOP_ICONS_HOMESCREEN;
				break;
			case 'n':
				if(action != -1)
					return _usage();
				action = DESKTOP_MESSAGE_SET_ICONS;
				what = DESKTOP_ICONS_NONE;
				break;
			default:
				return _usage();
		}
	if(action == -1 || optind != argc)
		return _usage();
	return (_desktopctl(action, what) == 0) ? 0 : 2;
}
