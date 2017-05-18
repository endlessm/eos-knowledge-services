/* Copyright 2017 Endless Mobile, Inc. */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EKS_TYPE_PROVIDER eks_provider_get_type ()
G_DECLARE_INTERFACE (EksProvider, eks_provider, EKS, PROVIDER, GObject)

struct _EksProviderInterface
{
  GTypeInterface parent_iface;

  GDBusInterfaceSkeleton * (*skeleton_for_interface) (EksProvider *self,
                                                      const gchar *interface);
};

GDBusInterfaceSkeleton * eks_provider_skeleton_for_interface (EksProvider *self,
                                                              const gchar *interface);

G_END_DECLS
