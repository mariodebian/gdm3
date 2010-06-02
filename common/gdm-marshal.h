
#ifndef __gdm_marshal_MARSHAL_H__
#define __gdm_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:STRING,STRING,STRING,STRING,STRING (gdm-marshal.list:1) */
extern void gdm_marshal_VOID__STRING_STRING_STRING_STRING_STRING (GClosure     *closure,
                                                                  GValue       *return_value,
                                                                  guint         n_param_values,
                                                                  const GValue *param_values,
                                                                  gpointer      invocation_hint,
                                                                  gpointer      marshal_data);

/* VOID:STRING,STRING,STRING,STRING (gdm-marshal.list:2) */
extern void gdm_marshal_VOID__STRING_STRING_STRING_STRING (GClosure     *closure,
                                                           GValue       *return_value,
                                                           guint         n_param_values,
                                                           const GValue *param_values,
                                                           gpointer      invocation_hint,
                                                           gpointer      marshal_data);

/* VOID:STRING,STRING,STRING (gdm-marshal.list:3) */
extern void gdm_marshal_VOID__STRING_STRING_STRING (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

/* VOID:STRING,STRING (gdm-marshal.list:4) */
extern void gdm_marshal_VOID__STRING_STRING (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:UINT,UINT (gdm-marshal.list:5) */
extern void gdm_marshal_VOID__UINT_UINT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* VOID:STRING,INT (gdm-marshal.list:6) */
extern void gdm_marshal_VOID__STRING_INT (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* VOID:DOUBLE (gdm-marshal.list:7) */
#define gdm_marshal_VOID__DOUBLE	g_cclosure_marshal_VOID__DOUBLE

G_END_DECLS

#endif /* __gdm_marshal_MARSHAL_H__ */

