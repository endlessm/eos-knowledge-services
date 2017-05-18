/* Copyright 2017 Endless Mobile, Inc. */

#include "eks-provider-iface.h"

G_DEFINE_INTERFACE (EksProvider, eks_provider, G_TYPE_OBJECT);

static void
eks_provider_default_init (EksProviderInterface *iface)
{
}

GDBusInterfaceSkeleton *
eks_provider_skeleton_for_interface (EksProvider *self,
                                     const gchar *interface)
{
  g_return_val_if_fail (EKS_IS_PROVIDER (self), NULL);
  g_return_val_if_fail (interface != NULL, NULL);

  EksProviderInterface *iface = EKS_PROVIDER_GET_IFACE (self);
  return (*iface->skeleton_for_interface) (self, interface);
}

