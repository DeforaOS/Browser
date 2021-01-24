/* $Id$ */
/* Copyright (c) 2015-2021 Pierre Pronchery <khorben@defora.org> */
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



#include <stdlib.h>
#include <libintl.h>
#include <gdk/gdkkeysyms.h>
#include <System.h>
#include <Desktop.h>
#include "callbacks.h"
#include "window.h"
#include "../../config.h"
#define _(string) gettext(string)
#define N_(string) (string)

/* constants */
#ifndef PROGNAME_BROWSER
# define PROGNAME_BROWSER	"browser"
#endif


/* BrowserWindow */
/* private */
/* types */
struct _BrowserWindow
{
	/* internal */
	Browser * browser;

	/* widgets */
	GtkWidget * window;
	/* preferences */
	GtkWidget * pr_window;
#if GTK_CHECK_VERSION(2, 6, 0)
	GtkWidget * pr_view;
#endif
	GtkWidget * pr_alternate;
	GtkWidget * pr_confirm;
	GtkWidget * pr_sort;
	GtkWidget * pr_hidden;
	GtkListStore * pr_mime_store;
	GtkWidget * pr_mime_view;
	GtkListStore * pr_plugin_store;
	GtkWidget * pr_plugin_view;
	/* about */
	GtkWidget * ab_window;
};


/* prototypes */
/* callbacks */
static void _browserwindow_on_close(gpointer data);
static gboolean _browserwindow_on_closex(gpointer data);
static void _browserwindow_on_location(gpointer data);
static void _browserwindow_on_properties(gpointer data);
#ifdef EMBEDDED
static void _browserwindow_on_copy(gpointer data);
static void _browserwindow_on_cut(gpointer data);
static void _browserwindow_on_new_window(gpointer data);
static void _browserwindow_on_open_file(gpointer data);
static void _browserwindow_on_paste(gpointer data);
static void _browserwindow_on_refresh(gpointer data);
#endif

#ifndef EMBEDDED
/* file menu */
static void _browserwindow_on_file_close(gpointer data);
static void _browserwindow_on_file_new_window(gpointer data);
static void _browserwindow_on_file_new_folder(gpointer data);
static void _browserwindow_on_file_new_symlink(gpointer data);
static void _browserwindow_on_file_open_file(gpointer data);
static void _browserwindow_on_file_properties(gpointer data);

/* edit menu */
static void _browserwindow_on_edit_copy(gpointer data);
static void _browserwindow_on_edit_cut(gpointer data);
static void _browserwindow_on_edit_delete(gpointer data);
static void _browserwindow_on_edit_paste(gpointer data);
static void _browserwindow_on_edit_preferences(gpointer data);
static void _browserwindow_on_edit_select_all(gpointer data);
static void _browserwindow_on_edit_unselect_all(gpointer data);

/* help menu */
static void _browserwindow_on_help_about(gpointer data);
static void _browserwindow_on_help_contents(gpointer data);

/* view menu */
static void _browserwindow_on_view_home(gpointer data);
static void _browserwindow_on_view_refresh(gpointer data);
# if GTK_CHECK_VERSION(2, 6, 0)
static void _browserwindow_on_view_details(gpointer data);
static void _browserwindow_on_view_icons(gpointer data);
static void _browserwindow_on_view_list(gpointer data);
static void _browserwindow_on_view_thumbnails(gpointer data);
# endif
#endif


/* constants */
#define ICON_NAME		"system-file-manager"

static const DesktopAccel _browserwindow_accel[] =
{
	{ G_CALLBACK(_browserwindow_on_location), GDK_CONTROL_MASK, GDK_KEY_L },
	{ G_CALLBACK(_browserwindow_on_properties), GDK_MOD1_MASK,
		GDK_KEY_Return },
#ifdef EMBEDDED
	{ G_CALLBACK(_browserwindow_on_close), GDK_CONTROL_MASK, GDK_KEY_W },
	{ G_CALLBACK(_browserwindow_on_copy), GDK_CONTROL_MASK, GDK_KEY_C },
	{ G_CALLBACK(_browserwindow_on_cut), GDK_CONTROL_MASK, GDK_KEY_X },
	{ G_CALLBACK(_browserwindow_on_new_window), GDK_CONTROL_MASK,
		GDK_KEY_N },
	{ G_CALLBACK(_browserwindow_on_open_file), GDK_CONTROL_MASK, GDK_KEY_O },
	{ G_CALLBACK(_browserwindow_on_paste), GDK_CONTROL_MASK, GDK_KEY_V },
	{ G_CALLBACK(_browserwindow_on_refresh), GDK_CONTROL_MASK, GDK_KEY_R },
#endif
	{ NULL, 0, 0 }
};

#ifndef EMBEDDED
static const DesktopMenu _browserwindow_menu_file[] =
{
	{ N_("_New window"), G_CALLBACK(_browserwindow_on_file_new_window),
		"window-new", GDK_CONTROL_MASK, GDK_KEY_N },
	{ N_("New _folder"), G_CALLBACK(_browserwindow_on_file_new_folder),
		"folder-new", 0, 0 },
	{ N_("New _symbolic link..."), G_CALLBACK(
			_browserwindow_on_file_new_symlink), NULL, 0, 0 },
	{ N_("_Open file..."), G_CALLBACK(_browserwindow_on_file_open_file),
		NULL, GDK_CONTROL_MASK, GDK_KEY_O },
	{ "", NULL, NULL, 0, 0 },
	{ N_("Propert_ies"), G_CALLBACK(_browserwindow_on_file_properties),
		"document-properties", GDK_MOD1_MASK, GDK_KEY_Return },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Close"), G_CALLBACK(_browserwindow_on_file_close),
		"window-close", GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenu _browserwindow_menu_edit[] =
{
	{ N_("_Cut"), G_CALLBACK(_browserwindow_on_edit_cut), "edit-cut",
		GDK_CONTROL_MASK, GDK_KEY_X },
	{ N_("Cop_y"), G_CALLBACK(_browserwindow_on_edit_copy), "edit-copy",
		GDK_CONTROL_MASK, GDK_KEY_C },
	{ N_("_Paste"), G_CALLBACK(_browserwindow_on_edit_paste), "edit-paste",
		GDK_CONTROL_MASK, GDK_KEY_V },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Delete"), G_CALLBACK(_browserwindow_on_edit_delete),
		"edit-delete", 0, 0 },
	{ "", NULL, NULL, 0, 0 },
	{ N_("Select _All"), G_CALLBACK(_browserwindow_on_edit_select_all),
		"edit-select-all", GDK_CONTROL_MASK, GDK_KEY_A },
	{ N_("_Unselect all"), G_CALLBACK(_browserwindow_on_edit_unselect_all),
		NULL, 0, 0 },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Preferences"), G_CALLBACK(_browserwindow_on_edit_preferences),
		"gtk-preferences", GDK_CONTROL_MASK, GDK_KEY_P },
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenu _browserwindow_menu_view[] =
{
	{ N_("_Refresh"), G_CALLBACK(_browserwindow_on_view_refresh),
		"view-refresh", GDK_CONTROL_MASK, GDK_KEY_R },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Home"), G_CALLBACK(_browserwindow_on_view_home), "go-home",
		GDK_MOD1_MASK, GDK_KEY_Home },
#if GTK_CHECK_VERSION(2, 6, 0)
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Details"), G_CALLBACK(_browserwindow_on_view_details),
		"browser-view-details", 0, 0 },
	{ N_("_Icons"), G_CALLBACK(_browserwindow_on_view_icons),
		"browser-view-icons", 0, 0 },
	{ N_("_List"), G_CALLBACK(_browserwindow_on_view_list),
		"browser-view-list", 0, 0 },
	{ N_("_Thumbnails"), G_CALLBACK(_browserwindow_on_view_thumbnails),
		NULL, 0, 0 },
#endif
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenu _browserwindow_menu_help[] =
{
	{ N_("_Contents"), G_CALLBACK(_browserwindow_on_help_contents),
		"help-contents", 0, GDK_KEY_F1 },
	{ N_("_About"), G_CALLBACK(_browserwindow_on_help_about), "help-about",
		0, 0 },
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenubar _browserwindow_menubar[] =
{
	{ N_("_File"), _browserwindow_menu_file },
	{ N_("_Edit"), _browserwindow_menu_edit },
	{ N_("_View"), _browserwindow_menu_view },
	{ N_("_Help"), _browserwindow_menu_help },
	{ NULL, NULL }
};
#endif


/* variables */
unsigned int browser_window_cnt = 0;


/* public */
/* functions */
/* browserwindow_new */
BrowserWindow * browserwindow_new(String const * directory)
{
	BrowserWindow * browser;
	GtkAccelGroup * group;
	GtkWidget * vbox;
#ifndef EMBEDDED
	GtkWidget * tb_menubar;
#endif

	if((browser = object_new(sizeof(*browser))) == NULL)
	{
		browser_error(NULL, (directory != NULL) ? directory : ".", 1);
		return NULL;
	}
	browser->window = NULL;

	/* window */
	group = gtk_accel_group_new();
	browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_add_accel_group(GTK_WINDOW(browser->window), group);
	g_object_unref(group);
	gtk_window_set_default_size(GTK_WINDOW(browser->window), 720, 480);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_icon_name(GTK_WINDOW(browser->window), ICON_NAME);
#endif
	gtk_window_set_title(GTK_WINDOW(browser->window), _("File manager"));
	g_signal_connect_swapped(browser->window, "delete-event", G_CALLBACK(
				_browserwindow_on_closex), browser);

	/* browser */
	browser_window_cnt++;
	if((browser->browser = browser_new(browser->window, group, directory))
			== NULL)
	{
		browserwindow_delete(browser);
		return NULL;
	}

	/* widgets */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	/* menubar */
#ifndef EMBEDDED
	tb_menubar = desktop_menubar_create(_browserwindow_menubar, browser,
			group);
	gtk_box_pack_start(GTK_BOX(vbox), tb_menubar, FALSE, FALSE, 0);
#endif
	desktop_accel_create(_browserwindow_accel, browser, group);
	gtk_box_pack_start(GTK_BOX(vbox), browser_get_widget(browser->browser),
			TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(browser->window), vbox);
	gtk_widget_show_all(browser->window);
	return browser;
}


/* browserwindow_delete */
void browserwindow_delete(BrowserWindow * browser)
{
	if(browser->browser != NULL)
		browser_delete(browser->browser);
	gtk_widget_destroy(browser->window);
	object_delete(browser);
	browser_window_cnt--;
}


/* accessors */
/* browserwindow_get_browser */
Browser * browserwindow_get_browser(BrowserWindow * browser)
{
	return browser->browser;
}


/* useful */
/* interface */
/* browserwindow_show_about */
void browserwindow_about(BrowserWindow * browser, gboolean show)
{
	browser_show_about(browser->browser, show);
}


/* browserwindow_show_preferences */
void browserwindow_show_preferences(BrowserWindow * browser, gboolean show)
{
	browser_show_preferences(browser->browser, show);
}


/* private */
/* functions */
/* callbacks */
/* browserwindow_on_close */
static void _browserwindow_on_close(gpointer data)
{
	BrowserWindow * browser = data;

	browserwindow_delete(browser);
	if(browser_window_cnt == 0)
		gtk_main_quit();
}


/* browserwindow_on_closex */
static gboolean _browserwindow_on_closex(gpointer data)
{
	BrowserWindow * browser = data;

	_browserwindow_on_close(browser);
	return TRUE;
}


/* browserwindow_on_location */
static void _browserwindow_on_location(gpointer data)
{
	BrowserWindow * browser = data;

	on_location(browser->browser);
}


/* browserwindow_on_properties */
static void _browserwindow_on_properties(gpointer data)
{
	BrowserWindow * browser = data;

	on_properties(browser->browser);
}


#ifdef EMBEDDED
/* browserwindow_on_copy */
static void _browserwindow_on_copy(gpointer data)
{
	BrowserWindow * browser = data;

	on_copy(browser->browser);
}


/* browserwindow_on_cut */
static void _browserwindow_on_cut(gpointer data)
{
	BrowserWindow * browser = data;

	on_cut(browser->browser);
}


/* browserwindow_on_new_window */
static void _browserwindow_on_new_window(gpointer data)
{
	BrowserWindow * browser = data;

	on_new_window(browser->browser);
}


/* browserwindow_on_open_file */
static void _browserwindow_on_open_file(gpointer data)
{
	BrowserWindow * browser = data;

	on_open_file(browser->browser);
}


/* browserwindow_on_paste */
static void _browserwindow_on_paste(gpointer data)
{
	BrowserWindow * browser = data;

	on_paste(browser->browser);
}


/* browserwindow_on_refresh */
static void _browserwindow_on_refresh(gpointer data)
{
	BrowserWindow * browser = data;

	on_refresh(browser->browser);
}
#endif


#ifndef EMBEDDED
/* file menu */
/* browserwindow_on_file_close */
static void _browserwindow_on_file_close(gpointer data)
{
	BrowserWindow * browser = data;

	_browserwindow_on_close(browser);
}


/* browserwindow_on_file_new_window */
static void _browserwindow_on_file_new_window(gpointer data)
{
	BrowserWindow * browser = data;

	on_new_window(browser->browser);
}


/* browserwindow_on_file_new_folder */
static void _browserwindow_on_file_new_folder(gpointer data)
{
	BrowserWindow * browser = data;

	on_new_folder(browser->browser);
}


/* browserwindow_on_file_new_symlink */
static void _browserwindow_on_file_new_symlink(gpointer data)
{
	BrowserWindow * browser = data;

	on_new_symlink(browser->browser);
}


/* browserwindow_on_file_open_file */
static void _browserwindow_on_file_open_file(gpointer data)
{
	BrowserWindow * browser = data;

	on_open_file(browser->browser);
}


/* browserwindow_on_file_properties */
static void _browserwindow_on_file_properties(gpointer data)
{
	BrowserWindow * browser = data;

	on_properties(browser->browser);
}


/* edit menu */
/* browserwindow_on_edit_copy */
static void _browserwindow_on_edit_copy(gpointer data)
{
	BrowserWindow * browser = data;

	browser_copy(browser->browser);
}


/* browserwindow_on_edit_cut */
static void _browserwindow_on_edit_cut(gpointer data)
{
	BrowserWindow * browser = data;

	browser_cut(browser->browser);
}


/* browserwindow_on_edit_delete */
static void _browserwindow_on_edit_delete(gpointer data)
{
	BrowserWindow * browser = data;

	browser_selection_delete(browser->browser);
}


/* browserwindow_on_edit_paste */
static void _browserwindow_on_edit_paste(gpointer data)
{
	BrowserWindow * browser = data;

	browser_paste(browser->browser);
}


/* browserwindow_on_edit_select_all */
static void _browserwindow_on_edit_select_all(gpointer data)
{
	BrowserWindow * browser = data;

	browser_select_all(browser->browser);
}


/* browserwindow_on_edit_preferences */
static void _browserwindow_on_edit_preferences(gpointer data)
{
	BrowserWindow * browser = data;

	browser_show_preferences(browser->browser, TRUE);
}


/* browserwindow_on_edit_unselect_all */
static void _browserwindow_on_edit_unselect_all(gpointer data)
{
	BrowserWindow * browser = data;

	browser_unselect_all(browser->browser);
}


/* view menu */
/* browserwindow_on_view_home */
static void _browserwindow_on_view_home(gpointer data)
{
	BrowserWindow * browser = data;

	on_home(browser->browser);
}


/* browserwindow_on_view_refresh */
static void _browserwindow_on_view_refresh(gpointer data)
{
	BrowserWindow * browser = data;

	browser_refresh(browser->browser);
}


# if GTK_CHECK_VERSION(2, 6, 0)
/* browserwindow_on_view_details */
static void _browserwindow_on_view_details(gpointer data)
{
	BrowserWindow * browser = data;

	browser_set_view(browser->browser, BV_DETAILS);
}


/* browserwindow_on_view_icons */
static void _browserwindow_on_view_icons(gpointer data)
{
	BrowserWindow * browser = data;

	browser_set_view(browser->browser, BV_ICONS);
}


/* browserwindow_on_view_list */
static void _browserwindow_on_view_list(gpointer data)
{
	BrowserWindow * browser = data;

	browser_set_view(browser->browser, BV_LIST);
}


/* browserwindow_on_view_thumbnails */
static void _browserwindow_on_view_thumbnails(gpointer data)
{
	BrowserWindow * browser = data;

	browser_set_view(browser->browser, BV_THUMBNAILS);
}
# endif /* GTK_CHECK_VERSION(2, 6, 0) */


/* help menu */
/* browserwindow_on_help_about */
static void _browserwindow_on_help_about(gpointer data)
{
	BrowserWindow * browser = data;

	browser_show_about(browser->browser, TRUE);
}


/* browserwindow_on_help_contents */
static void _browserwindow_on_help_contents(gpointer data)
{
	(void) data;

	desktop_help_contents(PACKAGE, PROGNAME_BROWSER);
}
#endif
