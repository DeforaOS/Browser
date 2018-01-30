/* $Id$ */
/* Copyright (c) 2007-2018 Pierre Pronchery <khorben@defora.org> */
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
 * 3. Neither the name of the authors nor the names of the contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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



#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include "Browser/vfs.h"
#include "../config.h"
#define _(string) gettext(string)


/* constants */
#ifndef PROGNAME_MOVE
# define PROGNAME_MOVE	"move"
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


/* Move */
/* private */
/* types */
typedef int Prefs;
#define PREFS_f 0x1
#define PREFS_i 0x2

typedef struct _Move
{
	Prefs * prefs;
	unsigned int filec;
	char ** filev;
	unsigned int cur;
	GtkWidget * window;
	GtkWidget * label;
	GtkWidget * progress;
} Move;


/* prototypes */
static int _move_error(Move * move, char const * message, int ret);
static int _move_filename_confirm(Move * move, char const * filename);
static int _move_filename_error(Move * move, char const * filename, int ret);


/* functions */
/* move */
static void _move_refresh(Move * move);
/* callbacks */
static void _move_on_closex(void);
static void _move_on_cancel(void);
static gboolean _move_idle_first(gpointer data);

static int _move(Prefs * prefs, unsigned int filec, char * filev[])
{
	static Move move;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkSizeGroup * left;
	GtkSizeGroup * right;
	GtkWidget * widget;
	PangoFontDescription * bold;

	if(filec < 2 || filev == NULL)
		return 1; /* FIXME report error */
	move.prefs = prefs;
	move.filec = filec;
	move.filev = filev;
	move.cur = 0;
	/* graphical interface */
	move.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_name(GTK_WINDOW(move.window), "stock_cut");
	gtk_window_set_resizable(GTK_WINDOW(move.window), FALSE);
	gtk_window_set_title(GTK_WINDOW(move.window), _("Move file(s)"));
	g_signal_connect(G_OBJECT(move.window), "delete-event", G_CALLBACK(
			_move_on_closex), NULL);
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox = gtk_vbox_new(FALSE, 4);
#endif
	left = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	right = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	/* current argument */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	widget = gtk_label_new(_("Moving: "));
	bold = pango_font_description_new();
	pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(widget, bold);
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_widget_modify_font(widget, bold);
	gtk_misc_set_alignment(GTK_MISC(widget), 0, 0);
#endif
	gtk_size_group_add_widget(left, widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	move.label = gtk_label_new("");
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_ellipsize(GTK_LABEL(move.label), PANGO_ELLIPSIZE_END);
	gtk_label_set_width_chars(GTK_LABEL(move.label), 25);
#endif
#if GTK_CHECK_VERSION(3, 0, 0)
	g_object_set(move.label, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(move.label), 0.0, 0.5);
#endif
	gtk_size_group_add_widget(right, move.label);
	gtk_box_pack_start(GTK_BOX(hbox), move.label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	/* progress bar */
	move.progress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(move.progress), TRUE);
#endif
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(move.progress), "");
	gtk_box_pack_start(GTK_BOX(vbox), move.progress, TRUE, TRUE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	widget = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect_swapped(G_OBJECT(widget), "clicked", G_CALLBACK(
				_move_on_cancel), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(move.window), 4);
	gtk_container_add(GTK_CONTAINER(move.window), vbox);
	g_idle_add(_move_idle_first, &move);
	_move_refresh(&move);
	gtk_widget_show_all(move.window);
	pango_font_description_free(bold);
	return 0;
}

static void _move_refresh(Move * move)
{
	char const * filename = move->filev[move->cur];
	char * p;
	char buf[64];
	double fraction;

	if((p = g_filename_to_utf8(filename, -1, NULL, NULL, NULL)) != NULL)
		filename = p;
	gtk_label_set_text(GTK_LABEL(move->label), filename);
	free(p);
	snprintf(buf, sizeof(buf), _("File %u of %u"), move->cur + 1,
			move->filec - 1);
	fraction = move->cur;
	fraction /= move->filec - 1;
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(move->progress), buf);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(move->progress),
			fraction);
}

static void _move_on_closex(void)
{
	gtk_main_quit();
}

static void _move_on_cancel(void)
{
	gtk_main_quit();
}

static int _move_single(Move * move, char const * src, char const * dst);
static gboolean _move_idle_multiple(gpointer data);
static gboolean _move_idle_first(gpointer data)
{
	Move * move = data;
	char const * filename = move->filev[move->filec - 1];
	struct stat st;

	if(browser_vfs_stat(move->filev[move->filec - 1], &st) != 0)
	{
		if(errno != ENOENT)
			_move_filename_error(move, filename, 0);
		else if(move->filec > 2)
		{
			errno = ENOTDIR;
			_move_filename_error(move, filename, 0);
		}
		else
			_move_single(move, move->filev[0], move->filev[1]);
	}
	else if(S_ISDIR(st.st_mode))
	{
		g_idle_add(_move_idle_multiple, move);
		return FALSE;
	}
	else if(move->filec > 2)
	{
		errno = ENOTDIR;
		_move_filename_error(move, filename, 0);
	}
	else
		_move_single(move, move->filev[0], move->filev[1]);
	gtk_main_quit();
	return FALSE;
}

/* move_single */
static int _single_dir(Move * move, char const * src, char const * dst,
		mode_t mode);
static int _single_fifo(Move * move, char const * src, char const * dst,
		mode_t mode);
static int _single_nod(Move * move, char const * src, char const * dst,
		mode_t mode, dev_t rdev);
static int _single_symlink(Move * move, char const * src, char const * dst);
static int _single_regular(Move * move, char const * src, char const * dst,
		mode_t mode);
static int _single_p(Move * move, char const * dst, struct stat const * st);

static int _move_single(Move * move, char const * src, char const * dst)
{
	int ret;
	struct stat st;

	if(browser_vfs_lstat(src, &st) != 0 && errno == ENOENT) /* XXX TOCTOU */
		return _move_filename_error(move, src, 1);
	if(*(move->prefs) & PREFS_i
			&& (browser_vfs_lstat(dst, &st) == 0 || errno != ENOENT)
			&& _move_filename_confirm(move, dst) != 1)
		return 0;
	if(rename(src, dst) == 0)
		return 0;
	if(errno != EXDEV)
		return _move_filename_error(move, src, 1);
	if(unlink(dst) != 0 && errno != ENOENT)
		return _move_filename_error(move, dst, 1);
	if(browser_vfs_lstat(src, &st) != 0)
		return _move_filename_error(move, dst, 1);
	if(S_ISDIR(st.st_mode))
		ret = _single_dir(move, src, dst, st.st_mode & 0777);
	else if(S_ISFIFO(st.st_mode))
		ret = _single_fifo(move, src, dst, st.st_mode & 0666);
	else if(S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
		ret = _single_nod(move, src, dst, st.st_mode, st.st_rdev);
	else if(S_ISLNK(st.st_mode))
		ret = _single_symlink(move, src, dst);
	else if(S_ISREG(st.st_mode))
		ret = _single_regular(move, src, dst, st.st_mode & 0777);
	else
	{
		errno = ENOSYS;
		return _move_filename_error(move, src, 1);
	}
	if(ret != 0)
		return ret;
	_single_p(move, dst, &st);
	return 0;
}

/* single_dir */
static int _single_recurse(Move * move, char const * src, char const * dst,
		mode_t mode);

static int _single_dir(Move * move, char const * src, char const * dst,
		mode_t mode)
{
	if(_single_recurse(move, src, dst, mode) != 0)
		return 1;
	if(rmdir(src) != 0) /* FIXME probably gonna fail, recurse before */
		_move_filename_error(move, src, 0);
	return 0;
}

static int _single_recurse(Move * move, char const * src, char const * dst,
		mode_t mode)
{
	int ret = 0;
	size_t srclen;
	size_t dstlen;
	DIR * dir;
	struct dirent * de;
	char * ssrc = NULL;
	char * sdst = NULL;
	char * p;

	if(mkdir(dst, mode) != 0)
		return _move_filename_error(move, dst, 1);
	srclen = strlen(src);
	dstlen = strlen(dst);
	if((dir = browser_vfs_opendir(src, NULL)) == NULL)
		return _move_filename_error(move, src, 1);
	while((de = browser_vfs_readdir(dir)) != NULL)
	{
		if(de->d_name[0] == '.' && (de->d_name[1] == '\0'
					|| (de->d_name[1] == '.'
						&& de->d_name[2] == '\0')))
			continue;
		if((p = realloc(ssrc, srclen + strlen(de->d_name) + 2)) == NULL)
		{
			ret |= _move_filename_error(move, src, 1);
			continue;
		}
		ssrc = p;
		if((p = realloc(sdst, dstlen + strlen(de->d_name) + 2)) == NULL)
		{
			ret |= _move_filename_error(move, src, 1);
			continue;
		}
		sdst = p;
		sprintf(ssrc, "%s/%s", src, de->d_name);
		sprintf(sdst, "%s/%s", dst, de->d_name);
		ret |= _move_single(move, ssrc, sdst);
	}
	browser_vfs_closedir(dir);
	free(ssrc);
	free(sdst);
	return ret;
}

static int _single_fifo(Move * move, char const * src, char const * dst,
		mode_t mode)
{
	if(mkfifo(dst, mode) != 0)
		return _move_filename_error(move, dst, 1);
	if(unlink(src) != 0)
		_move_filename_error(move, src, 0);
	return 0;
}

static int _single_nod(Move * move, char const * src, char const * dst,
		mode_t mode, dev_t rdev)
{
	if(mknod(dst, mode, rdev) != 0)
		return _move_filename_error(move, dst, 1);
	if(unlink(src) != 0)
		_move_filename_error(move, src, 0);
	return 0;
}

static int _single_symlink(Move * move, char const * src, char const * dst)
{
	char buf[PATH_MAX];
	ssize_t i;

	if((i = readlink(src, buf, sizeof(buf) - 1)) == -1)
		return _move_filename_error(move, src, 1);
	buf[i] = '\0';
	if(symlink(buf, dst) != 0)
		return _move_filename_error(move, dst, 1);
	if(unlink(src) != 0)
		_move_filename_error(move, src, 0);
	return 0;
}

static int _single_regular(Move * move, char const * src, char const * dst,
		mode_t mode)
{
	int ret = 0;
	FILE * fsrc;
	int fd;
	FILE * fdst;
	char buf[BUFSIZ];
	size_t size;

	if((fsrc = fopen(src, "r")) == NULL)
		return _move_filename_error(move, dst, 1);
	if((fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode)) < 0
			|| (fdst = fdopen(fd, "w")) == NULL)
	{
		ret |= _move_filename_error(move, dst, 1);
		if(fd >= 0)
			close(fd);
		fclose(fsrc);
		return ret;
	}
	while((size = fread(buf, sizeof(char), sizeof(buf), fsrc)) > 0)
		if(fwrite(buf, sizeof(char), size, fdst) != size)
			break;
	if(!feof(fsrc))
		ret |= _move_filename_error(move, (size == 0) ? src : dst, 1);
	if(fclose(fsrc) != 0)
		ret |= _move_filename_error(move, src, 1);
	if(fclose(fdst) != 0)
		ret |= _move_filename_error(move, dst, 1);
	if(unlink(src) != 0)
		_move_filename_error(move, src, 0);
	return ret;
}

static int _single_p(Move * move, char const * dst, struct stat const * st)
{
	struct timeval tv[2];

	if(lchown(dst, st->st_uid, st->st_gid) != 0) /* XXX TOCTOU */
	{
		_move_filename_error(move, dst, 0);
		if(lchmod(dst, st->st_mode & ~(S_ISUID | S_ISGID)) != 0)
			_move_filename_error(move, dst, 0);
	}
	else if(lchmod(dst, st->st_mode) != 0)
		_move_filename_error(move, dst, 0);
	tv[0].tv_sec = st->st_atime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = st->st_mtime;
	tv[1].tv_usec = 0;
	if(lutimes(dst, tv) != 0)
		_move_filename_error(move, dst, 0);
	return 0;
}

/* move_idle_multiple */
static int _move_multiple(Move * move, char const * src, char const * dst);

static gboolean _move_idle_multiple(gpointer data)
{
	Move * move = data;

	_move_multiple(move, move->filev[move->cur],
			move->filev[move->filec - 1]);
	move->cur++;
	if(move->cur == move->filec - 1)
	{
		gtk_main_quit();
		return FALSE;
	}
	_move_refresh(move);
	return TRUE;
}

static int _move_multiple(Move * move, char const * src, char const * dst)
{
	int ret;
	char * to;
	size_t len;
	char * p;
	char * q;

	if((p = strdup(src)) == NULL)
		return _move_filename_error(move, src, 1);
	to = basename(p);
	len = strlen(dst) + strlen(to) + 2;
	if((q = malloc(len)) == NULL)
	{
		free(p);
		return _move_filename_error(move, src, 1);
	}
	snprintf(q, len, "%s/%s", dst, to);
	ret = _move_single(move, src, q);
	free(p);
	free(q);
	return ret;
}


/* move_error */
static int _error_text(char const * message, int ret);

static int _move_error(Move * move, char const * message, int ret)
{
	GtkWidget * dialog;
	int error = errno;

	if(move == NULL)
		return _error_text(message, ret);
	dialog = gtk_message_dialog_new(GTK_WINDOW(move->window),
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
	fputs(PROGNAME_MOVE ": ", stderr);
	perror(message);
	return ret;
}


/* move_filename_confirm */
static int _move_filename_confirm(Move * move, char const * filename)
{
	char * p;
	GtkWidget * dialog;
	int res;

	if((p = g_filename_to_utf8(filename, -1, NULL, NULL, NULL)) != NULL)
		filename = p;
	dialog = gtk_message_dialog_new(GTK_WINDOW(move->window),
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


/* move_filename_error */
static int _move_filename_error(Move * move, char const * filename, int ret)
{
	char * p;
	int error = errno;

	if((p = g_filename_to_utf8(filename, -1, NULL, NULL, NULL)) != NULL)
		filename = p;
	errno = error;
	ret = _move_error(move, filename, ret);
	free(p);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-fi] source target\n"
"       %s [-fi] source... directory\n"
"  -f	Do not prompt for confirmation if the destination path exists\n"
"  -i	Prompt for confirmation if the destination path exists\n"),
			PROGNAME_MOVE, PROGNAME_MOVE);
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
		_move_error(NULL, "setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	memset(&prefs, 0, sizeof(prefs));
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "fi")) != -1)
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
			default:
				return _usage();
		}
	if(argc - optind < 2)
		return _usage();
	_move(&prefs, argc - optind, &argv[optind]);
	gtk_main();
	return 0;
}
