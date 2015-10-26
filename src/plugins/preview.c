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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include "Browser.h"
#define _(string) gettext(string)
#define N_(string) (string)


/* Preview */
/* private */
/* types */
typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	char * path;
	guint source;
	ssize_t size;

	/* widgets */
	GtkWidget * widget;
	GtkWidget * name;
	GtkWidget * toolbar;
	GtkToolItem * open;
	GtkToolItem * edit;
	GtkToolItem * copy;
	GtkToolItem * select_all;
	GtkToolItem * zoom_100;
	GtkToolItem * zoom_fit;
	GtkToolItem * zoom_out;
	GtkToolItem * zoom_in;
	GtkWidget * view_image;
	GtkWidget * view_image_image;
	GtkWidget * view_text;
	GtkTextBuffer * view_text_tbuf;
} Preview;


/* constants */
#define PREVIEW_DEFAULT_SIZE	96


/* prototypes */
/* plug-in */
static Preview * _preview_init(BrowserPluginHelper * helper);
static void _preview_destroy(Preview * preview);
static GtkWidget * _preview_get_widget(Preview * preview);
static void _preview_refresh(Preview * preview, GList * selection);

/* callbacks */
static void _preview_on_copy(gpointer data);
static void _preview_on_edit(gpointer data);
static gboolean _preview_on_idle_image(gpointer data);
static gboolean _preview_on_idle_text(gpointer data);
static void _preview_on_open(gpointer data);
static void _preview_on_select_all(gpointer data);
static void _preview_on_zoom_100(gpointer data);
static void _preview_on_zoom_fit(gpointer data);
static void _preview_on_zoom_in(gpointer data);
static void _preview_on_zoom_out(gpointer data);


/* public */
/* variables */
BrowserPluginDefinition plugin =
{
	N_("Preview"),
	"gtk-find",
	NULL,
	_preview_init,
	_preview_destroy,
	_preview_get_widget,
	_preview_refresh
};


/* private */
/* functions */
/* preview_init */
static Preview * _preview_init(BrowserPluginHelper * helper)
{
	Preview * preview;
	PangoFontDescription * font;
	GtkWidget * vbox;
	GtkWidget * widget;

	if((preview = object_new(sizeof(*preview))) == NULL)
		return NULL;
	preview->helper = helper;
	preview->path = NULL;
	preview->source = 0;
	preview->size = PREVIEW_DEFAULT_SIZE;
	/* widgets */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	preview->widget = vbox;
	/* toolbar */
	preview->toolbar = gtk_toolbar_new();
	gtk_widget_set_no_show_all(preview->toolbar, TRUE);
	/* mime */
	preview->open = gtk_tool_button_new_from_stock(GTK_STOCK_OPEN);
	g_signal_connect_swapped(preview->open, "clicked", G_CALLBACK(
				_preview_on_open), preview);
	gtk_toolbar_insert(GTK_TOOLBAR(preview->toolbar), preview->open, -1);
	preview->edit = gtk_tool_button_new_from_stock(GTK_STOCK_EDIT);
	g_signal_connect_swapped(preview->edit, "clicked", G_CALLBACK(
				_preview_on_edit), preview);
	gtk_toolbar_insert(GTK_TOOLBAR(preview->toolbar), preview->edit, -1);
	/* copy */
	preview->copy = gtk_tool_button_new_from_stock(GTK_STOCK_COPY);
	g_signal_connect_swapped(preview->copy, "clicked", G_CALLBACK(
				_preview_on_copy), preview);
	gtk_toolbar_insert(GTK_TOOLBAR(preview->toolbar), preview->copy, -1);
	/* select all */
#if GTK_CHECK_VERSION(2, 10, 0)
	preview->select_all = gtk_tool_button_new_from_stock(
			GTK_STOCK_SELECT_ALL);
#else
	widget = gtk_image_new_from_icon_name("edit-select-all",
			gtk_toolbar_get_icon_size(GTK_TOOLBAR(
					preview->toolbar)));
	preview->select_all = gtk_tool_button_new(widget, "Select all");
#endif
	g_signal_connect_swapped(preview->select_all, "clicked", G_CALLBACK(
				_preview_on_select_all), preview);
	gtk_toolbar_insert(GTK_TOOLBAR(preview->toolbar), preview->select_all,
			-1);
	/* zoom */
	preview->zoom_100 = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_100);
	g_signal_connect_swapped(preview->zoom_100, "clicked", G_CALLBACK(
				_preview_on_zoom_100), preview);
	gtk_toolbar_insert(GTK_TOOLBAR(preview->toolbar), preview->zoom_100,
			-1);
	preview->zoom_fit = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_FIT);
	g_signal_connect_swapped(preview->zoom_fit, "clicked", G_CALLBACK(
				_preview_on_zoom_fit), preview);
	gtk_toolbar_insert(GTK_TOOLBAR(preview->toolbar), preview->zoom_fit,
			-1);
	preview->zoom_out = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
	g_signal_connect_swapped(preview->zoom_out, "clicked", G_CALLBACK(
				_preview_on_zoom_out), preview);
	gtk_toolbar_insert(GTK_TOOLBAR(preview->toolbar), preview->zoom_out,
			-1);
	preview->zoom_in = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
	g_signal_connect_swapped(preview->zoom_in, "clicked", G_CALLBACK(
				_preview_on_zoom_in), preview);
	gtk_toolbar_insert(GTK_TOOLBAR(preview->toolbar), preview->zoom_in, -1);
	gtk_box_pack_start(GTK_BOX(vbox), preview->toolbar, FALSE, TRUE, 0);
	/* name */
	preview->name = gtk_label_new(NULL);
	gtk_label_set_ellipsize(GTK_LABEL(preview->name),
			PANGO_ELLIPSIZE_MIDDLE);
	gtk_misc_set_alignment(GTK_MISC(preview->name), 0.0, 0.5);
	font = pango_font_description_new();
	pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(preview->name, font);
#else
	gtk_widget_modify_font(preview->name, font);
#endif
	pango_font_description_free(font);
	gtk_box_pack_start(GTK_BOX(vbox), preview->name, FALSE, TRUE, 0);
	/* image */
	preview->view_image = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(preview->view_image),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_no_show_all(preview->view_image, TRUE);
	preview->view_image_image = gtk_image_new();
	gtk_widget_show(preview->view_image_image);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_container_add(GTK_CONTAINER(preview->view_image),
			preview->view_image_image);
#else
	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(preview->view_image),
			preview->view_image_image);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), preview->view_image, TRUE, TRUE, 0);
	/* text */
	preview->view_text = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(preview->view_text),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_no_show_all(preview->view_text, TRUE);
	font = pango_font_description_new();
	pango_font_description_set_family(font, "monospace");
	widget = gtk_text_view_new();
	preview->view_text_tbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(
				widget));
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(widget), FALSE);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(widget), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(widget), GTK_WRAP_WORD_CHAR);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(widget, font);
#else
	gtk_widget_modify_font(widget, font);
#endif
	gtk_widget_show(widget);
	pango_font_description_free(font);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_container_add(GTK_CONTAINER(preview->view_text), widget);
#else
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(
				preview->view_text), widget);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), preview->view_text, TRUE, TRUE, 0);
	gtk_widget_show_all(vbox);
	return preview;
}


/* preview_destroy */
static void _preview_destroy(Preview * preview)
{
	if(preview->source != 0)
		g_source_remove(preview->source);
	free(preview->path);
	object_delete(preview);
}


/* preview_get_widget */
static GtkWidget * _preview_get_widget(Preview * preview)
{
	return preview->widget;
}


/* preview_refresh */
static void _refresh_mime(Preview * preview, Mime * mime, char const * type);
static int _refresh_name(Preview * preview, char const * path);
static void _refresh_reset(Preview * preview);

static void _preview_refresh(Preview * preview, GList * selection)
{
	char * path = (selection != NULL) ? selection->data : NULL;
	Mime * mime = preview->helper->get_mime(preview->helper->browser);
	struct stat st;
	char const image[6] = "image/";
	char const text[5] = "text/";
	char const * types[] = { "application/x-perl",
		"application/x-shellscript",
		"application/xml",
		"application/xslt+xml" };
	char const * type;
	size_t i;

	_refresh_reset(preview);
	if(path == NULL)
		return;
	if(_refresh_name(preview, path) != 0)
		return;
	/* ignore directories */
	if(lstat(path, &st) == 0 && S_ISDIR(st.st_mode))
		return;
	/* XXX avoid stat() and use vfs_mime_type() instead */
	if((type = mime_type(mime, path)) == NULL)
		return;
	_refresh_mime(preview, mime, type);
	if(strncmp(type, image, sizeof(image)) == 0)
		preview->source = g_idle_add(_preview_on_idle_image, preview);
	else if(strncmp(type, text, sizeof(text)) == 0)
		preview->source = g_idle_add(_preview_on_idle_text, preview);
	else
		for(i = 0; i < sizeof(types) / sizeof(*types); i++)
			if(strcmp(types[i], type) == 0)
			{
				preview->source = g_idle_add(
						_preview_on_idle_text, preview);
				break;
			}
}

static void _refresh_mime(Preview * preview, Mime * mime, char const * type)
{
	if(mime_get_handler(mime, type, "open") != NULL)
	{
		gtk_widget_show(GTK_WIDGET(preview->open));
		gtk_widget_show(preview->toolbar);
	}
	if(mime_get_handler(mime, type, "edit") != NULL)
	{
		gtk_widget_show(GTK_WIDGET(preview->edit));
		gtk_widget_show(preview->toolbar);
	}
}

static int _refresh_name(Preview * preview, char const * path)
{
	BrowserPluginHelper * helper = preview->helper;
	gchar * p;

	free(preview->path);
	if((preview->path = strdup(path)) == NULL)
		return -helper->error(helper->browser, strerror(errno), 1);
	p = g_filename_display_basename(path);
	gtk_label_set_text(GTK_LABEL(preview->name), p);
	g_free(p);
	return 0;
}

static void _refresh_reset(Preview * preview)
{
	if(preview->source != 0)
		g_source_remove(preview->source);
	preview->source = 0;
	gtk_widget_hide(preview->toolbar);
	gtk_widget_hide(GTK_WIDGET(preview->open));
	gtk_widget_hide(GTK_WIDGET(preview->edit));
	gtk_widget_hide(GTK_WIDGET(preview->copy));
	gtk_widget_hide(GTK_WIDGET(preview->select_all));
	gtk_widget_hide(GTK_WIDGET(preview->zoom_100));
	gtk_widget_hide(GTK_WIDGET(preview->zoom_fit));
	gtk_widget_hide(GTK_WIDGET(preview->zoom_out));
	gtk_widget_hide(GTK_WIDGET(preview->zoom_in));
	gtk_widget_hide(preview->view_image);
	gtk_widget_hide(preview->view_text);
}


/* callbacks */
/* preview_on_copy */
static void _preview_on_copy(gpointer data)
{
	Preview * preview = data;
	GtkClipboard * clipboard;

	clipboard = gtk_widget_get_clipboard(preview->view_text,
			GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_copy_clipboard(preview->view_text_tbuf, clipboard);
}


/* preview_on_edit */
static void _preview_on_edit(gpointer data)
{
	Preview * preview = data;
	Mime * mime = preview->helper->get_mime(preview->helper->browser);

	if(preview->path != NULL)
		mime_action(mime, "edit", preview->path);
}


/* preview_on_idle_image */
static void _idle_image_100(Preview * preview);
static void _idle_image_scale(Preview * preview);

static gboolean _preview_on_idle_image(gpointer data)
{
	Preview * preview = data;

	preview->source = 0;
	/* show the toolbar */
	gtk_widget_show(GTK_WIDGET(preview->zoom_100));
	gtk_widget_show(GTK_WIDGET(preview->zoom_fit));
	gtk_widget_show(GTK_WIDGET(preview->zoom_out));
	gtk_widget_show(GTK_WIDGET(preview->zoom_in));
	gtk_widget_show(preview->toolbar);
	if(preview->size < 0)
		_idle_image_100(preview);
	else
		_idle_image_scale(preview);
	gtk_widget_show(preview->view_image);
	return FALSE;
}

static void _idle_image_100(Preview * preview)
{
	BrowserPluginHelper * helper = preview->helper;
	GdkPixbufAnimation * pixbuf;
	GError * error = NULL;

	if((pixbuf = gdk_pixbuf_animation_new_from_file(preview->path, &error))
			== NULL)
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
		return;
	}
	if(error != NULL)
	{
		helper->error(NULL, error->message, 1);
		g_error_free(error);
	}
	gtk_image_set_from_animation(GTK_IMAGE(preview->view_image_image),
			pixbuf);
	g_object_unref(pixbuf);
}

static void _idle_image_scale(Preview * preview)
{
	BrowserPluginHelper * helper = preview->helper;
	GdkPixbuf * pixbuf;
	GError * error = NULL;

#if GTK_CHECK_VERSION(2, 6, 0)
	if((pixbuf = gdk_pixbuf_new_from_file_at_scale(preview->path,
					preview->size, preview->size, TRUE,
					&error)) == NULL)
#else
	if((pixbuf = gdk_pixbuf_new_from_file_at_size(preview->path,
					preview->size, preview->size, &error))
			== NULL)
#endif
	{
		helper->error(helper->browser, error->message, 1);
		g_error_free(error);
		return;
	}
	if(error != NULL)
	{
		helper->error(NULL, error->message, 1);
		g_error_free(error);
	}
	gtk_image_set_from_pixbuf(GTK_IMAGE(preview->view_image_image), pixbuf);
	g_object_unref(pixbuf);
}


/* preview_on_idle_text */
static gboolean _preview_on_idle_text(gpointer data)
{
	Preview * preview = data;
	BrowserPluginHelper * helper = preview->helper;
	int fd;
	char buf[256];
	ssize_t s;

	preview->source = 0;
	gtk_widget_show(GTK_WIDGET(preview->copy));
	gtk_widget_show(GTK_WIDGET(preview->select_all));
	gtk_widget_show(preview->toolbar);
	gtk_text_buffer_set_text(preview->view_text_tbuf, "", 0);
	if((fd = open(preview->path, O_RDONLY)) < 0)
	{
		helper->error(helper->browser, strerror(errno), 1);
		return FALSE;
	}
	/* FIXME use a GIOChannel instead */
	if((s = read(fd, buf, sizeof(buf))) > 0)
	{
		if(s == sizeof(buf))
		{
			buf[sizeof(buf) - 3] = '.';
			buf[sizeof(buf) - 2] = '.';
			buf[sizeof(buf) - 1] = '.';
		}
		gtk_text_buffer_set_text(preview->view_text_tbuf, buf, s);
	}
	close(fd);
	gtk_widget_show(preview->view_text);
	return FALSE;
}


/* preview_on_open */
static void _preview_on_open(gpointer data)
{
	Preview * preview = data;
	Mime * mime = preview->helper->get_mime(preview->helper->browser);

	if(preview->path != NULL)
		mime_action(mime, "open", preview->path);
}


/* preview_on_select_all */
static void _preview_on_select_all(gpointer data)
{
	Preview * preview = data;
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_start_iter(preview->view_text_tbuf, &start);
	gtk_text_buffer_get_end_iter(preview->view_text_tbuf, &end);
	gtk_text_buffer_select_range(preview->view_text_tbuf, &start, &end);
}


/* preview_on_zoom_100 */
static void _preview_on_zoom_100(gpointer data)
{
	Preview * preview = data;

	preview->size = -1;
	if(preview->source != 0)
		g_source_remove(preview->source);
	/* XXX may not always be an image */
	preview->source = g_idle_add(_preview_on_idle_image, preview);
}


/* preview_on_zoom_fit */
static void _preview_on_zoom_fit(gpointer data)
{
	Preview * preview = data;

	preview->size = PREVIEW_DEFAULT_SIZE;
	if(preview->source != 0)
		g_source_remove(preview->source);
	/* XXX may not always be an image */
	preview->source = g_idle_add(_preview_on_idle_image, preview);
}


/* preview_on_zoom_in */
static void _preview_on_zoom_in(gpointer data)
{
	Preview * preview = data;

	preview->size = ((preview->size > 0) ? preview->size
			: PREVIEW_DEFAULT_SIZE) * 2;
	if(preview->source != 0)
		g_source_remove(preview->source);
	/* XXX may not always be an image */
	preview->source = g_idle_add(_preview_on_idle_image, preview);
}


/* preview_on_zoom_out */
static void _preview_on_zoom_out(gpointer data)
{
	Preview * preview = data;

	preview->size = ((preview->size > 0) ? preview->size
			: PREVIEW_DEFAULT_SIZE) / 2;
	/* stick with a minimum amount of pixels */
	if(preview->size < 3)
		preview->size = 3;
	if(preview->source != 0)
		g_source_remove(preview->source);
	/* XXX may not always be an image */
	preview->source = g_idle_add(_preview_on_idle_image, preview);
}
