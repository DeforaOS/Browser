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
