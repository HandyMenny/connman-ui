/*
 *
 *  Connection Manager UI
 *
 *  Copyright (C) 2012  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <connman-ui-gtk.h>
#include <gtktechnology.h>
#include <gtkservice.h>

#define CUI_RIGHT_MENU_UI_PATH CUI_UI_PATH "/right_menu.ui"

static GtkMenu *cui_right_menu = NULL;
static GtkMenu *cui_more_menu = NULL;
static GtkMenu *cui_tethering_menu = NULL;
static GtkMenuItem *cui_list_more_item = NULL;
static GtkMenuItem *cui_scan_spinner = NULL;
static GtkWidget *cui_item_mode_off = NULL;
static GtkWidget *cui_item_mode_on = NULL;
static GHashTable *tech_items = NULL;
static GHashTable *service_items = NULL;
static int item_position = 3;
static gboolean disabled = TRUE;

static void add_technology(const char *path)
{
	GtkTechnology *t;

	t = gtk_technology_new(path);
	gtk_menu_shell_insert(GTK_MENU_SHELL(cui_right_menu),
					(GtkWidget *)t, item_position);
	item_position++;

	gtk_widget_set_visible((GtkWidget *)t, TRUE);
	gtk_widget_show((GtkWidget *)t);

	gtk_menu_shell_append(GTK_MENU_SHELL(cui_tethering_menu),
			(GtkWidget *)gtk_technology_get_tethering_item(t));

	g_hash_table_insert(tech_items, t->path, t);
}

static void technology_added_cb(const char *path)
{
	add_technology(path);
}

static void technology_removed_cb(const char *path)
{
	g_hash_table_remove(tech_items, path);
}

static void delete_technology_item(gpointer data)
{
	GtkWidget *item = data;

	gtk_widget_destroy(item);
	item_position--;
}

static void add_or_update_service(const char *path, int position)
{
	GtkService *s;

	s = gtk_service_new(path);

	if (position > 9)
		gtk_menu_shell_append(GTK_MENU_SHELL(cui_more_menu),
							(GtkWidget *)s);
	else
		gtk_menu_shell_insert(GTK_MENU_SHELL(cui_right_menu),
						(GtkWidget *)s, position);

	gtk_widget_set_visible((GtkWidget *)s, TRUE);
	gtk_widget_show((GtkWidget *)s);

	g_hash_table_insert(service_items, s->path, s);
}

static void delete_service_item(gpointer data)
{
	GtkWidget *item = data;

	gtk_widget_destroy(item);
}

static void get_services_cb(void *user_data)
{
	GSList *services, *list;
	int item_position = 2;

	services = connman_service_get_services();
	if (services == NULL)
		return;

	gtk_widget_hide(GTK_WIDGET(cui_list_more_item));

	for (list = services; list; list = list->next, item_position++)
		add_or_update_service(list->data, item_position);

	if (item_position > 10)
		gtk_widget_show(GTK_WIDGET(cui_list_more_item));

	g_slist_free(services);
}

static void remove_service_cb(const char *path)
{
	g_hash_table_remove(service_items, path);
}

static void scanning_cb(void *user_data)
{
	GtkSpinner *spin;

	spin = (GtkSpinner *)gtk_bin_get_child(GTK_BIN(cui_scan_spinner));

	gtk_spinner_stop(spin);
	gtk_widget_hide((GtkWidget *)cui_scan_spinner);
	gtk_widget_hide((GtkWidget *)spin);
}

static void cui_item_offlinemode_activate(GtkMenuItem *menuitem,
						gpointer user_data)
{
	gboolean offlinemode;

	offlinemode = !connman_manager_get_offlinemode();

	connman_manager_set_offlinemode(offlinemode);
}

static void cui_popup_right_menu(GtkStatusIcon *trayicon,
						guint button,
						guint activate_time,
						gpointer user_data)
{
	GList *tech_list, *list;
	GtkSpinner *spin;

	spin = (GtkSpinner *)gtk_bin_get_child(GTK_BIN(cui_scan_spinner));

	gtk_widget_hide(GTK_WIDGET(cui_list_more_item));
	gtk_widget_show((GtkWidget *)cui_scan_spinner);
	gtk_widget_show((GtkWidget *)spin);

	gtk_spinner_start(spin);

	connman_service_set_removed_callback(remove_service_cb);

	connman_service_refresh_services_list(get_services_cb,
							scanning_cb, user_data);

	if (disabled == TRUE)
		goto popup;

	if (connman_manager_get_offlinemode()) {
		gtk_widget_show(cui_item_mode_on);
		gtk_widget_hide(cui_item_mode_off);
	} else {
		gtk_widget_show(cui_item_mode_off);
		gtk_widget_hide(cui_item_mode_on);
	}

	tech_list = connman_technology_get_technologies();
	if (tech_list == NULL)
		goto popup;

	for (list = tech_list; list != NULL; list = list->next)
		add_technology((const char *)list->data);

	g_list_free(tech_list);

popup:
	connman_technology_set_removed_callback(technology_removed_cb);
	connman_technology_set_added_callback(technology_added_cb);

	gtk_menu_popup(cui_right_menu, NULL, NULL,
				NULL, NULL, button, activate_time);
}

static void cui_popdown_right_menu(GtkMenu *menu, gpointer user_data)
{
	connman_technology_set_removed_callback(NULL);
	connman_technology_set_added_callback(NULL);
	connman_service_set_removed_callback(NULL);
	g_hash_table_remove_all(service_items);
	g_hash_table_remove_all(tech_items);
	connman_service_free_services_list();
}

static void cui_enable_item(gboolean enable)
{
	set_widget_hidden(cui_builder, "cui_item_offlinemode_on", enable);
	set_widget_hidden(cui_builder, "cui_item_offlinemode_off", enable);
	set_widget_hidden(cui_builder, "cui_sep_technology_up", enable);
	set_widget_hidden(cui_builder, "cui_sep_technology_down", enable);
	set_widget_hidden(cui_builder, "cui_item_tethering", enable);
	set_widget_hidden(cui_builder, "cui_sep_tethering", enable);

	disabled = enable;
}

void cui_right_menu_enable_only_quit(void)
{
	cui_enable_item(TRUE);
}

void cui_right_menu_enable_all(void)
{
	cui_enable_item(FALSE);
}

gint cui_load_right_menu(GtkBuilder *builder, GtkStatusIcon *trayicon)
{
	GtkImageMenuItem *cui_item_tethering;
	GtkMenuItem *cui_item_quit;
	GdkPixbuf *image = NULL;
	GError *error = NULL;

	gtk_builder_add_from_file(builder, CUI_RIGHT_MENU_UI_PATH, &error);
	if (error != NULL) {
		printf("Error: %s\n", error->message);
		g_error_free(error);

		return -EINVAL;
	}

	cui_builder = builder;

	cui_right_menu = (GtkMenu *) gtk_builder_get_object(builder,
							"cui_right_menu");
	cui_item_quit = (GtkMenuItem *) gtk_builder_get_object(builder,
							"cui_item_quit");
	cui_more_menu = (GtkMenu *) gtk_builder_get_object(builder,
							"cui_more_menu");
	cui_list_more_item = (GtkMenuItem *) gtk_builder_get_object(builder,
							"cui_list_more_item");
	cui_scan_spinner = (GtkMenuItem *) gtk_builder_get_object(builder,
							"cui_scan_spinner");

	cui_item_mode_off = (GtkWidget *) gtk_builder_get_object(builder,
							"cui_item_offlinemode_off");
	cui_item_mode_on = (GtkWidget *) gtk_builder_get_object(builder,
							"cui_item_offlinemode_on");
	cui_tethering_menu = (GtkMenu *) gtk_builder_get_object(builder,
							"cui_tethering_menu");

	gtk_container_add(GTK_CONTAINER(cui_scan_spinner), gtk_spinner_new());
	g_signal_connect(cui_right_menu, "deactivate",
				G_CALLBACK(cui_popdown_right_menu), NULL);
	g_signal_connect(cui_item_mode_off, "activate",
			G_CALLBACK(cui_item_offlinemode_activate), NULL);
	g_signal_connect(cui_item_mode_on, "activate",
			G_CALLBACK(cui_item_offlinemode_activate), NULL);

	service_items = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, delete_service_item);
	tech_items = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, delete_technology_item);

	cui_item_tethering = (GtkImageMenuItem *) gtk_builder_get_object(
						builder, "cui_item_tethering");


	cui_tray_hook_right_menu(cui_popup_right_menu);

	return 0;
}
