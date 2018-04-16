#include <gio/gio.h>

#include "eks-search-app.h"

gint
main (gint   argc,
      gchar *argv[])
{
    g_autoptr(GApplication) app = g_object_new (EKS_TYPE_SEARCH_APP,
                                                "application-id", "com.endlessm.EknServices3.SearchProviderV3",
                                                "flags", G_APPLICATION_IS_SERVICE,
                                                "inactivity-timeout", 12000,
                                                NULL);
    return g_application_run (app, argc, argv);
}
