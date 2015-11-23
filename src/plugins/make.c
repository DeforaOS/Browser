/* $Id$ */
/* Copyright (c) 2013-2015 Pierre Pronchery <khorben@defora.org> */
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
 * - detect the targets supported */



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
	/* additional actions */
	GtkWidget * configure;
	GtkWidget * autogensh;
	GtkWidget * gnuconfigure;

	/* tasks */
	MakeTask ** tasks;
	size_t tasks_cnt;
} Make;


/* constants */
#define MAKE_CONFIGURE	"configure"
#define MAKE_MAKE	"make"


/* prototypes */
static Make * _make_init(BrowserPluginHelper * helper);
static void _make_destroy(Make * make);
static GtkWidget * _make_get_widget(Make * make);
static void _make_refresh(Make * make, GList * selection);

/* accessors */
static gboolean _make_can_autogensh(char const * pathname);
static gboolean _make_can_configure(char const * pathname);
static gboolean _make_can_gnu_configure(char const * pathname);
static gboolean _make_is_managed(char const * pathname);

/* useful */
static int _make_add_task(Make * make, char const * title,
		char const * directory, char * argv[]);
static gboolean _make_find(char const * directory, char const * filename,
		int mode);
static int _make_target(Make * make, char const * filename,
		char const * target);

/* callbacks */
static void _make_on_all(gpointer data);
static void _make_on_autogensh(gpointer data);
static void _make_on_clean(gpointer data);
static void _make_on_configure(gpointer data);
static void _make_on_dist(gpointer data);
static void _make_on_distclean(gpointer data);
static void _make_on_gnu_configure(gpointer data);
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
	GtkWidget * widget;

	if((make = object_new(sizeof(*make))) == NULL)
		return NULL;
	make->helper = helper;
	make->filename = NULL;
	make->source = 0;
	/* widgets */
	make->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	font = pango_font_description_new();
	pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* label */
	make->name = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(make->name), PANGO_ELLIPSIZE_MIDDLE);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(make->name, font);
	g_object_set(make->name, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_widget_modify_font(make->name, font);
	gtk_misc_set_alignment(GTK_MISC(make->name), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(make->widget), make->name, FALSE, TRUE, 0);
	make->status = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(make->status), PANGO_ELLIPSIZE_END);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(make->status, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(make->status), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(make->widget), make->status, FALSE, TRUE, 0);
	/* directory */
	make->directory = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	widget = _init_button(group, "applications-development", _("Build"),
			G_CALLBACK(_make_on_all), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_CLEAR, _("Clean"),
			G_CALLBACK(_make_on_clean), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_SAVE, _("Dist"),
			G_CALLBACK(_make_on_dist), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_DELETE, _("Distclean"),
			G_CALLBACK(_make_on_distclean), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_HARDDISK, _("Install"),
			G_CALLBACK(_make_on_install), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_REVERT_TO_SAVED, _("Uninstall"),
			G_CALLBACK(_make_on_uninstall), make);
	gtk_box_pack_start(GTK_BOX(make->directory), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(make->directory);
	gtk_widget_set_no_show_all(make->directory, TRUE);
	gtk_box_pack_start(GTK_BOX(make->widget), make->directory, FALSE, TRUE,
			0);
	/* file */
	make->file = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	widget = _init_button(group, GTK_STOCK_CONVERT, _("Build"),
			G_CALLBACK(_make_on_target), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_CLEAR, _("Clean"),
			G_CALLBACK(_make_on_clean), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_SAVE, _("Dist"),
			G_CALLBACK(_make_on_dist), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_DELETE, _("Distclean"),
			G_CALLBACK(_make_on_distclean), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_HARDDISK, _("Install"),
			G_CALLBACK(_make_on_install), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	widget = _init_button(group, GTK_STOCK_REVERT_TO_SAVED, _("Uninstall"),
			G_CALLBACK(_make_on_uninstall), make);
	gtk_box_pack_start(GTK_BOX(make->file), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(make->file);
	gtk_widget_set_no_show_all(make->file, TRUE);
	gtk_box_pack_start(GTK_BOX(make->widget), make->file, FALSE, TRUE, 0);
	/* additional actions */
	make->configure = _init_button(group, GTK_STOCK_EXECUTE,
			_("Run configure"), G_CALLBACK(_make_on_configure),
			make);
	gtk_box_pack_start(GTK_BOX(make->widget), make->configure, FALSE, TRUE,
			0);
	make->autogensh = _init_button(group, GTK_STOCK_EXECUTE,
			_("Run ./autogen.sh"), G_CALLBACK(_make_on_autogensh),
			make);
	gtk_box_pack_start(GTK_BOX(make->widget), make->autogensh, FALSE, TRUE,
			0);
	make->gnuconfigure = _init_button(group, GTK_STOCK_EXECUTE,
			_("Run ./configure"), G_CALLBACK(
				_make_on_gnu_configure), make);
	gtk_box_pack_start(GTK_BOX(make->widget), make->gnuconfigure, FALSE,
			TRUE, 0);
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
static void _refresh_error(Make * make, char const * message);
static void _refresh_file(Make * make);
static void _refresh_hide(Make * make, gboolean name);
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
	if(path == NULL || selection->next != NULL)
	{
		_refresh_hide(make, TRUE);
		return;
	}
	if(lstat(path, &st) != 0
			|| (make->filename = strdup(path)) == NULL)
	{
		_refresh_hide(make, TRUE);
		if(errno != ENOENT)
			_refresh_error(make, path);
		return;
	}
	p = g_filename_display_basename(path);
	gtk_label_set_text(GTK_LABEL(make->name), p);
	g_free(p);
	_refresh_hide(make, FALSE);
	if(S_ISDIR(st.st_mode))
		_refresh_dir(make);
	else
		_refresh_file(make);
	if(_make_can_configure(make->filename))
		gtk_widget_show(make->configure);
	if(_make_can_autogensh(make->filename))
		gtk_widget_show(make->autogensh);
	if(_make_can_gnu_configure(make->filename))
		gtk_widget_show(make->gnuconfigure);
}

static void _refresh_dir(Make * make)
{
	/* check if it is managed */
	if(_make_is_managed(make->filename) == FALSE)
		_refresh_status(make, _("No Makefile found"));
	else
		gtk_widget_show(make->directory);
}

static void _refresh_error(Make * make, char const * message)
{
	BrowserPluginHelper * helper = make->helper;

	error_set("%s: %s", message, strerror(errno));
	helper->error(helper->browser, error_get(NULL), 1);
}

static void _refresh_file(Make * make)
{
	/* check if it is managed */
	if(_make_is_managed(make->filename) == FALSE)
		_refresh_status(make, _("No Makefile found"));
	else
		gtk_widget_show(make->file);
}

static void _refresh_hide(Make * make, gboolean name)
{
	name ? gtk_widget_hide(make->name) : gtk_widget_show(make->name);
	_refresh_status(make, NULL);
	gtk_widget_hide(make->directory);
	gtk_widget_hide(make->file);
	gtk_widget_hide(make->configure);
	gtk_widget_hide(make->autogensh);
	gtk_widget_hide(make->gnuconfigure);
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


/* accessors */
/* make_can_autogensh */
static gboolean _make_can_autogensh(char const * pathname)
{
	gboolean ret;
	char const autogensh[] = "autogen.sh";
	struct stat st;
	gchar * dirname;

	if(stat(pathname, &st) != 0)
		return FALSE;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(pathname)
		: g_path_get_dirname(pathname);
	ret = _make_find(dirname, autogensh, R_OK | X_OK);
	g_free(dirname);
	return ret;
}


/* make_can_configure */
static gboolean _make_can_configure(char const * pathname)
{
	gboolean ret;
	char const project_conf[] = "project.conf";
	struct stat st;
	gchar * dirname;

	if(stat(pathname, &st) != 0)
		return FALSE;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(pathname)
		: g_path_get_dirname(pathname);
	ret = _make_find(dirname, project_conf, R_OK);
	g_free(dirname);
	return ret;
}


/* make_can_gnu_configure */
static gboolean _make_can_gnu_configure(char const * pathname)
{
	gboolean ret;
	char const configure[] = "configure";
	struct stat st;
	gchar * dirname;

	if(stat(pathname, &st) != 0)
		return FALSE;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(pathname)
		: g_path_get_dirname(pathname);
	ret = _make_find(dirname, configure, R_OK | X_OK);
	g_free(dirname);
	return ret;
}


/* make_is_managed */
static gboolean _make_is_managed(char const * pathname)
{
	gboolean ret = FALSE;
	char const * makefile[] = { "Makefile", "makefile", "GNUmakefile" };
	struct stat st;
	gchar * dirname;
	size_t i;

	if(stat(pathname, &st) != 0)
		return FALSE;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(pathname)
		: g_path_get_dirname(pathname);
	for(i = 0; i < sizeof(makefile) / sizeof(*makefile); i++)
		if((ret = _make_find(dirname, makefile[i], R_OK)) == TRUE)
			break;
	g_free(dirname);
	return ret;
}


/* useful */
/* make_add_task */
static int _make_add_task(Make * make, char const * title,
		char const * directory, char * argv[])
{
	BrowserPluginHelper * helper = make->helper;
	MakeTask ** p;
	MakeTask * task;

	if((p = realloc(make->tasks, sizeof(*p) * (make->tasks_cnt + 1)))
			== NULL)
		return -helper->error(helper->browser, strerror(errno), 1);
	make->tasks = p;
	if((task = _common_task_new(helper, &plugin, title, directory, argv,
					NULL, NULL)) == NULL)
		return -helper->error(helper->browser, error_get(NULL), 1);
	make->tasks[make->tasks_cnt++] = task;
	return 0;
}


/* make_find */
static gboolean _make_find(char const * directory, char const * filename,
		int mode)
{
	gboolean ret = FALSE;
	gchar * p;

	p = g_build_path("/", directory, filename, NULL);
	ret = (access(p, mode) == 0) ? TRUE : FALSE;
	g_free(p);
	return ret;
}


/* make_target */
static int _make_target(Make * make, char const * filename, char const * target)
{
	BrowserPluginHelper * helper = make->helper;
	int ret;
	struct stat st;
	gchar * dirname;
	char * argv[3] = { NULL, NULL, NULL };
	char const * p;

	if(filename == NULL || lstat(filename, &st) != 0)
		return 0;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(filename)
		: g_path_get_dirname(filename);
	if(target != NULL && (argv[1] = strdup(target)) == NULL)
	{
		/* FIXME report the error */
		g_free(dirname);
		return -1;
	}
	if((p = helper->config_get(helper->browser, "make", "make")) == NULL)
		p = MAKE_MAKE;
	if((argv[0] = strdup(p)) == NULL)
	{
		/* FIXME report the error */
		free(argv[1]);
		g_free(dirname);
		return -1;
	}
	ret = _make_add_task(make, target, dirname, argv);
	free(argv[0]);
	free(argv[1]);
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


/* make_on_autogensh */
static void _make_on_autogensh(gpointer data)
{
	Make * make = data;
	struct stat st;
	gchar * dirname;
	char * argv[2] = { "./autogen.sh", NULL };

	if(make->filename == NULL || lstat(make->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(make->filename)
		: g_path_get_dirname(make->filename);
	_make_add_task(make, "./autogen.sh", dirname, argv);
	g_free(dirname);
	return;
}


/* make_on_clean */
static void _make_on_clean(gpointer data)
{
	Make * make = data;

	_make_target(make, make->filename, "clean");
}


/* _make_on_configure */
static void _make_on_configure(gpointer data)
{
	Make * make = data;
	BrowserPluginHelper * helper = make->helper;
	struct stat st;
	gchar * dirname;
	char * argv[2] = { NULL, NULL };
	char const * p;

	if(make->filename == NULL || lstat(make->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(make->filename)
		: g_path_get_dirname(make->filename);
	if((p = helper->config_get(helper->browser, "make", "configure"))
			== NULL)
		p = MAKE_CONFIGURE;
	if((argv[0] = strdup(p)) == NULL)
	{
		/* FIXME report the error */
		g_free(dirname);
		return;
	}
	_make_add_task(make, "configure", dirname, argv);
	free(argv[0]);
	g_free(dirname);
	return;
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


/* make_on_gnu_configure */
static void _make_on_gnu_configure(gpointer data)
{
	Make * make = data;
	struct stat st;
	gchar * dirname;
	char * argv[2] = { "./configure", NULL };

	if(make->filename == NULL || lstat(make->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(make->filename)
		: g_path_get_dirname(make->filename);
	_make_add_task(make, "./configure", dirname, argv);
	g_free(dirname);
	return;
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
