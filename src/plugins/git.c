/* $Id$ */
/* Copyright (c) 2012-2015 Pierre Pronchery <khorben@defora.org> */
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
 * - parse git's meta-data */



#include <System.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#define COMMON_PROMPT
#include "common.c"


/* Git */
/* private */
/* types */
typedef struct _CommonTask GitTask;

typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	char * filename;

	guint source;

	/* widgets */
	GtkWidget * widget;
	GtkWidget * name;
	GtkWidget * status;
	/* init */
	GtkWidget * init;
	/* directory */
	GtkWidget * directory;
	/* file */
	GtkWidget * file;

	/* tasks */
	GitTask ** tasks;
	size_t tasks_cnt;
} Git;


/* constants */
#define GIT_GIT		"git"


/* prototypes */
static Git * _git_init(BrowserPluginHelper * helper);
static void _git_destroy(Git * git);
static GtkWidget * _git_get_widget(Git * git);
static void _git_refresh(Git * git, GList * selection);

/* accessors */
static gboolean _git_is_managed(char const * filename);

/* useful */
static int _git_add_task(Git * git, char const * title,
		char const * directory, char * argv[],
		CommonTaskCallback callback);

/* callbacks */
static void _git_on_add(gpointer data);
static void _git_on_blame(gpointer data);
static void _git_on_clone(gpointer data);
static void _git_on_commit(gpointer data);
static void _git_on_diff(gpointer data);
static void _git_on_init(gpointer data);
static void _git_on_log(gpointer data);
static void _git_on_pull(gpointer data);
static void _git_on_push(gpointer data);
static void _git_on_reset(gpointer data);
static void _git_on_status(gpointer data);


/* public */
/* variables */
/* plug-in */
BrowserPluginDefinition plugin =
{
	N_("Git"),
	"applications-development",
	NULL,
	_git_init,
	_git_destroy,
	_git_get_widget,
	_git_refresh
};


/* private */
/* functions */
/* git_init */
static GtkWidget * _init_button(GtkSizeGroup * group, char const * icon,
		char const * label, GCallback callback, gpointer data);

static Git * _git_init(BrowserPluginHelper * helper)
{
	Git * git;
	PangoFontDescription * font;
	GtkSizeGroup * group;
	GtkWidget * widget;

	if((git = object_new(sizeof(*git))) == NULL)
		return NULL;
	git->helper = helper;
	git->filename = NULL;
	git->source = 0;
	/* widgets */
#if GTK_CHECK_VERSION(3, 0, 0)
	git->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	git->widget = gtk_vbox_new(FALSE, 4);
#endif
	font = pango_font_description_new();
	pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* label */
	git->name = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(git->name), PANGO_ELLIPSIZE_MIDDLE);
	gtk_misc_set_alignment(GTK_MISC(git->name), 0.0, 0.5);
	gtk_widget_modify_font(git->name, font);
	gtk_box_pack_start(GTK_BOX(git->widget), git->name, FALSE, TRUE, 0);
	git->status = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(git->status), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment(GTK_MISC(git->status), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(git->widget), git->status, FALSE, TRUE, 0);
	/* init */
#if GTK_CHECK_VERSION(3, 0, 0)
	git->init = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	git->init = gtk_vbox_new(FALSE, 4);
#endif
	widget = _init_button(group, GTK_STOCK_OK, _("Initialize"), G_CALLBACK(
				_git_on_init), git);
	gtk_box_pack_start(GTK_BOX(git->init), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_SAVE_AS, _("Clone..."),
			G_CALLBACK(_git_on_clone), git);
	gtk_box_pack_start(GTK_BOX(git->init), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(git->init);
	gtk_widget_set_no_show_all(git->init, TRUE);
	gtk_box_pack_start(GTK_BOX(git->widget), git->init, FALSE, TRUE, 0);
	/* directory */
#if GTK_CHECK_VERSION(3, 0, 0)
	git->directory = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	git->directory = gtk_vbox_new(FALSE, 4);
#endif
	widget = _init_button(group, GTK_STOCK_FIND_AND_REPLACE,
			_("Diff"), G_CALLBACK(_git_on_diff), git);
	gtk_box_pack_start(GTK_BOX(git->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_FIND, _("View log"),
			G_CALLBACK(_git_on_log), git);
	gtk_box_pack_start(GTK_BOX(git->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_PROPERTIES, _("Status"),
			G_CALLBACK(_git_on_status), git);
	gtk_box_pack_start(GTK_BOX(git->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_REFRESH, _("Pull"),
			G_CALLBACK(_git_on_pull), git);
	gtk_box_pack_start(GTK_BOX(git->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_CONNECT, _("Push"),
			G_CALLBACK(_git_on_push), git);
	gtk_box_pack_start(GTK_BOX(git->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_REVERT_TO_SAVED, _("Reset"),
			G_CALLBACK(_git_on_reset), git);
	gtk_box_pack_start(GTK_BOX(git->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_JUMP_TO, _("Commit"),
			G_CALLBACK(_git_on_commit), git);
	gtk_box_pack_start(GTK_BOX(git->directory), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(git->directory);
	gtk_widget_set_no_show_all(git->directory, TRUE);
	gtk_box_pack_start(GTK_BOX(git->widget), git->directory, FALSE, TRUE,
			0);
	/* file */
#if GTK_CHECK_VERSION(3, 0, 0)
	git->file = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	git->file = gtk_vbox_new(FALSE, 4);
#endif
	widget = _init_button(group, GTK_STOCK_FIND_AND_REPLACE,
			_("Diff"), G_CALLBACK(_git_on_diff), git);
	gtk_box_pack_start(GTK_BOX(git->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_INDEX, _("Annotate"),
			G_CALLBACK(_git_on_blame), git);
	gtk_box_pack_start(GTK_BOX(git->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_FIND, _("View log"),
			G_CALLBACK(_git_on_log), git);
	gtk_box_pack_start(GTK_BOX(git->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_ADD, _("Stage"),
			G_CALLBACK(_git_on_add), git);
	gtk_box_pack_start(GTK_BOX(git->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_REVERT_TO_SAVED, _("Reset"),
			G_CALLBACK(_git_on_reset), git);
	gtk_box_pack_start(GTK_BOX(git->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_JUMP_TO, _("Commit"),
			G_CALLBACK(_git_on_commit), git);
	gtk_box_pack_start(GTK_BOX(git->file), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(git->file);
	gtk_widget_set_no_show_all(git->file, TRUE);
	gtk_box_pack_start(GTK_BOX(git->widget), git->file, FALSE, TRUE, 0);
	gtk_widget_show_all(git->widget);
	pango_font_description_free(font);
	/* tasks */
	git->tasks = NULL;
	git->tasks_cnt = 0;
	return git;
}

static GtkWidget * _init_button(GtkSizeGroup * group, char const * icon,
		char const * label, GCallback callback, gpointer data)
{
	GtkWidget * hbox;
	GtkWidget * image;
	GtkWidget * widget;
	char const stock[] = "gtk-";

#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
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


/* git_destroy */
static void _git_destroy(Git * git)
{
	size_t i;

	for(i = 0; i < git->tasks_cnt; i++)
		_common_task_delete(git->tasks[i]);
	free(git->tasks);
	if(git->source != 0)
		g_source_remove(git->source);
	object_delete(git);
}


/* git_get_widget */
static GtkWidget * _git_get_widget(Git * git)
{
	return git->widget;
}


/* git_refresh */
static void _refresh_dir(Git * git);
static void _refresh_error(Git * git, char const * message);
static void _refresh_file(Git * git);
static void _refresh_hide(Git * git, gboolean name);
static void _refresh_status(Git * git, char const * status);

static void _git_refresh(Git * git, GList * selection)
{
	char * path = (selection != NULL) ? selection->data : NULL;
	struct stat st;
	gchar * p;

	if(git->source != 0)
		g_source_remove(git->source);
	free(git->filename);
	git->filename = NULL;
	if(path == NULL || selection->next != NULL)
	{
		_refresh_hide(git, TRUE);
		return;
	}
	if(lstat(path, &st) != 0
			|| (git->filename = strdup(path)) == NULL)
	{
		_refresh_error(git, path);
		return;
	}
	p = g_filename_display_basename(path);
	gtk_label_set_text(GTK_LABEL(git->name), p);
	g_free(p);
	if(S_ISDIR(st.st_mode))
		_refresh_dir(git);
	else
		_refresh_file(git);
}

static void _refresh_dir(Git * git)
{
	char const dir[] = "/.git";
	size_t len = strlen(git->filename);

	_refresh_hide(git, FALSE);
	/* consider ".git" folders like their parent */
	if((len = strlen(git->filename)) >= (sizeof(dir) - 1)
			&& strcmp(&git->filename[len - 4], dir) == 0)
		git->filename[len - 4] = '\0';
	if(_git_is_managed(git->filename) != TRUE)
	{
		_refresh_status(git, _("Not a Git repository"));
		gtk_widget_show(git->init);
		return;
	}
	gtk_widget_show(git->directory);
}

static void _refresh_error(Git * git, char const * message)
{
	BrowserPluginHelper * helper = git->helper;

	error_set("%s: %s", message, strerror(errno));
	helper->error(helper->browser, error_get(), 1);
}

static void _refresh_file(Git * git)
{
	_refresh_hide(git, FALSE);
	/* FIXME detect if the file is actually managed */
	gtk_widget_show(git->file);
}

static void _refresh_hide(Git * git, gboolean name)
{
	name ? gtk_widget_hide(git->name) : gtk_widget_show(git->name);
	_refresh_status(git, NULL);
	gtk_widget_hide(git->init);
	gtk_widget_hide(git->directory);
	gtk_widget_hide(git->file);
}

static void _refresh_status(Git * git, char const * status)
{
	if(status == NULL)
		gtk_widget_hide(git->status);
	else
	{
		gtk_label_set_text(GTK_LABEL(git->status), status);
		gtk_widget_show(git->status);
	}
}


/* accessors */
/* git_is_managed */
static gboolean _git_is_managed(char const * filename)
{
	char * base = strdup(filename);
	char * dir = base;
	String * p;
	struct stat st;
	int res;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, filename);
#endif
	for(; strcmp(dir, ".") != 0; dir = dirname(dir))
	{
		if((p = string_new_append(dir, "/.git", NULL)) == NULL)
		{
			free(base);
			return FALSE;
		}
		res = lstat(p, &st);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\" %d\n", __func__, p, res);
#endif
		string_delete(p);
		if(res == 0)
		{
			/* FIXME really implement */
			free(base);
			return TRUE;
		}
		if(strcmp(dir, "/") == 0)
			break;
	}
	free(base);
	return FALSE;
}


/* useful */
/* git_add_task */
static int _git_add_task(Git * git, char const * title,
		char const * directory, char * argv[],
		CommonTaskCallback callback)
{
	BrowserPluginHelper * helper = git->helper;
	GitTask ** p;
	GitTask * task;

	if((p = realloc(git->tasks, sizeof(*p) * (git->tasks_cnt + 1))) == NULL)
		return -helper->error(helper->browser, strerror(errno), 1);
	git->tasks = p;
	if((task = _common_task_new(helper, &plugin, title, directory, argv,
					callback, git)) == NULL)
		return -helper->error(helper->browser, error_get(), 1);
	git->tasks[git->tasks_cnt++] = task;
	return 0;
}


/* callbacks */
/* git_on_add */
static void _git_on_add(gpointer data)
{
	Git * git = data;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "add", "--", NULL, NULL };

	if(git->filename == NULL)
		return;
	dirname = g_path_get_dirname(git->filename);
	basename = g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git add", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}


/* git_on_blame */
static void _blame_on_callback(Git * git, CommonTask * task, int ret);

static void _git_on_blame(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "blame", "--", NULL, NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git blame", dirname, argv, _blame_on_callback);
	g_free(basename);
	g_free(dirname);
}

static void _blame_on_callback(Git * git, CommonTask * task, int ret)
{
	if(ret == 128)
		_common_task_message(task, GTK_MESSAGE_ERROR,
				_("This file is not managed by Git"), 1);
}


/* git_on_clone */
static void _clone_on_callback(Git * git, CommonTask * task, int ret);

static void _git_on_clone(gpointer data)
{
	Git * git = data;
	struct stat st;
	char * argv[] = { GIT_GIT, "clone", "--", NULL, NULL };
	char * dirname;
	char * p;

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	if(_common_prompt("Clone repository from:", &p) != GTK_RESPONSE_OK)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	argv[3] = p;
	_git_add_task(git, "git clone", dirname, argv, _clone_on_callback);
	g_free(dirname);
	free(p);
}

static void _clone_on_callback(Git * git, CommonTask * task, int ret)
{
	if(ret == 0)
		_common_task_message(task, GTK_MESSAGE_INFO,
				_("Repository cloned successfully"), 0);
	else
		_common_task_message(task, GTK_MESSAGE_ERROR,
				_("Could not clone repository"), 1);
}


/* git_on_commit */
static void _commit_on_callback(Git * git, CommonTask * task, int ret);

static void _git_on_commit(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "commit", "--", NULL, NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	basename = S_ISDIR(st.st_mode) ? g_strdup(".")
		: g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git commit", dirname, argv, _commit_on_callback);
	g_free(basename);
	g_free(dirname);
}

static void _commit_on_callback(Git * git, CommonTask * task, int ret)
{
	if(ret != 0)
		_common_task_message(task, GTK_MESSAGE_ERROR,
				_("Could not commit the file or directory"), 1);
}


/* git_on_diff */
static void _diff_on_callback(Git * git, CommonTask * task, int ret);

static void _git_on_diff(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "diff", "--", NULL, NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git diff", dirname, argv, _diff_on_callback);
	g_free(basename);
	g_free(dirname);
}

static void _diff_on_callback(Git * git, CommonTask * task, int ret)
{
	GtkTextBuffer * tbuf;

	if(ret != 0)
		_common_task_message(task, GTK_MESSAGE_ERROR,
				_("Could not diff the file or directory"), 1);
	else
	{
		tbuf = _common_task_get_buffer(task);
		/* XXX race condition */
		if(gtk_text_buffer_get_char_count(tbuf) == 0)
			_common_task_message(task, GTK_MESSAGE_INFO,
					_("No difference was found"), 0);
	}
}


/* git_on_init */
static void _init_on_callback(Git * git, CommonTask * task, int ret);

static void _git_on_init(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	char * argv[] = { GIT_GIT, "init", NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	_git_add_task(git, "git pull", dirname, argv, _init_on_callback);
	g_free(dirname);
}

static void _init_on_callback(Git * git, CommonTask * task, int ret)
{
	if(ret == 0)
		/* refresh upon success */
		git->helper->refresh(git->helper->browser);
	else
		_common_task_message(task, GTK_MESSAGE_ERROR,
				_("Could not initialize repository"), 1);
}


/* git_on_log */
static void _log_on_callback(Git * git, CommonTask * task, int ret);

static void _git_on_log(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "log", "--", NULL, NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git log", dirname, argv, _log_on_callback);
	g_free(basename);
	g_free(dirname);
}

static void _log_on_callback(Git * git, CommonTask * task, int ret)
{
	GtkTextBuffer * tbuf;

	if(ret != 0)
		return;
	tbuf = _common_task_get_buffer(task);
	/* XXX race condition */
	if(gtk_text_buffer_get_char_count(tbuf) == 0)
		_common_task_message(task, GTK_MESSAGE_ERROR,
				_("This file is not managed by Git"), 1);
}


/* git_on_pull */
static void _git_on_pull(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "pull", "--", NULL, NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git pull", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}


/* git_on_push */
static void _git_on_push(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "push", "--", NULL, NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git push", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}


/* git_on_reset */
static void _git_on_reset(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "reset", "--", NULL, NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git reset", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}


/* git_on_status */
static void _git_on_status(gpointer data)
{
	Git * git = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { GIT_GIT, "status", "--", NULL, NULL };

	if(git->filename == NULL || lstat(git->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(git->filename)
		: g_path_get_dirname(git->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(git->filename);
	argv[3] = basename;
	_git_add_task(git, "git status", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}
