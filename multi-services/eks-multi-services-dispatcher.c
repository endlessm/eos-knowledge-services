/* Copyright 2016 Endless Mobile, Inc. */

#include <gio/gio.h>
#include <glib.h>

#include <string.h>

static char *services_version;

static GOptionEntry entries[] = {
  { "services-version", 's', 0, G_OPTION_ARG_STRING, &services_version, "The eks-search-provider version to use", "VERSION" },
  { NULL }
};

static GStrv
envp_search (GStrv       envp,
             const char *variable)
{
  gsize variable_len = strlen (variable);

  for (; *envp != NULL; ++envp)
    {
      if (strncmp (*envp, variable, variable_len) == 0)
        return envp;
    }

  return NULL;
}

static void
insert_paths_to_env_var (GStrv               env,
                         const char         *variable,
                         const char * const *paths)
{
  GStrv envp_index = envp_search (env, variable);
  g_autofree char *join_envp = NULL;

  if (envp_index == NULL)
    return;

  join_envp = g_strjoinv (":", (GStrv) paths);

  /* Free the string already at envp_index */
  g_free (*envp_index);

  *envp_index = g_strdup_printf ("%s=%s", variable, join_envp);
}

static GSubprocess *
spawnv_with_appended_paths_and_fds (const char * const  *argv,
                                    const char * const  *executable_paths,
                                    const char * const  *ld_library_paths,
                                    const char * const  *xdg_data_dirs,
                                    GError             **error)
{
  g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_INHERIT_FDS);
  g_auto(GStrv) env = g_listenv ();

  insert_paths_to_env_var (env, "PATH", executable_paths);
  insert_paths_to_env_var (env, "LD_LIBRARY_PATH", ld_library_paths);
  insert_paths_to_env_var (env, "XDG_DATA_DIRS", xdg_data_dirs);

  return g_subprocess_launcher_spawnv (launcher, argv, error); 
}

static gboolean
dispatch_correct_service (const char  *services_version,
                          GError      **error)
{
  /* We keep track of this and then free it, the exec'd service
   * will become a child of init and inherit all our fds, thus
   * consuming the d-bus traffic that was destined for this process. */
  g_autoptr(GSubprocess) subprocess = NULL;

  if (g_strcmp0 (services_version, "1") == 0)
    {
      const char * const argv[] = {
        "/app/eos-knowledge-services/1/bin/eks-search-provider-v1",
        NULL
      };
      const char * const executable_paths[] = {
        "/app/sdk/1/bin",
        "/app/eos-knowledge-services/1/bin",
        NULL
      };
      const char * const ld_library_paths[] = {
        "/app/sdk/1/lib",
        "/app/eos-knowledge-services/1/lib",
        NULL
      };
      const char * const xdg_data_dirs[] = {
        "/app/sdk/1/share",
        "/app/eos-knowledge-services/1/share",
        NULL
      };

      subprocess = spawnv_with_appended_paths_and_fds (argv,
                                                       executable_paths,
                                                       ld_library_paths,
                                                       xdg_data_dirs,
                                                       error);
      return subprocess != NULL;
    }
  else if (g_strcmp0 (services_version, "2") == 0)
    {
      const char * argv[] = {
        "/app/eos-knowledge-services/2/bin/eks-search-provider-v2",
        NULL
      };
      const char * executable_paths[] = {
        "/app/sdk/3/bin",
        "/app/eos-knowledge-services/2/bin",
        NULL
      };
      const char * ld_library_paths[] = {
        "/app/sdk/3/lib",
        "/app/eos-knowledge-services/2/lib",
        NULL
      };
      const char * xdg_data_dirs[] = {
        "/app/sdk/3/share",
        "/app/eos-knowledge-services/2/share",
        NULL
      };

      subprocess = spawnv_with_appended_paths_and_fds (argv,
                                                       executable_paths,
                                                       ld_library_paths,
                                                       xdg_data_dirs,
                                                       error);
      return subprocess != NULL;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_FAILED,
               "Don't know how to spawn services version %s",
               services_version);
  return FALSE;
}

int
main (int    argc,
      char **argv)
{
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GOptionContext) context = g_option_context_new ("- multiplexer for EknServices");

  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &local_error))
    {
      g_message ("Option parsing failed: %s\n", local_error->message);
      return 1;
    }

  if (!dispatch_correct_service (services_version, &local_error))
    {
      g_message ("Failed to dispatch correct service for version %s: %s",
                 services_version,
                 local_error->message);
      return 1;
    }

  return 0;
}
