/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copyright 2016 Endless Mobile, Inc. */

#include "eks-subtree-dispatcher.h"

/**
 * EksSubtreeDispatcherPrivate
 *
 * The #EksSubtreeDispatcher is for use with the knowledge content global
 * search provider in eos-knowledge-serivces.
 */

struct _EksSubtreeDispatcherPrivate
{
  GDBusConnection *connection;
  guint registration_id;

  GPtrArray *interface_infos;
};
typedef struct _EksSubtreeDispatcherPrivate EksSubtreeDispatcherPrivate;

enum {
  PROP_0,
  PROP_INTERFACE_INFOS,
  NUM_PROPS,
};
static GParamSpec *obj_props[NUM_PROPS];

enum {
  DISPATCH_SUBTREE,
  NUM_SIGNALS,
};
static guint signals[NUM_SIGNALS];

G_DEFINE_TYPE_WITH_PRIVATE (EksSubtreeDispatcher, eks_subtree_dispatcher, G_TYPE_OBJECT);

static void
eks_subtree_dispatcher_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  EksSubtreeDispatcher *self = EKS_SUBTREE_DISPATCHER (object);
  EksSubtreeDispatcherPrivate *priv = eks_subtree_dispatcher_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_INTERFACE_INFOS:
      g_clear_pointer (&priv->interface_infos, g_ptr_array_unref);
      g_value_set_boxed (value, priv->interface_infos);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eks_subtree_dispatcher_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EksSubtreeDispatcher *self = EKS_SUBTREE_DISPATCHER (object);
  EksSubtreeDispatcherPrivate *priv = eks_subtree_dispatcher_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_INTERFACE_INFOS:
      priv->interface_infos = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eks_subtree_dispatcher_dispose (GObject *object)
{
  EksSubtreeDispatcher *self = EKS_SUBTREE_DISPATCHER (object);
  EksSubtreeDispatcherPrivate *priv = eks_subtree_dispatcher_get_instance_private (self);

  G_OBJECT_CLASS (eks_subtree_dispatcher_parent_class)->dispose (object);

  g_clear_pointer (&priv->interface_infos, g_ptr_array_unref);

  if (priv->registration_id > 0)
    {
      eks_subtree_dispatcher_unregister (self);
      g_warning ("EksSubtreeDispatcher was disposed while it was registered");
    }
}

static void
eks_subtree_dispatcher_class_init (EksSubtreeDispatcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = eks_subtree_dispatcher_dispose;
  object_class->get_property = eks_subtree_dispatcher_get_property;
  object_class->set_property = eks_subtree_dispatcher_set_property;

  /**
   * EksSubtreeDispatcher:interface-infos:
   *
   * A #GDBusInterfaceInfo containing the interfaces of all the children
   * subobjects of this tree.
   */
  obj_props[PROP_INTERFACE_INFOS] = g_param_spec_boxed ("interface-infos",
                                                        "Interface Infos",
                                                        "The interface infos of the children subobjects",
                                                        G_TYPE_PTR_ARRAY,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, NUM_PROPS, obj_props);

  /**
   * EksSubtreeDispatcher::dispatch-subtree:
   * @dispatcher: the dispatcher
   * @object_path: The object path to dispatch for.
   *
   * The dispatch-subtree signal.
   *
   * Returns: (transfer none): A #GDBusInterfaceSkeleton that implements
   * the object you want.
   */
  signals[DISPATCH_SUBTREE] = g_signal_new ("dispatch-subtree",
                                            G_TYPE_FROM_CLASS (klass),
                                            G_SIGNAL_RUN_LAST,
                                            0,
                                            g_signal_accumulator_first_wins, NULL, NULL,
                                            G_TYPE_DBUS_INTERFACE_SKELETON,
                                            2, G_TYPE_STRING, G_TYPE_STRING);
}

static void
eks_subtree_dispatcher_init (EksSubtreeDispatcher *self)
{
}

/* It doesn't matter what we pass back for children,
 * since DISPATCH_TO_UNENUMERATED_NODES will save us */
static char **
subtree_enumerate (GDBusConnection *connection,
                   const char      *sender,
                   const char      *object_path,
                   gpointer         user_data)
{
  return NULL;
}

static GDBusInterfaceInfo **
subtree_introspect (GDBusConnection *connection,
                    const char      *sender,
                    const char      *object_path,
                    const char      *node,
                    gpointer         user_data)
{
  EksSubtreeDispatcher *self = EKS_SUBTREE_DISPATCHER (user_data);
  EksSubtreeDispatcherPrivate *priv = eks_subtree_dispatcher_get_instance_private (self);

  /* Root has no interfaces. */
  if (node == NULL)
    return NULL;

  GPtrArray *ptr_array = g_ptr_array_new ();
  size_t i = 0;

  for (; i < priv->interface_infos->len; ++i) {
    g_ptr_array_add (ptr_array,
                     g_dbus_interface_info_ref (g_ptr_array_index(priv->interface_infos,
                                                                  i)));
  }

  g_ptr_array_add (ptr_array, NULL);
  return (GDBusInterfaceInfo **) g_ptr_array_free (ptr_array, FALSE);
}

static const GDBusInterfaceVTable *
subtree_dispatch (GDBusConnection *connection,
                  const gchar     *sender,
                  const gchar     *object_path,
                  const gchar     *interface_name,
                  const gchar     *node,
                  gpointer        *out_user_data,
                  gpointer         user_data)
{
  EksSubtreeDispatcher *self = EKS_SUBTREE_DISPATCHER (user_data);
  GDBusInterfaceSkeleton *skeleton;

  g_signal_emit (self, signals[DISPATCH_SUBTREE], 0, node, interface_name, &skeleton);

  if (!G_IS_DBUS_INTERFACE_SKELETON (skeleton))
    {
      g_warning ("Did not get skeleton for node %s", node);
      return NULL;
    }

  *out_user_data = skeleton;
  /* XXX: How do we get the hooked vtable? */
  return g_dbus_interface_skeleton_get_vtable (skeleton);
}

const GDBusSubtreeVTable subtree_vtable = {
  .enumerate  = subtree_enumerate,
  .introspect = subtree_introspect,
  .dispatch   = subtree_dispatch,
};

/**
 * eks_subtree_dispatcher_register:
 * @self: the subtree dispatcher
 * @connection: the dbus connection
 * @subtree_path: a subtree path to register with
 * @error: a GError for error reporting
 *
 * Register the dispatcher.
 */
gboolean
eks_subtree_dispatcher_register (EksSubtreeDispatcher  *self,
                                 GDBusConnection *connection,
                                 const char *subtree_path,
                                 GError **error)
{
  EksSubtreeDispatcherPrivate *priv = eks_subtree_dispatcher_get_instance_private (self);

  if (priv->registration_id == 0)
    {
      priv->registration_id = g_dbus_connection_register_subtree (connection, subtree_path, &subtree_vtable,
                                                                  G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES,
                                                                  self, NULL, error);
      if (!priv->registration_id)
          return FALSE;

      priv->connection = g_object_ref (connection);
    }

    return TRUE;
}

/**
 * eks_subtree_dispatcher_unregister:
 * @self: the subtree dispatcher
 *
 * Unregister the dispatcher.
 */
void
eks_subtree_dispatcher_unregister (EksSubtreeDispatcher *self)
{
  EksSubtreeDispatcherPrivate *priv = eks_subtree_dispatcher_get_instance_private (self);

  if (priv->registration_id > 0)
    {
      g_dbus_connection_unregister_subtree (priv->connection, priv->registration_id);
      priv->registration_id = 0;

      g_clear_object (&priv->connection);
    }
}
