/* $Id$ */
/* Copyright (c) 2007-2020 Pierre Pronchery <khorben@defora.org> */
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
 * - re-implement recursive copies */



#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <Desktop.h>
#include "Browser/vfs.h"
#include "../config.h"
#define _(string) gettext(string)


/* constants */
#ifndef PROGNAME_COPY
# define PROGNAME_COPY	"copy"
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


/* Copy */
/* private */
/* types */
typedef int Prefs;
#define PREFS_f 0x01
#define PREFS_i 0x02
#define PREFS_p 0x04
#define PREFS_R 0x08
#define PREFS_H 0x10
#define PREFS_L 0x20
#define PREFS_P 0x40

typedef struct _Copy
{
	Prefs * prefs;
	unsigned int filec;
	char ** filev;
	unsigned int cur;

	struct timeval tv;
	size_t size;
	size_t cnt;
	char buf[65536];
	size_t buf_cnt;
	int eof;
	GIOChannel * in_channel;
	guint in_id;
	GIOChannel * out_channel;
	guint out_id;

	/* widgets */
	GtkWidget * window;
	GtkWidget * filename;
	GtkWidget * progress;
	GtkWidget * flabel;
	GtkWidget * fspeed;
	GtkWidget * fremaining;
	GtkWidget * fprogress;
	int fpulse;			/* tells when to pulse */
} Copy;


/* prototypes */
static int _copy_error(Copy * copy, char const * message, int ret);
static void _copy_info(Copy * copy, char const * message, char const * info);
static int _copy_filename_confirm(Copy * copy, char const * filename);
static int _copy_filename_error(Copy * copy, char const * filename, int ret);
static void _copy_filename_info(Copy * copy, char const * filename,
		char const * info);


/* functions */
/* copy */
static void _copy_refresh(Copy * copy);
/* callbacks */
static void _copy_on_closex(void);
static void _copy_on_cancel(void);
static gboolean _copy_idle_first(gpointer data);

static int _copy(Prefs * prefs, unsigned int filec, char * filev[])
{
	static Copy copy;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkSizeGroup * left;
	GtkSizeGroup * right;
	GtkWidget * widget;
	PangoFontDescription * bold;

	if(filec < 2 || filev == NULL)
		return 1; /* FIXME report error */
	copy.prefs = prefs;
	copy.filec = filec;
	copy.filev = filev;
	copy.cur = 0;
	/* graphical interface */
	copy.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_name(GTK_WINDOW(copy.window), "stock_copy");
	gtk_window_set_resizable(GTK_WINDOW(copy.window), FALSE);
	gtk_window_set_title(GTK_WINDOW(copy.window), _("Copy file(s)"));
	g_signal_connect(G_OBJECT(copy.window), "delete-event", G_CALLBACK(
			_copy_on_closex), NULL);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	left = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	right = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* current argument */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	widget = gtk_label_new(_("Copying: "));
	bold = pango_font_description_new();
	pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(widget, bold);
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_widget_modify_font(widget, bold);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(left, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	copy.filename = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(copy.filename), FALSE);
	gtk_size_group_add_widget(right, copy.filename);
	gtk_box_pack_start(GTK_BOX(hbox), copy.filename, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	/* progress bar */
	copy.progress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(copy.progress), TRUE);
#endif
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(copy.progress), "");
	gtk_box_pack_start(GTK_BOX(vbox), copy.progress, TRUE, TRUE, 0);
	/* file copy */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	widget = gtk_label_new(_("Filename: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(widget, bold);
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_widget_modify_font(widget, bold);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(left, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	copy.flabel = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(copy.flabel), PANGO_ELLIPSIZE_START);
	gtk_label_set_width_chars(GTK_LABEL(copy.flabel), 25);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(copy.flabel, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(copy.flabel), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(right, copy.flabel);
	gtk_box_pack_start(GTK_BOX(hbox), copy.flabel, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	/* file copy speed */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	widget = gtk_label_new(_("Copied: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(widget, bold);
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_widget_modify_font(widget, bold);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(left, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	copy.fspeed = gtk_label_new("");
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(copy.fspeed, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(copy.fspeed), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(right, copy.fspeed);
	gtk_box_pack_start(GTK_BOX(hbox), copy.fspeed, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	/* file copy remaining */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	widget = gtk_label_new(_("Remaining: "));
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(widget, bold);
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_widget_modify_font(widget, bold);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(left, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	copy.fremaining = gtk_label_new("");
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(copy.fremaining, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(copy.fremaining), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(right, copy.fremaining);
	gtk_box_pack_start(GTK_BOX(hbox), copy.fremaining, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	/* file progress bar */
	copy.fprogress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(copy.fprogress), TRUE);
#endif
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(copy.fprogress), "");
	gtk_box_pack_start(GTK_BOX(vbox), copy.fprogress, TRUE, TRUE, 0);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	widget = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect_swapped(G_OBJECT(widget), "clicked", G_CALLBACK(
				_copy_on_cancel), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(copy.window), 4);
	gtk_container_add(GTK_CONTAINER(copy.window), vbox);
	g_idle_add(_copy_idle_first, &copy);
	_copy_refresh(&copy);
	gtk_widget_show_all(copy.window);
	pango_font_description_free(bold);
	return 0;
}

static void _copy_refresh(Copy * copy)
{
	char const * filename = copy->filev[copy->cur];
	char * p;
	char buf[64];
	double fraction;

	if((p = g_filename_to_utf8(filename, -1, NULL, NULL, NULL)) != NULL)
		filename = p;
	gtk_entry_set_text(GTK_ENTRY(copy->filename), filename);
	free(p);
	snprintf(buf, sizeof(buf), _("File %u of %u"), copy->cur + 1,
			copy->filec - 1);
	fraction = copy->cur;
	fraction /= copy->filec - 1;
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(copy->progress), buf);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(copy->progress),
			fraction);
}

static void _copy_on_closex(void)
{
	/* FIXME does not work anymore */
	gtk_main_quit();
}

static void _copy_on_cancel(void)
{
	/* FIXME does not work anymore */
	gtk_main_quit();
}

static int _copy_single(Copy * copy, char const * src, char const * dst);
static gboolean _copy_idle_multiple(gpointer data);

static gboolean _copy_idle_first(gpointer data)
{
	Copy * copy = data;
	char const * filename = copy->filev[copy->filec - 1];
	struct stat st;

	if(browser_vfs_stat(filename, &st) != 0)
	{
		if(errno != ENOENT)
			_copy_filename_error(copy, filename, 0);
		else if(copy->filec > 2)
		{
			errno = ENOTDIR;
			_copy_filename_error(copy, filename, 0);
		}
		else
			_copy_single(copy, copy->filev[0], copy->filev[1]);
	}
	else if(S_ISDIR(st.st_mode))
	{
		g_idle_add(_copy_idle_multiple, copy);
		return FALSE;
	}
	else if(copy->filec > 2)
	{
		errno = ENOTDIR;
		_copy_filename_error(copy, filename, 0);
	}
	else
		_copy_single(copy, copy->filev[0], copy->filev[1]);
	gtk_main_quit();
	return FALSE;
}

/* copy_single
 * XXX TOCTOU all over the place (*stat) but seems impossible to avoid */
static int _single_dir(Copy * copy, char const * src, char const * dst,
		mode_t mode);
static int _single_fifo(Copy * copy, char const * dst, mode_t mode);
static int _single_nod(Copy * copy, char const * src, char const * dst,
		mode_t mode, dev_t rdev);
static int _single_symlink(Copy * copy, char const * src, char const * dst);
static int _single_regular(Copy * copy, char const * src, char const * dst,
		mode_t mode);
static int _single_p(Copy * copy, char const * dst, struct stat const * st);
static void _single_remaining(Copy * copy, guint64 rate);
static gboolean _single_timeout(gpointer data);
static void _single_unit(guint64 size, double * fraction, char const ** unit,
		double * current);

static int _copy_single(Copy * copy, char const * src, char const * dst)
{
	int ret;
	char * p;
	struct stat st;
	struct stat st2;
	guint timeout;

	if((p = g_filename_to_utf8(src, -1, NULL, NULL, NULL)) != NULL)
		gtk_label_set_text(GTK_LABEL(copy->flabel), p);
	else
		gtk_label_set_text(GTK_LABEL(copy->flabel), src);
	free(p);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(copy->fprogress), 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(copy->fprogress), "");
	if(*(copy->prefs) & PREFS_P)
	{
		/* do not follow symlinks */
		if(browser_vfs_lstat(src, &st) != 0 && errno == ENOENT)
			return _copy_filename_error(copy, src, 1);
	}
	else if(browser_vfs_stat(src, &st) != 0 && errno == ENOENT)
		/* follow symlinks */
		return _copy_filename_error(copy, src, 1);
	if(browser_vfs_lstat(dst, &st2) == 0)
	{
		if(st.st_dev == st2.st_dev && st.st_ino == st2.st_ino)
		{
			fprintf(stderr, "%s: %s: \"%s\"%s\n", PROGNAME_COPY,
					dst, src,
					_(" is identical (not copied)"));
			return 0;
		}
		if(*(copy->prefs) & PREFS_i
				&& _copy_filename_confirm(copy, dst) != 1)
			return 0;
		if(unlink(dst) != 0)
			return _copy_filename_error(copy, dst, 1);
	}
	if(S_ISDIR(st.st_mode))
		ret = _single_dir(copy, src, dst, st.st_mode & 0777);
	else if(S_ISFIFO(st.st_mode))
		ret = _single_fifo(copy, dst, st.st_mode & 0666);
	else if(S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
		ret = _single_nod(copy, src, dst, st.st_mode, st.st_rdev);
	else if(S_ISLNK(st.st_mode))
		ret = _single_symlink(copy, src, dst);
	else if(S_ISREG(st.st_mode))
	{
		ret = _single_regular(copy, src, dst, st.st_mode & 0777);
		timeout = g_timeout_add(250, _single_timeout, copy);
		gtk_main(); /* XXX ugly */
		g_source_remove(timeout);
	}
	else
	{
		errno = ENOSYS;
		return _copy_error(copy, src, 1);
	}
	if(ret != 0)
		return ret;
	if(*(copy->prefs) & PREFS_p) /* XXX TOCTOU */
		_single_p(copy, dst, &st);
	return 0;
}

/* single_dir */
static int _single_recurse(Copy * copy, char const * src, char const * dst,
		mode_t mode);

static int _single_dir(Copy * copy, char const * src, char const * dst,
		mode_t mode)
{
	if(*(copy->prefs) & PREFS_R)
		return _single_recurse(copy, src, dst, mode);
	_copy_filename_info(copy, src, _("Omitting directory"));
	return 0;
}

static int _single_recurse(Copy * copy, char const * src, char const * dst,
		mode_t mode)
{
	int ret = 0;
	Copy copy2;
	Prefs prefs2 = *(copy->prefs);
	size_t srclen;
	size_t dstlen;
	DIR * dir;
	struct dirent * de;
	char * ssrc = NULL;
	char * sdst = NULL;
	char * p;

	memcpy(&copy2, copy, sizeof(copy2));
	copy2.prefs = &prefs2;
	if(browser_vfs_mkdir(dst, mode) != 0 && errno != EEXIST)
		return _copy_filename_error(copy, dst, 1);
	srclen = strlen(src);
	dstlen = strlen(dst);
	if((dir = browser_vfs_opendir(src, NULL)) == NULL)
		return _copy_filename_error(copy, src, 1);
	prefs2 |= (prefs2 & PREFS_H) ? PREFS_P : 0;
	while((de = browser_vfs_readdir(dir)) != NULL)
	{
		if(de->d_name[0] == '.' && (de->d_name[1] == '\0'
					|| (de->d_name[1] == '.'
						&& de->d_name[2] == '\0')))
			continue;
		if((p = realloc(ssrc, srclen + strlen(de->d_name) + 2)) == NULL)
		{
			ret |= _copy_filename_error(copy, src, 1);
			continue;
		}
		ssrc = p;
		if((p = realloc(sdst, dstlen + strlen(de->d_name) + 2)) == NULL)
		{
			ret |= _copy_filename_error(copy, src, 1);
			continue;
		}
		sdst = p;
		sprintf(ssrc, "%s/%s", src, de->d_name);
		sprintf(sdst, "%s/%s", dst, de->d_name);
		ret |= _copy_single(&copy2, ssrc, sdst);
	}
	browser_vfs_closedir(dir);
	free(ssrc);
	free(sdst);
	return ret;
}

static int _single_fifo(Copy * copy, char const * dst, mode_t mode)
{
	if(mkfifo(dst, mode) != 0)
		return _copy_filename_error(copy, dst, 1);
	return 0;
}

static int _single_nod(Copy * copy, char const * src, char const * dst,
		mode_t mode, dev_t rdev)
{
	if(mknod(dst, mode, rdev) != 0)
		return _copy_error(copy, dst, 1);
	if(unlink(src) != 0)
		_copy_error(copy, src, 0);
	return 0;
}

static int _single_symlink(Copy * copy, char const * src, char const * dst)
{
	char buf[PATH_MAX];
	ssize_t len;

	if((len = readlink(src, buf, sizeof(buf) - 1)) == -1)
		return _copy_filename_error(copy, src, 1);
	buf[len] = '\0';
	if(symlink(buf, dst) != 0)
		return _copy_filename_error(copy, dst, 1);
	return 0;
}

static gboolean _regular_idle_in(gpointer data);
static gboolean _regular_idle_out(gpointer data);

static int _single_regular(Copy * copy, char const * src, char const * dst,
		mode_t mode)
{
	int ret = 0;
	int in_fd;
	int out_fd;
	struct stat st;

	if(gettimeofday(&copy->tv, NULL) != 0)
		return _copy_error(copy, "gettimeofday", 1);
	if((in_fd = open(src, O_RDONLY)) < 0)
		return _copy_filename_error(copy, src, 1);
	if((out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode)) < 0)
	{
		ret = _copy_filename_error(copy, src, 1);
		close(in_fd);
		return ret;
	}
	copy->size = 0;
	if(fstat(in_fd, &st) == 0)
		copy->size = st.st_size;
	copy->cnt = 0;
	copy->buf_cnt = 0;
	copy->eof = 0;
	copy->in_channel = g_io_channel_unix_new(in_fd);
	g_io_channel_set_encoding(copy->in_channel, NULL, NULL);
	copy->in_id = 0;
	g_idle_add(_regular_idle_in, copy);
	copy->out_channel = g_io_channel_unix_new(out_fd);
	g_io_channel_set_encoding(copy->out_channel, NULL, NULL);
	copy->out_id = 0;
	return ret;
}

static gboolean _regular_channel(GIOChannel * source, GIOCondition condition,
		gpointer data);
static gboolean _channel_in(Copy * copy, GIOChannel * source);
static gboolean _channel_in_error(Copy * copy, GIOChannel * source,
		GError * error);
static gboolean _channel_out(Copy * copy, GIOChannel * source);
static gboolean _channel_out_error(Copy * copy, GIOChannel * source,
		GError * error);
static void _out_rate(Copy * copy);

static gboolean _regular_idle_in(gpointer data)
{
	Copy * copy = data;

	if(copy->in_id == 0)
		copy->in_id = g_io_add_watch(copy->in_channel, G_IO_IN,
				_regular_channel, copy);
	return FALSE;
}

static gboolean _regular_channel(GIOChannel * source, GIOCondition condition,
		gpointer data)
{
	Copy * copy = data;

	if(condition == G_IO_IN)
		return _channel_in(copy, source);
	else if(condition == G_IO_OUT)
		return _channel_out(copy, source);
	_copy_filename_error(copy, copy->filev[copy->cur], 0);
	gtk_main_quit();
	return FALSE;
}

static gboolean _channel_in(Copy * copy, GIOChannel * source)
{
	GIOStatus status;
	gsize read;
	GError * error = NULL;

	copy->in_id = 0;
	status = g_io_channel_read_chars(source, &copy->buf[copy->buf_cnt],
			sizeof(copy->buf) - copy->buf_cnt, &read, &error);
	if(status == G_IO_STATUS_ERROR)
		return _channel_in_error(copy, source, error);
	else if(status == G_IO_STATUS_EOF)
	{
		copy->eof = 1; /* reached end of input file */
		if(g_io_channel_shutdown(source, TRUE, &error)
				== G_IO_STATUS_ERROR)
			return _channel_in_error(copy, source, error);
	}
	else if(copy->buf_cnt + read != sizeof(copy->buf))
		g_idle_add(_regular_idle_in, copy); /* continue to read */
	if(copy->buf_cnt == 0)
		g_idle_add(_regular_idle_out, copy); /* begin to write */
	copy->buf_cnt += read;
	return FALSE;
}

static gboolean _channel_in_error(Copy * copy, GIOChannel * source,
		GError * error)
{
	_copy_filename_error(copy, copy->filev[copy->cur], 0);
	g_error_free(error);
	g_io_channel_unref(source);
	gtk_main_quit(); /* XXX ugly */
	return FALSE;
}

static gboolean _channel_out(Copy * copy, GIOChannel * source)
{
	gsize written;
	GError * error = NULL;

	copy->out_id = 0;
	/* write data */
	if(g_io_channel_write_chars(source, copy->buf, copy->buf_cnt, &written,
				&error) == G_IO_STATUS_ERROR)
		return _channel_out_error(copy, source, error);
	if(fsync(g_io_channel_unix_get_fd(source)) != 0)
		_copy_error(NULL, "fsync", -errno);
	if(copy->buf_cnt == sizeof(copy->buf))
		g_idle_add(_regular_idle_in, copy); /* read again */
	copy->buf_cnt -= written;
	memmove(copy->buf, &copy->buf[written], copy->buf_cnt);
	copy->cnt += written;
	_out_rate(copy);
	if(copy->buf_cnt > 0)
		g_idle_add(_regular_idle_out, copy); /* continue to write */
	else if(copy->eof == 1) /* reached end of input */
	{
		if(g_io_channel_shutdown(copy->out_channel, TRUE, &error)
				== G_IO_STATUS_ERROR)
			return _channel_out_error(copy, source, error);
		gtk_main_quit(); /* XXX ugly */
	}
	return FALSE;
}

static gboolean _channel_out_error(Copy * copy, GIOChannel * source,
		GError * error)
{
	(void) source;

	_copy_filename_error(copy, copy->filev[copy->cur], 0);
	g_error_free(error);
	gtk_main_quit(); /* XXX ugly */
	return FALSE;
}

static void _out_rate(Copy * copy)
{
	gdouble fraction;
	GtkProgressBar * bar = GTK_PROGRESS_BAR(copy->fprogress);
	char buf[16];

	if(copy->size == 0 || copy->cnt == 0)
	{
		copy->fpulse = 1;
		return;
	}
	fraction = copy->cnt;
	fraction /= copy->size;
	if(gtk_progress_bar_get_fraction(bar) == fraction)
		return;
	gtk_progress_bar_set_fraction(bar, fraction);
	snprintf(buf, sizeof(buf), "%.1f%%", fraction * 100);
	gtk_progress_bar_set_text(bar, buf);
}

static gboolean _regular_idle_out(gpointer data)
{
	Copy * copy = data;

	if(copy->out_id == 0)
		copy->out_id = g_io_add_watch(copy->out_channel, G_IO_OUT,
				_regular_channel, copy);
	return FALSE;
}

static int _single_p(Copy * copy, char const * dst, struct stat const * st)
{
	struct timeval tv[2];

	if(lchown(dst, st->st_uid, st->st_gid) != 0)
	{
		_copy_filename_error(copy, dst, 0);
		if(lchmod(dst, st->st_mode & ~(S_ISUID | S_ISGID)) != 0)
			_copy_filename_error(copy, dst, 0);
	}
	else if(lchmod(dst, st->st_mode) != 0)
		_copy_filename_error(copy, dst, 0);
	tv[0].tv_sec = st->st_atime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = st->st_mtime;
	tv[1].tv_usec = 0;
	if(lutimes(dst, tv) != 0)
		_copy_filename_error(copy, dst, 0);
	return 0;
}

static void _single_remaining(Copy * copy, guint64 rate)
{
	char buf[32];
	guint64 remaining;
	struct tm tm;

	if(rate == 0)
		return;
	remaining = (copy->size - copy->cnt) / rate;
	memset(&tm, 0, sizeof(tm));
	tm.tm_sec = remaining;
	/* minutes */
	if(tm.tm_sec > 60)
	{
		tm.tm_min = tm.tm_sec / 60;
		tm.tm_sec = tm.tm_sec - (tm.tm_min * 60);
	}
	/* hours */
	if(tm.tm_min > 60)
	{
		tm.tm_hour = tm.tm_min / 60;
		tm.tm_min = tm.tm_min - (tm.tm_hour * 60);
	}
	strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
	gtk_label_set_text(GTK_LABEL(copy->fremaining), buf);
}

static gboolean _single_timeout(gpointer data)
{
	Copy * copy = data;
	struct timeval tv;
	char buf[32];
	guint64 rate;
	double rate_fraction;
	char const * rate_unit;
	double total_fraction;
	char const * total_unit;
	double current_fraction;

	if(copy->fpulse == 1)
	{
		gtk_progress_bar_pulse(GTK_PROGRESS_BAR(copy->fprogress));
		copy->fpulse = 0;
	}
	if(copy->cnt == 0)
	{
		gtk_label_set_text(GTK_LABEL(copy->fspeed), _("0 bytes"));
		return TRUE;
	}
	if(gettimeofday(&tv, NULL) != 0)
		return _copy_error(copy, "gettimeofday", FALSE);
	if((tv.tv_sec = tv.tv_sec - copy->tv.tv_sec) < 0)
		tv.tv_sec = 0;
	if((tv.tv_usec = tv.tv_usec - copy->tv.tv_usec) < 0)
	{
		tv.tv_sec--;
		tv.tv_usec += 1000000;
	}
	rate = (copy->cnt * 1024) / ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
	_single_unit(rate, &rate_fraction, &rate_unit, NULL);
	current_fraction = copy->cnt;
	_single_unit(copy->size, &total_fraction, &total_unit,
			&current_fraction);
	snprintf(buf, sizeof(buf), _("%.1f of %.1f %s (%.1f %s/s)"),
			current_fraction, total_fraction, total_unit,
			rate_fraction, rate_unit);
	gtk_label_set_text(GTK_LABEL(copy->fspeed), buf);
	_single_remaining(copy, rate);
	return TRUE;
}

static void _single_unit(guint64 size, double * fraction, char const ** unit,
		double * current)
{
	/* bytes */
	*fraction = size;
	*unit = _("bytes");
	if(*fraction < 1024)
		return;
	/* kilobytes */
	*fraction /= 1024;
	if(current)
		*current /= 1024;
	*unit = _("kB");
	if(*fraction < 1024)
		return;
	/* megabytes */
	*fraction /= 1024;
	if(current)
		*current /= 1024;
	*unit = _("MB");
	if(*fraction < 1024)
		return;
	/* gigabytes */
	*fraction /= 1024;
	if(current)
		*current /= 1024;
	*unit = _("GB");
}

static int _copy_multiple(Copy * copy, char const * src, char const * dst);

static gboolean _copy_idle_multiple(gpointer data)
{
	Copy * copy = data;

	_copy_multiple(copy, copy->filev[copy->cur],
			copy->filev[copy->filec - 1]);
	copy->cur++;
	if(copy->cur == copy->filec - 1)
	{
		gtk_main_quit();
		return FALSE;
	}
	_copy_refresh(copy);
	return TRUE;
}

static int _copy_multiple(Copy * copy, char const * src, char const * dst)
{
	int ret;
	char * to;
	size_t len;
	char * p;
	char * q;

	if((p = strdup(src)) == NULL)
		return _copy_filename_error(copy, src, 1);
	to = basename(p);
	len = strlen(dst) + strlen(to) + 2;
	if((q = malloc(len)) == NULL)
	{
		free(p);
		return _copy_filename_error(copy, src, 1);
	}
	snprintf(q, len, "%s/%s", dst, to);
	ret = _copy_single(copy, src, q);
	free(p);
	free(q);
	return ret;
}


/* copy_error */
static int _error_text(char const * message, int ret);

static int _copy_error(Copy * copy, char const * message, int ret)
{
	GtkWidget * dialog;
	int error = errno;

	if(copy == NULL)
		return _error_text(message, ret);
	dialog = gtk_message_dialog_new(GTK_WINDOW(copy->window),
			GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s: %s", message, strerror(error));
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return ret;
}

static int _error_text(char const * message, int ret)
{
	fputs(PROGNAME_COPY ": ", stderr);
	perror(message);
	return ret;
}


/* copy_filename_confirm */
static int _copy_filename_confirm(Copy * copy, char const * filename)
{
	char * p;
	GtkWidget * dialog;
	int res;

	if((p = g_filename_to_utf8(filename, -1, NULL, NULL, NULL)) != NULL)
		filename = p;
	dialog = gtk_message_dialog_new(GTK_WINDOW(copy->window),
			GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_YES_NO,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Question"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			_("%s will be overwritten\nProceed?"), filename);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	free(p);
	return (res == GTK_RESPONSE_YES) ? 1 : 0;
}


/* copy_filename_error */
static int _copy_filename_error(Copy * copy, char const * filename, int ret)
{
	char * p;
	int error = errno;

	if((p = g_filename_to_utf8(filename, -1, NULL, NULL, NULL)) != NULL)
		filename = p;
	errno = error;
	ret = _copy_error(copy, filename, ret);
	free(p);
	return ret;
}


/* copy_info */
static void _copy_info(Copy * copy, char const * message, char const * info)
{
	GtkWidget * dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(copy->window),
			GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Information"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s: %s", message, info);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Information"));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/* copy_filename_info */
static void _copy_filename_info(Copy * copy, char const * filename,
		char const * info)
{
	char * p;

	if((p = g_filename_to_utf8(filename, -1, NULL, NULL, NULL)) != NULL)
		filename = p;
	_copy_info(copy, filename, info);
	free(p);
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-fip] source_file target_file\n\
       %s [-fip] source_file ... target\n\
       %s -R [-H | -L | -P][-fip] source_file ... target\n\
       %s -r [-H | -L | -P][-fip] source_file ... target\n\
  -f	Do not prompt for confirmation if the destination path exists\n\
  -i	Prompt for confirmation if the destination path exists\n\
  -p	Duplicate characteristics of the source files\n\
  -R	Copy file hierarchies\n\
  -r	Copy file hierarchies\n"), PROGNAME_COPY, PROGNAME_COPY, PROGNAME_COPY,
			PROGNAME_COPY);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	Prefs prefs;
	int o;

	if(setlocale(LC_ALL, "") == NULL)
		_copy_error(NULL, "setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	memset(&prefs, 0, sizeof(prefs));
	prefs = PREFS_i;
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "fipRrHLP")) != -1)
		switch(o)
		{
			case 'f':
				prefs -= prefs & PREFS_i;
				prefs |= PREFS_f;
				break;
			case 'i':
				prefs -= prefs & PREFS_f;
				prefs |= PREFS_i;
				break;
			case 'p':
				prefs |= PREFS_p;
				break;
			case 'R':
			case 'r':
				prefs |= PREFS_R;
				break;
			case 'H':
				prefs -= prefs & (PREFS_L | PREFS_P);
				prefs |= PREFS_H;
				break;
			case 'L':
				prefs -= prefs & (PREFS_H | PREFS_P);
				prefs |= PREFS_L;
				break;
			case 'P':
				prefs -= prefs & (PREFS_H | PREFS_L);
				prefs |= PREFS_P;
				break;
			default:
				return _usage();
		}
	if(argc - optind < 2)
		return _usage();
	if((prefs & (PREFS_H | PREFS_L | PREFS_P)) == 0)
		prefs |= (prefs & PREFS_R) ? PREFS_P : PREFS_H;
	_copy(&prefs, argc - optind, &argv[optind]);
	gtk_main();
	return 0;
}
