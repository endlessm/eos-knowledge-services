/* Copyright 2018 Endless Mobile, Inc. */

#include "eks-query-util.h"

#include <dmodel.h>

#include <gio/gio.h>

gboolean
models_for_result (DmEngine      *engine,
                   const char    *application_id,
                   GAsyncResult  *result,
                   GSList       **models,
                   gint          *upper_bound,
                   GError       **error)
{
  g_autoptr(DmQueryResults) results = NULL;
  if (!(results = dm_engine_query_finish (engine, result, error)))
      return FALSE;

  DmDomain *domain = dm_engine_get_domain_for_app (engine, application_id,
                                                   error);
  if (domain == NULL)
      return FALSE;

  if (models != NULL)
    *models = g_slist_copy_deep (dm_query_results_get_models (results),
                                 (GCopyFunc) g_object_ref,
                                 NULL);

  if (upper_bound != NULL)
    *upper_bound = dm_query_results_get_upper_bound (results);

  return TRUE;
}

gboolean
models_and_shards_for_result (DmEngine    *engine,
                              const char    *application_id,
                              GAsyncResult  *result,
                              GSList       **models,
                              GSList       **shards,
                              gint          *upper_bound,
                              GError       **error)
{
  g_autoptr(DmQueryResults) results = NULL;
  if (!(results = dm_engine_query_finish (engine, result, error)))
      return FALSE;

  DmDomain *domain = dm_engine_get_domain_for_app (engine, application_id,
                                                   error);
  if (domain == NULL)
      return FALSE;

  if (shards != NULL)
    *shards = g_slist_copy_deep (dm_domain_get_shards (domain),
                                 (GCopyFunc) g_object_ref,
                                 NULL);

  if (models != NULL)
    *models = g_slist_copy_deep (dm_query_results_get_models (results),
                                 (GCopyFunc) g_object_ref,
                                 NULL);

  if (upper_bound != NULL)
    *upper_bound = dm_query_results_get_upper_bound (results);

  return TRUE;
}

GStrv
strv_from_shard_list (GSList *string_list)
{
  GStrv strv = g_new0 (char *, g_slist_length (string_list) + 1);
  guint count = 0;

  for (GSList *l = string_list; l; l = l->next)
    strv[count++] = g_strdup (dm_shard_get_path (l->data));

  return strv;
}
