/* $Id$ */
/* Copyright (c) 2011-2018 Pierre Pronchery <khorben@defora.org> */
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



#include <System.h>
#include <libintl.h>
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)


/* Template */
/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	GtkWidget * widget;

	/* TODO implement */
} Template;


/* prototypes */
static Template * _template_init(BrowserPluginHelper * helper);
static void _template_destroy(Template * template);
static GtkWidget * _template_get_widget(Template * template);
static void _template_refresh(Template * template, GList * selection);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	N_("Template"),
	"image-missing",
	NULL,
	_template_init,
	_template_destroy,
	_template_get_widget,
	_template_refresh
};


/* private */
/* functions */
/* template_init */
static Template * _template_init(BrowserPluginHelper * helper)
{
	Template * template;

	if((template = object_new(sizeof(*template))) == NULL)
		return NULL;
	template->helper = helper;
	/* TODO implement */
	template->widget = gtk_label_new("Template");
	return template;
}


/* template_destroy */
static void _template_destroy(Template * template)
{
	/* TODO implement */
	object_delete(template);
}


/* template_get_widget */
static GtkWidget * _template_get_widget(Template * template)
{
	return template->widget;
}


/* template_refresh */
static void _template_refresh(Template * template, GList * selection)
{
	/* TODO implement */
}
