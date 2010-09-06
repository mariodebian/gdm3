/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GDM_GREETER_PANEL_H
#define __GDM_GREETER_PANEL_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDM_TYPE_GREETER_PANEL         (gdm_greeter_panel_get_type ())
#define GDM_GREETER_PANEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDM_TYPE_GREETER_PANEL, GdmGreeterPanel))
#define GDM_GREETER_PANEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDM_TYPE_GREETER_PANEL, GdmGreeterPanelClass))
#define GDM_IS_GREETER_PANEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDM_TYPE_GREETER_PANEL))
#define GDM_IS_GREETER_PANEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDM_TYPE_GREETER_PANEL))
#define GDM_GREETER_PANEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDM_TYPE_GREETER_PANEL, GdmGreeterPanelClass))

typedef struct GdmGreeterPanelPrivate GdmGreeterPanelPrivate;

typedef struct
{
        GtkWindow               parent;
        GdmGreeterPanelPrivate *priv;
} GdmGreeterPanel;

typedef struct
{
        GtkWindowClass   parent_class;

        void (* language_selected)           (GdmGreeterPanel *panel,
                                              const char      *text);

        void (* layout_selected)             (GdmGreeterPanel *panel,
                                              const char      *text);

        void (* session_selected)            (GdmGreeterPanel *panel,
                                              const char      *text);
} GdmGreeterPanelClass;

GType                  gdm_greeter_panel_get_type                       (void);

GtkWidget            * gdm_greeter_panel_new                            (GdkScreen *screen,
                                                                         int        monitor,
                                                                         gboolean   is_local);

void                   gdm_greeter_panel_show_user_options              (GdmGreeterPanel *panel);
void                   gdm_greeter_panel_hide_user_options              (GdmGreeterPanel *panel);
void                   gdm_greeter_panel_reset                          (GdmGreeterPanel *panel);
void                   gdm_greeter_panel_set_keyboard_layout            (GdmGreeterPanel *panel,
                                                                         const char      *layout_name);

void                   gdm_greeter_panel_set_default_language_name      (GdmGreeterPanel *panel,
                                                                         const char      *language_name);
void                   gdm_greeter_panel_set_default_session_name       (GdmGreeterPanel *panel,
                                                                         const char      *session_name);
G_END_DECLS

#endif /* __GDM_GREETER_PANEL_H */
