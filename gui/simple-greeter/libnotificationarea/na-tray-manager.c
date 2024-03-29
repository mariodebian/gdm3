/* na-tray-manager.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Used to be: eggtraymanager.c
 */

#include <config.h>
#include <string.h>
#include <libintl.h>

#include "na-tray-manager.h"

#include <gdkconfig.h>
#include <glib/gi18n.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#endif
#include <gtk/gtk.h>

#include "na-marshal.h"

/* Signals */
enum
{
  TRAY_ICON_ADDED,
  TRAY_ICON_REMOVED,
  MESSAGE_SENT,
  MESSAGE_CANCELLED,
  LOST_SELECTION,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ORIENTATION
};

typedef struct
{
  long id, len;
  long remaining_len;
  
  long timeout;
  char *str;
#ifdef GDK_WINDOWING_X11
  Window window;
#endif
} PendingMessage;

static guint manager_signals[LAST_SIGNAL];

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

#ifdef GDK_WINDOWING_X11
static gboolean na_tray_manager_check_running_screen_x11 (GdkScreen *screen);
#endif

static void na_tray_manager_finalize     (GObject      *object);
static void na_tray_manager_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec);
static void na_tray_manager_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec);

static void na_tray_manager_unmanage (NaTrayManager *manager);

G_DEFINE_TYPE (NaTrayManager, na_tray_manager, G_TYPE_OBJECT)

static void
na_tray_manager_init (NaTrayManager *manager)
{
  manager->invisible = NULL;
  manager->socket_table = g_hash_table_new (NULL, NULL);
}

static void
na_tray_manager_class_init (NaTrayManagerClass *klass)
{
  GObjectClass *gobject_class;
  
  gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = na_tray_manager_finalize;
  gobject_class->set_property = na_tray_manager_set_property;
  gobject_class->get_property = na_tray_manager_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
						      "orientation",
						      "orientation",
						      GTK_TYPE_ORIENTATION,
						      GTK_ORIENTATION_HORIZONTAL,
						      G_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT |
						      G_PARAM_STATIC_NAME |
						      G_PARAM_STATIC_NICK |
						      G_PARAM_STATIC_BLURB));
  
  manager_signals[TRAY_ICON_ADDED] =
    g_signal_new ("tray_icon_added",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, tray_icon_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_SOCKET);

  manager_signals[TRAY_ICON_REMOVED] =
    g_signal_new ("tray_icon_removed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, tray_icon_removed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_SOCKET);
  manager_signals[MESSAGE_SENT] =
    g_signal_new ("message_sent",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, message_sent),
		  NULL, NULL,
		  _na_marshal_VOID__OBJECT_STRING_LONG_LONG,
		  G_TYPE_NONE, 4,
		  GTK_TYPE_SOCKET,
		  G_TYPE_STRING,
		  G_TYPE_LONG,
		  G_TYPE_LONG);
  manager_signals[MESSAGE_CANCELLED] =
    g_signal_new ("message_cancelled",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, message_cancelled),
		  NULL, NULL,
		  _na_marshal_VOID__OBJECT_LONG,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_SOCKET,
		  G_TYPE_LONG);
  manager_signals[LOST_SELECTION] =
    g_signal_new ("lost_selection",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, lost_selection),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

#if defined (GDK_WINDOWING_X11)
  /* Nothing */
#elif defined (GDK_WINDOWING_WIN32)
  g_warning ("Port NaTrayManager to Win32");
#else
  g_warning ("Port NaTrayManager to this GTK+ backend");
#endif
}

static void
na_tray_manager_finalize (GObject *object)
{
  NaTrayManager *manager;
  
  manager = NA_TRAY_MANAGER (object);

  na_tray_manager_unmanage (manager);

  g_list_free (manager->messages);
  g_hash_table_destroy (manager->socket_table);
  
  G_OBJECT_CLASS (na_tray_manager_parent_class)->finalize (object);
}

static void
na_tray_manager_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  NaTrayManager *manager = NA_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      na_tray_manager_set_orientation (manager, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
na_tray_manager_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  NaTrayManager *manager = NA_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, manager->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

NaTrayManager *
na_tray_manager_new (void)
{
  NaTrayManager *manager;

  manager = g_object_new (NA_TYPE_TRAY_MANAGER, NULL);

  return manager;
}

#ifdef GDK_WINDOWING_X11

static gboolean
na_tray_manager_plug_removed (GtkSocket       *socket,
			      NaTrayManager   *manager)
{
  NaTrayChild *child = NA_TRAY_CHILD (socket);

  g_hash_table_remove (manager->socket_table,
                       GINT_TO_POINTER (child->icon_window));
  g_signal_emit (manager, manager_signals[TRAY_ICON_REMOVED], 0, child);

  /* This destroys the socket. */
  return FALSE;
}

static void
na_tray_manager_handle_dock_request (NaTrayManager       *manager,
				     XClientMessageEvent *xevent)
{
  Window icon_window = xevent->data.l[2];
  GtkWidget *child;

  if (g_hash_table_lookup (manager->socket_table,
                           GINT_TO_POINTER (icon_window)))
    {
      /* We already got this notification earlier, ignore this one */
      return;
    }

  child = na_tray_child_new (manager->screen, icon_window);
  if (child == NULL) /* already gone or other error */
    return;

  g_signal_emit (manager, manager_signals[TRAY_ICON_ADDED], 0,
		 child);

  /* If the child wasn't attached, then destroy it */

  if (!GTK_IS_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (child))))
    {
      gtk_widget_destroy (child);
      return;
    }

  g_signal_connect (child, "plug_removed",
		    G_CALLBACK (na_tray_manager_plug_removed), manager);

  gtk_socket_add_id (GTK_SOCKET (child), icon_window);

  if (!GTK_SOCKET (child)->plug_window)
    {
      /* Embedding failed, we won't get a plug-removed signal */
      g_signal_emit (manager, manager_signals[TRAY_ICON_REMOVED], 0, child);
      gtk_widget_destroy (child);
      return;
    }

  g_hash_table_insert (manager->socket_table,
                       GINT_TO_POINTER (icon_window), child);
  gtk_widget_show (child);
}

static void
pending_message_free (PendingMessage *message)
{
  g_free (message->str);
  g_free (message);
}

static GdkFilterReturn
na_tray_manager_handle_client_message_message_data (GdkXEvent *xev,
                                                    GdkEvent  *event,
                                                    gpointer   data)
{
  XClientMessageEvent *xevent;
  NaTrayManager       *manager;
  GList               *p;
  int                  len;
  
  xevent  = (XClientMessageEvent *) xev;
  manager = data;

  /* Try to see if we can find the pending message in the list */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *msg = p->data;

      if (xevent->window == msg->window)
	{
	  /* Append the message */
	  len = MIN (msg->remaining_len, 20);

	  memcpy ((msg->str + msg->len - msg->remaining_len),
		  &xevent->data, len);
	  msg->remaining_len -= len;

	  if (msg->remaining_len == 0)
	    {
	      GtkSocket *socket;

	      socket = g_hash_table_lookup (manager->socket_table,
                                            GINT_TO_POINTER (msg->window));

	      if (socket)
		  g_signal_emit (manager, manager_signals[MESSAGE_SENT], 0,
				 socket, msg->str, msg->id, msg->timeout);

	      pending_message_free (msg);
	      manager->messages = g_list_remove_link (manager->messages, p);
              g_list_free_1 (p);
	    }

          break;
	}
    }

  return GDK_FILTER_REMOVE;
}

static void
na_tray_manager_handle_begin_message (NaTrayManager       *manager,
				      XClientMessageEvent *xevent)
{
  GtkSocket      *socket;
  GList          *p;
  PendingMessage *msg;
  long            timeout;
  long            len;
  long            id;

  socket = g_hash_table_lookup (manager->socket_table,
                                GINT_TO_POINTER (xevent->window));
  /* we don't know about this tray icon, so ignore the message */
  if (!socket)
    return;

  /* Check if the same message is already in the queue and remove it if so */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *pmsg = p->data;

      if (xevent->window == pmsg->window &&
	  xevent->data.l[4] == pmsg->id)
	{
	  /* Hmm, we found it, now remove it */
	  pending_message_free (pmsg);
	  manager->messages = g_list_remove_link (manager->messages, p);
          g_list_free_1 (p);
	  break;
	}
    }

  timeout = xevent->data.l[2];
  len     = xevent->data.l[3];
  id      = xevent->data.l[4];

  if (len == 0)
    {
      g_signal_emit (manager, manager_signals[MESSAGE_SENT], 0,
                     socket, "", id, timeout);
    }
  else
    {
      /* Now add the new message to the queue */
      msg = g_new0 (PendingMessage, 1);
      msg->window = xevent->window;
      msg->timeout = timeout;
      msg->len = len;
      msg->id = id;
      msg->remaining_len = msg->len;
      msg->str = g_malloc (msg->len + 1);
      msg->str[msg->len] = '\0';
      manager->messages = g_list_prepend (manager->messages, msg);
    }
}

static void
na_tray_manager_handle_cancel_message (NaTrayManager       *manager,
				       XClientMessageEvent *xevent)
{
  GList     *p;
  GtkSocket *socket;
  
  /* Check if the message is in the queue and remove it if so */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *msg = p->data;

      if (xevent->window == msg->window &&
	  xevent->data.l[4] == msg->id)
	{
	  pending_message_free (msg);
	  manager->messages = g_list_remove_link (manager->messages, p);
          g_list_free_1 (p);
	  break;
	}
    }

  socket = g_hash_table_lookup (manager->socket_table,
                                GINT_TO_POINTER (xevent->window));
  
  if (socket)
    {
      g_signal_emit (manager, manager_signals[MESSAGE_CANCELLED], 0,
		     socket, xevent->data.l[2]);
    }
}

static GdkFilterReturn
na_tray_manager_handle_client_message_opcode (GdkXEvent *xev,
                                              GdkEvent  *event,
                                              gpointer   data)
{
  XClientMessageEvent *xevent;
  NaTrayManager       *manager;

  xevent  = (XClientMessageEvent *) xev;
  manager = data;

  switch (xevent->data.l[1])
    {
    case SYSTEM_TRAY_REQUEST_DOCK:
      /* Ignore this one since we don't know on which window this was received
       * and so we can't know for which screen this is. It will be handled
       * in na_tray_manager_window_filter() since we also receive it there */
      break;

    case SYSTEM_TRAY_BEGIN_MESSAGE:
      na_tray_manager_handle_begin_message (manager, xevent);
      return GDK_FILTER_REMOVE;

    case SYSTEM_TRAY_CANCEL_MESSAGE:
      na_tray_manager_handle_cancel_message (manager, xevent);
      return GDK_FILTER_REMOVE;
    default:
      break;
    }

  return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
na_tray_manager_window_filter (GdkXEvent *xev,
                               GdkEvent  *event,
                               gpointer   data)
{
  XEvent        *xevent = (GdkXEvent *)xev;
  NaTrayManager *manager = data;

  if (xevent->type == ClientMessage)
    {
      /* We handle this client message here. See comment in
       * na_tray_manager_handle_client_message_opcode() for details */
      if (xevent->xclient.message_type == manager->opcode_atom &&
          xevent->xclient.data.l[1]    == SYSTEM_TRAY_REQUEST_DOCK)
	{
          na_tray_manager_handle_dock_request (manager,
                                               (XClientMessageEvent *) xevent);
          return GDK_FILTER_REMOVE;
	}
    }
  else if (xevent->type == SelectionClear)
    {
      g_signal_emit (manager, manager_signals[LOST_SELECTION], 0);
      na_tray_manager_unmanage (manager);
    }

  return GDK_FILTER_CONTINUE;
}

#if 0
//FIXME investigate why this doesn't work
static gboolean
na_tray_manager_selection_clear_event (GtkWidget         *widget,
                                       GdkEventSelection *event,
                                       NaTrayManager     *manager)
{
  g_signal_emit (manager, manager_signals[LOST_SELECTION], 0);
  na_tray_manager_unmanage (manager);

  return FALSE;
}
#endif
#endif  

static void
na_tray_manager_unmanage (NaTrayManager *manager)
{
#ifdef GDK_WINDOWING_X11
  GdkDisplay *display;
  guint32     timestamp;
  GtkWidget  *invisible;

  if (manager->invisible == NULL)
    return;

  invisible = manager->invisible;
  g_assert (GTK_IS_INVISIBLE (invisible));
  g_assert (GTK_WIDGET_REALIZED (invisible));
  g_assert (GDK_IS_WINDOW (invisible->window));
  
  display = gtk_widget_get_display (invisible);
  
  if (gdk_selection_owner_get_for_display (display, manager->selection_atom) ==
      invisible->window)
    {
      timestamp = gdk_x11_get_server_time (invisible->window);      
      gdk_selection_owner_set_for_display (display,
                                           NULL,
                                           manager->selection_atom,
                                           timestamp,
                                           TRUE);
    }

  //FIXME: we should also use gdk_remove_client_message_filter when it's
  //available
  // See bug #351254
  gdk_window_remove_filter (invisible->window,
                            na_tray_manager_window_filter, manager);  

  manager->invisible = NULL; /* prior to destroy for reentrancy paranoia */
  gtk_widget_destroy (invisible);
  g_object_unref (G_OBJECT (invisible));
#endif
}

static void
na_tray_manager_set_orientation_property (NaTrayManager *manager)
{
#ifdef GDK_WINDOWING_X11
  GdkDisplay *display;
  Atom        orientation_atom;
  gulong      data[1];

  if (!manager->invisible || !manager->invisible->window)
    return;

  display = gtk_widget_get_display (manager->invisible);
  orientation_atom = gdk_x11_get_xatom_by_name_for_display (display,
                                                            "_NET_SYSTEM_TRAY_ORIENTATION");

  data[0] = manager->orientation == GTK_ORIENTATION_HORIZONTAL ?
		SYSTEM_TRAY_ORIENTATION_HORZ :
		SYSTEM_TRAY_ORIENTATION_VERT;

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		   GDK_WINDOW_XWINDOW (manager->invisible->window),
                   orientation_atom,
		   XA_CARDINAL, 32,
		   PropModeReplace,
		   (guchar *) &data, 1);
#endif
}

static void
na_tray_manager_set_visual_property (NaTrayManager *manager)
{
#ifdef GDK_WINDOWING_X11
  GdkDisplay *display;
  Visual     *xvisual;
  Atom        visual_atom;
  gulong      data[1];

  if (!manager->invisible || !manager->invisible->window)
    return;

  /* The visual property is a hint to the tray icons as to what visual they
   * should use for their windows. If the X server has RGBA colormaps, then
   * we tell the tray icons to use a RGBA colormap and we'll composite the
   * icon onto its parents with real transparency. Otherwise, we just tell
   * the icon to use our colormap, and we'll do some hacks with parent
   * relative backgrounds to simulate transparency.
   */

  display = gtk_widget_get_display (manager->invisible);
  visual_atom = gdk_x11_get_xatom_by_name_for_display (display,
						       "_NET_SYSTEM_TRAY_VISUAL");

  if (gdk_screen_get_rgba_visual (manager->screen) != NULL &&
      gdk_display_supports_composite (display))
    {
      xvisual = GDK_VISUAL_XVISUAL (gdk_screen_get_rgba_visual (manager->screen));
    }
  else
    {
      /* We actually want the visual of the tray where the icons will
       * be embedded. In almost all cases, this will be the same as the visual
       * of the screen.
       */
      GdkColormap *colormap;

      colormap = gdk_screen_get_default_colormap (manager->screen);
      xvisual = GDK_VISUAL_XVISUAL (gdk_colormap_get_visual (colormap));
    }

  data[0] = XVisualIDFromVisual (xvisual);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XWINDOW (manager->invisible->window),
                   visual_atom,
                   XA_VISUALID, 32,
                   PropModeReplace,
                   (guchar *) &data, 1);
#endif
}

static void
na_tray_manager_set_padding_property (NaTrayManager *manager)
{
#ifdef GDK_WINDOWING_X11
  GdkDisplay *display;
  Atom        orientation_atom;
  gulong      data[1];

  if (!manager->invisible || !manager->invisible->window)
    return;

  display = gtk_widget_get_display (manager->invisible);
  orientation_atom = gdk_x11_get_xatom_by_name_for_display (display,
                                                            "_NET_SYSTEM_TRAY_PADDING");

  data[0] = manager->padding;

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		   GDK_WINDOW_XWINDOW (manager->invisible->window),
                   orientation_atom,
		   XA_CARDINAL, 32,
		   PropModeReplace,
		   (guchar *) &data, 1);
#endif
}

#ifdef GDK_WINDOWING_X11

static gboolean
na_tray_manager_manage_screen_x11 (NaTrayManager *manager,
				   GdkScreen     *screen)
{
  GdkDisplay *display;
  Screen     *xscreen;
  GtkWidget  *invisible;
  char       *selection_atom_name;
  guint32     timestamp;
  
  g_return_val_if_fail (NA_IS_TRAY_MANAGER (manager), FALSE);
  g_return_val_if_fail (manager->screen == NULL, FALSE);

  /* If there's already a manager running on the screen
   * we can't create another one.
   */
#if 0
  if (na_tray_manager_check_running_screen_x11 (screen))
    return FALSE;
#endif
  
  manager->screen = screen;

  display = gdk_screen_get_display (screen);
  xscreen = GDK_SCREEN_XSCREEN (screen);
  
  invisible = gtk_invisible_new_for_screen (screen);
  gtk_widget_realize (invisible);
  
  gtk_widget_add_events (invisible,
                         GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK);

  selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
					 gdk_screen_get_number (screen));
  manager->selection_atom = gdk_atom_intern (selection_atom_name, FALSE);
  g_free (selection_atom_name);

  manager->invisible = invisible;
  g_object_ref (G_OBJECT (manager->invisible));

  na_tray_manager_set_orientation_property (manager);
  na_tray_manager_set_visual_property (manager);
  na_tray_manager_set_padding_property (manager);
  
  timestamp = gdk_x11_get_server_time (invisible->window);

  /* Check if we could set the selection owner successfully */
  if (gdk_selection_owner_set_for_display (display,
                                           invisible->window,
                                           manager->selection_atom,
                                           timestamp,
                                           TRUE))
    {
      XClientMessageEvent xev;
      GdkAtom             opcode_atom;
      GdkAtom             message_data_atom;

      xev.type = ClientMessage;
      xev.window = RootWindowOfScreen (xscreen);
      xev.message_type = gdk_x11_get_xatom_by_name_for_display (display,
                                                                "MANAGER");

      xev.format = 32;
      xev.data.l[0] = timestamp;
      xev.data.l[1] = gdk_x11_atom_to_xatom_for_display (display,
                                                         manager->selection_atom);
      xev.data.l[2] = GDK_WINDOW_XWINDOW (invisible->window);
      xev.data.l[3] = 0;	/* manager specific data */
      xev.data.l[4] = 0;	/* manager specific data */

      XSendEvent (GDK_DISPLAY_XDISPLAY (display),
		  RootWindowOfScreen (xscreen),
		  False, StructureNotifyMask, (XEvent *)&xev);

      opcode_atom = gdk_atom_intern ("_NET_SYSTEM_TRAY_OPCODE", FALSE);
      manager->opcode_atom = gdk_x11_atom_to_xatom_for_display (display,
                                                                opcode_atom);

      message_data_atom = gdk_atom_intern ("_NET_SYSTEM_TRAY_MESSAGE_DATA",
                                           FALSE);

      /* Add a window filter */
#if 0
      /* This is for when we lose the selection of _NET_SYSTEM_TRAY_Sx */
      g_signal_connect (invisible, "selection-clear-event",
                        G_CALLBACK (na_tray_manager_selection_clear_event),
                        manager);
#endif
      /* This is for SYSTEM_TRAY_REQUEST_DOCK and SelectionClear */
      gdk_window_add_filter (invisible->window,
                             na_tray_manager_window_filter, manager);
      /* This is for SYSTEM_TRAY_BEGIN_MESSAGE and SYSTEM_TRAY_CANCEL_MESSAGE */
      gdk_display_add_client_message_filter (display, opcode_atom,
                                             na_tray_manager_handle_client_message_opcode,
                                             manager);
      /* This is for _NET_SYSTEM_TRAY_MESSAGE_DATA */
      gdk_display_add_client_message_filter (display, message_data_atom,
                                             na_tray_manager_handle_client_message_message_data,
                                             manager);
      return TRUE;
    }
  else
    {
      gtk_widget_destroy (invisible);
      g_object_unref (invisible);
      manager->invisible = NULL;

      manager->screen = NULL;
 
      return FALSE;
    }
}

#endif

gboolean
na_tray_manager_manage_screen (NaTrayManager *manager,
			       GdkScreen     *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (manager->screen == NULL, FALSE);

#ifdef GDK_WINDOWING_X11
  return na_tray_manager_manage_screen_x11 (manager, screen);
#else
  return FALSE;
#endif
}

#ifdef GDK_WINDOWING_X11

static gboolean
na_tray_manager_check_running_screen_x11 (GdkScreen *screen)
{
  GdkDisplay *display;
  Atom        selection_atom;
  char       *selection_atom_name;

  display = gdk_screen_get_display (screen);
  selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
                                         gdk_screen_get_number (screen));
  selection_atom = gdk_x11_get_xatom_by_name_for_display (display,
                                                          selection_atom_name);
  g_free (selection_atom_name);

  if (XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
                          selection_atom) != None)
    return TRUE;
  else
    return FALSE;
}

#endif

gboolean
na_tray_manager_check_running (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

#ifdef GDK_WINDOWING_X11
  return na_tray_manager_check_running_screen_x11 (screen);
#else
  return FALSE;
#endif
}

void
na_tray_manager_set_orientation (NaTrayManager  *manager,
				 GtkOrientation  orientation)
{
  g_return_if_fail (NA_IS_TRAY_MANAGER (manager));

  if (manager->orientation != orientation)
    {
      manager->orientation = orientation;

      na_tray_manager_set_orientation_property (manager);

      g_object_notify (G_OBJECT (manager), "orientation");
    }
}

void
na_tray_manager_set_padding (NaTrayManager *manager,
                             gint           padding)
{
  g_return_if_fail (NA_IS_TRAY_MANAGER (manager));

  if (manager->padding != padding)
    {
      manager->padding = padding;

      na_tray_manager_set_padding_property (manager);
    }
}

GtkOrientation
na_tray_manager_get_orientation (NaTrayManager *manager)
{
  g_return_val_if_fail (NA_IS_TRAY_MANAGER (manager), GTK_ORIENTATION_HORIZONTAL);

  return manager->orientation;
}
