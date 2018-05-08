/* Copyright 2018 Endless Mobile, Inc. */

#include "eks-query-util.h"

#include <eos-knowledge-content.h>
#include <eos-shard/eos-shard-shard-file.h>

#include <gio/gio.h>

/* For instance on "/frob/foo/bar/baz" "foo", return "bar/baz" */
static char *
path_components_from_end_until (const char *path, const char *marker)
{
  g_auto(GStrv) split_path = g_strsplit (path, "/", -1);
  int len = g_strv_length (split_path);
  int i = len - 1;

  for (; i >= 0 && g_strcmp0 (split_path[i], marker) != 0; --i);

  return g_build_filenamev (&(split_path[i + 1]));
}

static gboolean
make_directory_with_parents_ignore_existing (GFile         *directory,
                                             GCancellable  *cancellable,
                                             GError       **error)
{
  g_autoptr(GError) local_error = NULL;

  if (!g_file_make_directory_with_parents (directory,
                                           cancellable,
                                           &local_error))
    {
      if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        return TRUE;

      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  return TRUE;
}

gboolean
eks_ensure_app_shards_are_symlinked_to_home_directory (EkncEngine  *engine,
                                                       const char  *application_id,
                                                       GError     **error)
{
  const char *home_directory = g_get_home_dir ();
  g_autofree char *home_shards_directory_path = g_build_filename (home_directory,
                                                                  ".local",
                                                                  "share",
                                                                  "com.endlessm.subscriptions",
                                                                  NULL);

  EkncDomain *domain = eknc_engine_get_domain_for_app (engine,
                                                       application_id,
                                                       error);
  if (domain == NULL)
      return FALSE;

  g_auto(GStrv) shard_paths = strv_from_shard_list (eknc_domain_get_shards (domain));
  GStrv iter = shard_paths;

  for (; *iter != NULL; ++iter)
    {
      const char *shard_file_path = *iter;
      g_autoptr(GFile) shard_file = g_file_new_for_path (shard_file_path);
      g_autofree char *child_shard_path_component =
        path_components_from_end_until (shard_file_path,
                                        "com.endlessm.subscriptions");
      g_autofree char *home_directory_shard_file_path =
        g_build_filename (home_shards_directory_path,
                          child_shard_path_component,
                          NULL);
      g_autoptr(GFile) home_directory_shard_file =
        g_file_new_for_path (home_directory_shard_file_path);
      g_autoptr(GFile) home_directory_shard_file_parent =
        g_file_get_parent (home_directory_shard_file);
      g_autoptr(GError) local_error = NULL;

      if (!make_directory_with_parents_ignore_existing (home_directory_shard_file_parent,
                                                        NULL,
                                                        error))
        return FALSE;

      if (!g_file_make_symbolic_link (home_directory_shard_file,
                                      shard_file_path,
                                      NULL,
                                      &local_error))
        {
          if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
            continue;

          g_propagate_error (error, g_steal_pointer (&local_error));
          return FALSE;
        }               
    }

  return TRUE;
}

