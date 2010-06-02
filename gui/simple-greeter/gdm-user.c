/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
 * Copyright (C) 2007-2008 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>

#include <float.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "gdm-user-manager.h"
#include "gdm-user-private.h"

#define GDM_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GDM_TYPE_USER, GdmUserClass))
#define GDM_IS_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDM_TYPE_USER))
#define GDM_USER_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), GDM_TYPE_USER, GdmUserClass))

#define GLOBAL_FACEDIR    DATADIR "/faces"
#define MAX_ICON_SIZE     128
#define MAX_FILE_SIZE     65536
#define MINIMAL_UID       100

enum {
        PROP_0,
        PROP_MANAGER,
        PROP_REAL_NAME,
        PROP_DISPLAY_NAME,
        PROP_USER_NAME,
        PROP_UID,
        PROP_HOME_DIR,
        PROP_SHELL,
        PROP_LOGIN_FREQUENCY,
};

enum {
        SESSIONS_CHANGED,
        LAST_SIGNAL
};

struct _GdmUser {
        GObject         parent;

        GdmUserManager *manager;

        uid_t           uid;
        char           *user_name;
        char           *real_name;
        char           *display_name;
        char           *home_dir;
        char           *shell;
        GList          *sessions;
        gulong          login_frequency;
};

typedef struct _GdmUserClass
{
        GObjectClass parent_class;

        void (* sessions_changed) (GdmUser *user);
} GdmUserClass;

static void gdm_user_finalize     (GObject      *object);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdmUser, gdm_user, G_TYPE_OBJECT)

static int
session_compare (const char *a,
                 const char *b)
{
        if (a == NULL) {
                return 1;
        } else if (b == NULL) {
                return -1;
        }

        return strcmp (a, b);
}

void
_gdm_user_add_session (GdmUser    *user,
                       const char *ssid)
{
        GList *li;

        g_return_if_fail (GDM_IS_USER (user));
        g_return_if_fail (ssid != NULL);

        li = g_list_find_custom (user->sessions, ssid, (GCompareFunc)session_compare);
        if (li == NULL) {
                g_debug ("GdmUser: adding session %s", ssid);
                user->sessions = g_list_prepend (user->sessions, g_strdup (ssid));
                g_signal_emit (user, signals[SESSIONS_CHANGED], 0);
        } else {
                g_debug ("GdmUser: session already present: %s", ssid);
        }
}

void
_gdm_user_remove_session (GdmUser    *user,
                          const char *ssid)
{
        GList *li;

        g_return_if_fail (GDM_IS_USER (user));
        g_return_if_fail (ssid != NULL);

        li = g_list_find_custom (user->sessions, ssid, (GCompareFunc)session_compare);
        if (li != NULL) {
                g_debug ("GdmUser: removing session %s", ssid);
                g_free (li->data);
                user->sessions = g_list_delete_link (user->sessions, li);
                g_signal_emit (user, signals[SESSIONS_CHANGED], 0);
        } else {
                g_debug ("GdmUser: session not found: %s", ssid);
        }
}

guint
gdm_user_get_num_sessions (GdmUser    *user)
{
        return g_list_length (user->sessions);
}

GList *
gdm_user_get_sessions (GdmUser *user)
{
        return user->sessions;
}

static void
_gdm_user_set_login_frequency (GdmUser *user,
                               gulong   login_frequency)
{
        user->login_frequency = login_frequency;
        g_object_notify (G_OBJECT (user), "login-frequency");
}

static void
gdm_user_set_property (GObject      *object,
                       guint         param_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
        GdmUser *user;

        user = GDM_USER (object);

        switch (param_id) {
        case PROP_MANAGER:
                user->manager = g_value_get_object (value);
                g_assert (user->manager);
                break;
        case PROP_LOGIN_FREQUENCY:
                _gdm_user_set_login_frequency (user, g_value_get_ulong (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}

static void
gdm_user_get_property (GObject    *object,
                       guint       param_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
        GdmUser *user;

        user = GDM_USER (object);

        switch (param_id) {
        case PROP_MANAGER:
                g_value_set_object (value, user->manager);
                break;
        case PROP_USER_NAME:
                g_value_set_string (value, user->user_name);
                break;
        case PROP_REAL_NAME:
                g_value_set_string (value, user->real_name);
                break;
        case PROP_DISPLAY_NAME:
                g_value_set_string (value, user->display_name);
                break;
        case PROP_HOME_DIR:
                g_value_set_string (value, user->home_dir);
                break;
        case PROP_UID:
                g_value_set_ulong (value, user->uid);
                break;
        case PROP_SHELL:
                g_value_set_string (value, user->shell);
                break;
        case PROP_LOGIN_FREQUENCY:
                g_value_set_ulong (value, user->login_frequency);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}

static void
gdm_user_class_init (GdmUserClass *class)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (class);

        gobject_class->set_property = gdm_user_set_property;
        gobject_class->get_property = gdm_user_get_property;
        gobject_class->finalize = gdm_user_finalize;

        g_object_class_install_property (gobject_class,
                                         PROP_MANAGER,
                                         g_param_spec_object ("manager",
                                                              _("Manager"),
                                                              _("The user manager object this user is controlled by."),
                                                              GDM_TYPE_USER_MANAGER,
                                                              (G_PARAM_READWRITE |
                                                               G_PARAM_CONSTRUCT_ONLY)));

        g_object_class_install_property (gobject_class,
                                         PROP_REAL_NAME,
                                         g_param_spec_string ("real-name",
                                                              "Real Name",
                                                              "The real name to display for this user.",
                                                              NULL,
                                                              G_PARAM_READABLE));

        g_object_class_install_property (gobject_class,
                                         PROP_DISPLAY_NAME,
                                         g_param_spec_string ("display-name",
                                                              "Display Name",
                                                              "The unique name to display for this user.",
                                                              NULL,
                                                              G_PARAM_READABLE));

        g_object_class_install_property (gobject_class,
                                         PROP_UID,
                                         g_param_spec_ulong ("uid",
                                                             "User ID",
                                                             "The UID for this user.",
                                                             0, G_MAXULONG, 0,
                                                             G_PARAM_READABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_USER_NAME,
                                         g_param_spec_string ("user-name",
                                                              "User Name",
                                                              "The login name for this user.",
                                                              NULL,
                                                              G_PARAM_READABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_HOME_DIR,
                                         g_param_spec_string ("home-directory",
                                                              "Home Directory",
                                                              "The home directory for this user.",
                                                              NULL,
                                                              G_PARAM_READABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_SHELL,
                                         g_param_spec_string ("shell",
                                                              "Shell",
                                                              "The shell for this user.",
                                                              NULL,
                                                              G_PARAM_READABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_LOGIN_FREQUENCY,
                                         g_param_spec_ulong ("login-frequency",
                                                             "login frequency",
                                                             "login frequency",
                                                             0,
                                                             G_MAXULONG,
                                                             0,
                                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

        signals [SESSIONS_CHANGED] =
                g_signal_new ("sessions-changed",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdmUserClass, sessions_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

static void
gdm_user_init (GdmUser *user)
{
        user->manager = NULL;
        user->user_name = NULL;
        user->real_name = NULL;
        user->display_name = NULL;
        user->sessions = NULL;
}

static void
gdm_user_finalize (GObject *object)
{
        GdmUser *user;

        user = GDM_USER (object);

        g_free (user->user_name);
        g_free (user->real_name);
        g_free (user->display_name);

        if (G_OBJECT_CLASS (gdm_user_parent_class)->finalize)
                (*G_OBJECT_CLASS (gdm_user_parent_class)->finalize) (object);
}

/**
 * _gdm_user_update:
 * @user: the user object to update.
 * @pwent: the user data to use.
 *
 * Updates the properties of @user using the data in @pwent.
 *
 * Since: 1.0
 **/
void
_gdm_user_update (GdmUser             *user,
                  const struct passwd *pwent)
{
        gchar *real_name = NULL;

        g_return_if_fail (GDM_IS_USER (user));
        g_return_if_fail (pwent != NULL);

        g_object_freeze_notify (G_OBJECT (user));

        /* Display Name */
        if (pwent->pw_gecos && pwent->pw_gecos[0] != '\0') {
                gchar *first_comma = NULL;
                gchar *valid_utf8_name = NULL;

                if (g_utf8_validate (pwent->pw_gecos, -1, NULL)) {
                        valid_utf8_name = pwent->pw_gecos;
                        first_comma = g_utf8_strchr (valid_utf8_name, -1, ',');
                } else {
                        g_warning ("User %s has invalid UTF-8 in GECOS field. "
                                   "It would be a good thing to check /etc/passwd.",
                                   pwent->pw_name ? pwent->pw_name : "");
                }

                if (first_comma) {
                        real_name = g_strndup (valid_utf8_name,
                                                  (first_comma - valid_utf8_name));
                } else if (valid_utf8_name) {
                        real_name = g_strdup (valid_utf8_name);
                } else {
                        real_name = NULL;
                }

                if (real_name && real_name[0] == '\0') {
                        g_free (real_name);
                        real_name = NULL;
                }
        } else {
                real_name = NULL;
        }

        if ((real_name && !user->real_name) ||
            (!real_name && user->real_name) ||
            (real_name &&
             user->real_name &&
             strcmp (real_name, user->real_name) != 0)) {
                g_free (user->real_name);
                user->real_name = real_name;
                g_object_notify (G_OBJECT (user), "real-name");
        } else {
                g_free (real_name);
        }

        /* Unique Display Name */
        if ((!user->real_name && user->display_name) ||
            (user->real_name &&
             user->display_name &&
             strncmp (user->real_name, user->display_name, strlen (user->real_name)) != 0)) {
                g_free (user->display_name);
                user->display_name = NULL;
                g_object_notify (G_OBJECT (user), "display-name");
        }

        /* UID */
        if (pwent->pw_uid != user->uid) {
                user->uid = pwent->pw_uid;
                g_object_notify (G_OBJECT (user), "uid");
        }

        /* Username */
        if ((pwent->pw_name && !user->user_name) ||
            (!pwent->pw_name && user->user_name) ||
            (pwent->pw_name &&
             user->user_name &&
             strcmp (user->user_name, pwent->pw_name) != 0)) {
                g_free (user->user_name);
                user->user_name = g_strdup (pwent->pw_name);
                g_object_notify (G_OBJECT (user), "user-name");
        }

        /* Home Directory */
        if ((pwent->pw_dir && !user->home_dir) ||
            (!pwent->pw_dir && user->home_dir) ||
            strcmp (user->home_dir, pwent->pw_dir) != 0) {
                g_free (user->home_dir);
                user->home_dir = g_strdup (pwent->pw_dir);
                g_object_notify (G_OBJECT (user), "home-directory");
        }

        /* Shell */
        if ((pwent->pw_shell && !user->shell) ||
            (!pwent->pw_shell && user->shell) ||
            (pwent->pw_shell &&
             user->shell &&
             strcmp (user->shell, pwent->pw_shell) != 0)) {
                g_free (user->shell);
                user->shell = g_strdup (pwent->pw_shell);
                g_object_notify (G_OBJECT (user), "shell");
        }

        g_object_thaw_notify (G_OBJECT (user));
}

/**
 * gdm_user_get_uid:
 * @user: the user object to examine.
 *
 * Retrieves the ID of @user.
 *
 * Returns: a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 *
 * Since: 1.0
 **/

uid_t
gdm_user_get_uid (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), -1);

        return user->uid;
}

/**
 * gdm_user_get_real_name:
 * @user: the user object to examine.
 *
 * Retrieves the display name of @user.
 *
 * Returns: a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 *
 * Since: 1.0
 **/
G_CONST_RETURN gchar *
gdm_user_get_real_name (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        return (user->real_name ? user->real_name : user->user_name);
}

/**
 * gdm_user_get_display_name:
 * @user: the user object to examine.
 *
 * Retrieves the unique display name of @user.
 *
 * Returns: a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 *
 * Since: 1.0
 **/
G_CONST_RETURN gchar *
gdm_user_get_display_name (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        return (user->display_name ? user->display_name
                : gdm_user_get_real_name (user));
}

/**
 * gdm_user_get_user_name:
 * @user: the user object to examine.
 *
 * Retrieves the login name of @user.
 *
 * Returns: a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 *
 * Since: 1.0
 **/

G_CONST_RETURN gchar *
gdm_user_get_user_name (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        return user->user_name;
}

/**
 * gdm_user_get_home_directory:
 * @user: the user object to examine.
 *
 * Retrieves the home directory of @user.
 *
 * Returns: a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 *
 * Since: 1.0
 **/

G_CONST_RETURN gchar *
gdm_user_get_home_directory (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        return user->home_dir;
}

/**
 * gdm_user_get_shell:
 * @user: the user object to examine.
 *
 * Retrieves the login shell of @user.
 *
 * Returns: a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 *
 * Since: 1.0
 **/

G_CONST_RETURN gchar *
gdm_user_get_shell (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        return user->shell;
}

gulong
gdm_user_get_login_frequency (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), 0);

        return user->login_frequency;
}

/**
 * _gdm_user_show_full_display_name:
 * @user: the user object to examine.
 *
 * Updates the unique display name of @user to "Real Name (username)".
 *
 * Since: 1.0
 **/
void
_gdm_user_show_full_display_name (GdmUser *user)
{
        char *uniq_name;

        g_return_if_fail (GDM_IS_USER (user));

        if (user->real_name != NULL) {
                uniq_name = g_strdup_printf ("%s (%s)",
                                             user->real_name,
                                             user->user_name);
        } else {
                uniq_name = NULL;
        }

        if ((user->real_name && !user->display_name) ||
            (!user->real_name && user->display_name) ||
            (user->real_name &&
             user->display_name &&
             strcmp (uniq_name, user->display_name) != 0)) {
                g_free (user->display_name);
                user->display_name = uniq_name;
                g_object_notify (G_OBJECT (user), "display-name");
        } else {
                g_free (uniq_name);
        }
}

/**
 * _gdm_user_show_short_display_name:
 * @user: the user object to examine.
 *
 * Resets the unique display name of @user.
 *
 * Since: 1.0
 **/
void
_gdm_user_show_short_display_name (GdmUser *user)
{
        g_return_if_fail (GDM_IS_USER (user));

        if (user->display_name) {
                g_free (user->display_name);
                user->display_name = NULL;
                g_object_notify (G_OBJECT (user), "display-name");
        }
}

int
gdm_user_collate (GdmUser *user1,
                  GdmUser *user2)
{
        const char *str1;
        const char *str2;
        gulong      num1;
        gulong      num2;

        g_return_val_if_fail (GDM_IS_USER (user1), 0);
        g_return_val_if_fail (GDM_IS_USER (user2), 0);

        if (user1->real_name != NULL) {
                str1 = user1->real_name;
        } else {
                str1 = user1->user_name;
        }

        if (user2->real_name != NULL) {
                str2 = user2->real_name;
        } else {
                str2 = user2->user_name;
        }

        num1 = user1->login_frequency;
        num2 = user2->login_frequency;
        g_debug ("Login freq 1=%u 2=%u", (guint)num1, (guint)num2);
        if (num1 > num2) {
                return -1;
        }

        if (num1 < num2) {
                return 1;
        }

        /* if login frequency is equal try names */
        if (str1 == NULL && str2 != NULL) {
                return -1;
        }

        if (str1 != NULL && str2 == NULL) {
                return 1;
        }

        if (str1 == NULL && str2 == NULL) {
                return 0;
        }

        return g_utf8_collate (str1, str2);
}

static gboolean
check_user_file (const char *filename,
                 gssize      max_file_size)
{
        struct stat fileinfo;

        if (max_file_size < 0) {
                max_file_size = G_MAXSIZE;
        }

        /* Exists/Readable? */
        if (stat (filename, &fileinfo) < 0) {
                g_debug ("File does not exist");
                return FALSE;
        }

        /* Is a regular file */
        if (G_UNLIKELY (!S_ISREG (fileinfo.st_mode))) {
                g_debug ("File is not a regular file");
                return FALSE;
        }

        /* Size is kosher? */
        if (G_UNLIKELY (fileinfo.st_size > max_file_size)) {
                g_debug ("File is too large");
                return FALSE;
        }

        return TRUE;
}

static char *
get_filesystem_type (const char *path)
{
        GFile      *file;
        GFileInfo  *file_info;
        GError     *error;
        char       *filesystem_type;

        file = g_file_new_for_path (path);
        error = NULL;
        file_info = g_file_query_filesystem_info (file,
                                                  G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
                                                  NULL,
                                                  &error);
        if (file_info == NULL) {
                g_warning ("Unable to query filesystem type for %s: %s", path, error->message);
                g_error_free (error);
                g_object_unref (file);
                return NULL;
        }

        filesystem_type = g_strdup (g_file_info_get_attribute_string (file_info,
                                                                      G_FILE_ATTRIBUTE_FILESYSTEM_TYPE));
        if (filesystem_type == NULL) {
                g_warning ("GIO returned NULL filesystem type for %s", path);
        }

        g_object_unref (file);
        g_object_unref (file_info);

        return filesystem_type;
}

static GdkPixbuf *
render_icon_from_cache (GdmUser *user,
                        int      icon_size)
{
        GdkPixbuf  *retval;
        char       *path;
        gboolean    is_autofs;
        gboolean    res;
        char       *filesystem_type;

        path = g_build_filename (GDM_CACHE_DIR, user->user_name, "face", NULL);
        res = check_user_file (path,
                               MAX_FILE_SIZE);
        if (res) {
                retval = gdk_pixbuf_new_from_file_at_size (path,
                                                           icon_size,
                                                           icon_size,
                                                           NULL);
        } else {
                g_debug ("Could not access face icon %s", path);
                retval = NULL;
        }
        g_free (path);

        return retval;
}

static void
rounded_rectangle (cairo_t *cr,
                   gdouble  aspect,
                   gdouble  x,
                   gdouble  y,
                   gdouble  corner_radius,
                   gdouble  width,
                   gdouble  height)
{
        gdouble radius;
        gdouble degrees;

        radius = corner_radius / aspect;
        degrees = G_PI / 180.0;

        cairo_new_sub_path (cr);
        cairo_arc (cr,
                   x + width - radius,
                   y + radius,
                   radius,
                   -90 * degrees,
                   0 * degrees);
        cairo_arc (cr,
                   x + width - radius,
                   y + height - radius,
                   radius,
                   0 * degrees,
                   90 * degrees);
        cairo_arc (cr,
                   x + radius,
                   y + height - radius,
                   radius,
                   90 * degrees,
                   180 * degrees);
        cairo_arc (cr,
                   x + radius,
                   y + radius,
                   radius,
                   180 * degrees,
                   270 * degrees);
        cairo_close_path (cr);
}

static cairo_surface_t *
surface_from_pixbuf (GdkPixbuf *pixbuf)
{
        cairo_surface_t *surface;
        cairo_t         *cr;

        surface = cairo_image_surface_create (gdk_pixbuf_get_has_alpha (pixbuf) ?
                                              CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
                                              gdk_pixbuf_get_width (pixbuf),
                                              gdk_pixbuf_get_height (pixbuf));
        cr = cairo_create (surface);
        gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
        cairo_paint (cr);
        cairo_destroy (cr);

        return surface;
}

/**
 * go_cairo_convert_data_to_pixbuf:
 * @src: a pointer to pixel data in cairo format
 * @dst: a pointer to pixel data in pixbuf format
 * @width: image width
 * @height: image height
 * @rowstride: data rowstride
 *
 * Converts the pixel data stored in @src in CAIRO_FORMAT_ARGB32 cairo format
 * to GDK_COLORSPACE_RGB pixbuf format and move them
 * to @dst. If @src == @dst, pixel are converted in place.
 **/

static void
go_cairo_convert_data_to_pixbuf (unsigned char *dst,
                                 unsigned char const *src,
                                 int width,
                                 int height,
                                 int rowstride)
{
        int i,j;
        unsigned int t;
        unsigned char a, b, c;

        g_return_if_fail (dst != NULL);

#define MULT(d,c,a,t) G_STMT_START { t = (a)? c * 255 / a: 0; d = t;} G_STMT_END

        if (src == dst || src == NULL) {
                for (i = 0; i < height; i++) {
                        for (j = 0; j < width; j++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                                MULT(a, dst[2], dst[3], t);
                                MULT(b, dst[1], dst[3], t);
                                MULT(c, dst[0], dst[3], t);
                                dst[0] = a;
                                dst[1] = b;
                                dst[2] = c;
#else
                                MULT(a, dst[1], dst[0], t);
                                MULT(b, dst[2], dst[0], t);
                                MULT(c, dst[3], dst[0], t);
                                dst[3] = dst[0];
                                dst[0] = a;
                                dst[1] = b;
                                dst[2] = c;
#endif
                                dst += 4;
                        }
                        dst += rowstride - width * 4;
                }
        } else {
                for (i = 0; i < height; i++) {
                        for (j = 0; j < width; j++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                                MULT(dst[0], src[2], src[3], t);
                                MULT(dst[1], src[1], src[3], t);
                                MULT(dst[2], src[0], src[3], t);
                                dst[3] = src[3];
#else
                                MULT(dst[0], src[1], src[0], t);
                                MULT(dst[1], src[2], src[0], t);
                                MULT(dst[2], src[3], src[0], t);
                                dst[3] = src[0];
#endif
                                src += 4;
                                dst += 4;
                        }
                        src += rowstride - width * 4;
                        dst += rowstride - width * 4;
                }
        }
#undef MULT
}

static void
cairo_to_pixbuf (guint8    *src_data,
                 GdkPixbuf *dst_pixbuf)
{
        unsigned char *src;
        unsigned char *dst;
        guint          w;
        guint          h;
        guint          rowstride;

        w = gdk_pixbuf_get_width (dst_pixbuf);
        h = gdk_pixbuf_get_height (dst_pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (dst_pixbuf);

        dst = gdk_pixbuf_get_pixels (dst_pixbuf);
        src = src_data;

        go_cairo_convert_data_to_pixbuf (dst, src, w, h, rowstride);
}

static GdkPixbuf *
frame_pixbuf (GdkPixbuf *source)
{
        GdkPixbuf       *dest;
        cairo_t         *cr;
        cairo_surface_t *surface;
        guint            w;
        guint            h;
        guint            rowstride;
        int              frame_width;
        double           radius;
        guint8          *data;

        frame_width = 2;

        w = gdk_pixbuf_get_width (source) + frame_width * 2;
        h = gdk_pixbuf_get_height (source) + frame_width * 2;
        radius = w / 10;

        dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                               TRUE,
                               8,
                               w,
                               h);
        rowstride = gdk_pixbuf_get_rowstride (dest);


        data = g_new0 (guint8, h * rowstride);

        surface = cairo_image_surface_create_for_data (data,
                                                       CAIRO_FORMAT_ARGB32,
                                                       w,
                                                       h,
                                                       rowstride);
        cr = cairo_create (surface);
        cairo_surface_destroy (surface);

        /* set up image */
        cairo_rectangle (cr, 0, 0, w, h);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
        cairo_fill (cr);

        rounded_rectangle (cr,
                           1.0,
                           frame_width + 0.5,
                           frame_width + 0.5,
                           radius,
                           w - frame_width * 2 - 1,
                           h - frame_width * 2 - 1);
        cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.3);
        cairo_fill_preserve (cr);

        surface = surface_from_pixbuf (source);
        cairo_set_source_surface (cr, surface, frame_width, frame_width);
        cairo_fill (cr);
        cairo_surface_destroy (surface);

        cairo_to_pixbuf (data, dest);

        cairo_destroy (cr);
        g_free (data);

        return dest;
}

GdkPixbuf *
gdm_user_render_icon (GdmUser   *user,
                      gint       icon_size)
{
        GdkPixbuf    *pixbuf;
        GdkPixbuf    *framed;
        char         *path;
        char         *tmp;
        gboolean      res;

        g_return_val_if_fail (GDM_IS_USER (user), NULL);
        g_return_val_if_fail (icon_size > 12, NULL);

        path = NULL;

        pixbuf = render_icon_from_cache (user, icon_size);
        if (pixbuf != NULL) {
                goto out;
        }

        /* Try ${GlobalFaceDir}/${username} */
        path = g_build_filename (GLOBAL_FACEDIR, user->user_name, NULL);
        res = check_user_file (path,
                               MAX_FILE_SIZE);
        if (res) {
                pixbuf = gdk_pixbuf_new_from_file_at_size (path,
                                                           icon_size,
                                                           icon_size,
                                                           NULL);
        } else {
                g_debug ("Could not access global face icon %s", path);
                pixbuf = NULL;
        }

        g_free (path);
        if (pixbuf != NULL) {
                goto out;
        }

        /* Finally, ${GlobalFaceDir}/${username}.png */
        tmp = g_strconcat (user->user_name, ".png", NULL);
        path = g_build_filename (GLOBAL_FACEDIR, tmp, NULL);
        g_free (tmp);
        res = check_user_file (path,
                               MAX_FILE_SIZE);
        if (res) {
                pixbuf = gdk_pixbuf_new_from_file_at_size (path,
                                                           icon_size,
                                                           icon_size,
                                                           NULL);
        } else {
                g_debug ("Could not access global face icon %s", path);
                pixbuf = NULL;
        }
        g_free (path);
 out:

        if (pixbuf != NULL) {
                framed = frame_pixbuf (pixbuf);
                if (framed != NULL) {
                        g_object_unref (pixbuf);
                        pixbuf = framed;
                }
        }

        return pixbuf;
}
