/* $Id$ */
static char const _copyright[] =
"Copyright © 2007-2016 Pierre Pronchery <khorben@defora.org>";
/* This file is part of DeforaOS Desktop Browser */
static char const _license[] =
"view is free software; you can redistribute it and/or modify it under the\n"
"terms of the GNU General Public License version 3 as published by the Free\n"
"Software Foundation.\n"
"\n"
"view is distributed in the hope that it will be useful, but WITHOUT ANY\n"
"WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS\n"
"FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more\n"
"details.\n"
"\n"
"You should have received a copy of the GNU General Public License along with\n"
"view; if not, see <http://www.gnu.org/licenses/>.\n";



#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <System.h>
#include <Desktop.h>
#include "../include/Browser/vfs.h"
#define Browser View /* XXX */
#include "browser.h"
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) (string)

#include "common.c"

/* constants */
#ifndef PROGNAME
# define PROGNAME	"view"
#endif
#ifndef PROGNAME_PROPERTIES
# define PROGNAME_PROPERTIES	"properties"
#endif

#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef BINDIR
# define BINDIR		PREFIX "/bin"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* view */
/* private */
/* types */
struct _Browser
{
	/* internal */
	Mime * mime;
	char * filename;

	/* plugins */
	BrowserPluginHelper helper;

	/* widgets */
	GtkWidget * window;
	GtkWidget * ab_window;
};


/* constants */
#ifndef EMBEDDED
static char const * _view_authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};
#endif


/* prototypes */
static View * _view_new(Mime * mime, char const * filename);
static View * _view_new_open(Mime * mime);
static void _view_delete(View * view);

/* accessors */
static String const * _view_config_get(View * view, String const * section,
		String const * variable);
static int _view_config_set(View * view, String const * section,
		String const * variable, String const * value);
static GdkPixbuf * _view_get_icon(View * view, char const * filename,
		char const * type, struct stat * lst, struct stat * st,
		int size);
static Mime * _view_get_mime(View * view);
static char const * _view_get_type(View * view, char const * filename,
		mode_t mode);
static int _view_set_location(View * view, char const * filename);

/* useful */
static int _view_error(View * view, char const * message, int ret);
static void _view_open_with(View * view, char const * program);
static void _view_open_with_dialog(View * view);

/* helpers */
static int _view_helper_set_location(View * view, char const * filename);

/* callbacks */
#ifdef EMBEDDED
static void _view_on_close(gpointer data);
#endif
static gboolean _view_on_closex(gpointer data);
#ifndef EMBEDDED
static void _view_on_file_edit(gpointer data);
static void _view_on_file_open_with(gpointer data);
static void _view_on_file_properties(gpointer data);
static void _view_on_file_close(gpointer data);
static void _view_on_help_contents(gpointer data);
static void _view_on_help_about(gpointer data);
#endif
static void _view_on_open_with(gpointer data);
static void _view_on_properties(gpointer data);


/* constants */
#ifndef EMBEDDED
static DesktopMenu _view_menu_file[] =
{
	{ N_("Open _with..."), G_CALLBACK(_view_on_file_open_with), NULL, 0,
		0 },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Close"), G_CALLBACK(_view_on_file_close), GTK_STOCK_CLOSE,
		GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, NULL, NULL, 0, 0 }
};

static DesktopMenu _view_menu_file_edit[] =
{
	{ N_("_Edit"), G_CALLBACK(_view_on_file_edit), GTK_STOCK_EDIT,
		GDK_CONTROL_MASK, GDK_KEY_E },
	{ N_("Open _with..."), G_CALLBACK(_view_on_file_open_with), NULL, 0,
		0 },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Properties"), G_CALLBACK(_view_on_file_properties),
		GTK_STOCK_PROPERTIES, GDK_MOD1_MASK, GDK_KEY_Return },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Close"), G_CALLBACK(_view_on_file_close), GTK_STOCK_CLOSE,
		GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, NULL, NULL, 0, 0 }
};

static DesktopMenu _view_menu_help[] =
{
	{ N_("Contents"), G_CALLBACK(_view_on_help_contents), "help-contents",
		0, GDK_KEY_F1 },
# if GTK_CHECK_VERSION(2, 6, 0)
	{ N_("_About"), G_CALLBACK(_view_on_help_about), GTK_STOCK_ABOUT, 0,
		0 },
# else
	{ N_("_About"), G_CALLBACK(_view_on_help_about), NULL, 0, 0 },
# endif
	{ NULL, NULL, NULL, 0, 0 }
};

static DesktopMenubar _view_menubar[] =
{
	{ N_("_File"), _view_menu_file },
	{ N_("_Help"), _view_menu_help },
	{ NULL, NULL }
};

static DesktopMenubar _view_menubar_edit[] =
{
	{ N_("_File"), _view_menu_file_edit },
	{ N_("_Help"), _view_menu_help },
	{ NULL, NULL }
};
#else
static DesktopAccel _view_accel[] =
{
	{ G_CALLBACK(_view_on_close), GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, 0, 0 }
};
#endif /* EMBEDDED */


/* variables */
static Mime * _mime = NULL;
static unsigned int _view_cnt = 0;


/* functions */
/* view_new */
static GtkWidget * _new_load(View * view);

static View * _view_new(Mime * mime, char const * filename)
{
	View * view;
	struct stat st;
	char const * type;
	char buf[256];
	GtkAccelGroup * group;
	GtkWidget * vbox;
	GtkWidget * widget;

	if((view = malloc(sizeof(*view))) == NULL)
		return NULL; /* FIXME handle error */
	view->mime = mime;
	view->filename = NULL;
	view->helper.browser = view;
	view->helper.config_get = _view_config_get;
	view->helper.config_set = _view_config_set;
	view->helper.error = _view_error;
	view->helper.get_icon = _view_get_icon;
	view->helper.get_mime = _view_get_mime;
	view->helper.get_type = _view_get_type;
	view->helper.set_location = _view_helper_set_location;
	view->window = NULL;
	view->ab_window = NULL;
	_view_cnt++;
	if((view->filename = strdup(filename)) == NULL
			|| lstat(filename, &st) != 0)
	{
		_view_error(view, strerror(errno), 1);
		_view_delete(view);
		return NULL;
	}
	if((type = mime_type(mime, filename)) == NULL)
	{
		_view_error(view, _("Unknown file type"), 1);
		_view_delete(view);
		return NULL;
	}
	group = gtk_accel_group_new();
	view->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_add_accel_group(GTK_WINDOW(view->window), group);
	g_object_unref(group);
	snprintf(buf, sizeof(buf), "%s%s", _("View - "), filename);
	gtk_window_set_title(GTK_WINDOW(view->window), buf);
	g_signal_connect_swapped(view->window, "delete-event", G_CALLBACK(
				_view_on_closex), view);
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	vbox = gtk_vbox_new(FALSE, 0);
#endif
#ifndef EMBEDDED
	widget = desktop_menubar_create(
			(mime_get_handler(mime, type, "edit") != NULL)
			? _view_menubar_edit : _view_menubar, view, group);
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);
#else
	desktop_accel_create(_view_accel, view, group);
#endif
	if((widget = _new_load(view)) == NULL)
	{
		_view_delete(view);
		return NULL;
	}
	gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(view->window), vbox);
	/* XXX fit the window to its content */
	gtk_window_set_default_size(GTK_WINDOW(view->window), 600, 400);
	gtk_widget_show_all(view->window);
	return view;
}

static GtkWidget * _new_load(View * view)
{
	Plugin * p;
	BrowserPluginDefinition * bpd;
	BrowserPlugin * bp;
	GList * l;
	GtkWidget * widget;

	if((p = plugin_new(LIBDIR, PACKAGE, "plugins", "preview")) == NULL)
		return NULL;
	if((bpd = plugin_lookup(p, "plugin")) == NULL)
	{
		plugin_delete(p);
		return NULL;
	}
	if(bpd->init == NULL
			|| bpd->destroy == NULL
			|| bpd->get_widget == NULL
			|| (bp = bpd->init(&view->helper)) == NULL)
	{
		plugin_delete(p);
		return NULL;
	}
	widget = bpd->get_widget(bp);
	l = g_list_append(NULL, view->filename);
	bpd->refresh(bp, l);
	g_list_free(l);
	return widget;
}


/* view_new_open */
static View * _view_new_open(Mime * mime)
{
	View * ret;
	GtkWidget * dialog;
	char * filename = NULL;

	dialog = gtk_file_chooser_dialog_new(_("View file..."), NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
					dialog));
	gtk_widget_destroy(dialog);
	if(filename == NULL)
		return NULL;
	ret = _view_new(mime, filename);
	free(filename);
	return ret;
}


/* view_delete */
static void _view_delete(View * view)
{
	free(view->filename);
	if(view->ab_window != NULL)
		gtk_widget_destroy(view->ab_window);
	if(view->window != NULL)
		gtk_widget_destroy(view->window);
	free(view);
	if(--_view_cnt == 0)
		gtk_main_quit();
}


/* accessors */
/* view_config_get */
static String const * _view_config_get(View * view, String const * section,
		String const * variable)
{
	if(section != NULL)
	{
		if(strcmp(section, "preview") == 0
				&& strcmp(variable, "ellipsize") == 0)
			return "0";
		if(strcmp(section, "preview") == 0
				&& strcmp(variable, "label") == 0)
			return "0";
		if(strcmp(section, "preview") == 0
				&& strcmp(variable, "size") == 0)
			return "-1";
	}
	return NULL;
}


/* view_config_set */
static int _view_config_set(View * view, String const * section,
		String const * variable, String const * value)
{
	/* FIXME implement */
	return -1;
}


/* view_get_icon */
static GdkPixbuf * _view_get_icon(View * view, char const * filename,
		char const * type, struct stat * lst, struct stat * st,
		int size)
{
	return browser_vfs_mime_icon(view->mime, filename, type, lst, st, size);
}


/* view_get_mime */
static Mime * _view_get_mime(View * view)
{
	return view->mime;
}


/* view_get_type */
static char const * _view_get_type(View * view,
		char const * filename, mode_t mode)
{
	return browser_vfs_mime_type(view->mime, filename, mode);
}


/* view_set_location */
static int _view_set_location(View * view, char const * filename)
{
	char * p;

	if((p = strdup(filename)) == NULL)
		return -error_set_code(1, "%s: %s", filename, strerror(errno));
	free(view->filename);
	view->filename = p;
	return 0;
}


/* useful */
/* view_error
 * POST	view is deleted if ret != 0 */
static void _error_response(GtkWidget * widget, gint arg, gpointer data);
static int _error_text(char const * message, int ret);

static int _view_error(View * view, char const * message, int ret)
{
	GtkWidget * dialog;

	if(view == NULL && ret != 0) /* XXX */
		return _error_text(message, ret);
	dialog = gtk_message_dialog_new((view != NULL && view->window != NULL)
			? GTK_WINDOW(view->window) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s",
#endif
			message);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(
				_error_response), (ret != 0) ? view : NULL);
	gtk_widget_show(dialog);
	return ret;
}

static void _error_response(GtkWidget * widget, gint arg, gpointer data)
{
	View * view = data;

	if(view != NULL)
		_view_delete(view);
	gtk_widget_destroy(widget);
}

static int _error_text(char const * message, int ret)
{
	fprintf(stderr, "%s: %s\n", PROGNAME, message);
	return ret;
}


/* view_open_with */
static void _view_open_with(View * view, char const * program)
{
	char * argv[] = { NULL, NULL, NULL };
	const unsigned int flags = 0;
	GError * error = NULL;

	if(program == NULL)
	{
		_view_open_with_dialog(view);
		return;
	}
	if((argv[0] = strdup(program)) == NULL)
	{
		_view_error(view, strerror(errno), 1);
		return;
	}
	argv[1] = view->filename;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		_view_error(view, error->message, 1);
		g_error_free(error);
	}
	free(argv[0]);
}


/* view_open_with_dialog */
static void _view_open_with_dialog(View * view)
{
	GtkWidget * dialog;
	GtkFileFilter * filter;
	char * filename = NULL;

	dialog = gtk_file_chooser_dialog_new(_("Open with..."),
			GTK_WINDOW(view->window),
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL);
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
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
					dialog));
	gtk_widget_destroy(dialog);
	if(filename == NULL)
		return;
	_view_open_with(view, filename);
	g_free(filename);
}


/* helpers */
/* view_helper_set_location */
static int _view_helper_set_location(View * view, char const * filename)
{
	int res;

	if((res = _view_set_location(view, filename)) != 0)
		return -_view_error(view, error_get(NULL), 1);
	return 0;
}


/* callbacks */
#ifdef EMBEDDED
/* view_on_close */
static void _view_on_close(gpointer data)
{
	View * view = data;

	_view_on_closex(view);
}
#endif


/* view_on_closex */
static gboolean _view_on_closex(gpointer data)
{
	View * view = data;

	_view_delete(view);
	if(_view_cnt == 0)
		gtk_main_quit();
	return FALSE;
}


#ifndef EMBEDDED
/* view_on_file_edit */
static void _view_on_file_edit(gpointer data)
{
	View * view = data;

	if(mime_action(_mime, "edit", view->filename) != 0)
		_view_error(view, _("Could not edit file"), 0);
}


/* view_on_file_open_with */
static void _view_on_file_open_with(gpointer data)
{
	View * view = data;

	_view_on_open_with(view);
}


/* view_on_file_properties */
static void _view_on_file_properties(gpointer data)
{
	View * view = data;

	_view_on_properties(view);
}


/* view_on_file_close */
static void _view_on_file_close(gpointer data)
{
	View * view = data;

	_view_on_closex(view);
}


/* view_on_help_contents */
static void _view_on_help_contents(gpointer data)
{
	desktop_help_contents(PACKAGE, PROGNAME);
}


/* view_on_help_about */
static gboolean _about_on_closex(gpointer data);

static void _view_on_help_about(gpointer data)
{
	View * view = data;

	if(view->ab_window != NULL)
	{
		gtk_widget_show(view->ab_window);
		return;
	}
	view->ab_window = desktop_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(view->ab_window), GTK_WINDOW(
				view->window));
	desktop_about_dialog_set_authors(view->ab_window, _view_authors);
	desktop_about_dialog_set_copyright(view->ab_window, _copyright);
	desktop_about_dialog_set_logo_icon_name(view->ab_window,
			"system-file-manager");
	desktop_about_dialog_set_license(view->ab_window, _license);
	desktop_about_dialog_set_name(view->ab_window, PACKAGE);
	desktop_about_dialog_set_version(view->ab_window, VERSION);
	g_signal_connect_swapped(G_OBJECT(view->ab_window), "delete-event",
			G_CALLBACK(_about_on_closex), view);
	gtk_widget_show(view->ab_window);
}

static gboolean _about_on_closex(gpointer data)
{
	View * view = data;

	gtk_widget_hide(view->ab_window);
	return TRUE;
}
#endif /* EMBEDDED */


/* view_on_open_with */
static void _view_on_open_with(gpointer data)
{
	View * view = data;

	_view_open_with_dialog(view);
}


/* view_on_properties */
static void _view_on_properties(gpointer data)
{
	View * view = data;

	_view_open_with(view, BINDIR "/" PROGNAME_PROPERTIES);
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s file...\n"), PROGNAME);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;
	int i;
	Mime * mime;

	if(setlocale(LC_ALL, "") == NULL)
		_view_error(NULL, "setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "")) != -1)
		switch(o)
		{
			default:
				return _usage();
		}
	if((mime = mime_new(NULL)) == NULL)
		return _view_error(NULL, error_get(NULL), 2);
	if(optind == argc)
		_view_new_open(mime);
	else
		for(i = optind; i < argc; i++)
			_view_new(mime, argv[i]);
	if(_view_cnt)
		gtk_main();
	mime_delete(mime);
	return 0;
}
