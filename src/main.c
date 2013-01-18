/* $Id$ */
/* Copyright (c) 2006-2013 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <locale.h>
#include <libintl.h>
#include "browser.h"
#include "../config.h"
#define _(string) gettext(string)


/* constants */
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* usage */
static int _usage(void)
{
#if GTK_CHECK_VERSION(2, 6, 0)
	fputs(_("Usage: browser [-D|-I|-L|-T] [directory...]\n"), stderr);
#else
	fputs(_("Usage: browser [-D] [directory...]\n"), stderr);
#endif
	return 1;
}


/* main */
int main(int argc, char * argv[])
{
	int o;
	int i;
	int view = -1;
	Browser * browser;

	setlocale(LC_ALL, "");
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
	{
		if((browser = browser_new(NULL)) != NULL && view != -1)
			browser_set_view(browser, view);
	}
	else
		for(i = optind; i < argc; i++)
			if((browser = browser_new(argv[i])) != NULL
					&& view != -1)
				browser_set_view(browser, view);
	gtk_main();
	return 0;
}
