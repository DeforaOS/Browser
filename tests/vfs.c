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



#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <Desktop/mime.h>
#include "Browser/vfs.h"

#ifndef PROGNAME
# define PROGNAME	"vfs"
#endif


/* vfs */
static int _vfs(void)
{
	int ret = 0;
	Mime * mime;
	char const downloads[] = "/home/user/Downloads";
	char const * type;
	GdkPixbuf * icon;

	if((mime = mime_new(NULL)) == NULL)
		return -1;
	if((type = browser_vfs_mime_type(mime, downloads, S_IFDIR)) == NULL
			|| strcmp(type, "inode/directory") != 0)
		ret = -1;
	else
		printf("%s\n", type);
	if((icon = browser_vfs_mime_icon(mime, downloads, type, NULL, NULL, 48))
			!= NULL)
		g_object_unref(icon);
	mime_delete(mime);
	return ret;
}


/* usage */
static int _usage(void)
{
	fputs("Usage: " PROGNAME "\n", stderr);
	return 1;
}


/* main */
int main(int argc, char * argv[])
{
	int o;

	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "")) != -1)
		switch(o)
		{
			default:
				return _usage();
		}
	if(optind != argc)
		return _usage();
	return (_vfs() == 0) ? 0 : 2;
}
