/* $Id$ */
/* Copyright (c) 2015-2022 Pierre Pronchery <khorben@defora.org> */
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

	/* XXX avoid failures when offscreen */
	if(getenv("DISPLAY") != NULL)
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
