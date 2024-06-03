/* $Id$ */
/* Copyright (c) 2011-2024 Pierre Pronchery <khorben@defora.org> */
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
 * - parse SVN's meta-data */



#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <System.h>
#include "common.c"


/* Subversion */
/* private */
/* types */
typedef struct _CommonTask SVNTask;

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
	/* additional actions */
	GtkWidget * add;

	/* tasks */
	SVNTask ** tasks;
	size_t tasks_cnt;
} SVN;


/* constants */
#define PROGNAME_SVN		"svn"


/* prototypes */
static SVN * _subversion_init(BrowserPluginHelper * helper);
static void _subversion_destroy(SVN * svn);
static GtkWidget * _subversion_get_widget(SVN * svn);
static void _subversion_refresh(SVN * svn, GList * selection);

/* accessors */
static String * _subversion_get_base(SVN * svn, char const * filename);
static gboolean _subversion_is_managed(SVN * svn, char const * filename);

/* useful */
static int _subversion_add_task(SVN * svn, char const * title,
		char const * directory, char * argv[]);

/* callbacks */
static void _subversion_on_add(gpointer data);
static void _subversion_on_blame(gpointer data);
static void _subversion_on_commit(gpointer data);
static void _subversion_on_diff(gpointer data);
static void _subversion_on_log(gpointer data);
static void _subversion_on_status(gpointer data);
static void _subversion_on_update(gpointer data);


/* public */
/* variables */
/* plug-in */
BrowserPluginDefinition plugin =
{
	N_("Subversion"),
	"applications-development",
	NULL,
	_subversion_init,
	_subversion_destroy,
	_subversion_get_widget,
	_subversion_refresh
};


/* private */
/* functions */
/* subversion_init */
static GtkWidget * _init_button(GtkSizeGroup * group, char const * icon,
		char const * label, GCallback callback, gpointer data);

static SVN * _subversion_init(BrowserPluginHelper * helper)
{
	SVN * svn;
	PangoFontDescription * font;
	GtkSizeGroup * group;
	GtkWidget * widget;

	if((svn = object_new(sizeof(*svn))) == NULL)
		return NULL;
	svn->helper = helper;
	svn->filename = NULL;
	svn->source = 0;
	/* widgets */
	svn->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	font = pango_font_description_new();
	pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* label */
	svn->name = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(svn->name), PANGO_ELLIPSIZE_MIDDLE);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(svn->name, font);
	g_object_set(svn->name, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_widget_modify_font(svn->name, font);
	gtk_misc_set_alignment(GTK_MISC(svn->name), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(svn->widget), svn->name, FALSE, TRUE, 0);
	svn->status = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(svn->status), PANGO_ELLIPSIZE_END);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(svn->status, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(svn->status), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(svn->widget), svn->status, FALSE, TRUE, 0);
	/* directory */
	svn->directory = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	widget = _init_button(group, GTK_STOCK_FIND_AND_REPLACE,
			_("Diff"), G_CALLBACK(_subversion_on_diff), svn);
	gtk_box_pack_start(GTK_BOX(svn->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_FIND, _("View log"),
			G_CALLBACK(_subversion_on_log), svn);
	gtk_box_pack_start(GTK_BOX(svn->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_PROPERTIES, _("Status"),
			G_CALLBACK(_subversion_on_status), svn);
	gtk_box_pack_start(GTK_BOX(svn->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_REFRESH, _("Update"),
			G_CALLBACK(_subversion_on_update), svn);
	gtk_box_pack_start(GTK_BOX(svn->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_JUMP_TO, _("Commit"),
			G_CALLBACK(_subversion_on_commit), svn);
	gtk_box_pack_start(GTK_BOX(svn->directory), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(svn->directory);
	gtk_widget_set_no_show_all(svn->directory, TRUE);
	gtk_box_pack_start(GTK_BOX(svn->widget), svn->directory, FALSE, TRUE,
			0);
	/* file */
	svn->file = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	widget = _init_button(group, GTK_STOCK_FIND_AND_REPLACE,
			_("Diff"), G_CALLBACK(_subversion_on_diff), svn);
	gtk_box_pack_start(GTK_BOX(svn->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_INDEX, _("Annotate"),
			G_CALLBACK(_subversion_on_blame), svn);
	gtk_box_pack_start(GTK_BOX(svn->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_FIND, _("View log"),
			G_CALLBACK(_subversion_on_log), svn);
	gtk_box_pack_start(GTK_BOX(svn->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_PROPERTIES, _("Status"),
			G_CALLBACK(_subversion_on_status), svn);
	gtk_box_pack_start(GTK_BOX(svn->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_REFRESH, _("Update"),
			G_CALLBACK(_subversion_on_update), svn);
	gtk_box_pack_start(GTK_BOX(svn->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_JUMP_TO, _("Commit"),
			G_CALLBACK(_subversion_on_commit), svn);
	gtk_box_pack_start(GTK_BOX(svn->file), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(svn->file);
	gtk_widget_set_no_show_all(svn->file, TRUE);
	gtk_box_pack_start(GTK_BOX(svn->widget), svn->file, FALSE, TRUE, 0);
	/* additional actions */
	svn->add = _init_button(group, GTK_STOCK_ADD, _("Add to repository"),
			G_CALLBACK(_subversion_on_add), svn);
	gtk_box_pack_start(GTK_BOX(svn->widget), svn->add, FALSE, TRUE, 0);
	gtk_widget_show_all(svn->widget);
	pango_font_description_free(font);
	/* tasks */
	svn->tasks = NULL;
	svn->tasks_cnt = 0;
	return svn;
}

static GtkWidget * _init_button(GtkSizeGroup * group, char const * icon,
		char const * label, GCallback callback, gpointer data)
{
	GtkWidget * hbox;
	GtkWidget * image;
	GtkWidget * widget;
	char const stock[] = "gtk-";

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
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


/* subversion_destroy */
static void _subversion_destroy(SVN * svn)
{
	size_t i;

	for(i = 0; i < svn->tasks_cnt; i++)
		_common_task_delete(svn->tasks[i]);
	free(svn->tasks);
	if(svn->source != 0)
		g_source_remove(svn->source);
	object_delete(svn);
}


/* subversion_get_widget */
static GtkWidget * _subversion_get_widget(SVN * svn)
{
	return svn->widget;
}


/* subversion_refresh */
static void _refresh_dir(SVN * svn);
static void _refresh_error(SVN * svn, char const * message);
static void _refresh_hide(SVN * svn, gboolean name);
static void _refresh_status(SVN * svn, char const * status);

static void _subversion_refresh(SVN * svn, GList * selection)
{
	char * path = (selection != NULL) ? selection->data : NULL;
	struct stat st;
	gchar * p;

	if(svn->source != 0)
		g_source_remove(svn->source);
	free(svn->filename);
	svn->filename = NULL;
	if(path == NULL || selection->next != NULL)
	{
		_refresh_hide(svn, TRUE);
		return;
	}
	if(lstat(path, &st) != 0 || (svn->filename = strdup(path)) == NULL)
	{
		_refresh_hide(svn, TRUE);
		if(errno != ENOENT)
			_refresh_error(svn, path);
		return;
	}
	p = g_filename_display_basename(path);
	gtk_label_set_text(GTK_LABEL(svn->name), p);
	g_free(p);
	_refresh_hide(svn, FALSE);
	if(S_ISDIR(st.st_mode))
		_refresh_dir(svn);
}

static void _refresh_dir(SVN * svn)
{
	char const dir[] = "/.svn";
	size_t len = strlen(svn->filename);
	char * p;
	struct stat st;

	/* consider ".svn" folders like their parent */
	if((len = strlen(svn->filename)) >= (sizeof(dir) - 1)
			&& strcmp(&svn->filename[len - 4], dir) == 0)
		svn->filename[len - 4] = '\0';
	/* check if it is an old SVN repository */
	len = strlen(svn->filename) + sizeof(dir) + 1;
	if((p = malloc(len)) != NULL)
	{
		snprintf(p, len, "%s%s", svn->filename, dir);
		if(lstat(p, &st) == 0)
		{
			free(p);
			gtk_widget_show(svn->directory);
			return;
		}
	}
	free(p);
	/* check if it is a newer SVN repository */
	if(_subversion_is_managed(svn, svn->filename))
		gtk_widget_show(svn->directory);
	else
		_refresh_status(svn, _("Not a Subversion repository"));
}

static void _refresh_error(SVN * svn, char const * message)
{
	BrowserPluginHelper * helper = svn->helper;

	error_set("%s: %s", message, strerror(errno));
	helper->error(helper->browser, error_get(NULL), 1);
}

static void _refresh_hide(SVN * svn, gboolean name)
{
	name ? gtk_widget_hide(svn->name) : gtk_widget_show(svn->name);
	_refresh_status(svn, NULL);
	gtk_widget_hide(svn->directory);
	gtk_widget_hide(svn->file);
	gtk_widget_hide(svn->add);
}

static void _refresh_status(SVN * svn, char const * status)
{
	if(status == NULL)
		status = "";
	gtk_label_set_text(GTK_LABEL(svn->status), status);
}


/* accessors */
/* subversion_get_base */
static String * _subversion_get_base(SVN * svn, char const * filename)
{
	String * cur;
	String * dir;
	String * p;
	struct stat st;
	int res;
	(void) svn;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, filename);
#endif
	cur = g_strdup(filename);
	while(string_compare(cur, ".") != 0)
	{
		if((p = string_new_append(dir, "/.svn", NULL)) == NULL)
			break;
		res = lstat(p, &st);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\" %d\n", __func__, p, res);
#endif
		if(res == 0)
		{
			g_free(cur);
			return p;
		}
		if(string_compare(dir, "/") == 0)
			break;
		dir = g_path_get_dirname(cur);
		g_free(cur);
		cur = dir;
	}
	g_free(cur);
	return NULL;
}


/* subversion_is_managed */
static gboolean _subversion_is_managed(SVN * svn, char const * filename)
{
	String * base;

	if((base = _subversion_get_base(svn, filename)) != NULL)
	{
		/* FIXME check if this file is managed */
		string_delete(base);
		return TRUE;
	}
	return FALSE;
}


/* useful */
/* subversion_add_task */
static int _subversion_add_task(SVN * svn, char const * title,
		char const * directory, char * argv[])
{
	BrowserPluginHelper * helper = svn->helper;
	SVNTask ** p;
	SVNTask * task;

	if((p = realloc(svn->tasks, sizeof(*p) * (svn->tasks_cnt + 1))) == NULL)
		return -helper->error(helper->browser, strerror(errno), 1);
	svn->tasks = p;
	if((task = _common_task_new(helper, &plugin, title, directory, argv,
					NULL, NULL)) == NULL)
		return -helper->error(helper->browser, error_get(NULL), 1);
	svn->tasks[svn->tasks_cnt++] = task;
	return 0;
}


/* callbacks */
/* subversion_on_add */
static void _subversion_on_add(gpointer data)
{
	SVN * svn = data;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { PROGNAME_SVN, "add", "--", NULL, NULL };

	if(svn->filename == NULL)
		return;
	dirname = g_path_get_dirname(svn->filename);
	basename = g_path_get_basename(svn->filename);
	argv[3] = basename;
	_subversion_add_task(svn, "svn add", dirname, argv);
	g_free(basename);
	g_free(dirname);
}


/* subversion_on_blame */
static void _subversion_on_blame(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { PROGNAME_SVN, "blame", "--", NULL, NULL };

	if(svn->filename == NULL || lstat(svn->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(svn->filename)
		: g_path_get_dirname(svn->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(svn->filename);
	argv[3] = basename;
	_subversion_add_task(svn, "svn blame", dirname, argv);
	g_free(basename);
	g_free(dirname);
}


/* subversion_on_commit */
static void _subversion_on_commit(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { PROGNAME_SVN, "commit", "--", NULL, NULL };

	if(svn->filename == NULL || lstat(svn->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(svn->filename)
		: g_path_get_dirname(svn->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(svn->filename);
	argv[3] = basename;
	_subversion_add_task(svn, "svn commit", dirname, argv);
	g_free(basename);
	g_free(dirname);
}


/* subversion_on_diff */
static void _subversion_on_diff(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { PROGNAME_SVN, "diff", "--", NULL, NULL };

	if(svn->filename == NULL || lstat(svn->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(svn->filename)
		: g_path_get_dirname(svn->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(svn->filename);
	argv[3] = basename;
	_subversion_add_task(svn, "svn diff", dirname, argv);
	g_free(basename);
	g_free(dirname);
}


/* subversion_on_log */
static void _subversion_on_log(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { PROGNAME_SVN, "log", "--", NULL, NULL };

	if(svn->filename == NULL || lstat(svn->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(svn->filename)
		: g_path_get_dirname(svn->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(svn->filename);
	argv[3] = basename;
	_subversion_add_task(svn, "svn log", dirname, argv);
	g_free(basename);
	g_free(dirname);
}


/* subversion_on_status */
static void _subversion_on_status(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { PROGNAME_SVN, "status", "--", NULL, NULL };

	if(svn->filename == NULL || lstat(svn->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(svn->filename)
		: g_path_get_dirname(svn->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(svn->filename);
	argv[3] = basename;
	_subversion_add_task(svn, "svn status", dirname, argv);
	g_free(basename);
	g_free(dirname);
}


/* subversion_on_update */
static void _subversion_on_update(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { PROGNAME_SVN, "update", "--", NULL, NULL };

	if(svn->filename == NULL || lstat(svn->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(svn->filename)
		: g_path_get_dirname(svn->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(svn->filename);
	argv[3] = basename;
	_subversion_add_task(svn, "svn update", dirname, argv);
	g_free(basename);
	g_free(dirname);
}
