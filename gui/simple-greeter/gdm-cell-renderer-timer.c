/*
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
 *  Written by: Ray Strode <rstrode@redhat.com>
 */

#include "config.h"
#include "gdm-cell-renderer-timer.h"
#include <glib/gi18n.h>

#define GDM_CELL_RENDERER_TIMER_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), GDM_TYPE_CELL_RENDERER_TIMER, GdmCellRendererTimerPrivate))

struct _GdmCellRendererTimerPrivate
{
        gdouble    value;
};

enum
{
        PROP_0,
        PROP_VALUE,
};

G_DEFINE_TYPE (GdmCellRendererTimer, gdm_cell_renderer_timer, GTK_TYPE_CELL_RENDERER)

static void
gdm_cell_renderer_timer_get_property (GObject *object,
                                      guint param_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
        GdmCellRendererTimer *renderer;

        renderer = GDM_CELL_RENDERER_TIMER (object);

        switch (param_id) {
                case PROP_VALUE:
                        g_value_set_double (value, renderer->priv->value);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
}

static void
gdm_cell_renderer_timer_set_value (GdmCellRendererTimer *renderer,
                                   gdouble               value)
{
        renderer->priv->value = value;
}

static void
gdm_cell_renderer_timer_set_property (GObject *object,
                                      guint param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
        GdmCellRendererTimer *renderer;

        renderer = GDM_CELL_RENDERER_TIMER (object);

        switch (param_id) {
                case PROP_VALUE:
                        gdm_cell_renderer_timer_set_value (renderer,
                                                           g_value_get_double (value));
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gdm_cell_renderer_timer_get_size (GtkCellRenderer *cell,
                                  GtkWidget       *widget,
                                  GdkRectangle    *cell_area,
                                  gint            *x_offset,
                                  gint            *y_offset,
                                  gint            *width,
                                  gint            *height)
{

        GdmCellRendererTimer *renderer;

        renderer = GDM_CELL_RENDERER_TIMER (cell);

        if (cell_area != NULL) {
                if (x_offset != NULL) {
                        *x_offset = 0;
                }

                if (y_offset != NULL) {
                        *y_offset = 0;
                }
        }

        if (width != NULL) {
                *width = cell->xpad * 2 + 24;
        }

        if (height != NULL) {
                *height = cell->ypad * 2 + 24;
        }
}

static double
get_opacity_for_value (double value)
{
        const double start_value = 0.05;
        const double end_value = 0.33;

        if (value < start_value) {
                return 0.0;
        }

        if (value >= end_value) {
                return 1.0;
        }

        return ((value - start_value) / (end_value - start_value));
}

static void
draw_timer (GdmCellRendererTimer *renderer,
            cairo_t              *context,
            GdkColor             *fg,
            GdkColor             *bg,
            int                   width,
            int                   height)
{
        double radius;
        double opacity;

        opacity = get_opacity_for_value (renderer->priv->value);

        if (opacity <= G_MINDOUBLE) {
                return;
        }

        radius = .5 * (MIN (width, height) / 2.0);

        cairo_translate (context, width / 2., height / 2.);

        cairo_set_source_rgba (context,
                               fg->red / 65535.0,
                               fg->green / 65535.0,
                               fg->blue / 65535.0,
                               opacity);

        cairo_move_to (context, 0, 0);
        cairo_arc (context, 0, 0, radius + 1, 0, 2 * G_PI);
        cairo_fill (context);

        cairo_set_source_rgb (context,
                              bg->red / 65535.0,
                              bg->green / 65535.0,
                              bg->blue / 65535.0);
        cairo_move_to (context, 0, 0);
        cairo_arc (context, 0, 0, radius, - G_PI / 2,
                   renderer->priv->value * 2 * G_PI - G_PI / 2);
        cairo_clip (context);
        cairo_paint_with_alpha (context, opacity);
}

static void
gdm_cell_renderer_timer_render (GtkCellRenderer      *cell,
                                GdkWindow            *window,
                                GtkWidget            *widget,
                                GdkRectangle         *background_area,
                                GdkRectangle         *cell_area,
                                GdkRectangle         *expose_area,
                                GtkCellRendererState  renderer_state)
{
        GdmCellRendererTimer *renderer;
        cairo_t              *context;
        GtkStateType          widget_state;

        renderer = GDM_CELL_RENDERER_TIMER (cell);

        if (renderer->priv->value <= G_MINDOUBLE) {
                return;
        }

        context = gdk_cairo_create (GDK_DRAWABLE (window));

        if (expose_area != NULL) {
                gdk_cairo_rectangle (context, expose_area);
                cairo_clip (context);
        }

        cairo_translate (context,
                         cell_area->x + cell->xpad,
                         cell_area->y + cell->ypad);

        widget_state = GTK_STATE_NORMAL;
        if (renderer_state & GTK_CELL_RENDERER_SELECTED) {
                if (GTK_WIDGET_HAS_FOCUS (widget)) {
                        widget_state = GTK_STATE_SELECTED;
                } else {
                        widget_state = GTK_STATE_ACTIVE;
                }
        }

        if (renderer_state & GTK_CELL_RENDERER_INSENSITIVE) {
                widget_state = GTK_STATE_INSENSITIVE;
        }

        draw_timer (renderer, context,
                    &widget->style->text_aa[widget_state],
                    &widget->style->base[widget_state],
                    cell_area->width, cell_area->height);

        cairo_destroy (context);
}

static void
gdm_cell_renderer_timer_class_init (GdmCellRendererTimerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

        object_class->get_property = gdm_cell_renderer_timer_get_property;
        object_class->set_property = gdm_cell_renderer_timer_set_property;

        cell_class->get_size = gdm_cell_renderer_timer_get_size;
        cell_class->render = gdm_cell_renderer_timer_render;

        g_object_class_install_property (object_class,
                                         PROP_VALUE,
                                         g_param_spec_double ("value",
                                         _("Value"),
                                         _("percentage of time complete"),
                                         0.0, 1.0, 0.0,
                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

        g_type_class_add_private (object_class,
                            sizeof (GdmCellRendererTimerPrivate));
}

static void
gdm_cell_renderer_timer_init (GdmCellRendererTimer *renderer)
{
        renderer->priv = GDM_CELL_RENDERER_TIMER_GET_PRIVATE (renderer);
}

GtkCellRenderer*
gdm_cell_renderer_timer_new (void)
{
        return g_object_new (GDM_TYPE_CELL_RENDERER_TIMER, NULL);
}

