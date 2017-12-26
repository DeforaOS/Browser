/* $Id$ */
/* Copyright (c) 2006-2017 Pierre Pronchery <khorben@defora.org> */
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
#include <locale.h>
#include <libintl.h>
#include <Desktop.h>
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME_OPEN
# define PROGNAME_OPEN	"open"
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


/* open */
/* private */
/* prototypes */
static int _open(char const * type, char const * action, int filec,
		char * filev[]);
static int _open_error(char const * message, int ret);

static int _usage(void);


/* functions */
/* open */
static int _open(char const * type, char const * action, int filec,
		char * filev[])
{
	int i;
	Mime * mime;
	int ret = 0;

	if((mime = mime_new(NULL)) == NULL)
		return -1;
	for(i = 0; i < filec; i++)
	{
		if(type == NULL)
		{
			if(mime_action(mime, action, filev[i]) == 0)
				continue;
		}
		else if(mime_action_type(mime, action, filev[i], type) == 0)
			continue;
		fprintf(stderr, "%s%s%s%s%s", PROGNAME_OPEN ": ", filev[i],
				_(": Could not perform action \""), action,
				"\"\n");
		ret = -1;
	}
	mime_delete(mime);
	return ret;
}


/* open_error */
static int _open_error(char const * message, int ret)
{
	fputs(PROGNAME_OPEN ": ", stderr);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-m mime type][-a action] file...\n"
"  -m	MIME type to force (default: auto-detected)\n"
"  -a	action to call (default: \"open\")\n"), PROGNAME_OPEN);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;
	char const * action = "open";
	char const * type = NULL;

	if(setlocale(LC_ALL, "") == NULL)
		_open_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "a:m:")) != -1)
		switch(o)
		{
			case 'a':
				action = optarg;
				break;
			case 'm':
				type = optarg;
				break;
			default:
				return _usage();
		}
	if(optind == argc)
		return _usage();
	return (_open(type, action, argc - optind, &argv[optind]) == 0) ? 0 : 2;
}
