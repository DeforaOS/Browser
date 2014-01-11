/* $Id$ */
/* Copyright (c) 2014 Pierre Pronchery <khorben@defora.org> */
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



#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <System.h>
#include <libintl.h>
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)

#ifndef PLUGIN_NAME
# define PLUGIN_NAME		"Trash"
#endif
#ifndef PLUGIN_ICON
# define PLUGIN_ICON		"user-trash"
#endif
#ifndef PLUGIN_DESCRIPTION
# define PLUGIN_DESCRIPTION	NULL
#endif

#ifndef DATA_DELETIONDATE
# define DATA_DELETIONDATE	"DeletionDate"
#endif
#ifndef DATA_EXTENSION
# define DATA_EXTENSION		".trashinfo"
#endif
#ifndef DATA_PATH
# define DATA_PATH		"Path"
#endif
#ifndef DATA_SECTION
# define DATA_SECTION		"Trash Info"
#endif
#ifndef DATA_TRASHINFO
# define DATA_TRASHINFO		"Trash/info"
#endif

#ifndef TEXT_DELETED
# define TEXT_DELETED		"Deleted"
#endif
#ifndef TEXT_MOVETOTRASH
# define TEXT_MOVETOTRASH	"Move to trash"
#endif


/* Trash */
/* private */
/* types */
typedef enum _TrashColumn
{
	TC_PIXBUF = 0,
	TC_FILENAME,
	TC_PATH,
	TC_PATH_ORIGINAL,
	TC_DELETED,
	TC_DELETED_DISPLAY,
	TC_UPDATED
} TrashColumn;
#define TC_LAST TC_UPDATED
#define TC_COUNT (TC_LAST + 1)

typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	guint source;

	/* widgets */
	GtkWidget * widget;
	/* toolbar */
	GtkToolItem * tb_restore;
	GtkToolItem * tb_delete;
	GtkWidget * view;
	GtkListStore * store;
} Trash;


/* prototypes */
/* plug-in */
static Trash * _trash_init(BrowserPluginHelper * helper);
static void _trash_destroy(Trash * trash);
static GtkWidget * _trash_get_widget(Trash * trash);
static void _trash_refresh(Trash * trash, GList * selection);

/* useful */
static gboolean _trash_confirm(Trash * trash, char const * message);
static int _trash_delete_selection(Trash * trash);
static void _trash_list(Trash * trash);
static int _trash_restore_selection(Trash * trash);

/* callbacks */
static void _trash_on_delete(gpointer data);
static void _trash_on_restore(gpointer data);
static void _trash_on_select_all(gpointer data);
static void _trash_on_selection_changed(GtkTreeSelection * treesel,
		gpointer data);
static gboolean _trash_on_timeout(gpointer data);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	PLUGIN_NAME,
	PLUGIN_ICON,
	PLUGIN_DESCRIPTION,
	_trash_init,
	_trash_destroy,
	_trash_get_widget,
	_trash_refresh
};


/* private */
/* functions */
/* trash_init */
static Trash * _trash_init(BrowserPluginHelper * helper)
{
	Trash * trash;
	GtkWidget * widget;
	GtkToolItem * toolitem;
	GtkTreeSelection * treesel;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	if((trash = object_new(sizeof(*trash))) == NULL)
		return NULL;
	trash->helper = helper;
	trash->source = 0;
#if GTK_CHECK_VERSION(3, 0, 0)
	trash->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	trash->widget = gtk_vbox_new(FALSE, 0);
#endif
	widget = gtk_toolbar_new();
	/* move to trash */
	/* FIXME handle sensitiveness of this button */
#if GTK_CHECK_VERSION(2, 8, 0)
	toolitem = gtk_tool_button_new(NULL, _(TEXT_MOVETOTRASH));
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toolitem), PLUGIN_ICON);
#else
	toolitem = gtk_tool_button_new(gtk_image_new_from_icon_name(PLUGIN_ICON,
				GTK_ICON_SIZE_SMALL_TOOLBAR),
			_(TEXT_MOVETOTRASH));
#endif
	/* FIXME handle the signal */
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	toolitem = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_SELECT_ALL);
	g_signal_connect_swapped(toolitem, "clicked", G_CALLBACK(
				_trash_on_select_all), trash);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
	/* restore */
	trash->tb_restore = gtk_tool_button_new_from_stock(GTK_STOCK_UNDO);
	g_signal_connect_swapped(trash->tb_restore, "clicked", G_CALLBACK(
				_trash_on_restore), trash);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), trash->tb_restore, -1);
	/* delete */
	trash->tb_delete = gtk_tool_button_new_from_stock(GTK_STOCK_DELETE);
	g_signal_connect_swapped(trash->tb_delete, "clicked", G_CALLBACK(
				_trash_on_delete), trash);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), trash->tb_delete, -1);
	gtk_box_pack_start(GTK_BOX(trash->widget), widget, FALSE, TRUE, 0);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	trash->store = gtk_list_store_new(TC_COUNT, GDK_TYPE_PIXBUF,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_UINT, G_TYPE_STRING, G_TYPE_BOOLEAN);
	trash->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				trash->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(trash->view), TRUE);
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(trash->view));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_MULTIPLE);
	g_signal_connect(treesel, "changed", G_CALLBACK(
				_trash_on_selection_changed), trash);
	_trash_on_selection_changed(treesel, trash);
	/* icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer,
			"pixbuf", TC_PIXBUF, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(trash->view), column);
	/* path to the original file */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"),
			renderer, "text", TC_PATH_ORIGINAL, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(trash->view), column);
	/* timestamp */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_(TEXT_DELETED),
			renderer, "text", TC_DELETED_DISPLAY, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(trash->view), column);
	gtk_container_add(GTK_CONTAINER(widget), trash->view);
	gtk_box_pack_start(GTK_BOX(trash->widget), widget, TRUE, TRUE, 0);
	gtk_widget_show_all(trash->widget);
	return trash;
}


/* trash_destroy */
static void _trash_destroy(Trash * trash)
{
	if(trash->source != 0)
		g_source_remove(trash->source);
	object_delete(trash);
}


/* trash_get_widget */
static GtkWidget * _trash_get_widget(Trash * trash)
{
	return trash->widget;
}


/* trash_refresh */
static void _trash_refresh(Trash * trash, GList * selection)
{
	if(selection == NULL)
	{
		/* there is no need to refresh anymore */
		if(trash->source > 0)
			g_source_remove(trash->source);
		trash->source = 0;
	}
	else
	{
		_trash_list(trash);
		if(trash->source == 0)
			/* keep refreshing the view */
			trash->source = g_timeout_add(5000, _trash_on_timeout,
					trash);
		/* FIXME complete the implementation (copy the selection) */
	}
}


/* useful */
/* trash_confirm */
static gboolean _trash_confirm(Trash * trash, char const * message)
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
			"%s", message);
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return (res == GTK_RESPONSE_ACCEPT) ? 1 : 0;
}


/* trash_delete_selection */
static int _delete_path(Trash * trash, GtkTreeModel * model,
		GtkTreePath * path);

static int _trash_delete_selection(Trash * trash)
{
	int ret = 0;
	GtkTreeSelection * treesel;
	GtkTreeModel * model;
	GList * rows;
	GList * l;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(trash->view));
	if((rows = gtk_tree_selection_get_selected_rows(treesel, &model))
			== NULL)
		/* nothing is selected */
		return 0;
	for(l = rows; l != NULL; l = l->next)
		ret |= _delete_path(trash, model, l->data);
	g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(rows);
	_trash_list(trash);
	return (ret == 0) ? 0 : -1;
}

static int _delete_path(Trash * trash, GtkTreeModel * model, GtkTreePath * path)
{
	int ret = -1;
	const char ext[] = DATA_EXTENSION;
	GtkTreeIter iter;
	gchar * filename;
	gchar * p;
	size_t len;

	if(gtk_tree_model_get_iter(model, &iter, path) != TRUE)
		/* XXX report error */
		return -1;
	gtk_tree_model_get(model, &iter, TC_FILENAME, &filename, TC_PATH, &p,
			-1);
	if(filename != NULL && (len = strlen(filename)) > sizeof(ext))
		filename[len - sizeof(ext) + 1] = '\0';
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\" \"%s\"\n", __func__, p, filename);
#endif
	/* FIXME implement */
#if 0
	ret = unlink(path);
#endif
	g_free(filename);
	g_free(p);
	return ret;
}


/* trash_list */
static void _list_add(Trash * trash, Config * config, char const * path,
		char const * filename, const time_t sixmonths);
static void _list_get_iter(Trash * trash, GtkTreeIter * iter,
		char const * path);
static char * _list_path(void);
static void _list_purge(Trash * trash);
static void _list_reset(Trash * trash);

static void _trash_list(Trash * trash)
{
	Config * config;
	char * path;
	DIR * dir;
	struct dirent * de;
	time_t sixmonths;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	/* FIXME report errors */
	if((path = _list_path()) == NULL)
		return;
	if((config = config_new()) == NULL)
	{
		free(path);
		return;
	}
	/* FIXME should try to create the directory */
	if((dir = opendir(path)) == NULL)
	{
		config_delete(config);
		free(path);
		return;
	}
	/* FIXME refresh only if necessary */
	_list_reset(trash);
	sixmonths = time(NULL) - 15552000;
	while((de = readdir(dir)) != NULL)
		_list_add(trash, config, path, de->d_name, sixmonths);
	closedir(dir);
	_list_purge(trash);
	config_delete(config);
	free(path);
}

static void _list_add(Trash * trash, Config * config, char const * path,
		char const * filename, const time_t sixmonths)
{
	const char ext[] = DATA_EXTENSION;
	const char section[] = DATA_SECTION;
	BrowserPluginHelper * helper = trash->helper;
	size_t len;
	GtkTreeIter iter;
	GdkPixbuf * pixbuf;
	char * p;
	char const * q;
	struct tm tm;
	time_t t = -1;
	char const * u;
	char buf[16];

	if((len = strlen(filename)) <= sizeof(ext))
		return;
	if(strncmp(&filename[len - sizeof(ext) + 1], ext, sizeof(ext)) != 0)
		return;
	config_reset(config);
	p = g_strdup_printf("%s/%s", path, filename);
	if(config_load(config, p) != 0
			|| (q = config_get(config, section, DATA_PATH)) == NULL)
	{
		g_free(p);
		return;
	}
	pixbuf = helper->get_icon(helper->browser, q, NULL, NULL, NULL, 24);
	if((u = config_get(config, section, DATA_DELETIONDATE)) != NULL
			&& strptime(u, "%Y-%m-%dT%H:%M:%S", &tm) != NULL)
	{
		t = mktime(&tm);
		len = strftime(buf, sizeof(buf), (t >= sixmonths)
				? "%b %e %H:%M" : "%b %e %Y", &tm);
		buf[len] = '\0';
		u = buf;
	}
	else
		u = "";
	_list_get_iter(trash, &iter, p);
	gtk_list_store_set(trash->store, &iter, TC_PIXBUF, pixbuf,
			TC_FILENAME, filename, TC_PATH, p,
			TC_PATH_ORIGINAL, q, TC_DELETED, t,
			TC_DELETED_DISPLAY, u, TC_UPDATED, TRUE, -1);
	g_free(p);
}

static void _list_get_iter(Trash * trash, GtkTreeIter * iter, char const * path)
{
	GtkTreeModel * model = GTK_TREE_MODEL(trash->store);
	gboolean valid;
	gchar * p;
	int res;

	for(valid = gtk_tree_model_get_iter_first(model, iter); valid == TRUE;
		valid = gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, TC_PATH, &p, -1);
		res = strcmp(path, p);
		g_free(p);
		if(res == 0)
			return;
	}
	gtk_list_store_append(trash->store, iter);
}

static char * _list_path(void)
{
	const char fallback[] = ".local/share";
	const char trash[] = DATA_TRASHINFO;
	char * ret;
	char const * homedir;
	size_t len;

	/* FIXME check $XDG_DATA_HOME first */
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	len = strlen(homedir) + 1 + sizeof(fallback) + 1 + sizeof(trash);
	if((ret = malloc(len)) == NULL)
		return NULL;
	snprintf(ret, len, "%s/%s/%s", homedir, fallback, trash);
	return ret;
}

static void _list_purge(Trash * trash)
{
	GtkTreeModel * model = GTK_TREE_MODEL(trash->store);
	GtkTreeIter iter;
	gboolean valid;
	gboolean updated;

	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;)
	{
		gtk_tree_model_get(model, &iter, TC_UPDATED, &updated, -1);
		valid = updated ? gtk_tree_model_iter_next(model, &iter)
			: gtk_list_store_remove(trash->store, &iter);
	}
}

static void _list_reset(Trash * trash)
{
	GtkTreeModel * model = GTK_TREE_MODEL(trash->store);
	GtkTreeIter iter;
	gboolean valid;

	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
		gtk_list_store_set(trash->store, &iter, TC_UPDATED, FALSE, -1);
}


/* trash_restore_selection */
static int _restore_path(Trash * trash, GtkTreeModel * model,
		GtkTreePath * path);

static int _trash_restore_selection(Trash * trash)
{
	/* FIXME code duplicated from _trash_delete_selection() */
	int ret = 0;
	GtkTreeSelection * treesel;
	GtkTreeModel * model;
	GList * rows;
	GList * l;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(trash->view));
	if((rows = gtk_tree_selection_get_selected_rows(treesel, &model))
			== NULL)
		/* nothing is selected */
		return 0;
	for(l = rows; l != NULL; l = l->next)
		ret |= _restore_path(trash, model, l->data);
	g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(rows);
	_trash_list(trash);
	return (ret == 0) ? 0 : -1;
}

static int _restore_path(Trash * trash, GtkTreeModel * model,
		GtkTreePath * path)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	/* FIXME implement */
	return -1;
}


/* callbacks */
/* trash_on_delete */
static void _trash_on_delete(gpointer data)
{
	Trash * trash = data;
	GtkTreeSelection * treesel;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(trash->view));
	if(gtk_tree_selection_count_selected_rows(treesel) > 0
			&& _trash_confirm(trash,
				_("This will delete the file(s) selected.\n"
					"Do you really want to proceed?")))
		_trash_delete_selection(trash);
}


/* trash_on_restore */
static void _trash_on_restore(gpointer data)
{
	Trash * trash = data;
	GtkTreeSelection * treesel;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(trash->view));
	if(gtk_tree_selection_count_selected_rows(treesel) > 0)
		_trash_restore_selection(trash);
}


/* trash_on_select_all */
static void _trash_on_select_all(gpointer data)
{
	Trash * trash = data;
	GtkTreeSelection * treesel;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(trash->view));
	gtk_tree_selection_select_all(treesel);
}


/* trash_on_selection_changed */
static void _trash_on_selection_changed(GtkTreeSelection * treesel,
		gpointer data)
{
	Trash * trash = data;
	gboolean sensitive;

	sensitive = (gtk_tree_selection_count_selected_rows(treesel) > 0)
		? TRUE : FALSE;
	gtk_widget_set_sensitive(GTK_WIDGET(trash->tb_restore), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(trash->tb_delete), sensitive);
}


/* trash_on_timeout */
static gboolean _trash_on_timeout(gpointer data)
{
	Trash * trash = data;

	_trash_list(trash);
	return TRUE;
}
