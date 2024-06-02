/* $Id$ */
/* Copyright (c) 2024 Pierre Pronchery <khorben@defora.org> */
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



#ifndef BROWSER_BROWSER_BROWSER_H
# define BROWSER_BROWSER_BROWSER_H

# include <gtk/gtk.h>


/* Browser */
/* public */
/* types */
typedef enum _BrowserView
{
	BROWSER_VIEW_DETAILS = 0,
# if GTK_CHECK_VERSION(2, 6, 0)
	BROWSER_VIEW_ICONS,
	BROWSER_VIEW_LIST,
	BROWSER_VIEW_THUMBNAILS
# endif
} BrowserView;
# define BROWSER_VIEW_FIRST	BROWSER_VIEW_DETAILS
# if GTK_CHECK_VERSION(2, 6, 0)
#  define BROWSER_VIEW_LAST	BROWSER_VIEW_DETAILS
# else
#  define BROWSER_VIEW_LAST	BROWSER_VIEW_THUMBNAILS
# endif
# define BROWSER_VIEW_COUNT	(BROWSER_VIEW_LAST + 1)

#endif /* !BROWSER_BROWSER_BROWSER_H */
