/* $Id$ */
/* Copyright (c) 2013 Pierre Pronchery <khorben@defora.org> */
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



#ifdef COMMON_RTRIM
# include <ctype.h>
# include <string.h>
#endif
#include <libintl.h>
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)


/* private */
/* types */
typedef struct _CommonTask
{
	BrowserPluginHelper * helper;

	GPid pid;
	guint source;

	/* stdout */
	gint o_fd;
	GIOChannel * o_channel;
	guint o_source;

	/* stderr */
	gint e_fd;
	GIOChannel * e_channel;
	guint e_source;

	/* widgets */
	GtkWidget * window;
	GtkWidget * view;
	GtkWidget * statusbar;
	guint statusbar_id;
} CommonTask;


/* prototypes */
/* tasks */
static CommonTask * _common_task_new(BrowserPluginHelper * helper,
		BrowserPluginDefinition * plugin, char const * title,
		char const * directory, char * argv[]);
static void _common_task_delete(CommonTask * task);
static void _common_task_set_status(CommonTask * task, char const * status);
static void _common_task_close(CommonTask * task);
static void _common_task_close_channel(CommonTask * task, GIOChannel * channel);

/* callbacks */
static gboolean _common_task_on_closex(gpointer data);
static void _common_task_on_child_watch(GPid pid, gint status, gpointer data);
static gboolean _common_task_on_io_can_read(GIOChannel * channel,
		GIOCondition condition, gpointer data);

#ifdef COMMON_RTRIM
static void _common_rtrim(char * string);
#endif


/* functions */
/* tasks */
/* common_task_new */
static CommonTask * _common_task_new(BrowserPluginHelper * helper,
		BrowserPluginDefinition * plugin, char const * title,
		char const * directory, char * argv[])
{
	CommonTask * task;
	GSpawnFlags flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
	gboolean res;
	GError * error = NULL;
	PangoFontDescription * font;
	char buf[256];
	GtkWidget * vbox;
	GtkWidget * widget;

	if((task = object_new(sizeof(*task))) == NULL)
		return NULL;
	task->helper = helper;
#ifdef DEBUG
	argv[0] = "echo";
#endif
	res = g_spawn_async_with_pipes(directory, argv, NULL, flags, NULL, NULL,
			&task->pid, NULL, &task->o_fd, &task->e_fd, &error);
	if(res != TRUE)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
		object_delete(task);
		return NULL;
	}
	/* widgets */
	font = pango_font_description_new();
	pango_font_description_set_family(font, "monospace");
	task->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(task->window), 600, 400);
#if GTK_CHECK_VERSION(2, 6, 0)
	if(plugin->icon != NULL)
		gtk_window_set_icon_name(GTK_WINDOW(task->window),
				plugin->icon);
#endif
	snprintf(buf, sizeof(buf), "%s - %s (%s)", _(plugin->name), title,
			directory);
	gtk_window_set_title(GTK_WINDOW(task->window), buf);
	g_signal_connect_swapped(task->window, "delete-event", G_CALLBACK(
				_common_task_on_closex), task);
	vbox = gtk_vbox_new(FALSE, 0);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	task->view = gtk_text_view_new();
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(task->view), FALSE);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(task->view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(task->view),
			GTK_WRAP_WORD_CHAR);
	gtk_widget_modify_font(task->view, font);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(widget),
			task->view);
	gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
	task->statusbar = gtk_statusbar_new();
	task->statusbar_id = 0;
	gtk_box_pack_start(GTK_BOX(vbox), task->statusbar, FALSE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(task->window), vbox);
	gtk_widget_show_all(task->window);
	pango_font_description_free(font);
	/* events */
	task->source = g_child_watch_add(task->pid, _common_task_on_child_watch,
			task);
	task->o_channel = g_io_channel_unix_new(task->o_fd);
	if((g_io_channel_set_encoding(task->o_channel, NULL, &error))
			!= G_IO_STATUS_NORMAL)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
	}
	task->o_source = g_io_add_watch(task->o_channel, G_IO_IN,
			_common_task_on_io_can_read, task);
	task->e_channel = g_io_channel_unix_new(task->e_fd);
	if((g_io_channel_set_encoding(task->e_channel, NULL, &error))
			!= G_IO_STATUS_NORMAL)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
	}
	task->e_source = g_io_add_watch(task->e_channel, G_IO_IN,
			_common_task_on_io_can_read, task);
	_common_task_set_status(task, _("Running command..."));
	return task;
}


/* common_task_delete */
static void _common_task_delete(CommonTask * task)
{
	_common_task_close(task);
	if(task->source != 0)
		g_source_remove(task->source);
	task->source = 0;
	gtk_widget_destroy(task->window);
	object_delete(task);
}


/* common_task_set_status */
static void _common_task_set_status(CommonTask * task, char const * status)
{
	GtkStatusbar * sb = GTK_STATUSBAR(task->statusbar);

	if(task->statusbar_id != 0)
		gtk_statusbar_remove(sb, gtk_statusbar_get_context_id(sb, ""),
				task->statusbar_id);
	task->statusbar_id = gtk_statusbar_push(sb,
			gtk_statusbar_get_context_id(sb, ""), status);
}


/* common_task_close */
static void _common_task_close(CommonTask * task)
{
	_common_task_close_channel(task, task->o_channel);
	_common_task_close_channel(task, task->e_channel);
}


/* common_task_close */
static void _common_task_close_channel(CommonTask * task, GIOChannel * channel)
{
	if(channel != NULL && channel == task->o_channel)
	{
		if(task->o_source != 0)
			g_source_remove(task->o_source);
		task->o_source = 0;
		g_io_channel_shutdown(task->o_channel, FALSE, NULL);
		g_io_channel_unref(task->o_channel);
		task->o_channel = NULL;
	}
	if(channel != NULL && task->e_channel != NULL)
	{
		if(task->e_source != 0)
			g_source_remove(task->e_source);
		task->e_source = 0;
		g_io_channel_shutdown(task->e_channel, FALSE, NULL);
		g_io_channel_unref(task->e_channel);
		task->e_channel = NULL;
	}
}


/* callbacks */
/* common_task_on_closex */
static gboolean _common_task_on_closex(gpointer data)
{
	CommonTask * task = data;

	gtk_widget_hide(task->window);
	_common_task_close(task);
	/* FIXME really implement */
	return TRUE;
}


/* common_task_on_child_watch */
static void _common_task_on_child_watch(GPid pid, gint status, gpointer data)
{
	CommonTask * task = data;
	char buf[256];

	task->source = 0;
	if(WIFEXITED(status))
	{
		snprintf(buf, sizeof(buf),
				_("Command exited with error code %d"),
				WEXITSTATUS(status));
		_common_task_set_status(task, buf);
	}
	else if(WIFSIGNALED(status))
	{
		snprintf(buf, sizeof(buf), _("Command exited with signal %d"),
				WTERMSIG(status));
		_common_task_set_status(task, buf);
	}
	g_spawn_close_pid(pid);
}


/* common_task_on_io_can_read */
static gboolean _common_task_on_io_can_read(GIOChannel * channel,
		GIOCondition condition, gpointer data)
{
	CommonTask * task = data;
	BrowserPluginHelper * helper = task->helper;
	char buf[256];
	gsize cnt = 0;
	GError * error = NULL;
	GIOStatus status;
	GtkTextBuffer * tbuf;
	GtkTextIter iter;

	if(condition != G_IO_IN)
		return FALSE;
	if(channel != task->o_channel && channel != task->e_channel)
		return FALSE;
	status = g_io_channel_read_chars(channel, buf, sizeof(buf), &cnt,
			&error);
	if(cnt > 0)
	{
		tbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(task->view));
		gtk_text_buffer_get_end_iter(tbuf, &iter);
		gtk_text_buffer_insert(tbuf, &iter, buf, cnt);
	}
	switch(status)
	{
		case G_IO_STATUS_NORMAL:
			break;
		case G_IO_STATUS_ERROR:
			helper->error(helper->browser, error->message, 1);
			g_error_free(error);
		case G_IO_STATUS_EOF:
		default: /* should not happen... */
			_common_task_close_channel(task, channel);
			return FALSE;
	}
	return TRUE;
}


#ifdef COMMON_RTRIM
/* common_rtrim */
static void _common_rtrim(char * string)
{
	unsigned char * s = (unsigned char *)string;
	size_t i;

	if(s == NULL || (i = strlen(string)) == 0)
		return;
	for(i--; s[i] != '\0' && isspace(s[i]); i--)
	{
		string[i] = '\0';
		if(i == 0)
			break;
	}
}
#endif