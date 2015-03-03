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



#include <System.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define COMMON_RTRIM
#include "common.c"


/* CVS */
/* private */
/* types */
typedef struct _CommonTask CVSTask;

typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	char * filename;

	guint source;

	/* widgets */
	GtkWidget * widget;
	GtkWidget * name;
	GtkWidget * status;
	/* checkout */
	GtkWidget * checkout;
	/* directory */
	GtkWidget * directory;
	GtkWidget * d_root;
	GtkWidget * d_repository;
	GtkWidget * d_tag;
	/* file */
	GtkWidget * file;
	GtkWidget * f_revision;
	/* additional actions */
	GtkWidget * add;

	/* tasks */
	CVSTask ** tasks;
	size_t tasks_cnt;
} CVS;


/* prototypes */
static CVS * _cvs_init(BrowserPluginHelper * helper);
static void _cvs_destroy(CVS * cvs);
static GtkWidget * _cvs_get_widget(CVS * cvs);
static void _cvs_refresh(CVS * cvs, GList * selection);

/* accessors */
static char * _cvs_get_entries(char const * pathname);
static char * _cvs_get_repository(char const * pathname);
static char * _cvs_get_root(char const * pathname);
static char * _cvs_get_tag(char const * pathname);
static gboolean _cvs_is_managed(char const * filename, char ** revision);

/* useful */
static int _cvs_add_task(CVS * cvs, char const * title, char const * directory,
		char * argv[], CommonTaskCallback callback);
static int _cvs_confirm_delete(char const * filename);
static GtkResponseType _cvs_prompt_checkout(char const * message, char ** path,
		char ** module);

/* callbacks */
static void _cvs_on_add(gpointer data);
static void _cvs_on_annotate(gpointer data);
static void _cvs_on_checkout(gpointer data);
static void _cvs_on_commit(gpointer data);
static void _cvs_on_delete(gpointer data);
static void _cvs_on_diff(gpointer data);
static void _cvs_on_log(gpointer data);
static void _cvs_on_status(gpointer data);
static void _cvs_on_update(gpointer data);


/* public */
/* variables */
/* plug-in */
BrowserPluginDefinition plugin =
{
	N_("CVS"),
	"applications-development",
	NULL,
	_cvs_init,
	_cvs_destroy,
	_cvs_get_widget,
	_cvs_refresh
};


/* private */
/* functions */
/* cvs_init */
static GtkWidget * _init_button(GtkSizeGroup * group, char const * icon,
		char const * label, GCallback callback, gpointer data);
static GtkWidget * _init_label(GtkSizeGroup * group, char const * label,
		GtkWidget ** widget);

static CVS * _cvs_init(BrowserPluginHelper * helper)
{
	CVS * cvs;
	PangoFontDescription * font;
	GtkSizeGroup * group;
	GtkSizeGroup * bgroup;
	GtkWidget * widget;

	if((cvs = object_new(sizeof(*cvs))) == NULL)
		return NULL;
	cvs->helper = helper;
	cvs->filename = NULL;
	cvs->source = 0;
	/* widgets */
#if GTK_CHECK_VERSION(3, 0, 0)
	cvs->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	cvs->widget = gtk_vbox_new(FALSE, 4);
#endif
	font = pango_font_description_new();
	pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	bgroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* label */
	cvs->name = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(cvs->name), PANGO_ELLIPSIZE_MIDDLE);
	gtk_misc_set_alignment(GTK_MISC(cvs->name), 0.0, 0.0);
	gtk_widget_modify_font(cvs->name, font);
	gtk_box_pack_start(GTK_BOX(cvs->widget), cvs->name, FALSE, TRUE, 0);
	cvs->status = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(cvs->status), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment(GTK_MISC(cvs->status), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(cvs->widget), cvs->status, FALSE, TRUE, 0);
	/* checkout */
#if GTK_CHECK_VERSION(3, 0, 0)
	cvs->checkout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	cvs->checkout = gtk_vbox_new(FALSE, 4);
#endif
	widget = _init_button(bgroup, GTK_STOCK_OK, _("Checkout..."),
			G_CALLBACK(_cvs_on_checkout), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->checkout), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(cvs->checkout);
	gtk_widget_set_no_show_all(cvs->checkout, TRUE);
	gtk_box_pack_start(GTK_BOX(cvs->widget), cvs->checkout, FALSE, TRUE, 0);
	/* directory */
#if GTK_CHECK_VERSION(3, 0, 0)
	cvs->directory = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	cvs->directory = gtk_vbox_new(FALSE, 4);
#endif
	widget = _init_label(group, _("Root:"), &cvs->d_root);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_label(group, _("Repository:"), &cvs->d_repository);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_label(group, _("Tag:"), &cvs->d_tag);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_FIND_AND_REPLACE,
			_("Request diff"), G_CALLBACK(_cvs_on_diff), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_INDEX, _("Annotate"),
			G_CALLBACK(_cvs_on_annotate), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_FIND, _("View log"),
			G_CALLBACK(_cvs_on_log), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_PROPERTIES, _("Status"),
			G_CALLBACK(_cvs_on_status), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_REFRESH, _("Update"),
			G_CALLBACK(_cvs_on_update), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_DELETE, _("Delete"),
			G_CALLBACK(_cvs_on_delete), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_JUMP_TO, _("Commit"),
			G_CALLBACK(_cvs_on_commit), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->directory), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(cvs->directory);
	gtk_widget_set_no_show_all(cvs->directory, TRUE);
	gtk_box_pack_start(GTK_BOX(cvs->widget), cvs->directory, FALSE, TRUE,
			0);
	/* file */
#if GTK_CHECK_VERSION(3, 0, 0)
	cvs->file = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	cvs->file = gtk_vbox_new(FALSE, 4);
#endif
	widget = _init_label(group, _("Revision:"), &cvs->f_revision);
	gtk_box_pack_start(GTK_BOX(cvs->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_FIND_AND_REPLACE,
			_("Request diff"), G_CALLBACK(_cvs_on_diff), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_INDEX, _("Annotate"),
			G_CALLBACK(_cvs_on_annotate), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_FIND, _("View log"),
			G_CALLBACK(_cvs_on_log), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_PROPERTIES, _("Status"),
			G_CALLBACK(_cvs_on_status), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_REFRESH, _("Update"),
			G_CALLBACK(_cvs_on_update), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_DELETE, _("Delete"),
			G_CALLBACK(_cvs_on_delete), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->file), widget, FALSE, TRUE, 0);
	widget = _init_button(bgroup, GTK_STOCK_JUMP_TO, _("Commit"),
			G_CALLBACK(_cvs_on_commit), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->file), widget, FALSE, TRUE, 0);
	gtk_widget_show_all(cvs->file);
	gtk_widget_set_no_show_all(cvs->file, TRUE);
	gtk_box_pack_start(GTK_BOX(cvs->widget), cvs->file, FALSE, TRUE, 0);
	/* additional actions */
	cvs->add = _init_button(bgroup, GTK_STOCK_ADD, _("Add to repository"),
			G_CALLBACK(_cvs_on_add), cvs);
	gtk_box_pack_start(GTK_BOX(cvs->widget), cvs->add, FALSE, TRUE, 0);
	gtk_widget_show_all(cvs->widget);
	pango_font_description_free(font);
	/* tasks */
	cvs->tasks = NULL;
	cvs->tasks_cnt = 0;
	return cvs;
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

static GtkWidget * _init_label(GtkSizeGroup * group, char const * label,
		GtkWidget ** widget)
{
	GtkWidget * hbox;

#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	*widget = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(*widget), 0.0, 0.0);
	gtk_size_group_add_widget(group, *widget);
	gtk_box_pack_start(GTK_BOX(hbox), *widget, FALSE, TRUE, 0);
	*widget = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(*widget), PANGO_ELLIPSIZE_MIDDLE);
	gtk_misc_set_alignment(GTK_MISC(*widget), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(hbox), *widget, TRUE, TRUE, 0);
	return hbox;
}


/* cvs_destroy */
static void _cvs_destroy(CVS * cvs)
{
	size_t i;

	for(i = 0; i < cvs->tasks_cnt; i++)
		_common_task_delete(cvs->tasks[i]);
	free(cvs->tasks);
	if(cvs->source != 0)
		g_source_remove(cvs->source);
	object_delete(cvs);
}


/* cvs_get_widget */
static GtkWidget * _cvs_get_widget(CVS * cvs)
{
	return cvs->widget;
}


/* cvs_refresh */
static void _refresh_dir(CVS * cvs);
static void _refresh_file(CVS * cvs);
static void _refresh_status(CVS * cvs, char const * status);

static void _cvs_refresh(CVS * cvs, GList * selection)
{
	char * path = (selection != NULL) ? selection->data : NULL;
	struct stat st;
	gchar * p;

	if(cvs->source != 0)
		g_source_remove(cvs->source);
	free(cvs->filename);
	cvs->filename = NULL;
	if(path == NULL || lstat(path, &st) != 0)
		return;
	if((cvs->filename = strdup(path)) == NULL)
		return;
	p = g_filename_display_basename(path);
	gtk_label_set_text(GTK_LABEL(cvs->name), p);
	g_free(p);
	_refresh_status(cvs, NULL);
	gtk_widget_hide(cvs->checkout);
	gtk_widget_hide(cvs->directory);
	gtk_widget_hide(cvs->file);
	gtk_widget_hide(cvs->add);
	if(S_ISDIR(st.st_mode))
		_refresh_dir(cvs);
	else
		_refresh_file(cvs);
}

static void _refresh_dir(CVS * cvs)
{
	BrowserPluginHelper * helper = cvs->helper;
	char const dir[] = "CVS";
	size_t len = strlen(cvs->filename);
	char * p = NULL;
	struct stat st;
	gchar * q;
	char const * filename = cvs->filename;

	/* reset the interface */
	gtk_label_set_text(GTK_LABEL(cvs->d_root), NULL);
	gtk_label_set_text(GTK_LABEL(cvs->d_repository), NULL);
	gtk_label_set_text(GTK_LABEL(cvs->d_tag), NULL);
	/* consider "CVS" folders as managed */
	if((len = strlen(filename)) >= 4
			&& strcmp(&filename[len - 4], "/CVS") == 0)
	{
		if((p = strdup(filename)) != NULL)
		{
			p[len - 4] = '\0';
			filename = p;
		}
	}
	/* check if it is a CVS repository */
	else
	{
		len = strlen(filename) + sizeof(dir) + 1;
		if((p = malloc(len)) == NULL)
		{
			helper->error(helper->browser, strerror(errno), 1);
			return;
		}
		snprintf(p, len, "%s/%s", filename, dir);
		if(lstat(p, &st) != 0)
		{
			/* check if the parent folder is managed */
			if(_cvs_is_managed(filename, NULL) == FALSE)
			{
				_refresh_status(cvs, _("Not a CVS repository"));
				gtk_widget_show(cvs->checkout);
			}
			else
			{
				_refresh_status(cvs, _("Not managed by CVS"));
				gtk_widget_show(cvs->add);
			}
			free(p);
			return;
		}
	}
	/* this folder is managed */
	gtk_widget_show(cvs->directory);
	/* obtain the CVS root */
	if((q = _cvs_get_root(filename)) != NULL)
	{
		gtk_label_set_text(GTK_LABEL(cvs->d_root), q);
		free(q);
	}
	/* obtain the CVS repository */
	if((q = _cvs_get_repository(filename)) != NULL)
	{
		gtk_label_set_text(GTK_LABEL(cvs->d_repository), q);
		free(q);
	}
	/* obtain the default CVS tag (if set) */
	if((q = _cvs_get_tag(filename)) != NULL)
	{
		if(q[0] == 'T' && q[1] != '\0')
			gtk_label_set_text(GTK_LABEL(cvs->d_tag), &q[1]);
		g_free(q);
	}
	free(p);
}

static void _refresh_file(CVS * cvs)
{
	char * revision = NULL;

	/* reset the interface */
	gtk_label_set_text(GTK_LABEL(cvs->f_revision), NULL);
	/* check if it is managed */
	if(_cvs_is_managed(cvs->filename, &revision) == FALSE)
		_refresh_status(cvs, _("Not a CVS repository"));
	else if(revision == NULL)
	{
		gtk_widget_show(cvs->add);
		_refresh_status(cvs, _("Not managed by CVS"));
	}
	else
	{
		gtk_widget_show(cvs->file);
		if(revision != NULL)
		{
			gtk_label_set_text(GTK_LABEL(cvs->f_revision),
					revision);
			free(revision);
		}
	}
}

static void _refresh_status(CVS * cvs, char const * status)
{
	if(status == NULL)
		gtk_widget_hide(cvs->status);
	else
	{
		gtk_label_set_text(GTK_LABEL(cvs->status), status);
		gtk_widget_show(cvs->status);
	}
}


/* accessors */
/* cvs_get_entries */
static char * _cvs_get_entries(char const * pathname)
{
	char * ret = NULL;
	char const entries[] = "CVS/Entries";
	gchar * dirname;
	size_t len;
	char * p;

	dirname = g_path_get_dirname(pathname);
	len = strlen(dirname) + sizeof(entries) + 1;
	if((p = malloc(len)) == NULL)
		return NULL;
	snprintf(p, len, "%s/%s", dirname, entries);
	g_file_get_contents(p, &ret, NULL, NULL);
	free(p);
	g_free(dirname);
	return ret;
}


/* cvs_get_repository */
static char * _cvs_get_repository(char const * pathname)
{
	char * ret = NULL;
	char const repository[] = "CVS/Repository";
	size_t len;
	char * p;

	len = strlen(pathname) + sizeof(repository) + 1;
	if((p = malloc(len)) == NULL)
		return NULL;
	snprintf(p, len, "%s/%s", pathname, repository);
	if(g_file_get_contents(p, &ret, NULL, NULL) == TRUE)
		_common_rtrim(ret);
	free(p);
	return ret;
}


/* cvs_get_root */
static char * _cvs_get_root(char const * pathname)
{
	char * ret = NULL;
	char const root[] = "CVS/Root";
	size_t len;
	char * p;

	len = strlen(pathname) + sizeof(root) + 1;
	if((p = malloc(len)) == NULL)
		return NULL;
	snprintf(p, len, "%s/%s", pathname, root);
	if(g_file_get_contents(p, &ret, NULL, NULL) == TRUE)
		_common_rtrim(ret);
	free(p);
	return ret;
}


/* cvs_get_tag */
static char * _cvs_get_tag(char const * pathname)
{
	char * ret = NULL;
	char const tag[] = "CVS/Tag";
	char * p;
	size_t len;

	len = strlen(pathname) + sizeof(tag) + 1;
	if((p = malloc(len)) == NULL)
		return NULL;
	snprintf(p, len, "%s/%s", pathname, tag);
	if(g_file_get_contents(p, &ret, NULL, NULL) == TRUE)
		_common_rtrim(ret);
	free(p);
	return ret;
}


/* cvs_is_managed */
/* XXX returns if the directory is managed, set *revision if the file is */
static gboolean _cvs_is_managed(char const * pathname, char ** revision)
{
	char * entries;
	gchar * basename;
	size_t len;
	char const * s;
	char buf[256];

	/* obtain the CVS entries */
	if((entries = _cvs_get_entries(pathname)) == NULL)
		return FALSE;
	/* lookup the filename within the entries */
	basename = g_path_get_basename(pathname);
	len = strlen(basename);
	for(s = entries; s != NULL; s = strchr(s, '\n'))
	{
		if((s = strchr(s, '/')) == NULL)
			break;
		if(strncmp(++s, basename, len) != 0 || s[len] != '/')
			continue;
		s += len;
		if(sscanf(s, "/%255[^/]/", buf) != 1)
			break;
		buf[sizeof(buf) - 1] = '\0';
		break;
	}
	g_free(basename);
	g_free(entries);
	if(s != NULL && revision != NULL)
		*revision = strdup(buf);
	return TRUE;
}


/* useful */
/* cvs_add_task */
static int _cvs_add_task(CVS * cvs, char const * title, char const * directory,
		char * argv[], CommonTaskCallback callback)
{
	BrowserPluginHelper * helper = cvs->helper;
	CVSTask ** p;
	CVSTask * task;

	if((p = realloc(cvs->tasks, sizeof(*p) * (cvs->tasks_cnt + 1))) == NULL)
		return -helper->error(helper->browser, strerror(errno), 1);
	cvs->tasks = p;
	if((task = _common_task_new(helper, &plugin, title, directory, argv,
					callback, cvs)) == NULL)
		return -helper->error(helper->browser, error_get(), 1);
	cvs->tasks[cvs->tasks_cnt++] = task;
	return 0;
}


/* cvs_confirm_delete */
static int _cvs_confirm_delete(char const * filename)
{
	GtkWidget * dialog;
	int res;

	/* XXX move to BrowserPluginHelper */
	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Question"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			_("Do you really want to delete %s?"), filename);
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return (res == GTK_RESPONSE_ACCEPT) ? 1 : 0;
}


/* cvs_prompt_checkout */
static GtkResponseType _cvs_prompt_checkout(char const * message, char ** path,
		char ** module)
{
	GtkResponseType ret;
	GtkSizeGroup * group;
	GtkWidget * dialog;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * epath;
	GtkWidget * emodule;

	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* FIXME make it transient */
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
# if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Question"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
# endif
			"%s", message);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
# if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
# else
	vbox = GTK_DIALOG(dialog)->vbox;
# endif
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	label = gtk_label_new(_("Path: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	epath = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(epath), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), epath, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	label = gtk_label_new(_("Module: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_size_group_add_widget(group, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	emodule = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(emodule), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), emodule, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show_all(vbox);
	if((ret = gtk_dialog_run(GTK_DIALOG(dialog))) == GTK_RESPONSE_OK)
	{
		if((*path = g_strdup(gtk_entry_get_text(GTK_ENTRY(epath))))
				== NULL
				|| (*module = g_strdup(gtk_entry_get_text(
							GTK_ENTRY(emodule))))
				== NULL)
		{
			ret = GTK_RESPONSE_NONE;
			g_free(*path);
		}
	}
	gtk_widget_destroy(dialog);
	return ret;
}


/* callbacks */
/* cvs_on_add */
static gboolean _add_is_binary(char const * type);
static void _add_on_callback(CVS * cvs, int ret);

static void _cvs_on_add(gpointer data)
{
	CVS * cvs = data;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "cvs", "add", "--", NULL, NULL, NULL };
	char const * type;

	if(cvs->filename == NULL)
		return;
	dirname = g_path_get_dirname(cvs->filename);
	basename = g_path_get_basename(cvs->filename);
	argv[3] = basename;
	type = cvs->helper->get_type(cvs->helper->browser, cvs->filename, 0);
	if(_add_is_binary(type))
	{
		argv[4] = argv[3];
		argv[3] = argv[2];
		argv[2] = "-kb";
	}
	_cvs_add_task(cvs, "cvs add", dirname, argv, _add_on_callback);
	g_free(basename);
	g_free(dirname);
}

static gboolean _add_is_binary(char const * type)
{
	char const text[] = "text/";
	char const * types[] = { "application/x-perl",
		"application/x-shellscript",
		"application/xml",
		"application/xslt+xml" };
	size_t i;

	if(type == NULL)
		return TRUE;
	if(strncmp(text, type, sizeof(text) - 1) == 0)
		return FALSE;
	for(i = 0; i < sizeof(types) / sizeof(*types); i++)
		if(strcmp(types[i], type) == 0)
			return FALSE;
	return TRUE;
}

static void _add_on_callback(CVS * cvs, int ret)
{
	if(ret == 0)
		/* refresh upon success */
		cvs->helper->refresh(cvs->helper->browser);
}


/* cvs_on_annotate */
static void _cvs_on_annotate(gpointer data)
{
	CVS * cvs = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "cvs", "annotate", "--", NULL, NULL };

	if(cvs->filename == NULL || lstat(cvs->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(cvs->filename)
		: g_path_get_dirname(cvs->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(cvs->filename);
	argv[3] = basename;
	_cvs_add_task(cvs, "cvs annotate", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}


/* cvs_on_checkout */
static void _cvs_on_checkout(gpointer data)
{
	CVS * cvs = data;
	struct stat st;
	char * argv[] = { "cvs", "checkout", "--", NULL, NULL };
	char * dirname;
	char * path;
	char * module;

	if(_cvs_prompt_checkout("Checkout repository from:", &path, &module)
			!= GTK_RESPONSE_OK)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(cvs->filename)
		: g_path_get_dirname(cvs->filename);
	/* FIXME escape arguments as required */
	argv[3] = g_strdup_printf("%s%s checkout %s", "-d:", path, module);
	_cvs_add_task(cvs, "cvs checkout", dirname, argv, NULL);
	g_free(argv[3]);
	g_free(dirname);
	free(path);
	free(module);
}


/* cvs_on_commit */
static void _on_commit_callback(CVS * cvs, int ret);

static void _cvs_on_commit(gpointer data)
{
	CVS * cvs = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "cvs", "commit", "--", NULL, NULL };

	if(cvs->filename == NULL || lstat(cvs->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(cvs->filename)
		: g_path_get_dirname(cvs->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(cvs->filename);
	argv[3] = basename;
	_cvs_add_task(cvs, "cvs commit", dirname, argv, _on_commit_callback);
	g_free(basename);
	g_free(dirname);
}

static void _on_commit_callback(CVS * cvs, int ret)
{
	if(ret == 0)
		/* refresh upon success */
		cvs->helper->refresh(cvs->helper->browser);
}


/* cvs_on_delete */
static void _cvs_on_delete(gpointer data)
{
	CVS * cvs = data;
	BrowserPluginHelper * helper = cvs->helper;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "cvs", "delete", "--", NULL, NULL };

	if(cvs->filename == NULL || lstat(cvs->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(cvs->filename)
		: g_path_get_dirname(cvs->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(cvs->filename);
	if((argv[3] = basename) == NULL)
		_cvs_add_task(cvs, "cvs delete", dirname, argv, NULL);
	else if(_cvs_confirm_delete(basename) == 1)
	{
		/* delete the file locally before asking CVS to */
		if(unlink(cvs->filename) != 0)
			helper->error(helper->browser, strerror(errno), 1);
		else
			_cvs_add_task(cvs, "cvs delete", dirname, argv, NULL);
	}
	g_free(basename);
	g_free(dirname);
}


/* cvs_on_diff */
static void _cvs_on_diff(gpointer data)
{
	CVS * cvs = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "cvs", "diff", "--", NULL, NULL };

	if(cvs->filename == NULL || lstat(cvs->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(cvs->filename)
		: g_path_get_dirname(cvs->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(cvs->filename);
	argv[3] = basename;
	_cvs_add_task(cvs, "cvs diff", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}


/* cvs_on_log */
static void _cvs_on_log(gpointer data)
{
	CVS * cvs = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "cvs", "log", "--", NULL, NULL };

	if(cvs->filename == NULL || lstat(cvs->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(cvs->filename)
		: g_path_get_dirname(cvs->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(cvs->filename);
	argv[3] = basename;
	_cvs_add_task(cvs, "cvs log", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}


/* cvs_on_status */
static void _cvs_on_status(gpointer data)
{
	CVS * cvs = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "cvs", "status", "--", NULL, NULL };

	if(cvs->filename == NULL || lstat(cvs->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(cvs->filename)
		: g_path_get_dirname(cvs->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(cvs->filename);
	argv[3] = basename;
	_cvs_add_task(cvs, "cvs status", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}


/* cvs_on_update */
static void _cvs_on_update(gpointer data)
{
	CVS * cvs = data;
	struct stat st;
	gchar * dirname;
	gchar * basename;
	char * argv[] = { "cvs", "update", "--", NULL, NULL };

	if(cvs->filename == NULL || lstat(cvs->filename, &st) != 0)
		return;
	dirname = S_ISDIR(st.st_mode) ? g_strdup(cvs->filename)
		: g_path_get_dirname(cvs->filename);
	basename = S_ISDIR(st.st_mode) ? NULL
		: g_path_get_basename(cvs->filename);
	argv[3] = basename;
	_cvs_add_task(cvs, "cvs update", dirname, argv, NULL);
	g_free(basename);
	g_free(dirname);
}
