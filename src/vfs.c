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



#include "vfs.h"


/* public */
/* functions */
/* vfs_opendir */
int vfs_closedir(DIR * dir)
{
	return closedir(dir);
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
