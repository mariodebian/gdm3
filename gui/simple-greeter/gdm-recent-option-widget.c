/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by: Ray Strode <rstrode@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include "gdm-recent-option-widget.h"

#define GDM_RECENT_OPTION_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GDM_TYPE_RECENT_OPTION_WIDGET, GdmRecentOptionWidgetPrivate))

struct GdmRecentOptionWidgetPrivate
{
        GConfClient                   *gconf_client;
        char                          *gconf_key;
        guint                          notification_id;
        GdmRecentOptionLookupItemFunc  lookup_item_func;

        int                            max_item_count;
};

enum {
        PROP_0,
        PROP_MAX_ITEM_COUNT,
};

enum {
        NUMBER_OF_SIGNALS
};

static void     gdm_recent_option_widget_class_init  (GdmRecentOptionWidgetClass *klass);
static void     gdm_recent_option_widget_init        (GdmRecentOptionWidget      *option_widget);
static void     gdm_recent_option_widget_finalize    (GObject              *object);

G_DEFINE_TYPE (GdmRecentOptionWidget, gdm_recent_option_widget, GDM_TYPE_OPTION_WIDGET)

static GSList *
gdm_recent_option_widget_get_list_from_gconf (GdmRecentOptionWidget *widget)
{
        GConfValue *value;
        GSList     *value_list;
        GSList     *list;
        GSList     *tmp;
        GError     *lookup_error;

        lookup_error = NULL;
        value = gconf_client_get (widget->priv->gconf_client,
                                  widget->priv->gconf_key,
                                  &lookup_error);

        if (lookup_error != NULL) {
                g_warning ("could not get gconf key '%s': %s",
                           widget->priv->gconf_key,
                           lookup_error->message);
                g_error_free (lookup_error);
                return NULL;
        }

        if (value == NULL) {
                g_warning ("gconf key '%s' is unset",
                           widget->priv->gconf_key);
                return NULL;
        }

        if (value->type != GCONF_VALUE_LIST ||
            gconf_value_get_list_type (value) != GCONF_VALUE_STRING) {
                g_warning ("gconf key is not a list of strings");
                return NULL;
        }

        value_list = gconf_value_get_list (value);

        list = NULL;
        for (tmp = value_list; tmp != NULL; tmp = tmp->next) {
                const char *id;

                id = gconf_value_get_string ((GConfValue *)tmp->data);

                list = g_slist_prepend (list, g_strdup (id));
        }
        list = g_slist_reverse (list);

        gconf_value_free (value);

        return list;
}

static GSList *
truncate_list (GSList *list,
               int     size)
{
    GSList     *tmp;
    GSList     *next_node;

    tmp = g_slist_nth (list, size);
    do {
        next_node = tmp->next;

        g_free (tmp->data);
        list = g_slist_delete_link (list, tmp);
        tmp = next_node;
    } while (tmp != NULL);

    return list;
}

static void
gdm_recent_option_widget_sync_items_from_gconf (GdmRecentOptionWidget *widget)
{
        GSList     *list;
        GSList     *tmp;
        gboolean    default_is_set;

        list = gdm_recent_option_widget_get_list_from_gconf (widget);

        if (widget->priv->max_item_count > 0 && g_slist_length (list) > widget->priv->max_item_count) {
                list = truncate_list (list, widget->priv->max_item_count);
                gconf_client_set_list (widget->priv->gconf_client,
                                       widget->priv->gconf_key,
                                       GCONF_VALUE_STRING, list, NULL);
                g_slist_foreach (list, (GFunc) g_free, NULL);
                g_slist_free (list);

                list = gdm_recent_option_widget_get_list_from_gconf (widget);
        }

        gdm_option_widget_remove_all_items (GDM_OPTION_WIDGET (widget));
        default_is_set = FALSE;

        for (tmp = list; tmp != NULL; tmp = tmp->next) {
                const char *key;
                char       *id;
                char       *name;
                char       *comment;

                key = (char *) tmp->data;

                id = widget->priv->lookup_item_func (widget, key, &name, &comment);

                if (id != NULL) {
                        gboolean item_exists;

                        item_exists = gdm_option_widget_lookup_item (GDM_OPTION_WIDGET (widget), id, NULL, NULL, NULL);

                        if (item_exists) {
                                continue;
                        }

                        gdm_option_widget_add_item (GDM_OPTION_WIDGET (widget),
                                                    id, name, comment,
                                                    GDM_OPTION_WIDGET_POSITION_MIDDLE);
                        if (!default_is_set) {
                                gdm_option_widget_set_active_item (GDM_OPTION_WIDGET (widget),
                                        id);
                                default_is_set = TRUE;
                        }

                        g_free (name);
                        g_free (comment);
                        g_free (id);
                }
        }

        g_slist_foreach (list, (GFunc) g_free, NULL);
        g_slist_free (list);
}

static void
gdm_recent_option_widget_notify_func (GConfClient *client,
                                      guint        connection_id,
                                      GConfEntry  *entry,
                                      gpointer     user_data)
{
        gdm_recent_option_widget_sync_items_from_gconf (GDM_RECENT_OPTION_WIDGET (user_data));
}

gboolean
gdm_recent_option_widget_set_gconf_key (GdmRecentOptionWidget          *widget,
                                        const char                     *gconf_key,
                                        GdmRecentOptionLookupItemFunc   lookup_item_func,
                                        GError                        **error)
{
        GError *notify_error;

        if (widget->priv->gconf_key != NULL &&
            strcmp (widget->priv->gconf_key, gconf_key) == 0) {
                return TRUE;
        }

        if (widget->priv->notification_id != 0) {
                gconf_client_notify_remove (widget->priv->gconf_client,
                                            widget->priv->notification_id);

                widget->priv->notification_id = 0;
        }

        g_free (widget->priv->gconf_key);
        widget->priv->gconf_key = g_strdup (gconf_key);

        widget->priv->lookup_item_func = lookup_item_func;

        gdm_recent_option_widget_sync_items_from_gconf (widget);

        notify_error = NULL;
        widget->priv->notification_id = gconf_client_notify_add (widget->priv->gconf_client,
                                                                 gconf_key,
                                                                 (GConfClientNotifyFunc)
                                                                 gdm_recent_option_widget_notify_func,
                                                                 widget, NULL, &notify_error);
        if (notify_error != NULL) {
                g_propagate_error (error, notify_error);
                return FALSE;
        }

        return TRUE;
}

static void
gdm_recent_option_widget_dispose (GObject *object)
{
        G_OBJECT_CLASS (gdm_recent_option_widget_parent_class)->dispose (object);
}

static void
gdm_recent_option_widget_set_property (GObject        *object,
                                       guint           prop_id,
                                       const GValue   *value,
                                       GParamSpec     *pspec)
{
        GdmRecentOptionWidget *self;

        self = GDM_RECENT_OPTION_WIDGET (object);

        switch (prop_id) {
        case PROP_MAX_ITEM_COUNT:
                self->priv->max_item_count = g_value_get_int (value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gdm_recent_option_widget_get_property (GObject        *object,
                                       guint           prop_id,
                                       GValue         *value,
                                       GParamSpec     *pspec)
{
        GdmRecentOptionWidget *self;

        self = GDM_RECENT_OPTION_WIDGET (object);

        switch (prop_id) {
        case PROP_MAX_ITEM_COUNT:
                g_value_set_int (value,
                                 self->priv->max_item_count);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gdm_recent_option_widget_class_init (GdmRecentOptionWidgetClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gdm_recent_option_widget_get_property;
        object_class->set_property = gdm_recent_option_widget_set_property;
        object_class->dispose = gdm_recent_option_widget_dispose;
        object_class->finalize = gdm_recent_option_widget_finalize;

        g_object_class_install_property (object_class,
                                         PROP_MAX_ITEM_COUNT,
                                         g_param_spec_int ("max-item-count",
                                                              _("Max Item Count"),
                                                              _("The maximum number of items to keep around in the list"),
                                                              0, G_MAXINT, 5,
                                                              (G_PARAM_READWRITE |
                                                               G_PARAM_CONSTRUCT)));


        g_type_class_add_private (klass, sizeof (GdmRecentOptionWidgetPrivate));
}

static void
gdm_recent_option_widget_init (GdmRecentOptionWidget *widget)
{
        widget->priv = GDM_RECENT_OPTION_WIDGET_GET_PRIVATE (widget);

        widget->priv->gconf_client = gconf_client_get_default ();
        widget->priv->max_item_count = 5;
}

static void
gdm_recent_option_widget_finalize (GObject *object)
{
        GdmRecentOptionWidget *widget;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDM_IS_OPTION_WIDGET (object));

        widget = GDM_RECENT_OPTION_WIDGET (object);

        g_return_if_fail (widget->priv != NULL);

        g_object_unref (widget->priv->gconf_client);

        G_OBJECT_CLASS (gdm_recent_option_widget_parent_class)->finalize (object);
}

GtkWidget *
gdm_recent_option_widget_new (const char *label_text,
                              int         max_item_count)
{
        GObject *object;

        object = g_object_new (GDM_TYPE_RECENT_OPTION_WIDGET,
                               "label-text", label_text,
                               "max-item-count", max_item_count,
                               NULL);

        return GTK_WIDGET (object);
}

void
gdm_recent_option_widget_add_item (GdmRecentOptionWidget *widget,
                                   const char            *id)
{
        GSList *list;

        list = gdm_recent_option_widget_get_list_from_gconf (widget);

        list = g_slist_prepend (list, g_strdup (id));

        gconf_client_set_list (widget->priv->gconf_client,
                          widget->priv->gconf_key,
                          GCONF_VALUE_STRING, list, NULL);

        g_slist_foreach (list, (GFunc) g_free, NULL);
        g_slist_free (list);

        gdm_recent_option_widget_sync_items_from_gconf (widget);
}
