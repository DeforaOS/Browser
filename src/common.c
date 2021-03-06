/* $Id$ */
/* Copyright (c) 2008-2021 Pierre Pronchery <khorben@defora.org> */
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
/* TODO:
 * - let the user browse for a destination when creating symlinks */



#include <stdio.h>
#include <libintl.h>


/* macros */
#ifndef _
# define _(string) gettext(string)
#endif
#define min(a, b) ((a) < (b)) ? (a) : (b)


/* prototypes */
#ifdef COMMON_DND
static int _common_drag_data_received(GdkDragContext * context,
		GtkSelectionData * seldata, char const * dest);
#endif

#ifdef COMMON_EXEC
static int _common_exec(char const * program, char const * flags, GList * args);
#endif

#ifdef COMMON_GET_ABSOLUTE_PATH
static char * _common_get_absolute_path(char const * path);
#endif

#ifdef COMMON_SIZE
static char const * _common_size(off_t size);
#endif

#ifdef COMMON_SYMLINK
static int _common_symlink(GtkWidget * window, char const * cur);
#endif


/* functions */
#ifdef COMMON_DND
/* common_drag_data_received */
static int _common_drag_data_received(GdkDragContext * context,
		GtkSelectionData * seldata, char const * dest)
{
	int ret = 0;
	size_t len;
	size_t i;
	GList * selection = NULL;
	char * p;
	GdkDragAction action;
#ifdef DEBUG
	GList * s;
#endif

#if GTK_CHECK_VERSION(2, 14, 0)
	if(gtk_selection_data_get_length(seldata) <= 0
			|| gtk_selection_data_get_data(seldata) == NULL)
		return 0;
	len = gtk_selection_data_get_length(seldata);
#else
	if(seldata->length <= 0 || seldata->data == NULL)
		return 0;
	len = seldata->length;
#endif
	for(i = 0; i < len; i += strlen(p) + 1)
	{
#if GTK_CHECK_VERSION(2, 14, 0)
		p = (char *)gtk_selection_data_get_data(seldata);
		p = &p[i];
#else
		p = (char *)&seldata->data[i];
#endif
		selection = g_list_append(selection, p);
	}
#if GTK_CHECK_VERSION(2, 22, 0)
	action = gdk_drag_context_get_suggested_action(context);
#else
	action = context->suggested_action;
#endif
#ifdef DEBUG
	fprintf(stderr, "%s%s%s%s%s", "DEBUG: ", action == GDK_ACTION_COPY
			? _("copying") : _("moving"), _(" to \""), dest,
			"\":\n");
	for(s = selection; s != NULL; s = s->next)
		fprintf(stderr, "DEBUG: \"%s\"\n", (char const *)s->data);
#else
	selection = g_list_append(selection, (char *)dest); /* XXX */
	if(action == GDK_ACTION_COPY)
		ret = _common_exec("copy", "-iR", selection);
	else if(action == GDK_ACTION_MOVE)
		ret = _common_exec("move", "-i", selection);
#endif
	g_list_free(selection);
	return ret;
}
#endif /* COMMON_DND */


#ifdef COMMON_EXEC
/* common_exec */
static int _common_exec(char const * program, char const * flags, GList * args)
{
	int ret = 0;
	unsigned long i = (flags != NULL) ? 3 : 2;
	char ** argv = NULL;
	GList * a;
	char ** p;
	GError * error = NULL;

	if(args == NULL)
		return 0;
	for(a = args; a != NULL; a = a->next)
	{
		if(a->data == NULL)
			continue;
		if((p = realloc(argv, sizeof(*argv) * (i + 2))) == NULL)
			break;
		argv = p;
		argv[i++] = a->data;
	}
	if(a != NULL)
	{
		free(argv);
		return -error_set_code(1, "%s: %s", program, strerror(errno));
	}
	if(argv == NULL)
		return 0;
#ifdef DEBUG
	argv[0] = strdup("echo");
#else
	argv[0] = strdup(program);
#endif
	if(argv[0] == NULL)
	{
		free(argv);
		return -error_set_code(1, "%s: %s", program, strerror(errno));
	}
	argv[i] = NULL;
	i = 0;
	if(flags != NULL)
		argv[++i] = strdup(flags); /* XXX may fail too */
	argv[i + 1] = "--";
	if(g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
				NULL, &error) != TRUE)
	{
		ret = error_set_code(1, "%s", error->message);
		g_error_free(error);
	}
	free(argv[0]);
	if(flags != NULL)
		free(argv[i]);
	return ret;
}
#endif /* COMMON_EXEC */


#ifdef COMMON_GET_ABSOLUTE_PATH
/* common_get_absolute_path */
static char * _common_get_absolute_path(char const * path)
{
	char * p;
	char * cur;
	size_t i;

	if(path == NULL)
		return NULL;
	if(g_path_is_absolute(path))
	{
		if((p = strdup(path)) == NULL)
			return NULL;
	}
	else
	{
		cur = g_get_current_dir();
		p = g_build_filename(cur, path, NULL);
		g_free(cur);
	}
	/* replace "/./" by "/" */
	for(i = strlen(p); (cur = strstr(p, "/./")) != NULL; i = strlen(p))
		memmove(cur, &cur[2], (p + i) - (cur + 1));
	/* replace "//" by "/" */
	for(i = strlen(p); (cur = strstr(p, "//")) != NULL; i = strlen(p))
		memmove(cur, &cur[1], (p + i) - (cur));
	/* remove single dots at the end of the address */
	i = strlen(p);
	if(i >= 2 && strcmp(&p[i - 2], "/.") == 0)
		p[i - 1] = '\0';
	/* trim slashes in the end if relevant */
	if(string_compare(p, "/") != 0)
		string_rtrim(p, "/");
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\") => \"%s\"\n", __func__, path, p);
#endif
	return p;
}
#endif /* COMMON_GET_ABSOLUTE_PATH */


#ifdef COMMON_SIZE
/* common_size */
static char const * _common_size(off_t size)
{
	static char buf[16];
	double sz = size;
	char * unit;

	if(sz < 1024)
	{
		snprintf(buf, sizeof(buf), "%.0f %s", sz, _("bytes"));
		return buf;
	}
	else if((sz /= 1024) < 1024)
		unit = N_("kB");
	else if((sz /= 1024) < 1024)
		unit = N_("MB");
	else if((sz /= 1024) < 1024)
		unit = N_("GB");
	else if((sz /= 1024) < 1024)
		unit = N_("TB");
	else
	{
		sz /= 1024;
		unit = N_("PB");
	}
	snprintf(buf, sizeof(buf), "%.1f %s", sz, _(unit));
	return buf;
}
#endif


#ifdef COMMON_SYMLINK
/* common_symlink */
static int _common_symlink(GtkWidget * window, char const * cur)
{
	static char const * newsymlink = NULL;
	int ret = 0;
	size_t len;
	char * path;
	GtkWidget * dialog;
	GtkWidget * hbox;
	GtkWidget * widget;
	char const * to = NULL;

	if(newsymlink == NULL)
		newsymlink = _("New symbolic link");
	len = strlen(cur) + strlen(newsymlink) + 2;
	if((path = malloc(len)) == NULL)
		return -1;
	snprintf(path, len, "%s/%s", cur, newsymlink);
	dialog = gtk_dialog_new_with_buttons(newsymlink,
			(window != NULL) ? GTK_WINDOW(window) : NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	if(window == NULL)
		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	widget = gtk_label_new(_("Destination:"));
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 4);
	widget = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 4);
	gtk_widget_show_all(hbox);
#if GTK_CHECK_VERSION(2, 14, 0)
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(
						dialog))), hbox, TRUE, TRUE, 4);
#else
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE,
			4);
#endif
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
		to = gtk_entry_get_text(GTK_ENTRY(widget));
	if(to != NULL && strlen(to) > 0 && symlink(to, path) != 0)
		ret = -1;
	gtk_widget_destroy(dialog);
	free(path);
	return ret;
}
#endif /* COMMON_SYMLINK */
