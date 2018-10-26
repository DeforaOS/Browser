/* $Id$ */
/* Copyright (c) 2006-2018 Pierre Pronchery <khorben@defora.org> */
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



#ifndef BROWSER_CALLBACKS_H
# define BROWSER_CALLBACKS_H

# include <gtk/gtk.h>


/* accelerators */
void on_location(gpointer data);
void on_new_window(gpointer data);
void on_open_file(gpointer data);

/* toolbar */
void on_back(gpointer data);
void on_copy(gpointer data);
void on_cut(gpointer data);
void on_delete(gpointer data);
void on_forward(gpointer data);
void on_home(gpointer data);
void on_new_folder(gpointer data);
void on_new_symlink(gpointer data);
void on_paste(gpointer data);
void on_preferences(gpointer data);
void on_properties(gpointer data);
void on_refresh(gpointer data);
void on_updir(gpointer data);
#if GTK_CHECK_VERSION(2, 6, 0)
void on_view_as(gpointer data);
#endif

/* view */
void on_view_home(gpointer data);
#if GTK_CHECK_VERSION(2, 6, 0)
void on_view_details(gpointer data);
void on_view_icons(gpointer data);
void on_view_list(gpointer data);
void on_view_thumbnails(gpointer data);
#endif

/* address bar */
void on_path_activate(gpointer data);

#endif /* !BROWSER_CALLBACKS_H */
