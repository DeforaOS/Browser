/* $Id$ */
/* Copyright (c) 2006-2014 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/param.h>
#ifndef __GNU__ /* XXX hurd portability */
# include <sys/mount.h>
# if defined(__linux__) || defined(__CYGWIN__)
#  define unmount(a, b) umount(a)
# endif
# ifndef unmount
#  define unmount unmount
# endif
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include "callbacks.h"
#include "browser.h"
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME
# define PROGNAME "browser"
#endif
#define COMMON_EXEC
#define COMMON_SYMLINK
#include "common.c"


/* public */
/* functions */
/* callbacks */
/* window */
/* on_closex */
gboolean on_closex(gpointer data)
{
	Browser * browser = data;

	browser_delete(browser);
	if(browser_cnt == 0)
		gtk_main_quit();
	return TRUE;
}


/* accelerators */
/* on_close */
void on_close(gpointer data)
{
	on_closex(data);
}


/* on_location */
void on_location(gpointer data)
{
	Browser * browser = data;

	browser_focus_location(browser);
}


/* on_new_window */
void on_new_window(gpointer data)
{
	Browser * browser = data;

	browser_new_copy(browser);
}


/* on_open_file */
void on_open_file(gpointer data)
{
	Browser * browser = data;

	browser_open(browser, NULL);
}


/* file menu */
/* on_file_new_window */
void on_file_new_window(gpointer data)
{
	on_new_window(data);
}


/* on_file_new_folder */
void on_file_new_folder(gpointer data)
{
	Browser * browser = data;
	char const * newfolder = _("New folder");
	char const * location;
	size_t len;
	char * path;

	if((location = browser_get_location(browser)) == NULL)
		return;
	len = strlen(location) + strlen(newfolder) + 2;
	if((path = malloc(len)) == NULL)
	{
		browser_error(browser, strerror(errno), 1);
		return;
	}
	snprintf(path, len, "%s/%s", location, newfolder);
	if(mkdir(path, 0777) != 0)
		browser_error(browser, strerror(errno), 1);
	free(path);
}


/* on_file_new_symlink */
void on_file_new_symlink(gpointer data)
{
	Browser * browser = data;
	char const * location;

	if((location = browser_get_location(browser)) == NULL)
		return;
	if(_common_symlink(browser->window, location) != 0)
		browser_error(browser, strerror(errno), 1);
}


/* on_file_close */
void on_file_close(gpointer data)
{
	on_closex(data);
}


/* on_file_open_file */
void on_file_open_file(gpointer data)
{
	on_open_file(data);
}


/* edit menu */
/* on_edit_copy */
void on_edit_copy(gpointer data)
{
	on_copy(data);
}


/* on_edit_cut */
void on_edit_cut(gpointer data)
{
	on_cut(data);
}


/* on_edit_delete */
void on_edit_delete(gpointer data)
{
	Browser * browser = data;
	GtkWidget * dialog;
	unsigned long cnt = 0;
	int res = GTK_RESPONSE_YES;
	GList * selection;
	GList * p;

	if((selection = browser_selection_copy(browser)) == NULL)
		return;
	for(p = selection; p != NULL; p = p->next)
		if(p->data != NULL)
			cnt++;
	if(cnt == 0)
		return;
	if(browser->prefs.confirm_before_delete == TRUE)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(browser->window),
				GTK_DIALOG_MODAL
				| GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
#if GTK_CHECK_VERSION(2, 6, 0)
				"%s", _("Warning"));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(
					dialog),
#endif
				_("Are you sure you want to delete %lu"
					" file(s)?"), cnt);
		gtk_window_set_title(GTK_WINDOW(dialog), _("Warning"));
		res = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
	if(res == GTK_RESPONSE_YES
			&& _common_exec("delete", "-ir", selection) != 0)
		browser_error(browser, strerror(errno), 1);
	g_list_foreach(selection, (GFunc)free, NULL);
	g_list_free(selection);
}


/* on_edit_paste */
void on_edit_paste(gpointer data)
{
	on_paste(data);
}


/* on_edit_select_all */
void on_edit_select_all(gpointer data)
{
	Browser * browser = data;

	browser_select_all(browser);
}


/* on_edit_preferences */
void on_edit_preferences(gpointer data)
{
	Browser * browser = data;

	browser_show_preferences(browser);
}


/* on_edit_unselect_all */
void on_edit_unselect_all(gpointer data)
{
	Browser * browser = data;

	browser_unselect_all(browser);
}


/* view menu */
/* on_view_home */
void on_view_home(gpointer data)
{
	on_home(data);
}


#if GTK_CHECK_VERSION(2, 6, 0)
/* on_view_details */
void on_view_details(gpointer data)
{
	Browser * browser = data;

	browser_set_view(browser, BV_DETAILS);
}


/* on_view_icons */
void on_view_icons(gpointer data)
{
	Browser * browser = data;

	browser_set_view(browser, BV_ICONS);
}


/* on_view_list */
void on_view_list(gpointer data)
{
	Browser * browser = data;

	browser_set_view(browser, BV_LIST);
}


/* on_view_thumbnails */
void on_view_thumbnails(gpointer data)
{
	Browser * browser = data;

	browser_set_view(browser, BV_THUMBNAILS);
}
#endif /* GTK_CHECK_VERSION(2, 6, 0) */


/* help menu */
/* on_help_about */
void on_help_about(gpointer data)
{
	Browser * browser = data;

	browser_about(browser);
}


/* on_help_contents */
void on_help_contents(gpointer data)
{
	desktop_help_contents(PACKAGE, PROGNAME);
}


/* toolbar */
/* on_back */
void on_back(gpointer data)
{
	Browser * browser = data;

	browser_go_back(browser);
}


/* on_copy */
void on_copy(gpointer data)
{
	Browser * browser = data;
	GtkWidget * entry;

	entry = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	if(gtk_window_get_focus(GTK_WINDOW(browser->window)) == entry)
	{
		gtk_editable_copy_clipboard(GTK_EDITABLE(entry));
		return;
	}
	g_list_foreach(browser->selection, (GFunc)free, NULL);
	g_list_free(browser->selection);
	browser->selection = browser_selection_copy(browser);
	browser->selection_cut = 0;
}


/* on_cut */
void on_cut(gpointer data)
{
	Browser * browser = data;
	GtkWidget * entry;

	entry = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	if(gtk_window_get_focus(GTK_WINDOW(browser->window)) == entry)
	{
		gtk_editable_cut_clipboard(GTK_EDITABLE(entry));
		return;
	}
	g_list_foreach(browser->selection, (GFunc)free, NULL);
	g_list_free(browser->selection);
	browser->selection = browser_selection_copy(browser);
	browser->selection_cut = 1;
}


/* on_forward */
void on_forward(gpointer data)
{
	Browser * browser = data;

	browser_go_forward(browser);
}


/* on_home */
void on_home(gpointer data)
{
	Browser * browser = data;

	browser_go_home(browser);
}


/* on_paste */
void on_paste(gpointer data)
{
	Browser * browser = data;
	GtkWidget * entry;

	entry = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	if(gtk_window_get_focus(GTK_WINDOW(browser->window)) == entry)
	{
		gtk_editable_paste_clipboard(GTK_EDITABLE(entry));
		return;
	}
	browser_selection_paste(browser);
}


/* properties */
/* on_properties */
void on_properties(gpointer data)
{
	Browser * browser = data;
	char const * location;
	char * p;
	GList * selection;

	if((location = browser_get_location(browser)) == NULL)
		return;
	if((selection = browser_selection_copy(browser)) == NULL)
	{
		if((p = strdup(location)) == NULL)
		{
			browser_error(browser, strerror(errno), 1);
			return;
		}
		selection = g_list_append(NULL, p);
	}
	if(_common_exec("properties", NULL, selection) != 0)
		browser_error(browser, strerror(errno), 1);
	g_list_foreach(selection, (GFunc)free, NULL);
	g_list_free(selection);
}


/* on_refresh */
void on_refresh(gpointer data)
{
	Browser * browser = data;

	browser_refresh(browser);
}


/* on_updir */
void on_updir(gpointer data)
{
	Browser * browser = data;
	char const * location;
	char * dir;

	if((location = browser_get_location(browser)) == NULL)
		return;
	dir = g_path_get_dirname(location);
	browser_set_location(browser, dir);
	g_free(dir);
}


#if GTK_CHECK_VERSION(2, 6, 0)
/* on_view_as */
void on_view_as(gpointer data)
{
	Browser * browser = data;
	BrowserView view;

	view = browser_get_view(browser);
	switch(view)
	{
		case BV_DETAILS:
			browser_set_view(browser, BV_ICONS);
			break;
		case BV_LIST:
			browser_set_view(browser, BV_THUMBNAILS);
			break;
		case BV_ICONS:
			browser_set_view(browser, BV_LIST);
			break;
		case BV_THUMBNAILS:
			browser_set_view(browser, BV_DETAILS);
			break;
	}
}
#endif


/* address bar */
/* on_path_activate */
void on_path_activate(gpointer data)
{
	Browser * browser = data;
	GtkWidget * widget;
	gchar const * p;

	widget = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	p = gtk_entry_get_text(GTK_ENTRY(widget));
	browser_set_location(browser, p);
}
