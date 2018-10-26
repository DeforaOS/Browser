/* $Id$ */
/* Copyright (c) 2012-2018 Pierre Pronchery <khorben@defora.org> */
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



#ifndef DESKTOP_BROWSER_PLUGIN_H
# define DESKTOP_BROWSER_PLUGIN_H

# include <sys/stat.h>
# include <gtk/gtk.h>
# include <Desktop.h>
# include "vfs.h"


/* Browser */
/* public */
/* types */
typedef struct _Browser Browser;

typedef struct _BrowserPlugin BrowserPlugin;

typedef struct _BrowserPluginHelper
{
	Browser * browser;
	char const * (*config_get)(Browser * browser, char const * section,
			char const * variable);
	int (*config_set)(Browser * browser, char const * section,
			char const * variable, char const * value);
	int (*error)(Browser * browser, char const * message, int ret);
	GdkPixbuf * (*get_icon)(Browser * browser, char const * filename,
			char const * type, struct stat * lst, struct stat * st,
			int size);
	Mime * (*get_mime)(Browser * browser);
	char const * (*get_type)(Browser * browser, char const * filename,
			mode_t mode);
	void (*refresh)(Browser * browser);
	int (*set_location)(Browser * browser, char const * path);
} BrowserPluginHelper;

typedef const struct _BrowserPluginDefinition
{
	char const * name;
	char const * icon;
	char const * description;
	BrowserPlugin * (*init)(BrowserPluginHelper * helper);
	void (*destroy)(BrowserPlugin * plugin);
	GtkWidget * (*get_widget)(BrowserPlugin * plugin);
	void (*refresh)(BrowserPlugin * plugin, GList * selection);
} BrowserPluginDefinition;

#endif /* !DESKTOP_BROWSER_PLUGIN_H */
