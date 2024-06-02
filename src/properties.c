/* $Id$ */
/* Copyright (c) 2007-2024 Pierre Pronchery <khorben@defora.org> */
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
 * - add a file count and disk usage tab for directories */



#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <System.h>
#include "Browser/vfs.h"
#define Browser Properties /* XXX */
#include "browser/browser.h"
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) (string)

#define COMMON_GET_ABSOLUTE_PATH
#include "common.c"

/* constants */
#ifndef PROGNAME_PROPERTIES
# define PROGNAME_PROPERTIES	"properties"
#endif
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* properties */
/* private */
/* types */
struct _Browser
{
	/* internal */
	Config * config;
	Mime * mime;
	char * filename;

	/* plugins */
	BrowserPluginHelper helper;

	/* widgets */
	GtkIconTheme * theme;
	GtkWidget * window;
	GtkWidget * notebook;
};


/* variables */
static unsigned int _properties_cnt = 0; /* XXX set as static in _properties */

/* functions */
static int _properties(Mime * mime, char const * plugin,
		int filec, char * const filev[]);

/* properties */
static Properties * _properties_new(Mime * mime, char const * plugin,
		char const * filename);
static void _properties_delete(Properties * properties);

/* accessors */
static char const * _properties_config_get(Properties * properties,
		char const * section, char const * variable);
static int _properties_config_set(Properties * properties, char const * section,
		char const * variable, char const * value);
static GdkPixbuf * _properties_get_icon(Properties * properties,
		char const * filename, char const * type, struct stat * lst,
		struct stat * st, int size);
static int _properties_get_icon_size(Properties * properties, BrowserView view);
static Mime * _properties_get_mime(Properties * properties);
static char const * _properties_get_type(Properties * properties,
		char const * filename, mode_t mode);
static BrowserView _properties_get_view(Properties * properties);
static int _properties_set_location(Properties * properties,
		char const * filename);

/* useful */
static int _properties_error(Properties * properties, char const * message,
		int ret);
static int _properties_load(Properties * properties, char const * name);

/* helpers */
static int _properties_helper_set_location(Properties * properties,
		char const * filename);

/* callbacks */
static void _properties_on_close(gpointer data);
static gboolean _properties_on_closex(gpointer data);


/* functions */
/* properties */
static int _properties(Mime * mime, char const * plugin,
		int filec, char * const filev[])
{
	int ret = 0;
	int i;
	Properties * properties;
	char * p;

	for(i = 0; i < filec; i++)
	{
		if((p = _common_get_absolute_path(filev[i])) == NULL)
			ret |= 1;
		else if((properties = _properties_new(mime, plugin, p)) == NULL)
			ret |= 1;
		else
			_properties_cnt++;
		g_free(p);
	}
	return ret;
}


/* properties_new */
static int _new_load(Properties * properties, char const * plugin);

static Properties * _properties_new(Mime * mime, char const * plugin,
		char const * filename)
{
	Properties * properties;
	GtkWidget * vbox;
	GtkWidget * bbox;
	GtkWidget * widget;
	gchar * p;
	char buf[256];

	if(filename == NULL)
		return NULL;
	if((properties = malloc(sizeof(*properties))) == NULL)
	{
		_properties_error(NULL, strerror(errno), 1);
		return NULL;
	}
	properties->config = NULL;
	properties->mime = mime;
	properties->filename = strdup(filename);
	properties->helper.browser = properties;
	properties->helper.config_get = _properties_config_get;
	properties->helper.config_set = _properties_config_set;
	properties->helper.error = _properties_error;
	properties->helper.get_icon = _properties_get_icon;
	properties->helper.get_icon_size = _properties_get_icon_size;
	properties->helper.get_mime = _properties_get_mime;
	properties->helper.get_type = _properties_get_type;
	properties->helper.get_view = _properties_get_view;
	properties->helper.set_location = _properties_helper_set_location;
	properties->window = NULL;
	if(properties->filename == NULL)
	{
		_properties_delete(properties);
		return NULL;
	}
	properties->theme = gtk_icon_theme_get_default();
	/* window */
	properties->window = gtk_dialog_new();
	p = g_filename_display_basename(filename);
	snprintf(buf, sizeof(buf), "%s%s", _("Properties of "), p);
	g_free(p);
	gtk_window_set_default_size(GTK_WINDOW(properties->window), 300, 400);
	gtk_window_set_icon_name(GTK_WINDOW(properties->window),
			"stock_properties");
	gtk_window_set_title(GTK_WINDOW(properties->window), buf);
	g_signal_connect_swapped(properties->window, "delete-event",
			G_CALLBACK(_properties_on_closex), properties);
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(properties->window));
#else
	vbox = GTK_DIALOG(properties->window)->vbox;
#endif
	/* notebook */
	properties->notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(properties->notebook), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), properties->notebook, TRUE, TRUE, 0);
	gtk_widget_show_all(vbox);
	/* button box */
#if GTK_CHECK_VERSION(2, 14, 0)
	bbox = gtk_dialog_get_action_area(GTK_DIALOG(properties->window));
#else
	bbox = GTK_DIALOG(properties->window)->action_area;
#endif
	widget = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect_swapped(widget, "clicked",
			G_CALLBACK(_properties_on_close), properties);
	gtk_container_add(GTK_CONTAINER(bbox), widget);
	gtk_widget_show_all(bbox);
	if(_new_load(properties, plugin) != 0)
		_properties_error(properties, error_get(NULL), -1);
	else if(filename != NULL)
	{
		if(_properties_set_location(properties, filename) != 0)
			_properties_error(properties, error_get(NULL), -1);
		else
			gtk_widget_show(properties->window);
	}
	return properties;
}

static int _new_load(Properties * properties, char const * plugin)
{
	char const * plugins = NULL;
	char * p;
	char * q;
	size_t i;
	int cnt = 0;

	if((properties->config = config_new()) != NULL
			&& config_load_preferences(properties->config,
				BROWSER_CONFIG_VENDOR, PACKAGE,
				BROWSER_CONFIG_FILE) == 0
			&& (plugins = config_get(properties->config, NULL,
					"properties")) == NULL)
		plugins = "properties,preview";
	if(plugin != NULL)
	{
		if(_properties_load(properties, plugin) == 0)
			cnt++;
	}
	else if(plugins != NULL && strlen(plugins)
			&& (p = strdup(plugins)) != NULL)
	{
		/* XXX if plugins is only commas nothing will be loaded */
		for(q = p, i = 0;;)
		{
			if(q[i] == '\0')
			{
				if(_properties_load(properties, q) == 0)
					cnt++;
				break;
			}
			if(q[i++] != ',')
				continue;
			q[i - 1] = '\0';
			if(_properties_load(properties, q) == 0)
				cnt++;
			q += i;
			i = 0;
		}
		free(p);
	}
	else
	{
		if(_properties_load(properties, "properties") == 0)
			cnt++;
		if(_properties_load(properties, "preview") == 0)
			cnt++;
	}
	/* consider ourselves successful if at least one plug-in was loaded */
	return (cnt > 0) ? 0 : -1;
}


/* properties_delete */
static void _properties_delete(Properties * properties)
{
	if(properties->window != NULL)
		gtk_widget_destroy(properties->window);
	free(properties->filename);
	if(properties->config != NULL)
		config_delete(properties->config);
	free(properties);
	_properties_cnt--;
}


/* accessors */
/* properties_config_get */
static char const * _properties_config_get(Properties * properties,
		char const * section, char const * variable)
{
	if(properties->config == NULL)
		return NULL;
	return config_get(properties->config, section, variable);
}


/* properties_config_set */
static int _properties_config_set(Properties * properties, char const * section,
		char const * variable, char const * value)
{
	if(properties->config == NULL)
		return -1;
	return config_set(properties->config, section, variable, value);
}


/* properties_get_icon */
static GdkPixbuf * _properties_get_icon(Properties * properties,
		char const * filename, char const * type, struct stat * lst,
		struct stat * st, int size)
{
	return browser_vfs_mime_icon(properties->mime, filename, type, lst, st,
			size);
}


/* properties_get_icon_size */
static int _properties_get_icon_size(Properties * properties, BrowserView view)
{
	(void) properties;
	(void) view;

	return BROWSER_ICON_SIZE_ICONS;
}


/* properties_get_mime */
static Mime * _properties_get_mime(Properties * properties)
{
	return properties->mime;
}


/* properties_get_type */
static char const * _properties_get_type(Properties * properties,
		char const * filename, mode_t mode)
{
	return browser_vfs_mime_type(properties->mime, filename, mode);
}


/* properties_get_view */
static BrowserView _properties_get_view(Properties * properties)
{
	(void) properties;

	return BROWSER_VIEW_DETAILS;
}


/* properties_set_location */
static int _properties_set_location(Properties * properties,
		char const * filename)
{
	char * p;

	if((p = strdup(filename)) == NULL)
		return -error_set_code(1, "%s: %s", filename, strerror(errno));
	free(properties->filename);
	properties->filename = p;
	return 0;
}


/* useful */
/* properties_error */
static void _error_response(GtkWidget * widget, gint arg, gpointer data);
static int _error_text(char const * message, int ret);

static int _properties_error(Properties * properties, char const * message,
		int ret)
{
	GtkWidget * dialog;

	if(properties == NULL)
		return _error_text(message, ret);
	dialog = gtk_message_dialog_new((properties != NULL
				&& properties->window != NULL)
			? GTK_WINDOW(properties->window) : NULL, 0,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s", message);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	if(properties != NULL && properties->window != NULL)
	{
		gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
		gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(
					properties->window));
		gtk_widget_show(properties->window);
	}
	g_signal_connect(dialog, "response", G_CALLBACK(_error_response),
			(ret < 0) ? &_properties_cnt : NULL);
	gtk_widget_show(dialog);
	return ret;
}

static void _error_response(GtkWidget * widget, gint arg, gpointer data)
{
	unsigned int * cnt = data;
	(void) arg;

	if(cnt == NULL)
		gtk_widget_destroy(widget);
	else if(--(*cnt) == 0)
		gtk_main_quit();
	else
		gtk_widget_destroy(widget);
}

static int _error_text(char const * message, int ret)
{
	fprintf(stderr, "%s: %s\n", PROGNAME_PROPERTIES, message);
	return ret;
}


/* properties_load */
static int _properties_load(Properties * properties, char const * name)
{
	Plugin * p;
	BrowserPluginDefinition * bpd;
	BrowserPlugin * bp;
	GdkPixbuf * icon = NULL;
	GtkWidget * hbox;
	GtkWidget * widget;
	GList * l;

	if((p = plugin_new(LIBDIR, PACKAGE, "plugins", name)) == NULL)
		return -1;
	if((bpd = plugin_lookup(p, "plugin")) == NULL)
	{
		plugin_delete(p);
		return -1;
	}
	if(bpd->init == NULL || bpd->destroy == NULL || bpd->get_widget == NULL
			|| (bp = bpd->init(&properties->helper)) == NULL)
	{
		plugin_delete(p);
		return -1;
	}
	widget = bpd->get_widget(bp);
	l = g_list_append(NULL, properties->filename);
	bpd->refresh(bp, l);
	g_list_free(l);
	/* label */
	if(bpd->icon != NULL)
		icon = gtk_icon_theme_load_icon(properties->theme, bpd->icon,
				24, 0, NULL);
	if(icon == NULL)
		icon = gtk_icon_theme_load_icon(properties->theme,
				"gnome-settings", 24, 0, NULL);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_image_new_from_pixbuf(icon),
			FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_(bpd->name)), TRUE,
			TRUE, 0);
	gtk_widget_show_all(hbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(properties->notebook), widget,
			hbox);
	/* XXX configure the default plug-in somewhere */
	if(strcmp(name, "properties") == 0)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(
					properties->notebook), -1);
	return 0;
}


/* helpers */
static int _properties_helper_set_location(Properties * properties,
		char const * filename)
{
	int res;

	if((res = _properties_set_location(properties, filename)) != 0)
		return -_properties_error(properties, error_get(NULL), 1);
	return 0;
}


/* callbacks */
static void _properties_on_close(gpointer data)
{
	Properties * properties = data;

	_properties_delete(properties);
	if(_properties_cnt == 0)
		gtk_main_quit();
}

static gboolean _properties_on_closex(gpointer data)
{
	_properties_on_close(data);
	return FALSE;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-p plug-in] file...\n"),
			PROGNAME_PROPERTIES);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int ret;
	int o;
	Mime * mime;
	char const * plugin = NULL;

	if(setlocale(LC_ALL, "") == NULL)
		_properties_error(NULL, strerror(errno), 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "p:")) != -1)
		switch(o)
		{
			case 'p':
				plugin = optarg;
				break;
			default:
				return _usage();
		}
	if(optind == argc)
		return _usage();
	mime = mime_new(NULL);
	ret = _properties(mime, plugin, argc - optind, &argv[optind]);
	gtk_main();
	if(mime != NULL)
		mime_delete(mime);
	return (ret == 0) ? 0 : 2;
}
