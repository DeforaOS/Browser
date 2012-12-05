/* $Id$ */
/* Copyright (c) 2012 Pierre Pronchery <khorben@defora.org> */
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



#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "vfs.h"


/* public */
/* functions */
/* vfs_lstat */
int vfs_lstat(char const * filename, struct stat * st)
{
	return lstat(filename, st);
}


/* vfs_closedir */
int vfs_closedir(DIR * dir)
{
	return closedir(dir);
}


/* vfs_folder_icon */
GdkPixbuf * vfs_mime_folder_icon(Mime * mime, char const * filename,
		struct stat * st, int size)
{
	GdkPixbuf * ret = NULL;
	char const * icon = NULL;
	struct stat s;
	char * p;
	struct stat lst;
	size_t i;
	struct
	{
		char const * name;
		char const * icon;
	} name_icon[] =
	{
		{ "Desktop",	"gnome-fs-desktop"	},
		{ "Documents",	"folder-documents"	},
		{ "Download",	"folder-download"	},
		{ "Downloads",	"folder-download"	},
		{ "Music",	"folder-music"		},
		{ "Pictures",	"folder-pictures"	},
		{ "public_html","folder-publicshared"	},
		{ "Templates",	"folder-templates"	},
		{ "Video",	"folder-videos"		},
		{ "Videos",	"folder-videos"		},
	};
	GtkIconTheme * icontheme;
	int flags = GTK_ICON_LOOKUP_FORCE_SIZE;

	if(st == NULL && lstat(filename, &s) == 0)
		st = &s;
	/* check if the folder is a mountpoint */
	if((p = strdup(filename)) != NULL
			&& st != NULL
			&& lstat(dirname(p), &lst) == 0
			&& st->st_dev != lst.st_dev)
		icon = "mount-point";
	if(p != NULL && icon == NULL)
		for(i = 0; i < sizeof(name_icon) / sizeof(*name_icon); i++)
			if(strcasecmp(basename(p), name_icon[i].name) == 0)
			{
				icon = name_icon[i].icon;
				break;
			}
	free(p);
	if(icon != NULL)
	{
		icontheme = gtk_icon_theme_get_default();
		ret = gtk_icon_theme_load_icon(icontheme, icon, size, flags,
				NULL);
	}
	/* generic fallback */
	if(ret == NULL)
		ret = vfs_mime_icon(mime, "inode/directory", st, size);
	return ret;
}


/* vfs_mime_icon */
GdkPixbuf * vfs_mime_icon(Mime * mime, char const * type, struct stat * st,
		int size)
{
	GdkPixbuf * pixbuf = NULL;
	char const * emblem;
	int esize;
	GdkPixbuf * epixbuf;
	GtkIconTheme * icontheme;
	int flags = GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE;

	mime_icons(mime, type, size, &pixbuf, -1);
	if(pixbuf == NULL)
		return NULL;
	/* determine the emblem */
	if(S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
		emblem = "emblem-system";
	else if(S_ISLNK(st->st_mode))
		emblem = "emblem-symbolic-link";
	else if((st->st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) == 0)
		emblem = "emblem-unreadable";
	else if((st->st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0)
		emblem = "emblem-readonly";
	else
		return pixbuf;
	/* determine the size of the emblem */
	switch(size)
	{
		case 24:
			esize = 12;
			break;
		case 48:
			esize = 24;
			break;
		case 96:
			esize = 32;
			break;
		default:
			return pixbuf;
	}
	/* obtain the emblem's icon */
	icontheme = gtk_icon_theme_get_default();
	if((epixbuf = gtk_icon_theme_load_icon(icontheme, emblem, esize, flags,
					NULL)) == NULL)
		return pixbuf;
	pixbuf = gdk_pixbuf_copy(pixbuf);
	/* blit the emblem */
#if 0 /* XXX does not show anything (bottom right) */
	gdk_pixbuf_composite(epixbuf, pixbuf, size - esize, size - esize,
			esize, esize, 0, 0, 1.0, 1.0, GDK_INTERP_NEAREST,
			255);
#else /* blitting at the top left instead */
	gdk_pixbuf_composite(epixbuf, pixbuf, 0, 0, esize, esize, 0, 0,
			1.0, 1.0, GDK_INTERP_NEAREST, 255);
#endif
	return pixbuf;
}


/* vfs_opendir */
DIR * vfs_opendir(char const * filename, struct stat * st)
{
	DIR * dir;
	int fd;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %p)\n", __func__, filename, st);
#endif
#if defined(__sun__)
	if((fd = open(filename, O_RDONLY)) < 0
			|| (dir = fdopendir(fd)) == NULL)
	{
		if(fd >= 0)
			close(fd);
		return NULL;
	}
#else
	if((dir = opendir(filename)) == NULL)
		return NULL;
	fd = dirfd(dir);
#endif
	if(st != NULL && fstat(fd, st) != 0)
	{
		vfs_closedir(dir);
		return NULL;
	}
	return dir;
}


/* vfs_readdir */
struct dirent * vfs_readdir(DIR * dir)
{
	return readdir(dir);
}


/* vfs_stat */
int vfs_stat(char const * filename, struct stat * st)
{
	return stat(filename, st);
}
