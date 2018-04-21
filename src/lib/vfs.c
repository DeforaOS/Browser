/* $Id$ */
/* Copyright (c) 2012-2018 Pierre Pronchery <khorben@defora.org> */
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



#if defined(__FreeBSD__)
# include <sys/param.h>
# include <sys/ucred.h>
# include <fstab.h>
#elif defined(__NetBSD__)
# include <sys/param.h>
# include <sys/types.h>
# include <sys/statvfs.h>
# include <fstab.h>
#elif defined(__OpenBSD__)
# include <fstab.h>
#elif defined(__sun)
# include <fcntl.h>
# include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <System/string.h>
#include "../../include/Browser/vfs.h"


/* private */
/* types */
typedef enum _VFSFlag
{
	VF_MOUNTED	= 0x01,
	VF_NETWORK	= 0x02,
	VF_READONLY	= 0x04,
	VF_REMOVABLE	= 0x08,
	VF_SHARED	= 0x10
} VFSFlag;


/* prototypes */
/* accessors */
static unsigned int _browser_vfs_get_flags_mountpoint(char const * mountpoint);


/* public */
/* functions */
/* accessors */
/* browser_vfs_can_eject */
int browser_vfs_can_eject(char const * mountpoint)
{
	unsigned int flags;

	flags = _browser_vfs_get_flags_mountpoint(mountpoint);
	return ((flags & VF_REMOVABLE) != 0) ? 1 : 0;
}


/* browser_vfs_can_mount */
int browser_vfs_can_mount(char const * mountpoint)
{
	unsigned int flags;

	flags = _browser_vfs_get_flags_mountpoint(mountpoint);
	return ((flags & VF_REMOVABLE) != 0
			&& (flags & VF_MOUNTED) == 0) ? 1 : 0;
}


/* browser_vfs_can_unmount */
int browser_vfs_can_unmount(char const * mountpoint)
{
	unsigned int flags;

	flags = _browser_vfs_get_flags_mountpoint(mountpoint);
	return ((flags & VF_REMOVABLE) != 0
			&& (flags & VF_MOUNTED) != 0) ? 1 : 0;
}


/* browser_vfs_is_mountpoint */
int browser_vfs_is_mountpoint(struct stat * lst, dev_t parent)
{
	return (lst->st_dev != parent) ? 1 : 0;
}


/* useful */
/* browser_vfs_lstat */
int browser_vfs_lstat(char const * filename, struct stat * st)
{
	return lstat(filename, st);
}


/* browser_vfs_closedir */
int browser_vfs_closedir(DIR * dir)
{
	return closedir(dir);
}


/* browser_vfs_mime_icon */
static GdkPixbuf * _mime_icon_emblem(GdkPixbuf * pixbuf, int size,
		char const * emblem);
static GdkPixbuf * _mime_icon_folder(Mime * mime, char const * filename,
		struct stat * lst, struct stat * st, int size);
static gboolean _mime_icon_folder_in_home(struct stat * pst);
static gboolean _mime_icon_folder_is_home(struct stat * st);

GdkPixbuf * browser_vfs_mime_icon(Mime * mime, char const * filename,
		char const * type, struct stat * lst, struct stat * st,
		int size)
{
	GdkPixbuf * ret = NULL;
	mode_t mode = (lst != NULL) ? lst->st_mode : 0;
	struct stat s;
	char const * emblem;

	if(filename == NULL)
		return NULL;
	if(type == NULL)
		type = browser_vfs_mime_type(mime, filename,
				S_ISLNK(mode) ? 0 : mode);
	if(st == NULL && browser_vfs_stat(filename, &s) == 0)
		st = &s;
	if(S_ISDIR(mode) || (st != NULL && S_ISDIR(st->st_mode)))
		ret = _mime_icon_folder(mime, filename, lst, st, size);
	else if(S_ISLNK(mode) && (st != NULL && S_ISDIR(st->st_mode)))
		ret = _mime_icon_folder(mime, filename, lst, st, size);
	else
		mime_icons(mime, type, size, &ret, -1);
	if(ret == NULL || lst == NULL)
		return ret;
	/* determine the emblem */
	if(S_ISCHR(lst->st_mode) || S_ISBLK(lst->st_mode))
		emblem = "emblem-system";
	else if(S_ISLNK(lst->st_mode))
		emblem = "emblem-symbolic-link";
	else if((lst->st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) == 0)
		emblem = "emblem-unreadable";
	else if((lst->st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0)
		emblem = "emblem-readonly";
	else
		emblem = NULL;
	/* apply the emblem if relevant */
	if(emblem != NULL)
		ret = _mime_icon_emblem(ret, size, emblem);
	return ret;
}

static GdkPixbuf * _mime_icon_emblem(GdkPixbuf * pixbuf, int size,
		char const * emblem)
{
	int esize;
	GdkPixbuf * epixbuf;
	GtkIconTheme * icontheme;
#if GTK_CHECK_VERSION(2, 14, 0)
	const unsigned int flags = GTK_ICON_LOOKUP_USE_BUILTIN
		| GTK_ICON_LOOKUP_FORCE_SIZE;
#else
	const unsigned int flags = GTK_ICON_LOOKUP_USE_BUILTIN;
#endif

	/* work on a copy */
	epixbuf = gdk_pixbuf_copy(pixbuf);
	g_object_unref(pixbuf);
	pixbuf = epixbuf;
	/* determine the size of the emblem */
	if(size >= 96)
		esize = 32;
	else if(size >= 48)
		esize = 24;
	else
		esize = 12;
	/* obtain the emblem's icon */
	icontheme = gtk_icon_theme_get_default();
	if((epixbuf = gtk_icon_theme_load_icon(icontheme, emblem, esize, flags,
					NULL)) == NULL)
		return pixbuf;
	/* blit the emblem */
#if 0 /* XXX does not show anything (bottom right) */
	gdk_pixbuf_composite(epixbuf, pixbuf, size - esize, size - esize,
			esize, esize, 0, 0, 1.0, 1.0, GDK_INTERP_NEAREST,
			255);
#else /* blitting at the top left instead */
	gdk_pixbuf_composite(epixbuf, pixbuf, 0, 0, esize, esize, 0, 0,
			1.0, 1.0, GDK_INTERP_NEAREST, 255);
#endif
	g_object_unref(epixbuf);
	return pixbuf;
}

static GdkPixbuf * _mime_icon_folder(Mime * mime, char const * filename,
		struct stat * lst, struct stat * st, int size)
{
	GdkPixbuf * ret = NULL;
	char const * icon = NULL;
	struct stat ls;
	struct stat ps;
	char * p;
	size_t i;
	struct
	{
		char const * name;
		char const * icon;
	} name_icon[] =
	{
		{ "DCIM",	"folder-pictures"	},
		{ "Desktop",	"user-desktop"		},
		{ "Documents",	"folder-documents"	},
		{ "Download",	"folder-download"	},
		{ "Downloads",	"folder-download"	},
		{ "Music",	"folder-music"		},
		{ "Pictures",	"folder-pictures"	},
		{ "public_html","folder-publicshare"	},
		{ "Templates",	"folder-templates"	},
		{ "Video",	"folder-videos"		},
		{ "Videos",	"folder-videos"		},
	};
	GtkIconTheme * icontheme;
	const unsigned int flags = GTK_ICON_LOOKUP_FORCE_SIZE;

	if(lst == NULL && browser_vfs_lstat(filename, &ls) == 0)
		lst = &ls;
	/* check if the folder is special */
	if((p = strdup(filename)) != NULL
			&& (lst == NULL || !S_ISLNK(lst->st_mode))
			&& st != NULL
			&& browser_vfs_lstat(dirname(p), &ps) == 0)
	{
		if(st->st_dev != ps.st_dev || st->st_ino == ps.st_ino)
			icon = "mount-point";
		else if(_mime_icon_folder_is_home(st))
			icon = "folder_home";
		else if(_mime_icon_folder_in_home(&ps))
			/* check if the folder is special */
			for(i = 0; i < sizeof(name_icon) / sizeof(*name_icon);
					i++)
				if(strcasecmp(basename(p), name_icon[i].name)
						== 0)
				{
					icon = name_icon[i].icon;
					break;
				}
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
		mime_icons(mime, "inode/directory", size, &ret, -1);
	return ret;
}

static gboolean _mime_icon_folder_in_home(struct stat * pst)
{
	static char const * homedir = NULL;
	static struct stat hst;

	if(homedir == NULL)
	{
		if((homedir = getenv("HOME")) == NULL
				&& (homedir = g_get_home_dir()) == NULL)
			return FALSE;
		if(browser_vfs_stat(homedir, &hst) != 0)
		{
			homedir = NULL;
			return FALSE;
		}
	}
	return (hst.st_dev == pst->st_dev && hst.st_ino == pst->st_ino)
		? TRUE : FALSE;
}

static gboolean _mime_icon_folder_is_home(struct stat * st)
{
	/* FIXME code duplicated from _mime_icon_folder_in_home() */
	static char const * homedir = NULL;
	static struct stat hst;

	if(homedir == NULL)
	{
		if((homedir = getenv("HOME")) == NULL
				&& (homedir = g_get_home_dir()) == NULL)
			return FALSE;
		if(browser_vfs_stat(homedir, &hst) != 0)
		{
			homedir = NULL;
			return FALSE;
		}
	}
	return (hst.st_dev == st->st_dev && hst.st_ino == st->st_ino)
		? TRUE : FALSE;
}


/* browser_vfs_mime_type */
char const * browser_vfs_mime_type(Mime * mime, char const * filename,
		mode_t mode)
{
	char const * ret = NULL;
	struct stat st;
	struct stat pst;
	String * p = NULL;

	if(S_ISDIR(mode))
	{
		/* look for mountpoints */
		if(filename != NULL
				&& browser_vfs_lstat(filename, &st) == 0
				&& (p = string_new(filename)) != NULL
				&& browser_vfs_lstat(dirname(p), &pst) == 0
				&& (st.st_dev != pst.st_dev
					|| st.st_ino == pst.st_ino))
			ret = "inode/mountpoint";
		else
			ret = "inode/directory";
		string_delete(p);
		return ret;
	}
	else if(S_ISBLK(mode))
		return "inode/blockdevice";
	else if(S_ISCHR(mode))
		return "inode/chardevice";
	else if(S_ISFIFO(mode))
		return "inode/fifo";
	else if(S_ISLNK(mode))
		return "inode/symlink";
#ifdef S_ISSOCK
	else if(S_ISSOCK(mode))
		return "inode/socket";
#endif
	if(mime != NULL && filename != NULL)
		ret = mime_type(mime, filename);
	if(ret == NULL && (mode & S_IXUSR) != 0)
		ret = "application/x-executable";
	return ret;
}


/* browser_vfs_opendir */
DIR * browser_vfs_opendir(char const * filename, struct stat * st)
{
	DIR * dir;
	int fd;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %p)\n", __func__, filename, st);
#endif
	if(st == NULL)
		return opendir(filename);
#if defined(__sun)
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
	if(fstat(fd, st) != 0)
	{
		browser_vfs_closedir(dir);
		return NULL;
	}
	return dir;
}


/* browser_vfs_readdir */
struct dirent * browser_vfs_readdir(DIR * dir)
{
	return readdir(dir);
}


/* browser_vfs_stat */
int browser_vfs_stat(char const * filename, struct stat * st)
{
	return stat(filename, st);
}


/* private */
/* functions */
/* browser_vfs_get_flags_mountpoint */
static unsigned int _browser_vfs_get_flags_mountpoint(char const * mountpoint)
{
	unsigned int flags;
#if defined(_PATH_FSTAB)
	struct fstab * f;
#endif
#if defined(ST_NOWAIT)
	struct statvfs * mnt;
	int res;
	int i;

	if((res = getmntinfo(&mnt, ST_NOWAIT)) <= 0)
		return 0;
	for(i = 0; i < res; i++)
	{
		if(strcmp(mnt[i].f_mntfromname, mountpoint) != 0)
			continue;
		flags = VF_MOUNTED;
		flags |= (mnt[i].f_flag & ST_LOCAL) ? 0 : VF_NETWORK;
		flags |= (mnt[i].f_flag & (ST_EXRDONLY | ST_EXPORTED))
			? VF_SHARED : 0;
		flags |= (mnt[i].f_flag & ST_RDONLY) ? VF_READONLY : 0;
		return flags;
	}
#elif defined(MNT_NOWAIT)
	struct statfs * mnt;
	int res;
	int i;
	unsigned int flags;

	if((res = getmntinfo(&mnt, MNT_NOWAIT)) <= 0)
		return 0;
	for(i = 0; i < res; i++)
	{
		if(strcmp(mnt[i].f_mntonname, mountpoint) != 0)
			continue;
		flags = VF_MOUNTED;
		flags |= (mnt[i].f_flags & MNT_LOCAL) ? 0 : VF_NETWORK;
		flags |= (mnt[i].f_flags & MNT_RDONLY) ? VF_READONLY : 0;
		return flags;
	}
#endif
#if defined(_PATH_FSTAB)
	if(setfsent() != 1)
		return -1;
	while((f = getfsent()) != NULL)
	{
		if(strcmp(f->fs_file, mountpoint) != 0
				|| strcmp(f->fs_type, "sw") == 0
				|| strcmp(f->fs_type, "xx") == 0)
			continue;
		flags = (strcmp(f->fs_vfstype, "nfs") == 0
				|| strcmp(f->fs_vfstype, "smbfs") == 0)
			? VF_NETWORK : 0;
		flags |= (strcmp(f->fs_type, "ro") == 0) ? VF_READONLY : 0;
		return flags;
	}
#endif
	return 0;
}
