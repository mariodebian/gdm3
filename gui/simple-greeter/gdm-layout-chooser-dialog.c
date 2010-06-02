/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Matthias Clasen <mclasen@redhat.com>
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
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gdm-layout-chooser-widget.h"
#include "gdm-layout-chooser-dialog.h"

#define GDM_LAYOUT_CHOOSER_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GDM_TYPE_LAYOUT_CHOOSER_DIALOG, GdmLayoutChooserDialogPrivate))

struct GdmLayoutChooserDialogPrivate
{
        GtkWidget *chooser_widget;
};


static void     gdm_layout_chooser_dialog_class_init  (GdmLayoutChooserDialogClass *klass);
static void     gdm_layout_chooser_dialog_init        (GdmLayoutChooserDialog      *layout_chooser_dialog);
static void     gdm_layout_chooser_dialog_finalize    (GObject                       *object);

G_DEFINE_TYPE (GdmLayoutChooserDialog, gdm_layout_chooser_dialog, GTK_TYPE_DIALOG)

char *
gdm_layout_chooser_dialog_get_current_layout_name (GdmLayoutChooserDialog *dialog)
{
        char *layout_name;

        g_return_val_if_fail (GDM_IS_LAYOUT_CHOOSER_DIALOG (dialog), NULL);

        layout_name = gdm_layout_chooser_widget_get_current_layout_name (GDM_LAYOUT_CHOOSER_WIDGET (dialog->priv->chooser_widget));

        return layout_name;
}

void
gdm_layout_chooser_dialog_set_current_layout_name (GdmLayoutChooserDialog *dialog,
                                                   const char             *layout_name)
{

        g_return_if_fail (GDM_IS_LAYOUT_CHOOSER_DIALOG (dialog));

        gdm_layout_chooser_widget_set_current_layout_name (GDM_LAYOUT_CHOOSER_WIDGET (dialog->priv->chooser_widget), layout_name);
}

static void
gdm_layout_chooser_dialog_size_request (GtkWidget      *widget,
                                        GtkRequisition *requisition)
{
        int            screen_w;
        int            screen_h;
        GtkRequisition child_requisition;

        if (GTK_WIDGET_CLASS (gdm_layout_chooser_dialog_parent_class)->size_request) {
                GTK_WIDGET_CLASS (gdm_layout_chooser_dialog_parent_class)->size_request (widget, requisition);
        }

        screen_w = gdk_screen_get_width (gtk_widget_get_screen (widget));
        screen_h = gdk_screen_get_height (gtk_widget_get_screen (widget));

        gtk_widget_get_child_requisition (GTK_BIN (widget)->child, &child_requisition);
        *requisition = child_requisition;

        requisition->width += 2 * GTK_CONTAINER (widget)->border_width;
        requisition->height += 2 * GTK_CONTAINER (widget)->border_width;

        requisition->width = MIN (requisition->width, .50 * screen_w);
        requisition->height = MIN (requisition->height, .80 * screen_h);
}

static void
gdm_layout_chooser_dialog_realize (GtkWidget *widget)
{
        GdmLayoutChooserDialog *chooser_dialog;

        chooser_dialog = GDM_LAYOUT_CHOOSER_DIALOG (widget);

        gtk_widget_show (chooser_dialog->priv->chooser_widget);

        GTK_WIDGET_CLASS (gdm_layout_chooser_dialog_parent_class)->realize (widget);
}

static void
gdm_layout_chooser_dialog_class_init (GdmLayoutChooserDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = gdm_layout_chooser_dialog_finalize;
        widget_class->size_request = gdm_layout_chooser_dialog_size_request;
        widget_class->realize = gdm_layout_chooser_dialog_realize;

        g_type_class_add_private (klass, sizeof (GdmLayoutChooserDialogPrivate));
}

static gboolean
respond (GdmLayoutChooserDialog *dialog)
{
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

        return FALSE;
}

static void
queue_response (GdmLayoutChooserDialog *dialog)
{
        g_idle_add ((GSourceFunc) respond, dialog);
}

static void
gdm_layout_chooser_dialog_init (GdmLayoutChooserDialog *dialog)
{

        dialog->priv = GDM_LAYOUT_CHOOSER_DIALOG_GET_PRIVATE (dialog);

        dialog->priv->chooser_widget = gdm_layout_chooser_widget_new ();
        gdm_chooser_widget_set_hide_inactive_items (GDM_CHOOSER_WIDGET (dialog->priv->chooser_widget),
                                                    FALSE);

        gdm_layout_chooser_widget_set_current_layout_name (GDM_LAYOUT_CHOOSER_WIDGET (dialog->priv->chooser_widget),
                                                               setlocale (LC_MESSAGES, NULL));
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog->priv->chooser_widget);

        g_signal_connect_swapped (G_OBJECT (dialog->priv->chooser_widget),
                                  "activated", G_CALLBACK (queue_response),
                                  dialog);

        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                GTK_STOCK_OK, GTK_RESPONSE_OK,
                                NULL);

        gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
        gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
        gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->chooser_widget), 5);
        gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_set_default_size (GTK_WINDOW (dialog), 512, 440);
        gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
gdm_layout_chooser_dialog_finalize (GObject *object)
{
        GdmLayoutChooserDialog *layout_chooser_dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDM_IS_LAYOUT_CHOOSER_DIALOG (object));

        layout_chooser_dialog = GDM_LAYOUT_CHOOSER_DIALOG (object);

        g_return_if_fail (layout_chooser_dialog->priv != NULL);

        G_OBJECT_CLASS (gdm_layout_chooser_dialog_parent_class)->finalize (object);
}

GtkWidget *
gdm_layout_chooser_dialog_new (void)
{
        GObject *object;

        object = g_object_new (GDM_TYPE_LAYOUT_CHOOSER_DIALOG,
                               "icon-name", "preferences-desktop-keyboard",
                               "title", _("Keyboard layouts"),
                               "border-width", 8,
                               "modal", TRUE,
                               NULL);

        return GTK_WIDGET (object);
}
