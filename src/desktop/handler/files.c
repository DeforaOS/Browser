/* $Id$ */
/* Copyright (c) 2020 Pierre Pronchery <khorben@defora.org> */
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
/* FIXME:
 * - remove (links to) application files upon deletion
 * - fix the popup menu for preferences
 * - do not let the symlink window be modal */



/* DesktopHandlerFiles */
/* private */
/* prototypes */
static void _desktophandler_files_init(DesktopHandler * handler);
static void _desktophandler_files_destroy(DesktopHandler * handler);
static void _desktophandler_files_popup(DesktopHandler * handler,
		XButtonEvent * xbev);
static void _desktophandler_files_refresh(DesktopHandler * handler);


/* functions */
/* desktophandler_files_init */
static int _files_init_add_home(DesktopHandler * handler);

static void _desktophandler_files_init(DesktopHandler * handler)
{
	char const * path;
	struct stat st;

	if((path = getenv("XDG_DESKTOP_DIR")) != NULL)
		handler->u.files.path = string_new(path);
	else
		handler->u.files.path = string_new_append(
				desktop_get_home(handler->desktop), "/",
				DESKTOP, NULL);
	handler->u.files.refresh_dir = NULL;
	handler->u.files.refresh_mtime = 0;
	handler->u.files.refresh_source = 0;
	handler->u.files.menu = NULL;
	desktop_set_alignment(handler->desktop, DESKTOP_ALIGNMENT_VERTICAL);
	/* FIXME let it be configured again */
	handler->u.files.show_hidden = FALSE;
	/* check for errors */
	if(handler->u.files.path == NULL)
	{
		desktop_error(handler->desktop, NULL, error_get(NULL), 1);
		return;
	}
	_files_init_add_home(handler);
	if(browser_vfs_stat(handler->u.files.path, &st) == 0)
	{
		if(!S_ISDIR(st.st_mode))
		{
			desktop_error(NULL, handler->u.files.path,
					strerror(ENOTDIR), 1);
			return;
		}
	}
	else if(errno != ENOENT)
		desktop_perror(NULL, handler->u.files.path, 1);
}

static int _files_init_add_home(DesktopHandler * handler)
{
	DesktopIcon * desktopicon;
	GdkPixbuf * icon;
	unsigned int size;

	if((desktopicon = desktopicon_new(handler->desktop, _("Home"),
					desktop_get_home(handler->desktop)))
				== NULL)
		return -desktop_serror(handler->desktop, _("Home"), 1);
	desktopicon_set_first(desktopicon, TRUE);
	desktopicon_set_immutable(desktopicon, TRUE);
	desktop_get_icon_size(handler->desktop, NULL, NULL, &size);
	icon = gtk_icon_theme_load_icon(desktop_get_theme(handler->desktop),
			"gnome-home", size, 0, NULL);
	if(icon == NULL)
		icon = gtk_icon_theme_load_icon(desktop_get_theme(
					handler->desktop), "gnome-fs-home",
				size, 0, NULL);
	if(icon != NULL)
		desktopicon_set_icon(desktopicon, icon);
	/* XXX report errors */
	desktop_icon_add(handler->desktop, desktopicon, FALSE);
	return 0;
}


/* desktophandler_files_destroy */
static void _desktophandler_files_destroy(DesktopHandler * handler)
{
	if(handler->u.files.refresh_source != 0)
		g_source_remove(handler->u.files.refresh_source);
	if(handler->u.files.refresh_dir != NULL)
		browser_vfs_closedir(handler->u.files.refresh_dir);
	if(handler->u.files.menu != NULL)
		gtk_widget_destroy(handler->u.files.menu);
}


/* desktophandler_files_popup */
static void _on_popup_new_folder(gpointer data);
static void _on_popup_new_text_file(gpointer data);
static void _on_popup_paste(gpointer data);
static void _on_popup_preferences(gpointer data);
static void _on_popup_symlink(gpointer data);

static void _desktophandler_files_popup(DesktopHandler * handler,
		XButtonEvent * xbev)
{
	GtkWidget * menuitem;
	GtkWidget * submenu;
	GtkWidget * image;

	if((xbev == NULL || xbev->button != 3) && handler->u.files.menu != NULL)
	{
		gtk_widget_destroy(handler->u.files.menu);
		handler->u.files.menu = NULL;
	}
	handler->u.files.menu = gtk_menu_new();
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_New"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("document-new",
				GTK_ICON_SIZE_MENU));
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	/* submenu for new documents */
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Folder"));
	image = gtk_image_new_from_icon_name("folder-new", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_new_folder), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Symbolic link..."));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_symlink), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Text file"));
	image = gtk_image_new_from_icon_name("stock_new-text",
			GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_new_text_file), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	/* edition */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Paste"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("edit-paste",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_paste), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	/* preferences */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Preferences"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			gtk_image_new_from_icon_name("gtk-preferences",
				GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_popup_preferences), handler);
	gtk_menu_shell_append(GTK_MENU_SHELL(handler->u.files.menu), menuitem);
	gtk_widget_show_all(handler->u.files.menu);
	gtk_menu_popup(GTK_MENU(handler->u.files.menu), NULL, NULL, NULL, NULL,
			3, (xbev != NULL)
			? xbev->time : gtk_get_current_event_time());
}

static void _on_popup_new_folder(gpointer data)
{
	static char const newfolder[] = N_("New folder");
	DesktopHandler * handler = data;
	String * path;

	gtk_widget_destroy(handler->u.files.menu);
	handler->u.files.menu = NULL;
	if((path = string_new_append(handler->u.files.path, "/", newfolder,
					NULL)) == NULL)
	{
		desktop_serror(handler->desktop, newfolder, 0);
		return;
	}
	if(browser_vfs_mkdir(path, 0777) != 0)
		desktop_perror(handler->desktop, path, 0);
	string_delete(path);
}

static void _on_popup_new_text_file(gpointer data)
{
	static char const newtext[] = N_("New text file.txt");
	DesktopHandler * handler = data;
	String * path;
	int fd;

	gtk_widget_destroy(handler->u.files.menu);
	handler->u.files.menu = NULL;
	if((path = string_new_append(handler->u.files.path, "/", _(newtext),
					NULL)) == NULL)
	{
		desktop_serror(handler->desktop, _(newtext), 0);
		return;
	}
	if((fd = creat(path, 0666)) < 0)
		desktop_perror(handler->desktop, path, 0);
	else
		close(fd);
	string_delete(path);
}

static void _on_popup_paste(gpointer data)
{
	DesktopHandler * handler = data;

	/* FIXME implement */
	gtk_widget_destroy(handler->u.files.menu);
	handler->u.files.menu = NULL;
}

static void _on_popup_preferences(gpointer data)
{
	DesktopHandler * handler = data;

	gtk_widget_destroy(handler->u.files.menu);
	handler->u.files.menu = NULL;
	desktop_show_preferences(handler->desktop);
}

static void _on_popup_symlink(gpointer data)
{
	DesktopHandler * handler = data;

	if(_common_symlink(desktop_get_window(handler->desktop),
				handler->u.files.path) != 0)
		desktop_perror(handler->desktop, handler->u.files.path, 0);
}


/* desktophandler_files_refresh */
static gboolean _desktophandler_files_on_refresh(gpointer data);
static gboolean _files_on_refresh_done(DesktopHandler * handler);
static gboolean _files_on_refresh_done_timeout(gpointer data);
static gboolean _files_on_refresh_loop(DesktopHandler * handler);
static int _files_on_refresh_loop_lookup(DesktopHandler * handler,
		char const * name);
static int _files_on_refresh_loop_opendir(DesktopHandler * handler);

static void _desktophandler_files_refresh(DesktopHandler * handler)
{
	if(handler->u.files.refresh_source != 0)
		g_source_remove(handler->u.files.refresh_source);
	if(handler->u.files.refresh_dir != NULL)
		browser_vfs_closedir(handler->u.files.refresh_dir);
	handler->u.files.refresh_dir = NULL;
	if(handler->u.files.path == NULL)
		return;
	handler->u.files.refresh_source = g_idle_add(
			_desktophandler_files_on_refresh, handler);
}

static gboolean _desktophandler_files_on_refresh(gpointer data)
{
	DesktopHandler * handler = data;
	unsigned int i;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	/* FIXME upon deletion one icon too many is removed */
	for(i = 0; i < IDLE_LOOP_ICON_CNT
			&& _files_on_refresh_loop(handler) == 0; i++);
	if(i == IDLE_LOOP_ICON_CNT)
		return TRUE;
	return _files_on_refresh_done(handler);
}

static gboolean _files_on_refresh_done(DesktopHandler * handler)
{
	if(handler->u.files.refresh_dir != NULL)
		browser_vfs_closedir(handler->u.files.refresh_dir);
	handler->u.files.refresh_dir = NULL;
	desktop_icons_cleanup(handler->desktop, TRUE);
	handler->u.files.refresh_source = g_timeout_add(1000,
			_files_on_refresh_done_timeout, handler);
	return FALSE;
}

static gboolean _files_on_refresh_done_timeout(gpointer data)
{
	DesktopHandler * handler = data;
	struct stat st;

	if(handler->u.files.path == NULL)
	{
		handler->u.files.refresh_source = 0;
		return FALSE;
	}
	if(browser_vfs_stat(handler->u.files.path, &st) != 0)
	{
		handler->u.files.refresh_source = 0;
		return desktop_perror(NULL, handler->u.files.path, FALSE);
	}
	if(st.st_mtime == handler->u.files.refresh_mtime)
		return TRUE;
	handler->u.files.refresh_source = 0;
	desktop_refresh(handler->desktop);
	return FALSE;
}

static int _files_on_refresh_loop(DesktopHandler * handler)
{
	const char ext[] = ".desktop";
	struct dirent * de;
	String * p;
	size_t len;
	gchar * q;
	DesktopIcon * desktopicon;
	GError * error = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(handler->u.files.refresh_dir == NULL
			&& _files_on_refresh_loop_opendir(handler) != 0)
		return -1;
	while((de = browser_vfs_readdir(handler->u.files.refresh_dir)) != NULL)
	{
		if(de->d_name[0] == '.')
		{
			if(de->d_name[1] == '\0')
				continue;
			if(de->d_name[1] == '.' && de->d_name[2] == '\0')
				continue;
			if(handler->u.files.show_hidden == 0)
				continue;
		}
		if(_files_on_refresh_loop_lookup(handler, de->d_name) == 1)
			continue;
		break;
	}
	if(de == NULL)
		return -1;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, de->d_name);
#endif
	if((p = string_new_append(handler->u.files.path, "/", de->d_name, NULL))
			== NULL)
		return -desktop_serror(handler->desktop, de->d_name, 1);
	/* detect desktop entries */
	if((len = strlen(de->d_name)) > sizeof(ext)
			&& strcmp(&de->d_name[len - sizeof(ext) + 1], ext) == 0)
		desktopicon = desktopicon_new_application(handler->desktop, p,
				handler->u.files.path);
	/* XXX not relative to the current folder */
	else if((q = g_filename_to_utf8(de->d_name, -1, NULL, NULL, &error))
			!= NULL)
	{
		desktopicon = desktopicon_new(handler->desktop, q, p);
		g_free(q);
	}
	else
	{
		desktop_error(NULL, NULL, error->message, 1);
		g_error_free(error);
		desktopicon = desktopicon_new(handler->desktop, de->d_name, p);
	}
	if(desktopicon != NULL)
		desktop_icon_add(handler->desktop, desktopicon, FALSE);
	string_delete(p);
	return 0;
}

static int _files_on_refresh_loop_lookup(DesktopHandler * handler,
		char const * name)
{
	DesktopIcon * icon;

	if((icon = desktop_icons_lookup(handler->desktop, name)) == NULL)
		return 0;
	desktopicon_set_updated(icon, TRUE);
	return 1;
}

static int _files_on_refresh_loop_opendir(DesktopHandler * handler)
{
	struct stat st;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__,
			handler->u.files.path);
#endif
	if((handler->u.files.refresh_dir = browser_vfs_opendir(
					handler->u.files.path, &st)) == NULL)
	{
		desktop_perror(NULL, handler->u.files.path, 1);
		handler->u.files.refresh_source = 0;
		return -1;
	}
	handler->u.files.refresh_mtime = st.st_mtime;
	return 0;
}
