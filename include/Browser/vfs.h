/* $Id$ */
/* Copyright (c) 2012-2020 Pierre Pronchery <khorben@defora.org> */
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



#ifndef DESKTOP_BROWSER_VFS_H
# define DESKTOP_BROWSER_VFS_H

# include <sys/stat.h>
# include <dirent.h>
# include <gtk/gtk.h>
# include <Desktop/mime.h>


/* public */
/* functions */
/* accessors */
int browser_vfs_can_eject(char const * mountpoint);
int browser_vfs_can_mount(char const * mountpoint);
int browser_vfs_can_unmount(char const * mountpoint);

int browser_vfs_is_mountpoint(struct stat * lst, dev_t parent);

/* useful */
int browser_vfs_mkdir(char const * path, mode_t mode);

/* DIR */
DIR * browser_vfs_opendir(char const * filename, struct stat * st);
int browser_vfs_closedir(DIR * dir);
struct dirent * browser_vfs_readdir(DIR * dir);

int browser_vfs_eject(char const * mountpoint);
int browser_vfs_mount(char const * mountpoint);
int browser_vfs_unmount(char const * mountpoint);

/* Mime */
GdkPixbuf * browser_vfs_mime_icon(Mime * mime, char const * filename,
		char const * type, struct stat * lst, struct stat * st,
		int size);
char const * browser_vfs_mime_type(Mime * mime, char const * filename,
		mode_t mode);

/* stat */
int browser_vfs_lstat(char const * filename, struct stat * st);
int browser_vfs_stat(char const * filename, struct stat * st);

#endif /* !DESKTOP_BROWSER_VFS_H */
