/* Copyright 2018 Endless Mobile, Inc. */

#include "eks-query-util.h"

#include <eos-knowledge-content.h>
#include <eos-shard/eos-shard-shard-file.h>

#include <gio/gio.h>

gboolean
models_for_result (EkncEngine    *engine,
                   const char    *application_id,
                   GAsyncResult  *result,
                   GSList       **models,
                   gint          *upper_bound,
                   GError       **error)
{
  g_autoptr(EkncQueryResults) results = NULL;
  if (!(results = eknc_engine_query_finish (engine, result, error)))
      return FALSE;

  EkncDomain *domain = eknc_engine_get_domain_for_app (engine,
                                                       application_id,
                                                       error);
  if (domain == NULL)
      return FALSE;

  if (models != NULL)
    *models = g_slist_copy_deep (eknc_query_results_get_models (results),
                                 (GCopyFunc) g_object_ref,
                                 NULL);

  if (upper_bound != NULL)
    *upper_bound = eknc_query_results_get_upper_bound (results);

  return TRUE;
}

gboolean
models_and_shards_for_result (EkncEngine    *engine,
                              const char    *application_id,
                              GAsyncResult  *result,
                              GSList       **models,
                              GSList       **shards,
                              gint          *upper_bound,
                              GError       **error)
{
  g_autoptr(EkncQueryResults) results = NULL;
  if (!(results = eknc_engine_query_finish (engine, result, error)))
      return FALSE;

  EkncDomain *domain = eknc_engine_get_domain_for_app (engine,
                                                       application_id,
                                                       error);
  if (domain == NULL)
      return FALSE;

  if (shards != NULL)
    *shards = g_slist_copy_deep (eknc_domain_get_shards (domain),
                                 (GCopyFunc) g_object_ref,
                                 NULL);

  if (models != NULL)
    *models = g_slist_copy_deep (eknc_query_results_get_models (results),
                                 (GCopyFunc) g_object_ref,
                                 NULL);

  if (upper_bound != NULL)
    *upper_bound = eknc_query_results_get_upper_bound (results);

  return TRUE;
}

GStrv
strv_from_shard_list (GSList *string_list)
{
  GStrv strv = g_new0 (char *, g_slist_length (string_list));
  guint count = 0;

  for (GSList *l = string_list; l; l = l->next)
    {
      EosShardShardFile *shard = l->data;
      char *shard_path = NULL;

      g_object_get (shard, "path", &shard_path, NULL);
      strv[count++] = shard_path;
    }

  return strv;
}
