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

#ifndef max
# define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
# define min(a, b) ((a) < (b) ? (a) : (b))
#endif


/* Preview */
/* private */
/* types */
typedef enum _PreviewImageHow
{
	PREVIEW_IMAGE_HOW_FIT = 0,
	PREVIEW_IMAGE_HOW_ORIGINAL,
	PREVIEW_IMAGE_HOW_SCALE
} PreviewImageHow;

typedef struct _BrowserPlugin
{
	BrowserPluginHelper * helper;

	char * path;
	guint source;

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
	PreviewImageHow view_image_how;
	gint view_image_height;
	gint view_image_width;
	gdouble view_image_scale;
	GtkWidget * view_text;
	GtkTextBuffer * view_text_tbuf;
} Preview;


/* constants */
#define PREVIEW_IMAGE_SIZE_DEFAULT	96


/* prototypes */
/* plug-in */
static Preview * _preview_init(BrowserPluginHelper * helper);
static void _preview_destroy(Preview * preview);
static GtkWidget * _preview_get_widget(Preview * preview);
static void _preview_refresh(Preview * preview, GList * selection);

/* accessors */
static void _preview_get_image_size(Preview * preview, gint * width,
		gint * height);
static void _preview_get_widget_size(Preview * preview, gint * width,
		gint * height);

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
	String const * p;

	if((preview = object_new(sizeof(*preview))) == NULL)
		return NULL;
	preview->helper = helper;
	preview->path = NULL;
	preview->source = 0;
	if((p = helper->config_get(helper->browser, "preview", "size")) != NULL)
		preview->view_image_how = strtol(p, NULL, 0);
	else
		preview->view_image_how = PREVIEW_IMAGE_HOW_FIT;
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
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(preview->name, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(preview->name), 0.0, 0.5);
#endif
	font = pango_font_description_new();
	pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(preview->name, font);
#else
	gtk_widget_modify_font(preview->name, font);
#endif
	pango_font_description_free(font);
	if((p = helper->config_get(helper->browser, "preview", "label")) != NULL
			&& strtol(p, NULL, 0) == 0)
		gtk_widget_set_no_show_all(preview->name, TRUE);
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
	preview->view_image_scale = 1.0;
	preview->view_image_height = -1;
	preview->view_image_width = -1;
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
	preview->view_image_width = -1;
	preview->view_image_height = -1;
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


/* accessors */
/* preview_get_image_size */
static void _preview_get_image_size(Preview * preview, gint * width,
		gint * height)
{
	BrowserPluginHelper * helper = preview->helper;
	GdkPixbuf * pixbuf;
	GError * error = NULL;

	/* check if we have cached the values */
	if(preview->view_image_width > 0
			&& preview->view_image_height > 0)
	{
		*width = preview->view_image_width;
		*height = preview->view_image_height;
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() %dx%d (cached)\n", __func__,
				*width, *height);
#endif
		return;
	}
	*width = PREVIEW_IMAGE_SIZE_DEFAULT;
	*height = PREVIEW_IMAGE_SIZE_DEFAULT;
	if(preview->path != NULL
			&& (pixbuf = gdk_pixbuf_new_from_file(preview->path,
					&error)) != NULL)
	{
		*width = gdk_pixbuf_get_width(pixbuf);
		*height = gdk_pixbuf_get_height(pixbuf);
		g_object_unref(pixbuf);
		/* cache the values */
		preview->view_image_width = *width;
		preview->view_image_height = *height;
	}
	if(error != NULL)
	{
		helper->error(NULL, error->message, 1);
		g_error_free(error);
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %dx%d\n", __func__, *width, *height);
#endif
}


/* preview_get_widget_size */
static void _preview_get_widget_size(Preview * preview, gint * width,
		gint * height)
{
#if GTK_CHECK_VERSION(2, 14, 0)
	GdkWindow * window;

	if((window = gtk_widget_get_window(preview->view_image)) != NULL)
	{
# if GTK_CHECK_VERSION(2, 24, 0)
		*width = gdk_window_get_width(window);
		*height = gdk_window_get_height(window);
# else
		gdk_drawable_get_size(GDK_DRAWABLE(window), width, height);
# endif
	}
	else
#endif
	{
		*width = PREVIEW_IMAGE_SIZE_DEFAULT;
		*height = PREVIEW_IMAGE_SIZE_DEFAULT;
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %dx%d\n", __func__, *width, *height);
#endif
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
static void _idle_image_fit(Preview * preview);
static void _idle_image_load(Preview * preview, gint height, gint width);
static void _idle_image_load_animation(Preview * preview);
static void _idle_image_load_pixbuf(Preview * preview, gint width, gint height);
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
	switch(preview->view_image_how)
	{
		case PREVIEW_IMAGE_HOW_FIT:
			_idle_image_fit(preview);
			break;
		case PREVIEW_IMAGE_HOW_ORIGINAL:
			_idle_image_100(preview);
			break;
		case PREVIEW_IMAGE_HOW_SCALE:
		default:
			_idle_image_scale(preview);
			break;
	}
	gtk_widget_show(preview->view_image);
	return FALSE;
}

static void _idle_image_100(Preview * preview)
{
	_idle_image_load(preview, 0, 0);
}

static void _idle_image_fit(Preview * preview)
{
	gint iwidth;
	gint iheight;
	gint wwidth;
	gint wheight;

	_preview_get_image_size(preview, &iwidth, &iheight);
	_preview_get_widget_size(preview, &wwidth, &wheight);
	/* scale the image accordingly */
	preview->view_image_scale = min((gdouble)wwidth / (gdouble)iwidth,
			(gdouble)wheight / (gdouble)iheight);
	_idle_image_load(preview, 0, 0);
}

static void _idle_image_load(Preview * preview, gint height, gint width)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%d, %d)\n", __func__, width, height);
#endif
	if(width <= 0 && height <= 0 && preview->view_image_scale == 1.0)
		_idle_image_load_animation(preview);
	else
	{
		if(width <= 0 || height <= 0)
			_preview_get_image_size(preview, &width, &height);
		_idle_image_load_pixbuf(preview,
				width * preview->view_image_scale,
				height * preview->view_image_scale);
	}
}

static void _idle_image_load_animation(Preview * preview)
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

static void _idle_image_load_pixbuf(Preview * preview, gint width, gint height)
{
	BrowserPluginHelper * helper = preview->helper;
	GdkPixbuf * pixbuf;
	GError * error = NULL;

	if(width <= 0)
		width = 1;
	if(height <= 0)
		height = 1;
#if GTK_CHECK_VERSION(2, 6, 0)
	if((pixbuf = gdk_pixbuf_new_from_file_at_scale(preview->path, width,
					height, TRUE, &error)) == NULL)
#else
	if((pixbuf = gdk_pixbuf_new_from_file_at_size(preview->path, width,
					height, &error)) == NULL)
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

static void _idle_image_scale(Preview * preview)
{
	_idle_image_load(preview, 0, 0);
}


/* preview_on_idle_text */
static gboolean _preview_on_idle_text(gpointer data)
{
	Preview * preview = data;
	BrowserPluginHelper * helper = preview->helper;
	GtkTextBuffer * tbuf;
	GtkTextIter iter;
	int fd;
	char buf[256];
	ssize_t s;
	String const * p = NULL;

	preview->source = 0;
	gtk_widget_show(GTK_WIDGET(preview->copy));
	gtk_widget_show(GTK_WIDGET(preview->select_all));
	gtk_widget_show(preview->toolbar);
	tbuf = preview->view_text_tbuf;
	gtk_text_buffer_set_text(tbuf, "", 0);
	gtk_text_buffer_get_end_iter(tbuf, &iter);
	if((fd = open(preview->path, O_RDONLY)) < 0)
	{
		helper->error(helper->browser, strerror(errno), 1);
		return FALSE;
	}
	/* FIXME use a GIOChannel instead */
	while((s = read(fd, buf, sizeof(buf))) > 0)
	{
		if(p == NULL)
		{
			if((p = helper->config_get(helper->browser, "preview",
							"ellipsize")) == NULL
					|| strtol(p, NULL, 0) != 0)
			{
				if(s == sizeof(buf))
				{
					buf[sizeof(buf) - 3] = '.';
					buf[sizeof(buf) - 2] = '.';
					buf[sizeof(buf) - 1] = '.';
				}
				gtk_text_buffer_set_text(tbuf, buf, s);
				break;
			}
		}
		gtk_text_buffer_insert(preview->view_text_tbuf, &iter, buf, s);
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

	preview->view_image_how = PREVIEW_IMAGE_HOW_ORIGINAL;
	preview->view_image_scale = 1.0;
	if(preview->source != 0)
		g_source_remove(preview->source);
	/* XXX may not always be an image */
	preview->source = g_idle_add(_preview_on_idle_image, preview);
}


/* preview_on_zoom_fit */
static void _preview_on_zoom_fit(gpointer data)
{
	Preview * preview = data;

	preview->view_image_how = PREVIEW_IMAGE_HOW_FIT;
	if(preview->source != 0)
		g_source_remove(preview->source);
	/* XXX may not always be an image */
	preview->source = g_idle_add(_preview_on_idle_image, preview);
}


/* preview_on_zoom_in */
static void _preview_on_zoom_in(gpointer data)
{
	Preview * preview = data;

	preview->view_image_how = PREVIEW_IMAGE_HOW_SCALE;
	preview->view_image_scale *= 1.25;
	if(preview->view_image_scale > 4.0)
		preview->view_image_scale = 4.0;
	if(preview->source != 0)
		g_source_remove(preview->source);
	/* XXX may not always be an image */
	preview->source = g_idle_add(_preview_on_idle_image, preview);
}


/* preview_on_zoom_out */
static void _preview_on_zoom_out(gpointer data)
{
	Preview * preview = data;

	preview->view_image_how = PREVIEW_IMAGE_HOW_SCALE;
	preview->view_image_scale /= 1.25;
	if(preview->view_image_scale < 0.1)
		preview->view_image_scale = 0.1;
	if(preview->source != 0)
		g_source_remove(preview->source);
	/* XXX may not always be an image */
	preview->source = g_idle_add(_preview_on_idle_image, preview);
}
