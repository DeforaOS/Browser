/* $Id$ */
/* Copyright (c) 2012-2014 Pierre Pronchery <khorben@defora.org> */
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



#ifndef DESKTOP_BROWSER_VFS_H
# define DESKTOP_BROWSER_VFS_H

# include <sys/stat.h>
# include <dirent.h>
# include <gtk/gtk.h>
# include <Desktop/mime.h>


/* public */
/* functions */
int browser_vfs_closedir(DIR * dir);
int browser_vfs_lstat(char const * filename, struct stat * st);
GdkPixbuf * browser_vfs_mime_icon(Mime * mime, char const * filename,
		char const * type, struct stat * lst, struct stat * st,
		int size);
char const * browser_vfs_mime_type(Mime * mime, char const * filename,
		mode_t mode);
DIR * browser_vfs_opendir(char const * filename, struct stat * st);
struct dirent * browser_vfs_readdir(DIR * dir);
int browser_vfs_stat(char const * filename, struct stat * st);

#endif /* !DESKTOP_BROWSER_VFS_H */
