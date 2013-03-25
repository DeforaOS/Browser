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
/* TODO:
 * - detect the targets supported
 * - let the make binary used be configurable */



#include <System.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "common.c"


/* Make */
/* private */
/* types */
typedef struct _CommonTask MakeTask;

typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	char * filename;

	guint source;

	/* widgets */
	GtkWidget * widget;
	GtkWidget * name;
	GtkWidget * status;
	/* directory */
	GtkWidget * directory;
	/* file */
	GtkWidget * file;

	/* tasks */
	MakeTask ** tasks;
	size_t tasks_cnt;
} Make;


/* constants */
#define MAKE		"make"


/* prototypes */
static Make * _make_init(BrowserPluginHelper * helper);
static void _make_destroy(Make * make);
static GtkWidget * _make_get_widget(Make * make);
static void _make_refresh(Make * make, GList * selection);

/* accessors */
static gboolean _make_is_managed(char const * filename);

/* useful */
static int _make_add_task(Make * make, char const * title,
		char const * directory, char * argv[]);
static int _make_target(Make * make, char const * filename,
		char const * target);

/* callbacks */
static void _make_on_all(gpointer data);
static void _make_on_clean(gpointer data);
static void _make_on_dist(gpointer data);
static void _make_on_distclean(gpointer data);
static void _make_on_install(gpointer data);
static void _make_on_target(gpointer data);
static void _make_on_uninstall(gpointer data);


/* public */
/* variables */
/* plug-in */
BrowserPluginDefinition plugin =
{
	N_("Make"),
	"gtk-execute",
	NULL,
	_make_init,
	_make_destroy,
	_make_get_widget,
	_make_refresh
};


/* private */
/* functions */
/* make_init */
static GtkWidget * _init_button(GtkSizeGroup * group, char const * icon,
		char const * label, GCallback callback, gpointer data);

static Make * _make_init(BrowserPluginHelper * helper)
{
	Make * make;
	PangoFontDescription * font;
	GtkSizeGroup * group;
	GtkSizeGroup * bgroup;
	GtkWidget * widget;

	if((make = object_new(sizeof(*make))) == NULL)
		return NULL;
	make->helper = helper;
	make->filename = NULL;
	make->source = 0;
	/* widgets */
	make->widget = gtk_vbox_new(FALSE, 4);
	font = pango_font_description_new();
	pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	bgroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* label */
	make->name = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(make->name), PANGO_ELLIPSIZE_MIDDLE);
	gtk_misc_set_alignment(GTK_MISC(make->name), 0.0, 0.0);
	gtk_widget_modify_font(make->name, font);
	gtk_box_pack_start(GTK_BOX(make->widget), make->name, FALSE, TRUE, 0);
	make->status = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(make->status), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment(GTK_MISC(make->status), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(make->widget), make->status, FALSE, TRUE, 0);
	/* directory */
	make->directory = gtk_vbox_new(FALSE, 4);
	widget = _init_button(bgroup, GTK_STOCK_EXECUTE, _("Build"),
			G_CALLBACK(_make_on_all), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_CLEAR, _("Clean"),
			G_CALLBACK(_make_on_clean), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_SAVE, _("Dist"),
			G_CALLBACK(_make_on_dist), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_DELETE, _("Distclean"),
			G_CALLBACK(_make_on_distclean), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_HARDDISK, _("Install"),
			G_CALLBACK(_make_on_install), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_REVERT_TO_SAVED, _("Uninstall"),
			G_CALLBACK(_make_on_uninstall), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(make->directory);
	gtk_widget_set_no_show_all(make->directory, TRUE);
	gtk_box_pack_start(GTK_BOX(make->widget), make->directory, FALSE, TRUE,
			0);
	/* file */
	make->file = gtk_vbox_new(FALSE, 4);
	widget = _init_button(bgroup, GTK_STOCK_CONVERT, _("Build"),
			G_CALLBACK(_make_on_target), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_CLEAR, _("Clean"),
			G_CALLBACK(_make_on_clean), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_SAVE, _("Dist"),
			G_CALLBACK(_make_on_dist), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_DELETE, _("Distclean"),
			G_CALLBACK(_make_on_distclean), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_HARDDISK, _("Install"),
			G_CALLBACK(_make_on_install), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_REVERT_TO_SAVED, _("Uninstall"),
			G_CALLBACK(_make_on_uninstall), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(make->file);
	gtk_widget_set_no_show_all(make->file, TRUE);
	gtk_box_pack_start(GTK_BOX(make->widget), make->file, FALSE, TRUE, 0);
	/* additional actions */
	gtk_widget_show_all(make->widget);
	pango_font_description_free(font);
	/* tasks */
	make->tasks = NULL;
	make->tasks_cnt = 0;
	return make;
}

static GtkWidget * _init_button(GtkSizeGroup * group, char const * icon,
		char const * label, GCallback callback, gpointer data)
{
	GtkWidget * hbox;
	GtkWidget * image;
	GtkWidget * widget;
	char const stock[] = "gtk-";

	hbox = gtk_hbox_new(FALSE, 4);
	widget = gtk_button_new_with_label(label);
	gtk_size_group_add_widget(group, widget);
	if(icon != NULL)
	{
		if(strncmp(icon, stock, sizeof(stock) - 1) == 0)
			image = gtk_image_new_from_stock(icon,
					GTK_ICON_SIZE_BUTTON);
		else
			image = gtk_image_new_from_icon_name(icon,
					GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image(GTK_BUTTON(widget), image);
	}
	g_signal_connect_swapped(widget, "clicked", callback, data);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	return hbox;
}


/* make_destroy */
static void _make_destroy(Make * make)
{
	size_t i;

	for(i = 0; i < make->tasks_cnt; i++)
		_common_task_delete(make->tasks[i]);
	free(make->tasks);
	if(make->source != 0)
		g_source_remove(make->source);
	object_delete(make);
}


/* make_get_widget */
static GtkWidget * _make_get_widget(Make * make)
{
	return make->widget;
}


/* make_refresh */
static void _refresh_dir(Make * make);
static void _refresh_file(Make * make);
static void _refresh_status(Make * make, char const * status);

static void _make_refresh(Make * make, GList * selection)
{
	char * path = (selection != NULL) ? selection->data : NULL;
	struct stat st;
	gchar * p;

	if(make->source != 0)
		g_source_remove(make->source);
	free(make->filename);
	make->filename = NULL;
	if(lstat(path, &st) != 0)
		return;
	if((make->filename = strdup(path)) == NULL)
		return;
	p = g_filename_display_basename(path);
	gtk_label_set_text(GTK_LABEL(make->name), p);
	g_free(p);
	_refresh_status(make, NULL);
	gtk_widget_hide(make->directory);
	gtk_widget_hide(make->file);
	if(S_ISDIR(st.st_mode))
		_refresh_dir(make);
	else
		_refresh_file(make);
}

static void _refresh_dir(Make * make)
{
	/* check if it is managed */
	if(_make_is_managed(make->filename) == FALSE)
		_refresh_status(make, _("No Makefile found"));
	else
		gtk_widget_show(make->directory);
}

static void _refresh_file(Make * make)
{
	/* check if it is managed */
	if(_make_is_managed(make->filename) == FALSE)
		_refresh_status(make, _("No Makefile found"));
	else
		gtk_widget_show(make->file);
}

static void _refresh_status(Make * make, char const * status)
{
	if(status == NULL)
		gtk_widget_hide(make->status);
	else
	{
		gtk_label_set_text(GTK_LABEL(make->status), status);
		gtk_widget_show(make->status);
	}
}


/* make_is_managed */
static gboolean _make_is_managed(char const * pathname)
{
	gboolean managed = FALSE;
	struct stat st;
	gchar * dirname;
	char const * makefile[] = { "Makefile", "makefile", "GNUmakefile" };
	size_t i;
	gchar * p;

	if(stat(pathname, &st) != 0)
		return FALSE;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(pathname)
		: g_path_get_dirname(pathname);
	for(i = 0; managed == FALSE && i < sizeof(makefile) / sizeof(*makefile);
			i++)
	{
		p = g_strdup_printf("%s/%s", dirname, makefile[i]);
		managed = (lstat(p, &st) == 0) ? TRUE : FALSE;
		g_free(p);
	}
	g_free(dirname);
	return managed;
}


/* useful */
/* make_add_task */
static int _make_add_task(Make * make, char const * title,
		char const * directory, char * argv[])
{
	BrowserPluginHelper * helper = make->helper;
	MakeTask ** p;
	MakeTask * task;
	GSpawnFlags flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
	gboolean res;
	GError * error = NULL;
	PangoFontDescription * font;
	char buf[256];
	GtkWidget * vbox;
	GtkWidget * widget;

	if((p = realloc(make->tasks, sizeof(*p) * (make->tasks_cnt + 1)))
			== NULL)
		return -helper->error(helper->browser, strerror(errno), 1);
	make->tasks = p;
	if((task = object_new(sizeof(*task))) == NULL)
		return -helper->error(helper->browser, error_get(), 1);
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
		return -1;
	}
	make->tasks[make->tasks_cnt++] = task;
	/* widgets */
	font = pango_font_description_new();
	pango_font_description_set_family(font, "monospace");
	task->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(task->window), 600, 400);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_icon_name(GTK_WINDOW(task->window), plugin.icon);
#endif
	snprintf(buf, sizeof(buf), "%s%s%s (%s)", _("Make"),
			(title != NULL) ? " - " : "",
			(title != NULL) ? title : "", directory);
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
	return 0;
}


/* make_target */
static int _make_target(Make * make, char const * filename, char const * target)
{
	int ret;
	struct stat st;
	gchar * dirname;
	char * argv[] = { MAKE, NULL, NULL };

	if(filename == NULL || lstat(filename, &st) != 0)
		return 0;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(filename)
		: g_path_get_dirname(filename);
	if(target != NULL && (argv[1] = strdup(target)) == NULL)
		/* FIXME report the error */
		return -1;
	ret = _make_add_task(make, target, dirname, argv);
	g_free(dirname);
	return ret;
}


/* callbacks */
/* make_on_all */
static void _make_on_all(gpointer data)
{
	Make * make = data;

	_make_target(make, make->filename, NULL);
}


/* make_on_clean */
static void _make_on_clean(gpointer data)
{
	Make * make = data;

	_make_target(make, make->filename, "clean");
}


/* make_on_dist */
static void _make_on_dist(gpointer data)
{
	Make * make = data;

	_make_target(make, make->filename, "dist");
}


/* make_on_distclean */
static void _make_on_distclean(gpointer data)
{
	Make * make = data;

	_make_target(make, make->filename, "distclean");
}


/* make_on_install */
static void _make_on_install(gpointer data)
{
	Make * make = data;

	_make_target(make, make->filename, "install");
}


/* make_on_target */
static void _make_on_target(gpointer data)
{
	Make * make = data;
	gchar * basename;

	basename = g_path_get_basename(make->filename);
	_make_target(make, make->filename, basename);
	g_free(basename);
}


/* make_on_uninstall */
static void _make_on_uninstall(gpointer data)
{
	Make * make = data;

	_make_target(make, make->filename, "uninstall");
}
