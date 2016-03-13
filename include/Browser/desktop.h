/* $Id$ */
/* Copyright (c) 2012 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Browser */
/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice
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



#ifndef DESKTOP_BROWSER_DESKTOP_H
# define DESKTOP_BROWSER_DESKTOP_H

# include <gtk/gtk.h>
# include <Desktop.h>


/* Desktop */
/* public */
/* types */
typedef enum _DesktopAlignment
{
	DESKTOP_ALIGNMENT_VERTICAL = 0,
	DESKTOP_ALIGNMENT_HORIZONTAL
} DesktopAlignment;

typedef enum _DesktopIcons
{
	DESKTOP_ICONS_NONE = 0,
	DESKTOP_ICONS_APPLICATIONS,
	DESKTOP_ICONS_CATEGORIES,
	DESKTOP_ICONS_FILES,
	DESKTOP_ICONS_HOMESCREEN
} DesktopIcons;
# define DESKTOP_ICONS_LAST DESKTOP_ICONS_HOMESCREEN
# define DESKTOP_ICONS_COUNT (DESKTOP_ICONS_LAST + 1)

typedef enum _DesktopLayout
{
	DESKTOP_LAYOUT_NORMAL = 0,
	DESKTOP_LAYOUT_LANDSCAPE,
	DESKTOP_LAYOUT_PORTRAIT,
	DESKTOP_LAYOUT_ROTATE,
	DESKTOP_LAYOUT_TOGGLE
} DesktopLayout;

typedef enum _DesktopMessage
{
	DESKTOP_MESSAGE_SET_ALIGNMENT = 0,
	DESKTOP_MESSAGE_SET_ICONS,
	DESKTOP_MESSAGE_SET_LAYOUT,
	DESKTOP_MESSAGE_SHOW
} DesktopMessage;

typedef enum _DesktopShow
{
	DESKTOP_SHOW_SETTINGS = 0
} DesktopMessageShow;


/* constants */
# define DESKTOP_CLIENT_MESSAGE	"DEFORAOS_DESKTOP_DESKTOP_CLIENT"

#endif /* !DESKTOP_BROWSER_DESKTOP_H */
