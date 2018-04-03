/* Copyright 2018 Endless Mobile, Inc. */

#include <dmodel.h>
#include <gio/gio.h>

#include "eks-errors.h"

GError *
eks_map_error_to_eks_error (const GError *error)
{
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
      g_error_matches (error, DM_DOMAIN_ERROR, DM_DOMAIN_ERROR_PATH_NOT_FOUND))
    {
      return g_error_new (EKS_ERROR,
                          EKS_ERROR_APP_NOT_FOUND,
                          error->message);
    }
  else if (g_error_matches (error, DM_DOMAIN_ERROR,
                            DM_DOMAIN_ERROR_UNSUPPORTED_VERSION))
    {
      return g_error_new (EKS_ERROR,
                          EKS_ERROR_UNSUPPORTED_VERSION,
                          error->message);
    }
  else if (g_error_matches (error, DM_DOMAIN_ERROR,
                            DM_DOMAIN_ERROR_ID_NOT_FOUND))
    {
      return g_error_new (EKS_ERROR,
                          EKS_ERROR_ID_NOT_FOUND,
                          error->message);
    }
  else if (g_error_matches (error, DM_DOMAIN_ERROR,
                            DM_DOMAIN_ERROR_ID_NOT_VALID))
    {
      return g_error_new (EKS_ERROR,
                          EKS_ERROR_INVALID_REQUEST,
                          error->message);
    }
  else if (g_error_matches (error, DM_DOMAIN_ERROR,
                            DM_DOMAIN_ERROR_BAD_MANIFEST) ||
           g_error_matches (error, DM_DOMAIN_ERROR,
                            DM_DOMAIN_ERROR_BAD_RESULTS) ||
           g_error_matches (error, DM_DOMAIN_ERROR,
                            DM_DOMAIN_ERROR_EMPTY))
    {
      return g_error_new (EKS_ERROR,
                          EKS_ERROR_MALFORMED_APP,
                          error->message);
    }

  return g_error_copy (error);
}

static const GDBusErrorEntry eks_error_entries[] =
{
  { EKS_ERROR_APP_NOT_FOUND, "com.endlessm.EknServices.SearchProvider.AppNotFound" },
  { EKS_ERROR_UNSUPPORTED_VERSION, "com.endlessm.EknServices.SearchProvider.UnsupportedVersion" },
  { EKS_ERROR_ID_NOT_FOUND, "com.endlessm.EknServices.SearchProvider.IdNotFound" },
  { EKS_ERROR_MALFORMED_APP, "com.endlessm.EknServices.SearchProvider.MalformedApp" },
  { EKS_ERROR_INVALID_REQUEST, "com.endlessm.EknServices.SearchProvider.InvalidRequest" }
};

GQuark
eks_error_quark (void)
{
  static volatile gsize quark_volatile = 0;
  g_dbus_error_register_error_domain ("eks-error-quark",
                                      &quark_volatile,
                                      eks_error_entries,
                                      G_N_ELEMENTS (eks_error_entries));

  return (GQuark) quark_volatile;
}
