/* $Id$ */
/* Copyright (c) 2011-2015 Pierre Pronchery <khorben@defora.org> */
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
 * - parse SVN's meta-data */



#include <System.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
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


/* prototypes */
static SVN * _subversion_init(BrowserPluginHelper * helper);
static void _subversion_destroy(SVN * svn);
static GtkWidget * _subversion_get_widget(SVN * svn);
static void _subversion_refresh(SVN * svn, GList * selection);

/* accessors */
static gboolean _subversion_is_managed(char const * filename);

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
#if GTK_CHECK_VERSION(3, 0, 0)
	svn->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	svn->widget = gtk_vbox_new(FALSE, 4);
#endif
	font = pango_font_description_new();
	pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* label */
	svn->name = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(svn->name), PANGO_ELLIPSIZE_MIDDLE);
	gtk_misc_set_alignment(GTK_MISC(svn->name), 0.0, 0.0);
	gtk_widget_modify_font(svn->name, font);
	gtk_box_pack_start(GTK_BOX(svn->widget), svn->name, FALSE, TRUE, 0);
	svn->status = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(svn->status), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment(GTK_MISC(svn->status), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(svn->widget), svn->status, FALSE, TRUE, 0);
	/* directory */
#if GTK_CHECK_VERSION(3, 0, 0)
	svn->directory = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	svn->directory = gtk_vbox_new(FALSE, 4);
#endif
	widget = _init_button(group, GTK_STOCK_FIND_AND_REPLACE,
			_("Request diff"), G_CALLBACK(_subversion_on_diff),
			svn);
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
#if GTK_CHECK_VERSION(3, 0, 0)
	svn->file = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	svn->file = gtk_vbox_new(FALSE, 4);
#endif
	widget = _init_button(group, GTK_STOCK_FIND_AND_REPLACE,
			_("Request diff"), G_CALLBACK(_subversion_on_diff),
			svn);
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
	if(path == NULL || lstat(path, &st) != 0)
		return;
	if((svn->filename = strdup(path)) == NULL)
		return;
	p = g_filename_display_basename(path);
	gtk_label_set_text(GTK_LABEL(svn->name), p);
	g_free(p);
	_refresh_status(svn, NULL);
	gtk_widget_hide(svn->directory);
	gtk_widget_hide(svn->file);
	gtk_widget_hide(svn->add);
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
	if(_subversion_is_managed(svn->filename))
		gtk_widget_show(svn->directory);
	else
		_refresh_status(svn, _("Not a Subversion repository"));
}

static void _refresh_status(SVN * svn, char const * status)
{
	if(status == NULL)
		status = "";
	gtk_label_set_text(GTK_LABEL(svn->status), status);
}


/* accessors */
/* subversion_is_managed */
static gboolean _subversion_is_managed(char const * filename)
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
		if((p = string_new_append(dir, "/.svn", NULL)) == NULL)
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
/* svn_add_task */
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
		return -helper->error(helper->browser, error_get(), 1);
	svn->tasks[svn->tasks_cnt++] = task;
	return 0;
}


/* callbacks */
/* svn_on_add */
static void _subversion_on_add(gpointer data)
{
	SVN * svn = data;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "svn", "add", "--", NULL, NULL };

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
	char * argv[] = { "svn", "blame", "--", NULL, NULL };

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


/* svn_on_commit */
static void _subversion_on_commit(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "svn", "commit", "--", NULL, NULL };

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


/* svn_on_diff */
static void _subversion_on_diff(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "svn", "diff", "--", NULL, NULL };

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


/* svn_on_log */
static void _subversion_on_log(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "svn", "log", "--", NULL, NULL };

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


/* svn_on_status */
static void _subversion_on_status(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "svn", "status", "--", NULL, NULL };

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


/* svn_on_update */
static void _subversion_on_update(gpointer data)
{
	SVN * svn = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "svn", "update", "--", NULL, NULL };

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
