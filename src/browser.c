/* $Id$ */
static char const _copyright[] =
"Copyright © 2006-2015 Pierre Pronchery <khorben@defora.org>";
/* This file is part of DeforaOS Desktop Browser */
static char const _license[] =
"This program is free software: you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation, version 3 of the License.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program.  If not, see <http://www.gnu.org/licenses/>.";
/* TODO:
 * - re-implement MIME preferences
 * - use the friendly-name for MIME types in the browser view
 * - allow plug-ins to be re-ordered */



#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <libintl.h>
#include <gdk/gdkkeysyms.h>
#include <Desktop.h>
#include "callbacks.h"
#include "window.h"
#include "browser.h"
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) (string)

#define COMMON_CONFIG_FILENAME
#define COMMON_DND
#define COMMON_EXEC
#define COMMON_GET_ABSOLUTE_PATH
#define COMMON_SIZE
#include "common.c"

/* constants */
#ifndef PROGNAME
# define PROGNAME	"browser"
#endif
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef BINDIR
# define BINDIR		PREFIX "/bin"
#endif
#ifndef LIBDIR
# define LIBDIR		PREFIX "/lib"
#endif

#define IDLE_LOOP_ICON_CNT	16	/* number of icons added in a loop */


/* Browser */
/* private */
/* types */
typedef enum _BrowserColumn
{
	BC_UPDATED = 0,
	BC_PATH,
	BC_DISPLAY_NAME,
	BC_PIXBUF_24,
# if GTK_CHECK_VERSION(2, 6, 0)
	BC_PIXBUF_48,
	BC_PIXBUF_96,
# endif
	BC_INODE,
	BC_IS_DIRECTORY,
	BC_IS_EXECUTABLE,
	BC_IS_MOUNT_POINT,
	BC_SIZE,
	BC_DISPLAY_SIZE,
	BC_OWNER,
	BC_GROUP,
	BC_DATE,
	BC_DISPLAY_DATE,
	BC_MIME_TYPE
} BrowserColumn;
# define BC_LAST BC_MIME_TYPE
# define BC_COUNT (BC_LAST + 1)

typedef enum _BrowserMimeColumn
{
	BMC_ICON,
	BMC_NAME
} BrowserMimeColumn;
#define BMC_LAST BMC_NAME
#define BMC_COUNT (BMC_LAST + 1)

typedef enum _BrowserPluginColumn
{
	BPC_NAME = 0,
	BPC_ENABLED,
	BPC_ICON,
	BPC_NAME_DISPLAY,
	BPC_PLUGIN,
	BPC_BROWSERPLUGINDEFINITION,
	BPC_BROWSERPLUGIN,
	BPC_WIDGET
} BrowserPluginColumn;
#define BPC_LAST BPC_WIDGET
#define BPC_COUNT (BPC_LAST + 1)

struct _Browser
{
	guint source;

	/* config */
	Config * config;
	BrowserPrefs prefs;

	/* mime */
	Mime * mime;

	/* history */
	GList * history;
	GList * current;

	/* refresh */
	guint refresh_id;
	DIR * refresh_dir;
	dev_t refresh_dev;
	ino_t refresh_ino;
	time_t refresh_mti;
	unsigned int refresh_cnt;
	unsigned int refresh_hid;
	GtkTreeIter refresh_iter;

	/* selection */
	GList * selection;
	gboolean selection_cut;

	/* helper */
	BrowserPluginHelper pl_helper;

	/* widgets */
	GtkIconTheme * theme;
	GtkWidget * window;
	GtkWidget * widget;
#if GTK_CHECK_VERSION(2, 6, 0)
	GdkPixbuf * loading;
#endif
#if GTK_CHECK_VERSION(2, 18, 0)
	GtkWidget * infobar;
	GtkWidget * infobar_label;
#endif
	GtkToolItem * tb_back;
	GtkToolItem * tb_updir;
	GtkToolItem * tb_forward;
	GtkWidget * tb_path;
	GtkWidget * scrolled;
	GtkWidget * detailview;
#if GTK_CHECK_VERSION(2, 6, 0)
	GtkWidget * iconview;
	BrowserView view;
#endif
	GtkListStore * store;
	GtkWidget * statusbar;
	guint statusbar_id;
	/* plug-ins */
	GtkWidget * pl_view;
	GtkListStore * pl_store;
	GtkWidget * pl_combo;
	GtkWidget * pl_box;
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


/* constants */
static char const * _authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};

/* toolbar */
static DesktopToolbar _browser_toolbar[] =
{
	{ N_("Back"), G_CALLBACK(on_back), GTK_STOCK_GO_BACK, GDK_MOD1_MASK,
		GDK_KEY_Left, NULL },
	{ N_("Up"), G_CALLBACK(on_updir), GTK_STOCK_GO_UP, 0, 0, NULL },
	{ N_("Forward"), G_CALLBACK(on_forward), GTK_STOCK_GO_FORWARD,
		GDK_MOD1_MASK, GDK_KEY_Right, NULL },
	{ N_("Refresh"), G_CALLBACK(on_refresh), GTK_STOCK_REFRESH, 0, 0,
		NULL },
	{ "", NULL, NULL, 0, 0, NULL },
	{ N_("Home"), G_CALLBACK(on_home), GTK_STOCK_HOME, 0, 0, NULL },
	{ "", NULL, NULL, 0, 0, NULL },
	{ N_("Cut"), G_CALLBACK(on_cut), GTK_STOCK_CUT, 0, 0, NULL },
	{ N_("Copy"), G_CALLBACK(on_copy), GTK_STOCK_COPY, 0, 0, NULL },
	{ N_("Paste"), G_CALLBACK(on_paste), GTK_STOCK_PASTE, 0, 0, NULL },
	{ "", NULL, NULL, 0, 0, NULL },
	{ N_("Properties"), G_CALLBACK(on_properties), GTK_STOCK_PROPERTIES, 0,
		0, NULL },
	{ NULL, NULL, NULL, 0, 0, NULL }
};


/* prototypes */
/* accessors */
static char const * _browser_config_get(Browser * browser, char const * section,
		char const * variable);
static int _browser_config_set(Browser * browser, char const * section,
		char const * variable, char const * value);
static gboolean _browser_plugin_is_enabled(Browser * browser,
		char const * plugin);
static GdkPixbuf * _browser_get_icon(Browser * browser, char const * filename,
		char const * type, struct stat * lst, struct stat * st,
		int size);
static Mime * _browser_get_mime(Browser * browser);
static GList * _browser_get_selection(Browser * browser);
static char const * _browser_get_type(Browser * browser, char const * filename,
		mode_t mode);
static void _browser_set_status(Browser * browser, char const * status);

/* useful */
static void _browser_plugin_refresh(Browser * browser);
static void _browser_refresh_do(Browser * browser, DIR * dir, struct stat * st);

static int _config_load_boolean(Config * config, char const * variable,
		gboolean * value);
static int _config_load_string(Config * config, char const * variable,
		char ** value);
static int _config_save_boolean(Config * config, char const * variable,
		gboolean value);

/* callbacks */
static void _browser_on_plugin_combo_change(gpointer data);
static void _browser_on_selection_changed(gpointer data);


/* public */
/* functions */
/* browser_new */
static gboolean _new_idle(gpointer data);
static void _idle_load_plugins(Browser * browser);
static GtkListStore * _create_store(Browser * browser);

Browser * browser_new(GtkWidget * window, GtkAccelGroup * group,
		String const * directory)
{
	Browser * browser;
	GtkWidget * vbox;
	GtkWidget * toolbar;
	GtkWidget * widget;
	GtkToolItem * toolitem;
	GtkWidget * hpaned;
	GtkCellRenderer * renderer;
#if GTK_CHECK_VERSION(2, 6, 0)
	GtkWidget * menu;
	GtkWidget * menuitem;
#endif
	char * p;

	if((browser = malloc(sizeof(*browser))) == NULL)
	{
		browser_error(NULL, (directory != NULL) ? directory : ".", 1);
		return NULL;
	}
	browser->source = 0;
	browser->window = NULL;
	browser->theme = gtk_icon_theme_get_default();
#if GTK_CHECK_VERSION(2, 6, 0)
	/* XXX ignore errors */
	browser->loading = gtk_icon_theme_load_icon(browser->theme,
			"image-loading", 96,
			GTK_ICON_LOOKUP_GENERIC_FALLBACK
			| GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
#endif

	/* config */
	/* set defaults */
#if GTK_CHECK_VERSION(2, 6, 0)
	browser->prefs.default_view = BV_ICONS;
#endif
	browser->prefs.alternate_rows = TRUE;
	browser->prefs.confirm_before_delete = TRUE;
	browser->prefs.sort_folders_first = TRUE;
	browser->prefs.show_hidden_files = FALSE;
	if((browser->config = config_new()) == NULL
			|| browser_config_load(browser) != 0)
		browser_error(browser, _("Error while loading configuration"),
				1);

	/* mime */
	browser->mime = mime_new(NULL); /* FIXME share MIME instances */

	/* history */
	browser->history = NULL;
	browser->current = NULL;

	/* refresh */
	browser->refresh_id = 0;
	browser->refresh_dir = NULL;
	browser->refresh_dev = 0;

	/* selection */
	browser->selection = NULL;
	browser->selection_cut = 0;

	/* plug-ins */
	browser->pl_helper.browser = browser;
	browser->pl_helper.config_get = _browser_config_get;
	browser->pl_helper.config_set = _browser_config_set;
	browser->pl_helper.error = browser_error;
	browser->pl_helper.get_icon = _browser_get_icon;
	browser->pl_helper.get_mime = _browser_get_mime;
	browser->pl_helper.get_type = _browser_get_type;
	browser->pl_helper.refresh = browser_refresh;
	browser->pl_helper.set_location = browser_set_location;

	/* widgets */
	browser->window = window;
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	vbox = gtk_vbox_new(FALSE, 0);
#endif
	browser->widget = vbox;
	/* toolbar */
	toolbar = desktop_toolbar_create(_browser_toolbar, browser, group);
	browser->tb_back = _browser_toolbar[0].widget;
	browser->tb_updir = _browser_toolbar[1].widget;
	browser->tb_forward = _browser_toolbar[2].widget;
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_back), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_updir), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_forward), FALSE);
#if GTK_CHECK_VERSION(2, 6, 0)
	toolitem = gtk_menu_tool_button_new(NULL, _("View as..."));
	g_signal_connect_swapped(toolitem, "clicked", G_CALLBACK(on_view_as),
			browser);
	menu = gtk_menu_new();
	menuitem = gtk_image_menu_item_new_with_label(_("Details"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("browser-view-details",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuitem, "activate",
			G_CALLBACK(on_view_details), browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_label(_("Icons"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("browser-view-icons",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuitem, "activate",
			G_CALLBACK(on_view_icons), browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_label(_("List"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("browser-view-list",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(on_view_list),
			browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_menu_item_new_with_label(_("Thumbnails"));
	g_signal_connect_swapped(menuitem, "activate",
			G_CALLBACK(on_view_thumbnails), browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolitem), menu);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
#endif
#ifdef EMBEDDED
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_PREFERENCES);
	g_signal_connect_swapped(toolitem, "clicked",
			G_CALLBACK(on_preferences), browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
	/* toolbar */
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
			GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
#ifndef EMBEDDED
	widget = gtk_label_new(_(" Location: "));
	toolitem = gtk_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolitem), widget);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
#endif
#if GTK_CHECK_VERSION(2, 24, 0)
	browser->tb_path = gtk_combo_box_text_new_with_entry();
#else
	browser->tb_path = gtk_combo_box_entry_new_text();
#endif
	widget = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	if(directory != NULL)
		gtk_entry_set_text(GTK_ENTRY(widget), directory);
	g_signal_connect_swapped(widget, "activate",
			G_CALLBACK(on_path_activate), browser);
	toolitem = gtk_tool_item_new();
	gtk_tool_item_set_expand(toolitem, TRUE);
	gtk_container_add(GTK_CONTAINER(toolitem), browser->tb_path);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_JUMP_TO);
	g_signal_connect_swapped(toolitem, "clicked",
			G_CALLBACK(on_path_activate), browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 18, 0)
	/* infobar */
	browser->infobar = gtk_info_bar_new_with_buttons(GTK_STOCK_CLOSE,
			GTK_RESPONSE_CLOSE, NULL);
	gtk_info_bar_set_message_type(GTK_INFO_BAR(browser->infobar),
			GTK_MESSAGE_ERROR);
	g_signal_connect(browser->infobar, "close", G_CALLBACK(gtk_widget_hide),
			NULL);
	g_signal_connect(browser->infobar, "response", G_CALLBACK(
				gtk_widget_hide), NULL);
	widget = gtk_info_bar_get_content_area(GTK_INFO_BAR(browser->infobar));
	browser->infobar_label = gtk_label_new(NULL);
	gtk_widget_show(browser->infobar_label);
	gtk_box_pack_start(GTK_BOX(widget), browser->infobar_label, TRUE, TRUE,
			0);
	gtk_widget_set_no_show_all(browser->infobar, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), browser->infobar, FALSE, TRUE, 0);
#endif
	/* paned */
#if GTK_CHECK_VERSION(3, 0, 0)
	hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
#else
	hpaned = gtk_hpaned_new();
#endif
	gtk_paned_set_position(GTK_PANED(hpaned), 200);
	/* plug-ins */
#if GTK_CHECK_VERSION(3, 0, 0)
	browser->pl_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	browser->pl_view = gtk_vbox_new(FALSE, 4);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(browser->pl_view), 4);
	browser->pl_store = gtk_list_store_new(BPC_COUNT, G_TYPE_STRING,
			G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_STRING,
			G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER,
			G_TYPE_POINTER);
	browser->pl_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(
				browser->pl_store));
	g_signal_connect_swapped(browser->pl_combo, "changed",
			G_CALLBACK(_browser_on_plugin_combo_change), browser);
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(browser->pl_combo),
			renderer, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(browser->pl_combo),
			renderer, "pixbuf", BPC_ICON, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(browser->pl_combo),
			renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(browser->pl_combo),
			renderer, "text", BPC_NAME_DISPLAY, NULL);
	gtk_box_pack_start(GTK_BOX(browser->pl_view), browser->pl_combo, FALSE,
			TRUE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	browser->pl_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	browser->pl_box = gtk_vbox_new(FALSE, 4);
#endif
	gtk_box_pack_start(GTK_BOX(browser->pl_view), browser->pl_box, TRUE,
			TRUE, 0);
	gtk_paned_add1(GTK_PANED(hpaned), browser->pl_view);
	gtk_widget_set_no_show_all(browser->pl_view, TRUE);
	/* view */
	browser->scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(browser->scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_paned_add2(GTK_PANED(hpaned), browser->scrolled);
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
	/* statusbar */
	browser->statusbar = gtk_statusbar_new();
	browser->statusbar_id = 0;
	gtk_box_pack_start(GTK_BOX(vbox), browser->statusbar, FALSE, TRUE, 0);
	/* store */
	browser->store = _create_store(browser);
	browser->detailview = NULL;
#if GTK_CHECK_VERSION(2, 6, 0)
	browser->iconview = NULL;
	browser->view = browser->prefs.default_view;
	browser_set_view(browser, browser->view);
	if(browser->iconview != NULL)
		gtk_widget_grab_focus(browser->iconview);
#else
	browser_set_view(browser, BV_DETAILS);
	gtk_widget_grab_focus(browser->detailview);
#endif

	/* preferences */
	browser->pr_window = NULL;

	/* about */
	browser->ab_window = NULL;

	/* open directory */
	if(directory != NULL && (p = strdup(directory)) != NULL)
	{
		browser->history = g_list_append(browser->history, p);
		browser->current = browser->history;
	}
	browser->source = g_idle_add(_new_idle, browser);

	gtk_widget_show_all(browser->widget);
	return browser;
}

static gboolean _new_idle(gpointer data)
{
	Browser * browser = data;
	char const * location;

	browser->source = 0;
	_idle_load_plugins(browser);
	if((location = browser_get_location(browser)) == NULL)
		browser_go_home(browser);
	else
		browser_set_location(browser, location);
	return FALSE;
}

static void _idle_load_plugins(Browser * browser)
{
	char const * plugins;
	char * p;
	char * q;
	size_t i;

	if((plugins = config_get(browser->config, NULL, "plugins")) == NULL
			|| strlen(plugins) == 0)
		return;
	if((p = strdup(plugins)) == NULL)
		return; /* XXX report error */
	for(q = p, i = 0;;)
	{
		if(q[i] == '\0')
		{
			browser_load(browser, q);
			break;
		}
		if(q[i++] != ',')
			continue;
		q[i - 1] = '\0';
		browser_load(browser, q);
		q += i;
		i = 0;
	}
	free(p);
}

static int _sort_func(GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b,
		gpointer data)
{
	Browser * browser = data;
	gboolean is_dir_a;
	gboolean is_dir_b;
	gchar * name_a;
	gchar * name_b;
	int ret = 0;

	gtk_tree_model_get(model, a, BC_IS_DIRECTORY, &is_dir_a,
			BC_DISPLAY_NAME, &name_a, -1);
	gtk_tree_model_get(model, b, BC_IS_DIRECTORY, &is_dir_b,
			BC_DISPLAY_NAME, &name_b, -1);
	if(browser->prefs.sort_folders_first)
	{
		if(!is_dir_a && is_dir_b)
			ret = 1;
		else if(is_dir_a && !is_dir_b)
			ret = -1;
	}
	if(ret == 0)
		ret = g_utf8_collate(name_a, name_b);
	g_free(name_a);
	g_free(name_b);
	return ret;
}

static GtkListStore * _create_store(Browser * browser)
{
	GtkListStore * store;

	store = gtk_list_store_new(BC_COUNT,
			G_TYPE_BOOLEAN,			/* BC_UPDATED */
			G_TYPE_STRING,			/* BC_PATH */
			G_TYPE_STRING,			/* BC_DISPLAY_NAME */
			GDK_TYPE_PIXBUF,		/* BC_PIXBUF_24 */
#if GTK_CHECK_VERSION(2, 6, 0)
			GDK_TYPE_PIXBUF,		/* BC_PIXBUF_48 */
			GDK_TYPE_PIXBUF,		/* BC_PIXBUF_96 */
#endif
			G_TYPE_UINT64,			/* BC_INODE */
			G_TYPE_BOOLEAN,			/* BC_IS_DIRECTORY */
			G_TYPE_BOOLEAN,			/* BC_IS_EXECUTABLE */
			G_TYPE_BOOLEAN,			/* BC_IS_MOUNT_POINT */
			G_TYPE_UINT64,			/* BC_SIZE */
			G_TYPE_STRING,			/* BC_DISPLAY_SIZE */
			G_TYPE_STRING,			/* BC_OWNER */
			G_TYPE_STRING,			/* BC_GROUP */
			G_TYPE_UINT64,			/* BC_DATE */
			G_TYPE_STRING,			/* BC_DISPLAY_DATE */
			G_TYPE_STRING);			/* BC_MIME_TYPE */
	gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(store),
			_sort_func, browser, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING); /* FIXME make it an option */
	return store;
}


/* browser_new_copy */
Browser * browser_new_copy(Browser * browser)
{
	BrowserWindow * window;
	char const * location;

	if(browser == NULL)
	{
		if((window = browserwindow_new(NULL)) == NULL)
			return NULL;
		return browserwindow_get_browser(window);
	}
	location = browser_get_location(browser);
	if((window = browserwindow_new(location)) == NULL)
		return NULL;
	return browserwindow_get_browser(window);
}


/* browser_delete */
static void _delete_plugins(Browser * browser);

void browser_delete(Browser * browser)
{
	if(browser->source != 0)
		g_source_remove(browser->source);
	_delete_plugins(browser);
	if(browser->config != NULL)
		config_delete(browser->config);
	if(browser->refresh_id)
		g_source_remove(browser->refresh_id);
	g_list_foreach(browser->history, (GFunc)free, NULL);
	g_list_free(browser->history);
	g_list_foreach(browser->selection, (GFunc)free, NULL);
	g_list_free(browser->selection);
	if(browser->detailview != NULL)
		g_object_unref(browser->detailview);
#if GTK_CHECK_VERSION(2, 6, 0)
	if(browser->iconview != NULL)
		g_object_unref(browser->iconview);
	if(browser->loading != NULL)
		g_object_unref(browser->loading);
#endif
	g_object_unref(browser->store);
	free(browser);
}

static void _delete_plugins(Browser * browser)
{
	GtkTreeModel * model = GTK_TREE_MODEL(browser->pl_store);
	GtkTreeIter iter;
	gboolean valid;
	BrowserPluginDefinition * bpd;
	BrowserPlugin * bp;
	Plugin * plugin;

	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, BPC_PLUGIN, &plugin,
				BPC_BROWSERPLUGINDEFINITION, &bpd,
				BPC_BROWSERPLUGIN, &bp, -1);
		if(bpd->destroy != NULL)
			bpd->destroy(bp);
		plugin_delete(plugin);
	}
}


/* useful */
/* browser_error */
static int _browser_error(char const * message, int ret);
/* callbacks */
#if !GTK_CHECK_VERSION(2, 18, 0)
static void _error_response(gpointer data);
#endif

int browser_error(Browser * browser, char const * message, int ret)
{
#if !GTK_CHECK_VERSION(2, 18, 0)
	GtkWidget * dialog;
#endif

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %d)\n", __func__, message, ret);
#endif
	if(browser == NULL)
		return _browser_error(message, ret);
#if GTK_CHECK_VERSION(2, 18, 0)
	gtk_label_set_text(GTK_LABEL(browser->infobar_label), message);
	gtk_widget_show(browser->infobar);
#else
	dialog = gtk_message_dialog_new((browser->window == NULL)
			? GTK_WINDOW(browser->window) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
# if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
# endif
			"%s", message);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	if(ret < 0)
	{
		g_signal_connect_swapped(dialog, "response",
				G_CALLBACK(_error_response), browser);
		ret = -ret;
	}
	else
		g_signal_connect(dialog, "response", G_CALLBACK(
					gtk_widget_destroy), NULL);
	gtk_widget_show(dialog);
#endif
	return ret;
}

static int _browser_error(char const * message, int ret)
{
	fprintf(stderr, "%s: %s\n", PROGNAME, message);
	return ret;
}

#if !GTK_CHECK_VERSION(2, 18, 0)
static void _error_response(gpointer data)
{
	Browser * browser = data;

	if(browser_cnt > 0) /* XXX ugly */
		browser_delete(browser);
	if(browser_cnt == 0)
		gtk_main_quit();
}
#endif


/* browser_config_load */
int browser_config_load(Browser * browser)
{
	char * filename;
#if GTK_CHECK_VERSION(2, 6, 0)
	char * p = NULL;
#endif

	if(browser->config == NULL)
		return 0; /* XXX ignore error */
	if((filename = _common_config_filename(BROWSER_CONFIG_FILE)) == NULL)
		return -1;
	if(config_load(browser->config, filename) != 0)
		browser_error(NULL, error_get(NULL), 1);
	free(filename);
#if GTK_CHECK_VERSION(2, 6, 0)
	/* XXX deserves a rework (enum) */
	if(_config_load_string(browser->config, "default_view", &p) == 0
			&& p != NULL)
	{
		if(strcmp(p, "details") == 0)
			browser->prefs.default_view = BV_DETAILS;
		else if(strcmp(p, "icons") == 0)
			browser->prefs.default_view = BV_ICONS;
		else if(strcmp(p, "list") == 0)
			browser->prefs.default_view = BV_LIST;
		else if(strcmp(p, "thumbnails") == 0)
			browser->prefs.default_view = BV_THUMBNAILS;
		free(p);
	}
#endif
	_config_load_boolean(browser->config, "alternate_rows",
			&browser->prefs.alternate_rows);
	_config_load_boolean(browser->config, "confirm_before_delete",
			&browser->prefs.confirm_before_delete);
	_config_load_boolean(browser->config, "sort_folders_first",
			&browser->prefs.sort_folders_first);
	_config_load_boolean(browser->config, "show_hidden_files",
			&browser->prefs.show_hidden_files);
	return 0;
}


/* browser_config_save */
int browser_config_save(Browser * browser)
{
	int ret = 0;
	char * filename;
#if GTK_CHECK_VERSION(2, 6, 0)
	char * str[BV_COUNT] = { "details", "icons", "list", "thumbnails" };
#endif

	if(browser->config == NULL)
		return 0; /* XXX ignore error */
	if((filename = _common_config_filename(BROWSER_CONFIG_FILE)) == NULL)
		return 1;
#if GTK_CHECK_VERSION(2, 6, 0)
	/* XXX deserves a rework (enum) */
	if(browser->prefs.default_view >= BV_FIRST
			&& browser->prefs.default_view <= BV_LAST)
		ret |= config_set(browser->config, NULL, "default_view",
				str[browser->prefs.default_view]);
#endif
	ret |= _config_save_boolean(browser->config, "alternate_rows",
			browser->prefs.alternate_rows);
	ret |= _config_save_boolean(browser->config, "confirm_before_delete",
			browser->prefs.confirm_before_delete);
	ret |= _config_save_boolean(browser->config, "sort_folders_first",
			browser->prefs.sort_folders_first);
	ret |= _config_save_boolean(browser->config, "show_hidden_files",
			browser->prefs.show_hidden_files);
	if(ret == 0)
		ret |= config_save(browser->config, filename);
	free(filename);
	return ret;
}


/* browser_copy */
void browser_copy(Browser * browser)
{
	GtkWidget * entry;

	entry = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	if(browser->window != NULL
			&& gtk_window_get_focus(GTK_WINDOW(browser->window))
			== entry)
	{
		gtk_editable_copy_clipboard(GTK_EDITABLE(entry));
		return;
	}
	g_list_foreach(browser->selection, (GFunc)free, NULL);
	g_list_free(browser->selection);
	browser->selection = browser_selection_copy(browser);
	browser->selection_cut = 0;
}


/* browser_cut */
void browser_cut(Browser * browser)
{
	GtkWidget * entry;

	entry = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	if(browser->window != NULL
			&& gtk_window_get_focus(GTK_WINDOW(browser->window))
			== entry)
	{
		gtk_editable_cut_clipboard(GTK_EDITABLE(entry));
		return;
	}
	g_list_foreach(browser->selection, (GFunc)free, NULL);
	g_list_free(browser->selection);
	browser->selection = browser_selection_copy(browser);
	browser->selection_cut = 1;
}


/* browser_paste */
void browser_paste(Browser * browser)
{
	GtkWidget * entry;

	entry = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	if(browser->window != NULL
			&& gtk_window_get_focus(GTK_WINDOW(browser->window))
			== entry)
	{
		gtk_editable_paste_clipboard(GTK_EDITABLE(entry));
		return;
	}
	browser_selection_paste(browser);
}


/* browser_focus_location */
void browser_focus_location(Browser * browser)
{
	gtk_widget_grab_focus(browser->tb_path);
}


/* browser_get_location */
char const * browser_get_location(Browser * browser)
{
	if(browser->current == NULL)
		return NULL;
	return browser->current->data;
}


/* browser_get_path_entry */
char const * browser_get_path_entry(Browser * browser)
{
	GtkWidget * widget;

	widget = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	return gtk_entry_get_text(GTK_ENTRY(widget));
}


/* browser_get_view */
BrowserView browser_get_view(Browser * browser)
{
	return browser->view;
}


/* browser_get_widget */
GtkWidget * browser_get_widget(Browser * browser)
{
	return browser->widget;
}


/* browser_get_window */
GtkWidget * browser_get_window(Browser * browser)
{
	return browser->window;
}


/* browser_go_back */
void browser_go_back(Browser * browser)
{
	char const * location;

	if(browser->current == NULL || browser->current->prev == NULL)
		return;
	browser->current = g_list_previous(browser->current);
	if((location = browser_get_location(browser)) == NULL)
		return;
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_back),
			browser->current->prev != NULL);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_updir),
			strcmp(location, "/") != 0);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_forward),
			TRUE);
	browser_refresh(browser);
}


/* browser_go_forward */
void browser_go_forward(Browser * browser)
{
	char const * location;

	if(browser->current == NULL || browser->current->next == NULL) /* XXX */
		return;
	browser->current = browser->current->next;
	if((location = browser_get_location(browser)) == NULL)
		return;
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_back), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_updir),
			strcmp(location, "/") != 0);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_forward),
			browser->current->next != NULL);
	browser_refresh(browser); /* FIXME if it fails history is wrong */
}


/* browser_go_home */
void browser_go_home(Browser * browser)
{
	char const * home;

	if((home = getenv("HOME")) == NULL)
		home = g_get_home_dir();
	/* XXX use open while set_location should only update the toolbar? */
	browser_set_location(browser, (home != NULL) ? home : "/");
}


/* browser_load */
int browser_load(Browser * browser, char const * plugin)
{
	Plugin * p;
	BrowserPluginDefinition * bpd;
	BrowserPlugin * bp;
	GtkWidget * widget;
	GtkTreeIter iter;
	GtkIconTheme * theme;
	GdkPixbuf * icon = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, plugin);
#endif
	if(_browser_plugin_is_enabled(browser, plugin))
		return 0;
	if((p = plugin_new(LIBDIR, PACKAGE, "plugins", plugin)) == NULL)
		return -browser_error(NULL, error_get(NULL), 1);
	if((bpd = plugin_lookup(p, "plugin")) == NULL)
	{
		plugin_delete(p);
		return -browser_error(NULL, error_get(NULL), 1);
	}
	if(bpd->init == NULL || bpd->destroy == NULL || bpd->get_widget == NULL
			|| (bp = bpd->init(&browser->pl_helper)) == NULL)
	{
		plugin_delete(p);
		return -browser_error(NULL, error_get(NULL), 1);
	}
	widget = bpd->get_widget(bp);
	gtk_widget_hide(widget);
	theme = gtk_icon_theme_get_default();
	if(bpd->icon != NULL)
		icon = gtk_icon_theme_load_icon(theme, bpd->icon, 24, 0, NULL);
	if(icon == NULL)
		icon = gtk_icon_theme_load_icon(theme, "gnome-settings", 24, 0,
				NULL);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_list_store_insert_with_values(browser->pl_store, &iter, -1,
#else
	gtk_list_store_append(browser->pl_store, &iter);
	gtk_list_store_set(browser->pl_store, &iter,
#endif
			BPC_NAME, plugin, BPC_ICON, icon,
			BPC_NAME_DISPLAY, _(bpd->name),
			BPC_PLUGIN, p, BPC_BROWSERPLUGINDEFINITION, bpd,
			BPC_BROWSERPLUGIN, bp, BPC_WIDGET, widget, -1);
	if(icon != NULL)
		g_object_unref(icon);
	gtk_box_pack_start(GTK_BOX(browser->pl_box), widget, TRUE, TRUE, 0);
	if(gtk_widget_get_no_show_all(browser->pl_view) == TRUE)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(browser->pl_combo), 0);
		gtk_widget_set_no_show_all(browser->pl_view, FALSE);
		gtk_widget_show_all(browser->pl_view);
	}
	return 0;
}


/* browser_open */
void browser_open(Browser * browser, char const * path)
{
	GtkWidget * dialog;

	if(path == NULL)
	{
		dialog = gtk_file_chooser_dialog_new(_("Open file..."),
				(browser->window != NULL)
				? GTK_WINDOW(browser->window) : NULL,
				GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
				GTK_RESPONSE_ACCEPT, NULL);
		if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
			path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
						dialog));
		gtk_widget_destroy(dialog);
	}
	if(browser->mime != NULL && path != NULL)
		mime_action(browser->mime, "open", path);
}


/* browser_open_with */
static void _open_with_default(Browser * browser, char const * path,
		char const * with);

void browser_open_with(Browser * browser, char const * path)
{
	GtkWidget * dialog;
	GtkWidget * vbox;
	GtkWidget * widget = NULL;
	GtkFileFilter * filter;
	char * filename = NULL;
	gboolean active;
	pid_t pid;

	dialog = gtk_file_chooser_dialog_new(_("Open with..."),
			(browser->window != NULL)
			? GTK_WINDOW(browser->window) : NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL);
	/* set the default folder to BINDIR */
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), BINDIR);
	/* add file filters */
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Executable files"));
	gtk_file_filter_add_mime_type(filter, "application/x-executable");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Shell scripts"));
	gtk_file_filter_add_mime_type(filter, "application/x-shellscript");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	/* allow overriding the default handler */
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif
	if(browser_vfs_mime_type(browser->mime, path, 0) != NULL)
	{
		widget = gtk_check_button_new_with_mnemonic(
				_("_Set as the default handler"));
		gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, TRUE, 0);
		gtk_widget_show_all(vbox);
	}
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
					dialog));
	active = (widget != NULL && gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(widget))) ? TRUE : FALSE;
	gtk_widget_destroy(dialog);
	if(filename == NULL)
		return;
	if(active)
		_open_with_default(browser, path, filename);
	if((pid = fork()) == -1)
		browser_error(browser, strerror(errno), 1);
	else if(pid == 0)
	{
		if(close(0) != 0)
			browser_error(NULL, strerror(errno), 0);
		execlp(filename, filename, path, NULL);
		browser_error(NULL, strerror(errno), 0);
		exit(2);
	}
	g_free(filename);
}

static void _open_with_default(Browser * browser, char const * path,
		char const * with)
{
	char const * type;

	/* XXX report errors */
	if((type = mime_type(browser->mime, path)) == NULL)
		return;
	if(mime_set_handler(browser->mime, type, "open", with) == 0)
		/* XXX may fail too */
		mime_save(browser->mime);
}


/* browser_properties */
void browser_properties(Browser * browser)
{
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


/* browser_refresh */
void browser_refresh(Browser * browser)
{
	char const * location;
	DIR * dir;
	struct stat st;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %s\n", __func__, (browser->current != NULL
				&& browser->current->data != NULL)
			? (char *)browser->current->data : "NULL");
#endif
	if((location = browser_get_location(browser)) == NULL)
		return;
	if((dir = browser_vfs_opendir(location, &st)) == NULL) /* XXX */
		browser_error(browser, strerror(errno), 1);
	else
		_browser_refresh_do(browser, dir, &st);
}


/* _refresh_new */
static int _refresh_new_loop(Browser * browser);
static gboolean _refresh_new_idle(gpointer data);
static void _refresh_done(Browser * browser);

static void _refresh_new(Browser * browser)
{
	unsigned int i;

	gtk_list_store_clear(browser->store);
	for(i = 0; i < IDLE_LOOP_ICON_CNT
			&& _refresh_new_loop(browser) == 0; i++);
	if(i == IDLE_LOOP_ICON_CNT)
		browser->refresh_id = g_idle_add(_refresh_new_idle, browser);
	else
		_refresh_done(browser);
}


/* _refresh_new_loop */
static int _loop_status(Browser * browser, char const * prefix);
static void _loop_insert(Browser * browser, GtkTreeIter * iter,
		char const * path, char const * display, struct stat * lst,
		struct stat * st, gboolean updated);

static int _refresh_new_loop(Browser * browser)
{
	struct dirent * de;
	char const * location;
	GtkTreeIter iter;
	char * path;
	struct stat lst;
	struct stat st;

	while((de = browser_vfs_readdir(browser->refresh_dir)) != NULL)
	{
		if(de->d_name[0] == '.')
		{
			/* skip "." and ".." */
			if(de->d_name[1] == '\0' || (de->d_name[1] == '.'
						&& de->d_name[2] == '\0'))
				continue;
			browser->refresh_hid++;
		}
		browser->refresh_cnt++;
		if(de->d_name[0] != '.' || browser->prefs.show_hidden_files)
			break;
	}
	if(de == NULL)
		return _loop_status(browser, NULL);
	_loop_status(browser, _("Refreshing folder: "));
	location = browser_get_location(browser);
	if((path = g_build_filename(location, de->d_name, NULL)) == NULL
			|| browser_vfs_lstat(path, &lst) != 0)
	{
		browser_error(NULL, strerror(errno), 1);
		if(path != NULL)
			g_free(path);
		return 0;
	}
	if(S_ISLNK(lst.st_mode) && browser_vfs_stat(path, &st) == 0)
		_loop_insert(browser, &iter, path, de->d_name, &lst, &st, 0);
	else
		_loop_insert(browser, &iter, path, de->d_name, &lst, &lst, 0);
	g_free(path);
	return 0;
}

static int _loop_status(Browser * browser, char const * prefix)
{
	char status[64];

	snprintf(status, sizeof(status), _("%s%u file%c (%u hidden)"),
			(prefix != NULL) ? prefix : "",
			browser->refresh_cnt, browser->refresh_cnt <= 1
			? '\0' : 's', browser->refresh_hid); /* XXX translate */
	_browser_set_status(browser, status);
	return 1;
}


/* _loop_insert */
static void _insert_all(Browser * browser, struct stat * lst, struct stat * st,
		char const ** display, uint64_t * inode, uint64_t * size,
		char const ** dsize, struct passwd ** pw, struct group ** gr,
		char const ** ddate, char const ** type, char const * path,
		GdkPixbuf ** icon24, GdkPixbuf ** icon48, GdkPixbuf ** icon96);

static void _loop_insert(Browser * browser, GtkTreeIter * iter,
		char const * path, char const * display, struct stat * lst,
		struct stat * st, gboolean updated)
{
	struct passwd * pw = NULL;
	struct group * gr = NULL;
	uint64_t inode = 0;
	uint64_t size = 0;
	char const * dsize = "";
	char const * ddate = "";
	char const * type = NULL;
	GdkPixbuf * icon24 = NULL;
#if GTK_CHECK_VERSION(2, 6, 0)
	GdkPixbuf * icon48 = NULL;
	GdkPixbuf * icon96 = NULL;
#endif
	char uid[16] = "";
	char gid[16] = "";

#ifdef DEBUG
	fprintf(stderr, "%s%s(\"%s\")\n", "DEBUG: ", __func__, display);
#endif
	snprintf(uid, sizeof(uid), "%lu", (unsigned long)lst->st_uid);
	snprintf(gid, sizeof(gid), "%lu", (unsigned long)lst->st_gid);
	_insert_all(browser, lst, st, &display, &inode, &size, &dsize, &pw, &gr,
			&ddate, &type, path, &icon24
#if GTK_CHECK_VERSION(2, 6, 0)
			, &icon48, &icon96);
	gtk_list_store_insert_with_values(browser->store, iter, -1,
#else
			, NULL, NULL);
	gtk_list_store_insert_after(browser->store, iter, NULL);
	gtk_list_store_set(browser->store, iter,
#endif
			BC_UPDATED, updated, BC_PATH, path,
			BC_DISPLAY_NAME, display, BC_INODE, inode,
			BC_IS_DIRECTORY, S_ISDIR(st->st_mode),
			BC_IS_EXECUTABLE, st->st_mode & S_IXUSR,
			BC_IS_MOUNT_POINT, browser_vfs_is_mountpoint(lst,
				browser->refresh_dev) ? TRUE : FALSE,
			BC_PIXBUF_24, icon24,
#if GTK_CHECK_VERSION(2, 6, 0)
			BC_PIXBUF_48, icon48, BC_PIXBUF_96, icon96,
#endif
			BC_SIZE, size, BC_DISPLAY_SIZE, dsize,
			BC_OWNER, (pw != NULL) ? pw->pw_name : uid,
			BC_GROUP, (gr != NULL) ? gr->gr_name : gid,
			BC_DATE, lst->st_mtime, BC_DISPLAY_DATE, ddate,
			BC_MIME_TYPE, (type != NULL) ? type : "", -1);
	if(icon24 != NULL)
		g_object_unref(icon24);
#if GTK_CHECK_VERSION(2, 6, 0)
	if(icon48 != NULL)
		g_object_unref(icon48);
	if(icon96 != NULL)
		g_object_unref(icon96);
#endif
}

/* insert_all */
static char const * _insert_date(time_t date);

static void _insert_all(Browser * browser, struct stat * lst, struct stat * st,
		char const ** display, uint64_t * inode, uint64_t * size,
		char const ** dsize, struct passwd ** pw, struct group ** gr,
		char const ** ddate, char const ** type, char const * path,
		GdkPixbuf ** icon24, GdkPixbuf ** icon48, GdkPixbuf ** icon96)
{
	const char image[6] = "image/";
	char const * p;
	GError * error = NULL;

	if((p = g_filename_to_utf8(*display, -1, NULL, NULL, &error)) == NULL)
	{
		browser_error(NULL, error->message, 1);
		g_error_free(error);
	}
	else
		*display = p; /* XXX memory leak */
	*inode = lst->st_ino;
	*size = lst->st_size;
	*dsize = _common_size(lst->st_size);
	*pw = getpwuid(lst->st_uid);
	*gr = getgrgid(lst->st_gid);
	*ddate = _insert_date(lst->st_mtime);
	*type = browser_vfs_mime_type(browser->mime, path, lst->st_mode);
	/* load the icons */
	if(browser->mime == NULL)
		return;
	if(icon24 != NULL)
		*icon24 = browser_vfs_mime_icon(browser->mime, path, *type, lst, st,
				24);
	if(icon48 != NULL)
		*icon48 = browser_vfs_mime_icon(browser->mime, path, *type, lst, st,
				48);
	if(icon96 != NULL)
	{
#if GTK_CHECK_VERSION(2, 6, 0)
		if(*type != NULL && strncmp(*type, image, sizeof(image)) == 0
				&& browser->loading != NULL)
		{
			g_object_ref(browser->loading);
			*icon96 = browser->loading;
		}
		else
#endif
		*icon96 = browser_vfs_mime_icon(browser->mime, path, *type, lst, st,
				96);
	}
}

static char const * _insert_date(time_t date)
{
	static char buf[16];
	time_t sixmonths; /* XXX set it per refresh */
	struct tm tm;
	size_t len;

	sixmonths = time(NULL) - 15552000;
	localtime_r(&date, &tm);
	len = strftime(buf, sizeof(buf), (date < sixmonths)
			? "%b %e %H:%M" : "%b %e %Y", &tm);
	buf[len] = '\0';
	return buf;
}

static gboolean _refresh_new_idle(gpointer data)
{
	Browser * browser = data;
	unsigned int i;

	for(i = 0; i < IDLE_LOOP_ICON_CNT
			&& _refresh_new_loop(browser) == 0; i++);
	if(i == IDLE_LOOP_ICON_CNT)
		return TRUE;
	_refresh_done(browser);
	return FALSE;
}

#if GTK_CHECK_VERSION(2, 6, 0)
static gboolean _done_thumbnails(gpointer data);
#endif
static gboolean _done_timeout(gpointer data);
static void _refresh_done(Browser * browser)
{
#if GTK_CHECK_VERSION(2, 6, 0)
	GtkTreeModel * model = GTK_TREE_MODEL(browser->store);
	GtkTreeIter * iter = &browser->refresh_iter;
#endif

	browser_vfs_closedir(browser->refresh_dir);
	browser->refresh_dir = NULL;
#if GTK_CHECK_VERSION(2, 6, 0)
	if(gtk_tree_model_get_iter_first(model, iter) == TRUE)
		browser->refresh_id = g_idle_add(_done_thumbnails, browser);
	else
#endif
		browser->refresh_id = g_timeout_add(1000, _done_timeout,
				browser);
}

#if GTK_CHECK_VERSION(2, 6, 0)
static gboolean _done_thumbnails(gpointer data)
{
	Browser * browser = data;
	GtkTreeModel * model = GTK_TREE_MODEL(browser->store);
	GtkTreeIter * iter = &browser->refresh_iter;
	size_t i;
	char * type;
	char * path;
	GdkPixbuf * icon;
	GError * error = NULL;
	const char image[6] = "image/";
	char const * p;

	for(i = 0; i < IDLE_LOOP_ICON_CNT; i++)
	{
		gtk_tree_model_get(model, iter, BC_MIME_TYPE, &type,
				BC_PATH, &path, -1);
		if(type != NULL && path != NULL
				&& strcmp(type, "inode/symlink") == 0)
		{
			/* lookup the real type of symbolic links */
			free(type);
			type = NULL;
			if((p = mime_type(browser->mime, path)) != NULL)
				type = strdup(p);
		}
		if(type != NULL && path != NULL
				&& strncmp(type, image, sizeof(image)) == 0)
		{
			if((icon = gdk_pixbuf_new_from_file_at_size(path, 96,
							96, &error)) == NULL)
				icon = browser_vfs_mime_icon(browser->mime,
						path, type, NULL, NULL, 96);
			if(error != NULL)
			{
				browser_error(NULL, error->message, 1);
				g_error_free(error);
				error = NULL;
			}
			if(icon != NULL)
			{
				gtk_list_store_set(browser->store, iter,
						BC_PIXBUF_96, icon, -1);
				g_object_unref(icon);
			}
		}
		free(type);
		free(path);
		if(gtk_tree_model_iter_next(model, iter) != TRUE)
			break;
	}
	if(i == IDLE_LOOP_ICON_CNT)
		return TRUE;
	browser->refresh_id = g_timeout_add(1000, _done_timeout, browser);
	return FALSE;
}
#endif

static gboolean _done_timeout(gpointer data)
{
	Browser * browser = data;
	char const * location;
	struct stat st;

	if((location = browser_get_location(browser)) == NULL)
	{
		browser->refresh_id = 0;
		return FALSE;
	}
	if(browser_vfs_stat(location, &st) != 0)
	{
		browser->refresh_id = 0;
		browser_error(NULL, strerror(errno), 1);
		return FALSE;
	}
	if(st.st_mtime == browser->refresh_mti)
		return TRUE;
	browser_refresh(browser);
	return FALSE;
}

/* refresh_current */
static int _current_loop(Browser * browser);
static gboolean _current_idle(gpointer data);
static void _current_deleted(Browser * browser);

static void _refresh_current(Browser * browser)
{
	unsigned int i;

	for(i = 0; i < IDLE_LOOP_ICON_CNT && _current_loop(browser) == 0; i++);
	if(i == IDLE_LOOP_ICON_CNT)
	{
		browser->refresh_id = g_idle_add(_current_idle, browser);
		return;
	}
	_current_deleted(browser);
	_refresh_done(browser);
}

static void _loop_update(Browser * browser, GtkTreeIter * iter,
		char const * path, char const * display, struct stat * lst,
		struct stat * st);
static int _current_loop(Browser * browser)
{
	struct dirent * de;
	char const * location;
	char * path;
	struct stat lst;
	struct stat st;
	struct stat * p = &lst;
	GtkTreeModel * model = GTK_TREE_MODEL(browser->store);
	GtkTreeIter iter;
	gboolean valid;
	uint64_t inode;

	while((de = browser_vfs_readdir(browser->refresh_dir)) != NULL)
	{
		if(de->d_name[0] == '.')
		{
			/* skip "." and ".." */
			if(de->d_name[1] == '\0' || (de->d_name[1] == '.'
						&& de->d_name[2] == '\0'))
				continue;
			browser->refresh_hid++;
		}
		browser->refresh_cnt++;
		if(de->d_name[0] != '.' || browser->prefs.show_hidden_files)
			break;
	}
	if(de == NULL)
		return _loop_status(browser, NULL);
	_loop_status(browser, _("Refreshing folder: "));
	location = browser_get_location(browser);
	if((path = g_build_filename(location, de->d_name, NULL)) == NULL
			|| browser_vfs_lstat(path, &lst) != 0)
	{
		browser_error(NULL, strerror(errno), 1);
		if(path != NULL)
			g_free(path);
		return 0;
	}
	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, BC_INODE, &inode, -1);
		if(inode == lst.st_ino)
			break;
	}
	if(S_ISLNK(lst.st_mode) && browser_vfs_stat(path, &st) == 0)
		p = &st;
	if(valid != TRUE)
		_loop_insert(browser, &iter, path, de->d_name, &lst, p, 1);
	else
		_loop_update(browser, &iter, path, de->d_name, &lst, p);
	g_free(path);
	return 0;
}

static void _loop_update(Browser * browser, GtkTreeIter * iter,
		char const * path, char const * display, struct stat * lst,
		struct stat * st)
{
	struct passwd * pw = NULL;
	struct group * gr = NULL;
	uint64_t inode = 0;
	uint64_t size = 0;
	char const * dsize = "";
	char const * ddate = "";
	char const * type = NULL;
	GdkPixbuf * icon24 = NULL;
#if GTK_CHECK_VERSION(2, 6, 0)
	GdkPixbuf * icon48 = NULL;
	GdkPixbuf * icon96 = NULL;
#endif
	char uid[16] = "";
	char gid[16] = "";

#ifdef DEBUG
	fprintf(stderr, "%s%s(\"%s\")\n", "DEBUG: ", __func__, display);
#endif
	snprintf(uid, sizeof(uid), "%lu", (unsigned long)lst->st_uid);
	snprintf(gid, sizeof(gid), "%lu", (unsigned long)lst->st_gid);
	_insert_all(browser, lst, st, &display, &inode, &size, &dsize, &pw, &gr,
			&ddate, &type, path, &icon24
#if GTK_CHECK_VERSION(2, 6, 0)
			, &icon48, &icon96
#endif
		   );
	gtk_list_store_set(browser->store, iter, BC_UPDATED, TRUE,
			BC_PATH, path, BC_DISPLAY_NAME, display,
			BC_INODE, inode, BC_IS_DIRECTORY, S_ISDIR(st->st_mode),
			BC_IS_EXECUTABLE, st->st_mode & S_IXUSR,
			BC_IS_MOUNT_POINT, browser_vfs_is_mountpoint(lst,
				browser->refresh_dev) ? TRUE : FALSE,
			BC_PIXBUF_24, icon24,
#if GTK_CHECK_VERSION(2, 6, 0)
			BC_PIXBUF_48, icon48,
			BC_PIXBUF_96, icon96,
#endif
			BC_SIZE, size, BC_DISPLAY_SIZE, dsize,
			BC_OWNER, (pw != NULL) ? pw->pw_name : uid,
			BC_GROUP, (gr != NULL) ? gr->gr_name : gid,
			BC_DATE, lst->st_mtime, BC_DISPLAY_DATE, ddate,
			BC_MIME_TYPE, (type != NULL) ? type : "", -1);
	/* FIXME refresh the plug-in if the icon is currently selected */
}

static gboolean _current_idle(gpointer data)
{
	Browser * browser = data;
	unsigned int i;

	for(i = 0; i < IDLE_LOOP_ICON_CNT && _current_loop(browser) == 0; i++);
	if(i == IDLE_LOOP_ICON_CNT)
		return TRUE;
	_current_deleted(browser);
	_refresh_done(browser);
	return FALSE;
}

static void _current_deleted(Browser * browser)
{
	GtkTreeModel * model = GTK_TREE_MODEL(browser->store);
	GtkTreeIter iter;
	gboolean valid;
	gboolean updated;

	valid = gtk_tree_model_get_iter_first(model, &iter);
	while(valid == TRUE)
	{
		gtk_tree_model_get(model, &iter, BC_UPDATED, &updated, -1);
		gtk_list_store_set(browser->store, &iter, BC_UPDATED, FALSE,
				-1);
		if(updated == TRUE)
			valid = gtk_tree_model_iter_next(model, &iter);
		else
			valid = gtk_list_store_remove(browser->store, &iter);
	}
}


/* browser_select_all */
void browser_select_all(Browser * browser)
{
	GtkTreeSelection * sel;

#if GTK_CHECK_VERSION(2, 6, 0)
	if(browser_get_view(browser) != BV_DETAILS)
	{
		gtk_icon_view_select_all(GTK_ICON_VIEW(browser->iconview));
		return;
	}
#endif
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->detailview));
	gtk_tree_selection_select_all(sel);
}


/* browser_selection_copy */
GList * browser_selection_copy(Browser * browser)
{
	GtkTreeSelection * treesel;
	GList * sel;
	GList * p;
	GtkTreeIter iter;
	char * q;

#if GTK_CHECK_VERSION(2, 6, 0)
	if(browser_get_view(browser) != BV_DETAILS)
		sel = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(
					browser->iconview));
	else
#endif
	if((treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(
						browser->detailview))) == NULL)
		return NULL;
	else
		sel = gtk_tree_selection_get_selected_rows(treesel, NULL);
	for(p = NULL; sel != NULL; sel = sel->next)
	{
		if(gtk_tree_model_get_iter(GTK_TREE_MODEL(browser->store),
					&iter, sel->data) == FALSE)
			continue;
		gtk_tree_model_get(GTK_TREE_MODEL(browser->store), &iter,
				BC_PATH, &q, -1);
		p = g_list_append(p, q);
	}
	g_list_foreach(sel, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(sel); /* XXX can probably be optimized for re-use */
	return p;
}


/* browser_selection_delete */
void browser_selection_delete(Browser * browser)
{
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
		dialog = gtk_message_dialog_new(
				(browser->window != NULL)
				? GTK_WINDOW(browser->window) : NULL,
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


/* browser_selection_paste */
void browser_selection_paste(Browser * browser)
{
	char const * location;
	char * p;

	if(browser->selection == NULL
			|| (location = browser_get_location(browser)) == NULL)
		return;
	if((p = strdup(location)) == NULL)
	{
		browser_error(browser, strerror(errno), 1);
		return;
	}
	browser->selection = g_list_append(browser->selection, p);
	if(browser->selection_cut != 1)
	{
		/* copy the selection */
		if(_common_exec("copy", "-iR", browser->selection) != 0)
			browser_error(browser, strerror(errno), 1);
		browser->selection = g_list_remove(browser->selection, p);
		free(p);
	}
	else
	{
		/* move the selection */
		if(_common_exec("move", "-i", browser->selection) != 0)
			browser_error(browser, strerror(errno), 1);
		else
		{
			g_list_foreach(browser->selection, (GFunc)free, NULL);
			g_list_free(browser->selection);
			browser->selection = NULL;
		}
	}
}


/* browser_show_about */
static gboolean _about_on_closex(gpointer data);

void browser_show_about(Browser * browser, gboolean show)
{
	if(browser->ab_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(browser->ab_window));
		return;
	}
	browser->ab_window = desktop_about_dialog_new();
	if(browser->window != NULL)
		gtk_window_set_transient_for(GTK_WINDOW(browser->ab_window),
				GTK_WINDOW(browser->window));
	desktop_about_dialog_set_authors(browser->ab_window, _authors);
	desktop_about_dialog_set_comments(browser->ab_window,
			_("File manager for the DeforaOS desktop"));
	desktop_about_dialog_set_copyright(browser->ab_window, _copyright);
	desktop_about_dialog_set_logo_icon_name(browser->ab_window,
			"system-file-manager");
	desktop_about_dialog_set_license(browser->ab_window, _license);
	desktop_about_dialog_set_name(browser->ab_window, PACKAGE);
	desktop_about_dialog_set_translator_credits(browser->ab_window,
			_("translator-credits"));
	desktop_about_dialog_set_version(browser->ab_window, VERSION);
	desktop_about_dialog_set_website(browser->ab_window,
			"http://www.defora.org/");
	g_signal_connect_swapped(browser->ab_window, "delete-event",
			G_CALLBACK(_about_on_closex), browser);
	gtk_widget_show(browser->ab_window);
}

static gboolean _about_on_closex(gpointer data)
{
	Browser * browser = data;

	gtk_widget_hide(browser->ab_window);
	return TRUE;
}


/* browser_show_preferences */
static void _preferences_set(Browser * browser);
static void _preferences_set_plugins(Browser * browser);
/* callbacks */
static void _preferences_on_mime_edit(gpointer data);
static void _preferences_on_mime_foreach(void * data, char const * name,
		GdkPixbuf * icon24, GdkPixbuf * icon48, GdkPixbuf * icon96);
static void _preferences_on_plugin_toggled(GtkCellRendererToggle * renderer,
		char * path, gpointer data);
static gboolean _preferences_on_closex(gpointer data);
static void _preferences_on_response(GtkWidget * widget, gint response,
		gpointer data);
static void _preferences_on_apply(gpointer data);
static void _preferences_on_cancel(gpointer data);
static void _preferences_on_ok(gpointer data);

void browser_show_preferences(Browser * browser, gboolean show)
{
	GtkWidget * widget;
	GtkWidget * vbox;
	GtkWidget * notebook;
	GtkWidget * hbox;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	if(browser->pr_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(browser->pr_window));
		return;
	}
	browser->pr_window = gtk_dialog_new_with_buttons(_("Preferences"),
			(browser->window != NULL)
			? GTK_WINDOW(browser->window) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
			GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	g_signal_connect_swapped(browser->pr_window, "delete-event",
			G_CALLBACK(_preferences_on_closex), browser);
	g_signal_connect(browser->pr_window, "response",
			G_CALLBACK(_preferences_on_response), browser);
	/* notebook */
	notebook = gtk_notebook_new();
	/* appearance tab */
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox = gtk_vbox_new(FALSE, 4);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
#if GTK_CHECK_VERSION(2, 6, 0)
# if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
# else
	hbox = gtk_hbox_new(FALSE, 4);
# endif
	widget = gtk_label_new(_("Default view:"));
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
# if GTK_CHECK_VERSION(2, 24, 0)
	widget = gtk_combo_box_text_new();
	browser->pr_view = widget;
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget),
			_("Details"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), _("Icons"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), _("List"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget),
			_("Thumbnails"));
# else
	widget = gtk_combo_box_new_text();
	browser->pr_view = widget;
	gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Details"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Icons"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("List"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Thumbnails"));
# endif
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
#endif
	browser->pr_alternate = gtk_check_button_new_with_mnemonic(
			_("_Alternate row colors in detailed view"));
	gtk_box_pack_start(GTK_BOX(vbox), browser->pr_alternate, FALSE, FALSE,
			0);
	browser->pr_confirm = gtk_check_button_new_with_mnemonic(
			_("_Confirm before deletion"));
	gtk_box_pack_start(GTK_BOX(vbox), browser->pr_confirm, FALSE, FALSE, 0);
	browser->pr_sort = gtk_check_button_new_with_mnemonic(
			_("Sort _folders first"));
	gtk_box_pack_start(GTK_BOX(vbox), browser->pr_sort, FALSE, FALSE, 0);
	browser->pr_hidden = gtk_check_button_new_with_mnemonic(
			_("Show _hidden files"));
	gtk_box_pack_start(GTK_BOX(vbox), browser->pr_hidden, FALSE, FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox,
			gtk_label_new_with_mnemonic(_("_Appearance")));
	/* file associations tab */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	browser->pr_mime_store = gtk_list_store_new(BMC_COUNT, GDK_TYPE_PIXBUF,
			G_TYPE_STRING);
	browser->pr_mime_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				browser->pr_mime_store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(browser->pr_mime_view),
			FALSE);
	column = gtk_tree_view_column_new_with_attributes(NULL,
			gtk_cell_renderer_pixbuf_new(), "pixbuf", BMC_ICON,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(browser->pr_mime_view),
			column);
	column = gtk_tree_view_column_new_with_attributes(_("Type"),
			gtk_cell_renderer_text_new(), "text", BMC_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(browser->pr_mime_view),
			column);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(
				browser->pr_mime_store), BMC_NAME,
			GTK_SORT_ASCENDING);
	mime_foreach(browser->mime, _preferences_on_mime_foreach, browser);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(widget), browser->pr_mime_view);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox = gtk_vbox_new(FALSE, 4);
#endif
	widget = gtk_button_new_from_stock(GTK_STOCK_EDIT);
	g_signal_connect_swapped(widget, "clicked",
			G_CALLBACK(_preferences_on_mime_edit), browser);
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox,
			gtk_label_new_with_mnemonic(_("_File associations")));
	/* plug-ins tab */
	browser->pr_plugin_store = gtk_list_store_new(BPC_COUNT, G_TYPE_STRING,
			G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_STRING,
			G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER,
			G_TYPE_POINTER);
	browser->pr_plugin_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				browser->pr_plugin_store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(
				browser->pr_plugin_view), FALSE);
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(renderer, "toggled", G_CALLBACK(
				_preferences_on_plugin_toggled), browser);
	column = gtk_tree_view_column_new_with_attributes(_("Enabled"),
			renderer, "active", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(browser->pr_plugin_view),
			column);
	column = gtk_tree_view_column_new_with_attributes(NULL,
			gtk_cell_renderer_pixbuf_new(), "pixbuf", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(browser->pr_plugin_view),
			column);
	column = gtk_tree_view_column_new_with_attributes(_("Name"),
			gtk_cell_renderer_text_new(), "text", 3, NULL);
	gtk_tree_view_column_set_sort_column_id(column, 3);
	gtk_tree_view_append_column(GTK_TREE_VIEW(browser->pr_plugin_view),
			column);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(
				browser->pr_plugin_store), BPC_NAME_DISPLAY,
			GTK_SORT_ASCENDING);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(widget), browser->pr_plugin_view);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
			gtk_label_new_with_mnemonic(_("_Plug-ins")));
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(browser->pr_window));
#else
	vbox = GTK_DIALOG(browser->pr_window)->vbox;
#endif
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
	_preferences_set(browser);
	gtk_widget_show_all(browser->pr_window);
}

static void _preferences_set(Browser * browser)
{
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_combo_box_set_active(GTK_COMBO_BOX(browser->pr_view),
			browser->prefs.default_view);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(browser->pr_alternate),
			browser->prefs.alternate_rows);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(browser->pr_confirm),
			browser->prefs.confirm_before_delete);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(browser->pr_sort),
			browser->prefs.sort_folders_first);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(browser->pr_hidden),
			browser->prefs.show_hidden_files);
	_preferences_set_plugins(browser);
}

static void _preferences_set_plugins(Browser * browser)
{
	DIR * dir;
	struct dirent * de;
	GtkIconTheme * theme;
#ifdef __APPLE__
	char const ext[] = ".dylib";
#else
	char const ext[] = ".so";
#endif
	size_t len;
	Plugin * p;
	BrowserPluginDefinition * bpd;
	GtkTreeIter iter;
	gboolean enabled;
	GdkPixbuf * icon;

	gtk_list_store_clear(browser->pr_plugin_store);
	if((dir = opendir(LIBDIR "/" PACKAGE "/plugins")) == NULL)
		return;
	theme = gtk_icon_theme_get_default();
	while((de = readdir(dir)) != NULL)
	{
		if((len = strlen(de->d_name)) < sizeof(ext))
			continue;
		if(strcmp(&de->d_name[len - sizeof(ext) + 1], ext) != 0)
			continue;
		de->d_name[len - sizeof(ext) + 1] = '\0';
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, de->d_name);
#endif
		if((p = plugin_new(LIBDIR, PACKAGE, "plugins", de->d_name))
				== NULL)
			continue;
		if((bpd = plugin_lookup(p, "plugin")) == NULL)
		{
			plugin_delete(p);
			continue;
		}
		enabled = _browser_plugin_is_enabled(browser, de->d_name);
		icon = NULL;
		if(bpd->icon != NULL)
			icon = gtk_icon_theme_load_icon(theme, bpd->icon, 24,
					0, NULL);
		if(icon == NULL)
			icon = gtk_icon_theme_load_icon(theme, "gnome-settings",
					24, 0, NULL);
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_list_store_insert_with_values(browser->pr_plugin_store,
				&iter, -1,
#else
		gtk_list_store_append(browser->pr_plugin_store, &iter);
		gtk_list_store_set(browser->pr_plugin_store, &iter,
#endif
				BPC_NAME, de->d_name, BPC_ENABLED, enabled,
				BPC_ICON, icon, BPC_NAME_DISPLAY, _(bpd->name),
				-1);
		if(icon != NULL)
			g_object_unref(icon);
		plugin_delete(p);
	}
	closedir(dir);
}

static void _preferences_on_mime_edit(gpointer data)
{
	Browser * browser = data;
	GtkTreeSelection * selection;
	GtkTreeModel * model;
	GtkTreeIter iter;
	char * type;
	GtkWidget * dialog;
	int flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
	GtkSizeGroup * group;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * widget;
	GtkWidget * open;
	GtkWidget * view;
	GtkWidget * edit;
	char const * p;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				browser->pr_mime_view));
	if(gtk_tree_selection_get_selected(selection, &model, &iter) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, BMC_NAME, &type, -1);
	dialog = gtk_dialog_new_with_buttons(_("Edit file association"),
			GTK_WINDOW(browser->pr_window), flags, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_APPLY,
			GTK_RESPONSE_APPLY, NULL);
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif
	gtk_box_set_spacing(GTK_BOX(vbox), 4);
	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* type */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	widget = gtk_label_new(_("Type:"));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	widget = gtk_label_new(type);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	/* open */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	widget = gtk_label_new(_("Open with:"));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	open = gtk_entry_new();
	p = mime_get_handler(browser->mime, type, "open");
	gtk_entry_set_text(GTK_ENTRY(open), (p != NULL) ? p : "");
	gtk_box_pack_start(GTK_BOX(hbox), open, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	/* view */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	widget = gtk_label_new(_("View with:"));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	view = gtk_entry_new();
	p = mime_get_handler(browser->mime, type, "view");
	gtk_entry_set_text(GTK_ENTRY(view), (p != NULL) ? p : "");
	gtk_box_pack_start(GTK_BOX(hbox), view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	/* edit */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	widget = gtk_label_new(_("Edit with:"));
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(group, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	edit = gtk_entry_new();
	p = mime_get_handler(browser->mime, type, "edit");
	gtk_entry_set_text(GTK_ENTRY(edit), (p != NULL) ? p : "");
	gtk_box_pack_start(GTK_BOX(hbox), edit, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show_all(vbox);
	/* response */
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY)
	{
		mime_set_handler(browser->mime, type, "open",
				gtk_entry_get_text(GTK_ENTRY(open)));
		mime_set_handler(browser->mime, type, "view",
				gtk_entry_get_text(GTK_ENTRY(view)));
		mime_set_handler(browser->mime, type, "edit",
				gtk_entry_get_text(GTK_ENTRY(edit)));
		mime_save(browser->mime);
	}
	gtk_widget_destroy(dialog);
	free(type);
}

static void _preferences_on_mime_foreach(void * data, char const * name,
		GdkPixbuf * icon24, GdkPixbuf * icon48, GdkPixbuf * icon96)
{
	Browser * browser = data;
	GtkTreeIter iter;

#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_list_store_insert_with_values(browser->pr_mime_store, &iter, -1,
#else
	gtk_list_store_append(browser->pr_mime_store, &iter);
	gtk_list_store_set(browser->pr_mime_store, &iter,
#endif
			BMC_NAME, name, BMC_ICON, icon24, -1);
}

static void _preferences_on_plugin_toggled(GtkCellRendererToggle * renderer,
		char * path, gpointer data)
{
	Browser * browser = data;
	GtkTreeIter iter;

	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(
				browser->pr_plugin_store), &iter, path);
	gtk_list_store_set(browser->pr_plugin_store, &iter, BPC_ENABLED,
			!gtk_cell_renderer_toggle_get_active(renderer), -1);
}

static gboolean _preferences_on_closex(gpointer data)
{
	Browser * browser = data;

	_preferences_on_cancel(browser);
	return TRUE;
}

static void _preferences_on_response(GtkWidget * widget, gint response,
		gpointer data)
{
	if(response == GTK_RESPONSE_APPLY)
		_preferences_on_apply(data);
	else
	{
		gtk_widget_hide(widget);
		if(response == GTK_RESPONSE_OK)
			_preferences_on_ok(data);
		else if(response == GTK_RESPONSE_CANCEL)
			_preferences_on_cancel(data);
	}
}

static void _preferences_on_apply(gpointer data)
{
	Browser * browser = data;
	GtkTreeModel * model = GTK_TREE_MODEL(browser->pr_plugin_store);
	GtkTreeIter iter;
	gboolean valid;
	gchar * p;
	gboolean enabled;
	String * value = string_new("");
	String * sep = "";
	int res = (value != NULL) ? 0 : 1;

	/* appearance */
#if GTK_CHECK_VERSION(2, 6, 0)
	browser->prefs.default_view = gtk_combo_box_get_active(GTK_COMBO_BOX(
				browser->pr_view));
#endif
	browser->prefs.alternate_rows = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(browser->pr_alternate));
	if(browser->detailview != NULL)
		gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(browser->detailview),
				browser->prefs.alternate_rows);
	browser->prefs.confirm_before_delete = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(browser->pr_confirm));
	browser->prefs.sort_folders_first = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(browser->pr_sort));
	browser->prefs.show_hidden_files = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(browser->pr_hidden));
	/* plug-ins */
	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, BPC_NAME, &p,
				BPC_ENABLED, &enabled, -1);
		if(enabled)
		{
			browser_load(browser, p);
			res |= string_append(&value, sep);
			res |= string_append(&value, p);
			sep = ",";
		}
		else
			browser_unload(browser, p);
		g_free(p);
	}
	if(res == 0)
		config_set(browser->config, NULL, "plugins", value);
	string_delete(value);
	browser_refresh(browser);
}

static void _preferences_on_cancel(gpointer data)
{
	Browser * browser = data;

	gtk_widget_hide(browser->pr_window);
	_preferences_set(browser);
}

static void _preferences_on_ok(gpointer data)
{
	Browser * browser = data;

	gtk_widget_hide(browser->pr_window);
	_preferences_on_apply(browser);
	browser_config_save(browser);
}


/* browser_unload */
int browser_unload(Browser * browser, char const * plugin)
{
	GtkTreeModel * model = GTK_TREE_MODEL(browser->pl_store);
	GtkTreeIter iter;
	gboolean valid;
	gchar * p;
	Plugin * pp;
	BrowserPluginDefinition * bpd;
	BrowserPlugin * bp;
	GtkWidget * widget;
	gboolean enabled = FALSE;

	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, BPC_NAME, &p, BPC_PLUGIN, &pp,
				BPC_BROWSERPLUGINDEFINITION, &bpd,
				BPC_BROWSERPLUGIN, &bp, BPC_WIDGET, &widget,
				-1);
		enabled = (strcmp(p, plugin) == 0) ? TRUE : FALSE;
		g_free(p);
		if(enabled)
			break;
	}
	if(enabled != TRUE)
		return 0;
	gtk_list_store_remove(browser->pl_store, &iter);
	gtk_container_remove(GTK_CONTAINER(browser->pl_box), widget);
	if(bpd->destroy != NULL)
		bpd->destroy(bp);
	plugin_delete(pp);
	if(gtk_tree_model_iter_n_children(model, NULL) == 0)
	{
		gtk_widget_set_no_show_all(browser->pl_view, TRUE);
		gtk_widget_hide(browser->pl_view);
	}
	else if(gtk_combo_box_get_active(GTK_COMBO_BOX(browser->pl_combo)) < 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(browser->pl_combo), 0);
	return 0;
}


/* browser_unselect_all */
void browser_unselect_all(Browser * browser)
{
	GtkTreeSelection * sel;

#if GTK_CHECK_VERSION(2, 6, 0)
	if(browser_get_view(browser) != BV_DETAILS)
	{
		gtk_icon_view_unselect_all(GTK_ICON_VIEW(browser->iconview));
		return;
	}
#endif
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->detailview));
	gtk_tree_selection_unselect_all(sel);
}


/* private */
/* functions */
/* accessors */
/* browser_config_get */
static char const * _browser_config_get(Browser * browser, char const * section,
		char const * variable)
{
	char const * ret;
	String * s = NULL;

	if(section != NULL && (s = string_new_append("plugin::", section, NULL))
			== NULL)
	{
		browser_error(NULL, error_get(NULL), 1);
		return NULL;
	}
	ret = config_get(browser->config, s, variable);
	string_delete(s);
	return ret;
}


/* browser_config_set */
static int _browser_config_set(Browser * browser, char const * section,
		char const * variable, char const * value)
{
	int ret;
	String * s = NULL;
	char * filename;

	if(section != NULL && (s = string_new_append("plugin::", section, NULL))
			== NULL)
		return -browser_error(NULL, error_get(NULL), 1);
	if((ret = config_set(browser->config, s, variable, value)) == 0
			&& (filename = _common_config_filename(
					BROWSER_CONFIG_FILE)) != NULL)
	{
		if(config_save(browser->config, filename) != 0)
			browser_error(NULL, error_get(NULL), 1);
		free(filename);
	}
	string_delete(s);
	return ret;
}


/* browser_plugin_is_enabled */
static gboolean _browser_plugin_is_enabled(Browser * browser,
		char const * plugin)
{
	GtkTreeModel * model = GTK_TREE_MODEL(browser->pl_store);
	GtkTreeIter iter;
	gchar * p;
	gboolean valid;
	int res;

	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, BPC_NAME, &p, -1);
		res = strcmp(p, plugin);
		g_free(p);
		if(res == 0)
			return TRUE;
	}
	return FALSE;
}


/* browser_get_icon */
static GdkPixbuf * _browser_get_icon(Browser * browser, char const * filename,
		char const * type, struct stat * lst, struct stat * st,
		int size)
{
	return browser_vfs_mime_icon(browser->mime, filename, type, lst, st,
			size);
}


/* browser_get_mime */
static Mime * _browser_get_mime(Browser * browser)
{
	return browser->mime;
}


/* browser_get_selection */
static GList * _browser_get_selection(Browser * browser)
{
	GtkTreeSelection * treesel;

#if GTK_CHECK_VERSION(2, 6, 0)
	if(browser_get_view(browser) != BV_DETAILS)
		return gtk_icon_view_get_selected_items(GTK_ICON_VIEW(
					browser->iconview));
#endif
	if((treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(
						browser->detailview))) == NULL)
		return NULL;
	return gtk_tree_selection_get_selected_rows(treesel, NULL);
}


/* browser_get_type */
static char const * _browser_get_type(Browser * browser, char const * filename,
		mode_t mode)
{
	return browser_vfs_mime_type(browser->mime, filename, mode);
}


/* browser_set_status */
static void _browser_set_status(Browser * browser, char const * status)
{
	GtkStatusbar * sb = GTK_STATUSBAR(browser->statusbar);

	if(browser->statusbar_id != 0)
		gtk_statusbar_remove(sb, gtk_statusbar_get_context_id(sb, ""),
				browser->statusbar_id);
	browser->statusbar_id = gtk_statusbar_push(sb,
			gtk_statusbar_get_context_id(sb, ""), status);
}


/* browser_set_location */
static int _location_directory(Browser * browser, char const * path, DIR * dir,
		struct stat * st);

int browser_set_location(Browser * browser, char const * path)
{
	int ret = 0;
	char * realpath = NULL;
	DIR * dir;
	struct stat st;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, path);
#endif
	if((realpath = _common_get_absolute_path(path)) == NULL)
		return -1;
	/* XXX check browser_cnt to disallow filenames at startup */
	if(/* browser_cnt && */ g_file_test(realpath, G_FILE_TEST_IS_REGULAR))
	{
		if(browser->mime != NULL)
			mime_action(browser->mime, "open", realpath);
	}
	else if(g_file_test(realpath, G_FILE_TEST_IS_DIR)
			&& (dir = browser_vfs_opendir(realpath, &st)) != NULL)
	{
		if(_location_directory(browser, realpath, dir, &st) == 0)
			gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_updir),
					strcmp(browser->current->data, "/"));
		else
			browser_vfs_closedir(dir);
	}
	else
		/* XXX errno may not be set */
		ret = -browser_error(browser, strerror(errno), 1);
	free(realpath);
	return ret;
}

static int _location_directory(Browser * browser, char const * path, DIR * dir,
		struct stat * st)
{
	char * p;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, path);
#endif
	if((p = strdup(path)) == NULL)
		return -1;
	if(browser->history == NULL)
	{
		if((browser->history = g_list_alloc()) == NULL)
			return 1;
		browser->history->data = p;
		browser->current = browser->history;
	}
	else if(strcmp(browser->current->data, p) != 0)
	{
		g_list_foreach(browser->current->next, (GFunc)free, NULL);
		g_list_free(browser->current->next);
		browser->current->next = NULL;
		browser->history = g_list_append(browser->history, p);
		browser->current = g_list_last(browser->history);
		gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_back),
				browser->current->prev != NULL ? TRUE : FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_forward),
				FALSE);
	}
	else
		free(p);
	_browser_refresh_do(browser, dir, st);
	return 0;
}


/* browser_set_view */
/* types */
typedef struct _IconCallback
{
	Browser * browser;
	int isdir;
	int isexec;
	int ismnt;
	char * path;
} IconCallback;

static void _view_details(Browser * browser);
static void _view_details_column_text(GtkTreeView * view,
		GtkCellRenderer * renderer, char const * title, int id,
		int sort);
#if GTK_CHECK_VERSION(2, 6, 0)
static void _view_icons(Browser * browser);
static void _view_icons_selection(Browser * browser, GList * sel);
static void _view_icons_view(Browser * browser);
static void _view_icons_on_icon_default(GtkIconView * view, GtkTreePath * path,
		gpointer data);
static void _view_icons_on_icon_drag_data_get(GtkWidget * widget,
		GdkDragContext * context, GtkSelectionData * seldata,
		guint info, guint time, gpointer data);
static void _view_icons_on_icon_drag_data_received(GtkWidget * widget,
		GdkDragContext * context, gint x, gint y,
		GtkSelectionData * seldata, guint info, guint time,
		gpointer data);
static void _view_list(Browser * browser);
static void _view_thumbnails(Browser * browser);
#endif
/* callbacks */
static gboolean _view_on_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data);
static GtkTreePath * _view_on_button_press_path(Browser * browser,
		GdkEventButton * event);
static void _view_on_button_press_directory(GtkWidget * menu,
		IconCallback * ic);
static void _view_on_button_press_file(Browser * browser, GtkWidget * menu,
		char * mimetype, IconCallback * ic);
static void _view_on_button_press_mime(Mime * mime, char const * mimetype,
		char const * action, char const * label, GCallback callback,
		IconCallback * ic, GtkWidget * menu);
static gboolean _view_on_button_press_show(Browser * browser,
		GdkEventButton * event, GtkWidget * menu);
static void _view_on_button_press_icon_delete(gpointer data);
static void _view_on_button_press_icon_open(gpointer data);
static void _view_on_button_press_icon_open_new_window(gpointer data);
static void _view_on_button_press_icon_edit(gpointer data);
static void _view_on_button_press_icon_open_with(gpointer data);
static void _view_on_button_press_icon_run(gpointer data);
static void _view_on_button_press_icon_paste(gpointer data);
static void _view_on_button_press_icon_unmount(gpointer data);
static gboolean _view_on_button_press_popup(Browser * browser,
		GdkEventButton * event, GtkWidget * menu, IconCallback * ic);
static void _view_on_button_press_popup_new_text_file(gpointer data);
static void _view_on_button_press_popup_new_folder(gpointer data);
static void _view_on_button_press_popup_new_symlink(gpointer data);
static void _view_on_detail_default(GtkTreeView * view, GtkTreePath * path,
		GtkTreeViewColumn * column, gpointer data);
static void _view_on_detail_default_do(Browser * browser, GtkTreePath * path);
static void _view_on_filename_edited(GtkCellRendererText * renderer,
		gchar * path, gchar * filename, gpointer data);
static gboolean _view_on_popup_menu(GtkWidget * widget, gpointer data);

void browser_set_view(Browser * browser, BrowserView view)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%u)\n", __func__, view);
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
	switch(view)
	{
		case BV_DETAILS:
			_view_details(browser);
			break;
		case BV_ICONS:
			_view_icons(browser);
			break;
		case BV_LIST:
			_view_list(browser);
			break;
		case BV_THUMBNAILS:
			_view_thumbnails(browser);
			break;
	}
# if GTK_CHECK_VERSION(3, 0, 0)
	/* XXX necessary with Gtk+ 3 */
	if(view != BV_DETAILS)
		browser_refresh(browser);
# endif
#else
	_view_details(browser);
#endif
	browser->view = view;
}

static void _view_details(Browser * browser)
{
	GtkTreeSelection * treesel;
	GtkTreeViewColumn * column;
	GtkCellRenderer * renderer;
	GtkTreeView * view;
#if GTK_CHECK_VERSION(2, 6, 0)
	GList * sel = NULL;
	GList * p;
#endif

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %u\n", __func__, browser->view);
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
	if(browser->view != BV_DETAILS)
	{
		sel = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(
					browser->iconview));
		if(browser->iconview != NULL)
			gtk_container_remove(GTK_CONTAINER(browser->scrolled),
					browser->iconview);
		if(browser->detailview != NULL)
			gtk_container_add(GTK_CONTAINER(browser->scrolled),
					browser->detailview);
	}
#endif
	if(browser->detailview != NULL)
	{
		gtk_widget_show(browser->detailview);
		return;
	}
	browser->detailview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				browser->store));
	g_object_ref(browser->detailview);
	view = GTK_TREE_VIEW(browser->detailview);
	gtk_tree_view_set_rules_hint(view, browser->prefs.alternate_rows);
	if((treesel = gtk_tree_view_get_selection(view)) != NULL)
	{
		gtk_tree_selection_set_mode(treesel, GTK_SELECTION_MULTIPLE);
#if GTK_CHECK_VERSION(2, 6, 0)
		if(sel != NULL)
		{
			for(p = sel; p != NULL; p = p->next)
				gtk_tree_selection_select_path(treesel,
						p->data);
			g_list_foreach(sel, (GFunc)gtk_tree_path_free, NULL);
			g_list_free(sel);
		}
#endif
		g_signal_connect_swapped(treesel, "changed",
				G_CALLBACK(_browser_on_selection_changed),
				browser);
	}
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer,
			"pixbuf", BC_PIXBUF_24, NULL);
	gtk_tree_view_append_column(view, column);
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "editable", TRUE, "ellipsize",
			PANGO_ELLIPSIZE_END, NULL);
	g_signal_connect(renderer, "edited", G_CALLBACK(
				_view_on_filename_edited), browser);
	_view_details_column_text(view, renderer, _("Filename"),
			BC_DISPLAY_NAME, BC_DISPLAY_NAME);
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, NULL);
	_view_details_column_text(view, renderer, _("Size"), BC_DISPLAY_SIZE,
			BC_SIZE);
	_view_details_column_text(view, NULL, _("Owner"), BC_OWNER, BC_OWNER);
	_view_details_column_text(view, NULL, _("Group"), BC_GROUP, BC_GROUP);
	_view_details_column_text(view, NULL, _("Date"), BC_DISPLAY_DATE,
			BC_DATE);
	_view_details_column_text(view, NULL, _("MIME type"), BC_MIME_TYPE,
			BC_MIME_TYPE);
	gtk_tree_view_set_headers_visible(view, TRUE);
	g_signal_connect(view, "button-press-event", G_CALLBACK(
				_view_on_button_press), browser);
	g_signal_connect(view, "popup-menu", G_CALLBACK(_view_on_popup_menu),
			browser);
	g_signal_connect(view, "row-activated", G_CALLBACK(
				_view_on_detail_default), browser);
	gtk_container_add(GTK_CONTAINER(browser->scrolled),
			browser->detailview);
	gtk_widget_show(browser->detailview);
}

static void _view_details_column_text(GtkTreeView * view,
		GtkCellRenderer * renderer, char const * title, int id,
		int sort)
{
	GtkTreeViewColumn * column;

	if(renderer == NULL)
		renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(title, renderer,
			"text", id, NULL);
#if GTK_CHECK_VERSION(2, 4, 0)
	gtk_tree_view_column_set_expand(column, TRUE);
#endif
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, sort);
	gtk_tree_view_append_column(view, column);
}

#if GTK_CHECK_VERSION(2, 6, 0)
static void _view_icons(Browser * browser)
{
# if GTK_CHECK_VERSION(2, 8, 0)
	GtkCellRenderer * renderer;

	_view_icons_view(browser);
	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "follow-state", TRUE, NULL);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(browser->iconview), renderer,
			TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(browser->iconview),
			renderer, "pixbuf", BC_PIXBUF_48, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(browser->iconview), renderer,
			TRUE);
	g_object_set(renderer, "editable", TRUE,
			"ellipsize", PANGO_ELLIPSIZE_END,
			"width-chars", 16, "wrap-mode", PANGO_WRAP_WORD_CHAR,
			"xalign", 0.5, NULL);
	g_signal_connect(renderer, "edited", G_CALLBACK(
				_view_on_filename_edited), browser);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(browser->iconview),
			renderer, "text", BC_DISPLAY_NAME, NULL);
# else
	_view_icons_view(browser);
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(browser->iconview),
			BC_PIXBUF_48);
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(browser->iconview),
			BC_DISPLAY_NAME);
	gtk_icon_view_set_item_width(GTK_ICON_VIEW(browser->iconview),
			BROWSER_ICON_WRAP_WIDTH);
# endif /* !GTK_CHECK_VERSION(2, 8, 0) */
# if GTK_CHECK_VERSION(3, 0, 0)
	gtk_icon_view_set_item_orientation(GTK_ICON_VIEW(browser->iconview),
			GTK_ORIENTATION_VERTICAL);
# else
	gtk_icon_view_set_orientation(GTK_ICON_VIEW(browser->iconview),
			GTK_ORIENTATION_VERTICAL);
# endif
}

static void _view_icons_selection(Browser * browser, GList * sel)
{
	GList * p;

	if(sel == NULL)
		return;
	gtk_icon_view_unselect_all(GTK_ICON_VIEW(browser->iconview));
	for(p = sel; p != NULL; p = p->next)
		gtk_icon_view_select_path(GTK_ICON_VIEW(browser->iconview),
				p->data);
	g_list_foreach(sel, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(sel);
}

static void _view_icons_view(Browser * browser)
{
	GtkTreeSelection * treesel;
	GList * sel = NULL;
# if GTK_CHECK_VERSION(2, 8, 0)
	GtkTargetEntry targets[] = { { "deforaos_browser_dnd", 0, 0 } };
	size_t targets_cnt = sizeof(targets) / sizeof(*targets);
# endif

# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %u\n", __func__, browser->view);
# endif
	if(browser->view == BV_DETAILS)
	{
		if((treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(
							browser->detailview)))
				!= NULL)
			sel = gtk_tree_selection_get_selected_rows(treesel,
					NULL);
		if(browser->detailview != NULL)
			gtk_container_remove(GTK_CONTAINER(browser->scrolled),
					browser->detailview);
		if(browser->iconview != NULL)
			gtk_container_add(GTK_CONTAINER(browser->scrolled),
					browser->iconview);
	}
	if(browser->iconview != NULL)
	{
# if GTK_CHECK_VERSION(2, 8, 0)
		gtk_cell_layout_clear(GTK_CELL_LAYOUT(browser->iconview));
# endif
		_view_icons_selection(browser, sel);
		gtk_widget_show(browser->iconview);
		return;
	}
	browser->iconview = gtk_icon_view_new_with_model(GTK_TREE_MODEL(
				browser->store));
	g_object_ref(browser->iconview);
	/* this needs to be done now */
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(browser->iconview),
			GTK_SELECTION_MULTIPLE);
	g_signal_connect_swapped(browser->iconview, "selection-changed",
			G_CALLBACK(_browser_on_selection_changed), browser);
	_view_icons_selection(browser, sel);
# if GTK_CHECK_VERSION(2, 8, 0)
	gtk_icon_view_enable_model_drag_source(GTK_ICON_VIEW(browser->iconview),
			GDK_BUTTON1_MASK, targets, targets_cnt,
			GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_icon_view_enable_model_drag_dest(GTK_ICON_VIEW(browser->iconview),
			targets, targets_cnt,
			GDK_ACTION_COPY | GDK_ACTION_MOVE);
# endif
	g_signal_connect(browser->iconview, "item-activated",
			G_CALLBACK(_view_icons_on_icon_default), browser);
	g_signal_connect(browser->iconview, "button-press-event",
			G_CALLBACK(_view_on_button_press), browser);
	g_signal_connect(browser->iconview, "popup-menu",
			G_CALLBACK(_view_on_popup_menu), browser);
# if GTK_CHECK_VERSION(2, 8, 0)
	g_signal_connect(browser->iconview, "drag-data-get",
			G_CALLBACK(_view_icons_on_icon_drag_data_get), browser);
	g_signal_connect(browser->iconview, "drag-data-received",
			G_CALLBACK(_view_icons_on_icon_drag_data_received),
			browser);
# endif
	gtk_container_add(GTK_CONTAINER(browser->scrolled), browser->iconview);
	gtk_widget_show(browser->iconview);
}

static void _view_icons_on_icon_drag_data_get(GtkWidget * widget,
		GdkDragContext * context, GtkSelectionData * seldata,
		guint info, guint time, gpointer data)
	/* XXX could be more optimal */
{
	Browser * browser = data;
	GList * selection;
	GList * s;
	size_t len = 0;
	size_t l;
	char * p = NULL;
	char * q;

	selection = browser_selection_copy(browser);
	for(s = selection; s != NULL; s = s->next)
	{
		l = strlen(s->data) + 1;
		if((q = realloc(p, len + l)) == NULL)
			continue; /* XXX report error */
		p = q;
		memcpy(&p[len], s->data, l);
		len += l;
	}
	gtk_selection_data_set_text(seldata, p, len);
	free(p);
	g_list_foreach(selection, (GFunc)free, NULL);
	g_list_free(selection);
}

static void _view_icons_on_icon_drag_data_received(GtkWidget * widget,
		GdkDragContext * context, gint x, gint y,
		GtkSelectionData * seldata, guint info, guint time,
		gpointer data)
	/* FIXME - may not be an icon view
	 *       - not fully checking if the source matches */
{
	Browser * browser = data;
	GtkTreePath * path;
	GtkTreeIter iter;
	char const * location;
	gchar * p = NULL;

	path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(browser->iconview),
			x, y);
	if(path == NULL)
		location = browser_get_location(browser);
	else if(gtk_tree_model_get_iter(GTK_TREE_MODEL(browser->store), &iter,
				path) == FALSE)
		return;
	else
	{
		gtk_tree_model_get(GTK_TREE_MODEL(browser->store), &iter,
				BC_PATH, &p, -1);
		location = p;
	}
	if(_common_drag_data_received(context, seldata, location) != 0)
		browser_error(browser, strerror(errno), 1);
	g_free(p);
}

static void _view_icons_on_icon_default(GtkIconView * view, GtkTreePath * path,
		gpointer data)
{
	Browser * browser = data;

	if(GTK_ICON_VIEW(browser->iconview) != view)
		return;
	_view_on_detail_default_do(browser, path);
}

static void _view_list(Browser * browser)
{
# if GTK_CHECK_VERSION(2, 8, 0)
	GtkCellRenderer * renderer;

	_view_icons_view(browser);
	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "follow-state", TRUE, NULL);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(browser->iconview), renderer,
			TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(browser->iconview),
			renderer, "pixbuf", BC_PIXBUF_24, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(browser->iconview), renderer,
			TRUE);
	g_object_set(renderer, "editable", TRUE,
			"ellipsize", PANGO_ELLIPSIZE_END,
			"width-chars", 20, "wrap-mode", PANGO_WRAP_WORD_CHAR,
			"xalign", 0.0, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(browser->iconview),
			renderer, "text", BC_DISPLAY_NAME, NULL);
# else
	_view_icons_view(browser);
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(browser->iconview),
			BC_PIXBUF_24);
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(browser->iconview),
			BC_DISPLAY_NAME);
	gtk_icon_view_set_item_width(GTK_ICON_VIEW(browser->iconview),
			BROWSER_LIST_WRAP_WIDTH + 24);
# endif /* !GTK_CHECK_VERSION(2, 8, 0) */
# if GTK_CHECK_VERSION(3, 0, 0)
	gtk_icon_view_set_item_orientation(GTK_ICON_VIEW(browser->iconview),
			GTK_ORIENTATION_VERTICAL);
# else
	gtk_icon_view_set_orientation(GTK_ICON_VIEW(browser->iconview),
			GTK_ORIENTATION_HORIZONTAL);
# endif
}

static void _view_thumbnails(Browser * browser)
{
# if GTK_CHECK_VERSION(2, 8, 0)
	GtkCellRenderer * renderer;

	_view_icons_view(browser);
	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "follow-state", TRUE, NULL);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(browser->iconview), renderer,
			TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(browser->iconview),
			renderer, "pixbuf", BC_PIXBUF_96, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(browser->iconview), renderer,
			TRUE);
	g_object_set(renderer, "editable", TRUE,
			"ellipsize", PANGO_ELLIPSIZE_END,
			"width-chars", 22, "wrap-mode", PANGO_WRAP_WORD_CHAR,
			"xalign", 0.5, NULL);
	g_signal_connect(renderer, "edited", G_CALLBACK(
				_view_on_filename_edited), browser);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(browser->iconview),
			renderer, "text", BC_DISPLAY_NAME, NULL);
# else
	_view_icons_view(browser);
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(browser->iconview),
			BC_PIXBUF_96);
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(browser->iconview),
			BC_DISPLAY_NAME);
	gtk_icon_view_set_item_width(GTK_ICON_VIEW(browser->iconview),
			BROWSER_THUMBNAIL_WRAP_WIDTH);
# endif /* !GTK_CHECK_VERSION(2, 8, 0) */
# if GTK_CHECK_VERSION(3, 0, 0)
	gtk_icon_view_set_item_orientation(GTK_ICON_VIEW(browser->iconview),
			GTK_ORIENTATION_VERTICAL);
# else
	gtk_icon_view_set_orientation(GTK_ICON_VIEW(browser->iconview),
			GTK_ORIENTATION_VERTICAL);
# endif
}
#endif

static gboolean _view_on_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data)
{
	static IconCallback ic;
	Browser * browser = data;
	GtkTreePath * path = NULL;
	GtkTreeIter iter;
	GtkTreeSelection * sel;
	GtkWidget * menuitem;
	char * mimetype = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %d\n", __func__, event->button);
#endif
	if(event->type != GDK_BUTTON_PRESS
			|| (event->button != 3 && event->button != 0))
		return FALSE;
	widget = gtk_menu_new();
	/* FIXME prevents actions to be called but probably leaks memory
	g_signal_connect(widget, "deactivate", G_CALLBACK(gtk_widget_destroy),
			NULL); */
	ic.browser = browser;
	ic.isdir = 0;
	ic.isexec = 0;
	ic.ismnt = 0;
	ic.path = NULL;
	if((path = _view_on_button_press_path(browser, event)) == NULL)
		return _view_on_button_press_popup(browser, event, widget, &ic);
	/* FIXME error checking + sub-functions */
	gtk_tree_model_get_iter(GTK_TREE_MODEL(browser->store), &iter, path);
#if GTK_CHECK_VERSION(2, 6, 0)
	if(browser_get_view(browser) != BV_DETAILS)
	{
		if(gtk_icon_view_path_is_selected(GTK_ICON_VIEW(
						browser->iconview), path)
				== FALSE)
		{
			browser_unselect_all(browser);
			gtk_icon_view_select_path(GTK_ICON_VIEW(
						browser->iconview), path);
		}
	}
	else
#endif
	{
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(
					browser->detailview));
		if(!gtk_tree_selection_iter_is_selected(sel, &iter))
		{
			browser_unselect_all(browser);
			gtk_tree_selection_select_iter(sel, &iter);
		}
	}
	gtk_tree_model_get(GTK_TREE_MODEL(browser->store), &iter,
			BC_PATH, &ic.path, BC_IS_DIRECTORY, &ic.isdir,
			BC_IS_EXECUTABLE, &ic.isexec,
			BC_IS_MOUNT_POINT, &ic.ismnt, BC_MIME_TYPE, &mimetype,
			-1);
	if(ic.isdir == TRUE)
		_view_on_button_press_directory(widget, &ic);
	else
		_view_on_button_press_file(browser, widget, mimetype, &ic);
	g_free(mimetype);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(widget), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(
			GTK_STOCK_PROPERTIES, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				on_properties), browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(widget), menuitem);
#if !GTK_CHECK_VERSION(2, 6, 0)
	gtk_tree_path_free(path);
#endif
	return _view_on_button_press_show(browser, event, widget);
}

static GtkTreePath * _view_on_button_press_path(Browser * browser,
		GdkEventButton * event)
{
	BrowserView view;
	GtkTreePath * path;

	view = browser_get_view(browser);
	if(event->button == 3)
	{
#if GTK_CHECK_VERSION(2, 6, 0)
		if(view != BV_DETAILS)
			path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(
						browser->iconview),
					(int)event->x, (int)event->y);
		else
#endif
			gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(
						browser->detailview),
					(int)event->x, (int)event->y,
					&path, NULL, NULL, NULL);
	}
	else
	{
		path = NULL;
		/* FIXME only considers one selected item */
#if GTK_CHECK_VERSION(2, 6, 0)
		if(view != BV_DETAILS)
			gtk_icon_view_get_cursor(GTK_ICON_VIEW(
						browser->iconview), &path,
					NULL);
		else
#endif
			gtk_tree_view_get_cursor(GTK_TREE_VIEW(
						browser->detailview), &path,
					NULL);
	}
	return path;
}

static void _view_on_button_press_directory(GtkWidget * menu, IconCallback * ic)
{
	GtkWidget * menuitem;

	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_view_on_button_press_icon_open), ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(
			_("Open in new _window"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("window-new",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_view_on_button_press_icon_open_new_window),
			ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_CUT, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(on_cut),
			ic->browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(on_copy),
			ic->browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
	if(ic->browser->selection == NULL)
		gtk_widget_set_sensitive(menuitem, FALSE);
	else /* FIXME only if just this one is selected */
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_view_on_button_press_icon_paste), ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	if(ic->ismnt)
	{
		menuitem = gtk_menu_item_new_with_mnemonic(_("_Unmount"));
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_view_on_button_press_icon_unmount), ic);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_view_on_button_press_icon_delete), ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

static void _view_on_button_press_file(Browser * browser, GtkWidget * menu,
		char * mimetype, IconCallback * ic)
{
	GtkWidget * menuitem;

	_view_on_button_press_mime(browser->mime, mimetype, "open",
			GTK_STOCK_OPEN, G_CALLBACK(
				_view_on_button_press_icon_open), ic, menu);
	_view_on_button_press_mime(browser->mime, mimetype, "edit",
#if GTK_CHECK_VERSION(2, 6, 0)
			GTK_STOCK_EDIT,
#else
			"_Edit",
#endif
			G_CALLBACK(_view_on_button_press_icon_edit), ic, menu);
	if(ic->isexec)
	{
		menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_EXECUTE,
				NULL);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_view_on_button_press_icon_run), ic);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	menuitem = gtk_menu_item_new_with_mnemonic(_("Open _with..."));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_view_on_button_press_icon_open_with), ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_CUT, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(on_cut),
			browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(on_copy),
			browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
	gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_view_on_button_press_icon_delete), ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

static void _view_on_button_press_mime(Mime * mime, char const * mimetype,
		char const * action, char const * label, GCallback callback,
		IconCallback * ic, GtkWidget * menu)
{
	GtkWidget * menuitem;

	if(mime == NULL || mime_get_handler(mime, mimetype, action) == NULL)
		return;
	if(strncmp(label, "gtk-", 4) == 0)
		menuitem = gtk_image_menu_item_new_from_stock(label, NULL);
	else
		menuitem = gtk_menu_item_new_with_mnemonic(label);
	g_signal_connect_swapped(menuitem, "activate", callback, ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

static gboolean _view_on_button_press_show(Browser * browser, GdkEventButton * event,
		GtkWidget * menu)
{
#if GTK_CHECK_VERSION(2, 6, 0)
	if(browser_get_view(browser) != BV_DETAILS)
		gtk_menu_attach_to_widget(GTK_MENU(menu), browser->iconview,
				NULL);
	else
#endif
		gtk_menu_attach_to_widget(GTK_MENU(menu), browser->detailview,
				NULL);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button,
			event->time);
	return TRUE;
}

static void _view_on_button_press_icon_delete(gpointer data)
{
	IconCallback * cb = data;

	/* FIXME not selected => cursor */
	on_delete(cb->browser);
}

static void _view_on_button_press_icon_open(gpointer data)
{
	IconCallback * cb = data;

	if(cb->isdir)
		browser_set_location(cb->browser, cb->path);
	else if(cb->browser->mime != NULL)
		mime_action(cb->browser->mime, "open", cb->path);
}

static void _view_on_button_press_icon_open_new_window(gpointer data)
{
	IconCallback * cb = data;

	if(!cb->isdir)
		return;
	browserwindow_new(cb->path);
}

static void _view_on_button_press_icon_edit(gpointer data)
{
	IconCallback * cb = data;

	if(cb->browser->mime != NULL)
		mime_action(cb->browser->mime, "edit", cb->path);
}

static void _view_on_button_press_icon_run(gpointer data)
	/* FIXME does not work with scripts */
{
	IconCallback * cb = data;
	GtkWidget * dialog;
	int res;
	GError * error = NULL;
	char * argv[2];

	dialog = gtk_message_dialog_new((cb->browser->window != NULL)
			? GTK_WINDOW(cb->browser->window) : NULL,
			GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
			GTK_BUTTONS_YES_NO,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Warning"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s", _("Are you sure you want to execute this file?"));
	gtk_window_set_title(GTK_WINDOW(dialog), _("Warning"));
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if(res != GTK_RESPONSE_YES)
		return;
	argv[0] = cb->path;
	if(g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, &error) != TRUE)
	{
		browser_error(cb->browser, error->message, 1);
		g_error_free(error);
	}
}

static void _view_on_button_press_icon_open_with(gpointer data)
{
	IconCallback * cb = data;

	browser_open_with(cb->browser, cb->path);
}

static void _view_on_button_press_icon_paste(gpointer data)
{
	IconCallback * cb = data;
	char const * location;

	if((location = browser_get_location(cb->browser)) == NULL)
		return;
	/* XXX the following assignments are totally ugly */
	if(cb->path != NULL)
		cb->browser->current->data = cb->path;
	browser_selection_paste(cb->browser);
	cb->browser->current->data = location;
}

static void _view_on_button_press_icon_unmount(gpointer data)
{
	IconCallback * cb = data;

#ifndef unmount
	errno = ENOSYS;
#else
	if(unmount(cb->path, 0) != 0)
#endif
		browser_error(cb->browser, strerror(errno), 1);
}

static gboolean _view_on_button_press_popup(Browser * browser,
		GdkEventButton * event, GtkWidget * menu, IconCallback * ic)
{
	GtkWidget * menuitem;
	GtkWidget * submenu;
#if GTK_CHECK_VERSION(2, 8, 0)
	GtkWidget * image;
#endif

	browser_unselect_all(browser);
	/* new submenu */
	menuitem = gtk_menu_item_new_with_label(_("New"));
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
#if GTK_CHECK_VERSION(2, 8, 0) /* XXX actually depends on the icon theme */
	menuitem = gtk_image_menu_item_new_with_label(_("Folder"));
	image = gtk_image_new_from_icon_name("folder-new", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
#else
	menuitem = gtk_menu_item_new_with_label(_("Folder"));
#endif
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_view_on_button_press_popup_new_folder), ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_menu_item_new_with_label(_("Symbolic link..."));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_view_on_button_press_popup_new_symlink), ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_label(_("Text file"));
	image = gtk_image_new_from_icon_name("stock_new-text",
			GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_view_on_button_press_popup_new_text_file), ic);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	/* cut/copy/paste */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_CUT, NULL);
	gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY, NULL);
	gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
	if(browser->selection != NULL)
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_view_on_button_press_icon_paste), ic);
	else
		gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PROPERTIES,
			NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				on_properties), browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	return _view_on_button_press_show(browser, event, menu);
}

static void _view_on_button_press_popup_new_text_file(gpointer data)
{
	char const * newtext = _("New text file.txt");
	IconCallback * ic = data;
	Browser * browser = ic->browser;
	char const * location;
	size_t len;
	char * path;
	int fd;

	if((location = browser_get_location(browser)) == NULL)
		return;
	len = strlen(location) + strlen(newtext) + 2;
	if((path = malloc(len)) == NULL)
	{
		browser_error(browser, strerror(errno), 1);
		return;
	}
	snprintf(path, len, "%s/%s", location, newtext);
	if((fd = creat(path, 0666)) < 0)
		browser_error(browser, strerror(errno), 1);
	else
		close(fd);
	free(path);
}

static void _view_on_button_press_popup_new_folder(gpointer data)
{
	IconCallback * ic = data;
	Browser * browser = ic->browser;

	on_new_folder(browser);
}

static void _view_on_button_press_popup_new_symlink(gpointer data)
{
	IconCallback * ic = data;
	Browser * browser = ic->browser;

	on_new_symlink(browser);
}

static void _view_on_detail_default(GtkTreeView * view, GtkTreePath * path,
		GtkTreeViewColumn * column, gpointer data)
{
	Browser * browser = data;

	if(GTK_TREE_VIEW(browser->detailview) != view)
		return;
	_view_on_detail_default_do(browser, path);
}

static void _view_on_detail_default_do(Browser * browser, GtkTreePath * path)
{
	char * location;
	GtkTreeIter iter;
	gboolean is_dir;

	if(gtk_tree_model_get_iter(GTK_TREE_MODEL(browser->store), &iter, path)
			== FALSE)
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(browser->store), &iter, BC_PATH,
			&location, BC_IS_DIRECTORY, &is_dir, -1);
	if(is_dir)
		browser_set_location(browser, location);
	else if(browser->mime == NULL
			|| mime_action(browser->mime, "open", location) != 0)
		browser_open_with(browser, location);
	g_free(location);
}

static void _view_on_filename_edited(GtkCellRendererText * renderer,
		gchar * path, gchar * filename, gpointer data)
{
	Browser * browser = data;
	GtkTreeModel * model = GTK_TREE_MODEL(browser->store);
	GtkTreeIter iter;
	int isdir = 0;
	char * to;
	ssize_t len;
	char * q;
	char * f = filename;
	GError * error = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\") \"%s\"\n", __func__, path,
			filename);
#endif
	if(gtk_tree_model_get_iter_from_string(model, &iter, path) != TRUE)
		return; /* XXX report error */
	path = NULL;
	gtk_tree_model_get(model, &iter, BC_IS_DIRECTORY, &isdir, BC_PATH,
			&path, -1);
	if(path == NULL)
		return; /* XXX report error */
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, path);
#endif
	if((q = strrchr(path, '/')) == NULL)
	{
		free(path);
		return; /* XXX report error */
	}
	len = q - path;
	/* obtain the real new filename */
	if((q = g_filename_from_utf8(filename, -1, NULL, NULL, &error)) == NULL)
	{
		browser_error(NULL, error->message, 1);
		g_error_free(error);
	}
	else
		f = q;
	/* generate the complete new path */
	if((to = malloc(len + strlen(f) + 2)) == NULL)
	{
		browser_error(NULL, strerror(errno), 1);
		free(path);
		return;
	}
	strncpy(to, path, len);
	sprintf(&to[len], "/%s", f);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, to);
#endif
	/* rename */
	if(rename(path, to) != 0)
		browser_error(browser, strerror(errno), 1);
	else if(strchr(filename, '/') == NULL)
		gtk_list_store_set(browser->store, &iter, BC_PATH, to,
				BC_DISPLAY_NAME, filename, -1);
	free(to);
	free(q);
	free(path);
}

static gboolean _view_on_popup_menu(GtkWidget * widget, gpointer data)
{
	GdkEventButton event;

	memset(&event, 0, sizeof(event));
	event.type = GDK_BUTTON_PRESS;
	event.button = 0;
	event.time = gtk_get_current_event_time();
	return _view_on_button_press(widget, &event, data);
}


/* useful */
/* browser_plugin_refresh */
static void _plugin_refresh_do(Browser * browser, char const * path);
static void _plugin_refresh_do_list(Browser * browser, GList * list);

static void _browser_plugin_refresh(Browser * browser)
{
	char const * location;
	GtkTreeModel * model = GTK_TREE_MODEL(browser->store);
	GtkTreeIter iter;
	GList * sel;
	GList * s;
	GList * l;
	gchar * path;

	if((sel = _browser_get_selection(browser)) == NULL)
	{
		if((location = browser_get_location(browser)) != NULL)
			_plugin_refresh_do(browser, location);
		return;
	}
	for(l = NULL, s = sel; s != NULL; s = s->next)
	{
		if(gtk_tree_model_get_iter(model, &iter, s->data) == FALSE)
			continue;
		gtk_tree_model_get(model, &iter, BC_PATH, &path, -1);
		l = g_list_append(l, path);
	}
	_plugin_refresh_do_list(browser, l);
	g_list_foreach(l, (GFunc)g_free, NULL);
	g_list_free(l);
	g_list_foreach(sel, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(sel);
}

static void _plugin_refresh_do(Browser * browser, char const * path)
{
	GtkTreeModel * model = GTK_TREE_MODEL(browser->pl_store);
	GtkTreeIter iter;
	BrowserPluginDefinition * bpd;
	BrowserPlugin * bp;
	GList * l;

	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(browser->pl_combo),
				&iter) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, BPC_BROWSERPLUGINDEFINITION, &bpd,
			BPC_BROWSERPLUGIN, &bp, -1);
	if(bpd->refresh != NULL)
	{
		l = g_list_append(NULL, path);
		bpd->refresh(bp, l);
		g_list_free(l);
	}
}

static void _plugin_refresh_do_list(Browser * browser, GList * list)
{
	GtkTreeModel * model = GTK_TREE_MODEL(browser->pl_store);
	GtkTreeIter iter;
	BrowserPluginDefinition * bpd;
	BrowserPlugin * bp;

	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(browser->pl_combo),
				&iter) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, BPC_BROWSERPLUGINDEFINITION, &bpd,
			BPC_BROWSERPLUGIN, &bp, -1);
	if(bpd->refresh != NULL)
		bpd->refresh(bp, list);
}


/* browser_refresh_do */
static void _refresh_title(Browser * browser);
static void _refresh_path(Browser * browser);
static void _refresh_new(Browser * browser);
static void _refresh_current(Browser * browser);

static void _browser_refresh_do(Browser * browser, DIR * dir, struct stat * st)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %s\n", __func__,
			(char *)browser->current->data);
#endif
	if(browser->refresh_id != 0)
		g_source_remove(browser->refresh_id);
	browser->refresh_id = 0;
	if(browser->refresh_dir != NULL)
		browser_vfs_closedir(browser->refresh_dir);
	browser->refresh_dir = dir;
	browser->refresh_mti = st->st_mtime;
	browser->refresh_cnt = 0;
	browser->refresh_hid = 0;
	_refresh_title(browser);
	_refresh_path(browser);
	_browser_set_status(browser, _("Refreshing folder..."));
	_browser_plugin_refresh(browser);
	if(st->st_dev != browser->refresh_dev
			|| st->st_ino != browser->refresh_ino)
	{
		browser->refresh_dev = st->st_dev;
		browser->refresh_ino = st->st_ino;
		_refresh_new(browser);
	}
	else
		_refresh_current(browser);
}

static void _refresh_title(Browser * browser)
{
	char const * title;
	char * p;
	GError * error = NULL;
	char buf[256];

	if(browser->window == NULL)
		return;
	title = browser_get_location(browser);
	if((p = g_filename_to_utf8(title, -1, NULL, NULL, &error)) == NULL)
	{
		browser_error(NULL, error->message, 1);
		g_error_free(error);
	}
	else
		title = p;
	snprintf(buf, sizeof(buf), "%s%s%s", _("File manager"), " - ", title);
	free(p);
	gtk_window_set_title(GTK_WINDOW(browser->window), buf);
}

static void _refresh_path(Browser * browser)
{
	static unsigned int cnt = 0;
	char const * location;
	GtkWidget * widget;
	char * p;
	GError * error = NULL;
	unsigned int i;
	char * q;

	location = browser_get_location(browser);
	widget = gtk_bin_get_child(GTK_BIN(browser->tb_path));
	if((p = g_filename_to_utf8(location, -1, NULL, NULL, &error)) == NULL)
	{
		browser_error(NULL, error->message, 1);
		g_error_free(error);
	}
	gtk_entry_set_text(GTK_ENTRY(widget), (p != NULL) ? p : location);
	free(p);
	for(i = 0; i < cnt; i++)
#if GTK_CHECK_VERSION(2, 24, 0)
		gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(browser->tb_path),
				0);
#else
		gtk_combo_box_remove_text(GTK_COMBO_BOX(browser->tb_path), 0);
#endif
	if((p = g_path_get_dirname(location)) == NULL)
		return;
	if(strcmp(p, ".") != 0)
	{
#if GTK_CHECK_VERSION(2, 24, 0)
		gtk_combo_box_text_append_text(
				GTK_COMBO_BOX_TEXT(browser->tb_path), p);
#else
		gtk_combo_box_append_text(GTK_COMBO_BOX(browser->tb_path), p);
#endif
		for(cnt = 1; strcmp(p, "/") != 0; cnt++)
		{
			q = g_path_get_dirname(p);
			g_free(p);
			p = q;
#if GTK_CHECK_VERSION(2, 24, 0)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(
						browser->tb_path), p);
#else
			gtk_combo_box_append_text(GTK_COMBO_BOX(
						browser->tb_path), p);
#endif
		}
	}
	g_free(p);
}


/* config_load_boolean */
static int _config_load_boolean(Config * config, char const * variable,
		gboolean * value)
{
	char const * str;

	if((str = config_get(config, NULL, variable)) == NULL)
		return -1;
	if(strcmp(str, "0") == 0)
		*value = FALSE;
	else if(strcmp(str, "1") == 0)
		*value = TRUE;
	else
		return -1;
	return 0;
}


/* config_load_string */
static int _config_load_string(Config * config, char const * variable,
		char ** value)
{
	char const * str;
	char * p;

	if((str = config_get(config, NULL, variable)) == NULL)
		return 0;
	if((p = strdup(str)) == NULL)
		return -1;
	free(*value);
	*value = p;
	return 0;
}


/* config_save_boolean */
static int _config_save_boolean(Config * config, char const * variable,
		gboolean value)
{
	return config_set(config, NULL, variable, value ? "1" : "0");
}


/* callbacks */
/* browser_on_plugin_combo */
static void _browser_on_plugin_combo_change(gpointer data)
{
	Browser * browser = data;
	GtkTreeModel * model = GTK_TREE_MODEL(browser->pl_store);
	GtkTreeIter iter;
	gboolean valid;
	BrowserPluginDefinition * bpd;
	BrowserPlugin * bp;
	GtkWidget * widget;

	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(browser->pl_store), &iter,
				BPC_BROWSERPLUGINDEFINITION, &bpd,
				BPC_BROWSERPLUGIN, &bp,
				BPC_WIDGET, &widget, -1);
		/* signal the previous plug-in that it is no longer active */
		if(gtk_widget_get_visible(widget) && bpd->refresh != NULL)
			bpd->refresh(bp, NULL);
		gtk_widget_hide(widget);
	}
	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(browser->pl_combo),
				&iter) != TRUE)
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(browser->pl_store), &iter, BPC_WIDGET,
			&widget, -1);
	gtk_widget_show(widget);
	_browser_plugin_refresh(browser);
}


/* browser_on_selection_changed */
static void _browser_on_selection_changed(gpointer data)
{
	Browser * browser = data;
	
	_browser_plugin_refresh(browser);
}
