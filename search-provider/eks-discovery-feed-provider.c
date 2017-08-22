/* Copyright 2016 Endless Mobile, Inc. */

#include "eks-discovery-feed-provider.h"
#include "eks-provider-iface.h"

#include "eks-knowledge-app-dbus.h"
#include "eks-discovery-feed-provider-dbus.h"

#include <eos-knowledge-content.h>
#include <eos-shard/eos-shard-shard-file.h>

#include <stdio.h>
#include <string.h>

#define NUMBER_OF_ARTICLES 5
#define DAYS_IN_YEAR 365

struct _EksDiscoveryFeedDatabaseContentProvider
{
  GObject parent_instance;

  gchar *application_id;
  EksDiscoveryFeedContent *content_skeleton;
  EksDiscoveryFeedContent *content_app_proxy;
  EksDiscoveryFeedQuote *quote_skeleton;
  EksDiscoveryFeedQuote *quote_app_proxy;
  EksDiscoveryFeedWord *word_skeleton;
  EksDiscoveryFeedWord *word_app_proxy;
  EksDiscoveryFeedNews *news_skeleton;
  EksDiscoveryFeedNews *news_app_proxy;
  EksDiscoveryFeedVideo *video_skeleton;
  EksDiscoveryFeedVideo *video_app_proxy;
  EksDiscoveryFeedArtwork *artwork_skeleton;
  EksDiscoveryFeedArtwork *artwork_app_proxy;
  GCancellable *cancellable;
};

static void eks_discovery_feed_database_content_provider_interface_init (EksProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EksDiscoveryFeedDatabaseContentProvider,
                         eks_discovery_feed_database_content_provider,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EKS_TYPE_PROVIDER,
                                                eks_discovery_feed_database_content_provider_interface_init))

enum {
  PROP_0,
  PROP_APPLICATION_ID,
  NPROPS
};

static GParamSpec *eks_discovery_feed_database_content_provider_props [NPROPS] = { NULL, };

static void
eks_discovery_feed_database_content_provider_get_property (GObject    *object,
                                                           guint       prop_id,
                                                           GValue     *value,
                                                           GParamSpec *pspec)
{
  EksDiscoveryFeedDatabaseContentProvider *self = EKS_DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_value_set_string (value, self->application_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eks_discovery_feed_database_content_provider_set_property (GObject      *object,
                                                           guint         prop_id,
                                                           const GValue *value,
                                                           GParamSpec   *pspec)
{
  EksDiscoveryFeedDatabaseContentProvider *self = EKS_DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_clear_pointer (&self->application_id, g_free);
      self->application_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eks_discovery_feed_database_content_provider_finalize (GObject *object)
{
  EksDiscoveryFeedDatabaseContentProvider *self = EKS_DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER (object);

  g_clear_pointer (&self->application_id, g_free);
  g_clear_object (&self->content_skeleton);
  g_clear_object (&self->quote_skeleton);
  g_clear_object (&self->word_skeleton);
  g_clear_object (&self->news_skeleton);
  g_clear_object (&self->video_skeleton);
  g_clear_object (&self->content_app_proxy);
  g_clear_object (&self->quote_app_proxy);
  g_clear_object (&self->word_app_proxy);
  g_clear_object (&self->news_app_proxy);
  g_clear_object (&self->video_app_proxy);
  g_clear_object (&self->artwork_app_proxy);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (eks_discovery_feed_database_content_provider_parent_class)->finalize (object);
}

static void
eks_discovery_feed_database_content_provider_class_init (EksDiscoveryFeedDatabaseContentProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = eks_discovery_feed_database_content_provider_get_property;
  object_class->set_property = eks_discovery_feed_database_content_provider_set_property;
  object_class->finalize = eks_discovery_feed_database_content_provider_finalize;

  eks_discovery_feed_database_content_provider_props[PROP_APPLICATION_ID] =
    g_param_spec_string ("application-id", "Application Id", "Application Id",
      "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     NPROPS,
                                     eks_discovery_feed_database_content_provider_props);
}

static  gchar *
object_path_from_app_id (const gchar *application_id)
{
  g_autoptr(GRegex) dot_regex = g_regex_new ("\\.", 0, 0, NULL);
  g_autofree gchar *replaced = g_regex_replace (dot_regex, application_id, -1, 0, "/", 0, NULL);
  return g_strconcat ("/", replaced, NULL);
}

static gboolean
ensure_content_app_proxy (EksDiscoveryFeedDatabaseContentProvider *self)
{
  if (self->content_app_proxy != NULL)
    return TRUE;

  g_autofree gchar *object_path = object_path_from_app_id (self->application_id);
  g_autoptr(GError) error = NULL;
  self->content_app_proxy = eks_discovery_feed_content_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                               G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                                                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                               self->application_id,
                                                                               object_path,
                                                                               NULL,
                                                                               &error);
  if (error != NULL)
    {
      g_warning ("Error initializing dbus proxy: %s", error->message);
      return FALSE;
    }
  return TRUE;
}

static gboolean
ensure_quote_app_proxy (EksDiscoveryFeedDatabaseContentProvider *self)
{
  if (self->quote_app_proxy != NULL)
    return TRUE;

  g_autofree gchar *object_path = object_path_from_app_id (self->application_id);
  GError *error = NULL;
  self->quote_app_proxy = eks_discovery_feed_quote_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                           G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                                                                           G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                           self->application_id,
                                                                           object_path,
                                                                           NULL,
                                                                           &error);
  if (error != NULL)
    {
      g_warning ("Error initializing dbus proxy: %s\n", error->message);
      g_clear_error (&error);
      return FALSE;
    }
  return TRUE;
}

static gboolean
ensure_word_app_proxy (EksDiscoveryFeedDatabaseContentProvider *self)
{
  if (self->word_app_proxy != NULL)
    return TRUE;

  g_autofree gchar *object_path = object_path_from_app_id (self->application_id);
  GError *error = NULL;
  self->word_app_proxy = eks_discovery_feed_word_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                                                                         G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                         self->application_id,
                                                                         object_path,
                                                                         NULL,
                                                                         &error);
  if (error != NULL)
    {
      g_warning ("Error initializing dbus proxy: %s\n", error->message);
      g_clear_error (&error);
      return FALSE;
    }
  return TRUE;
}

static gboolean
ensure_news_app_proxy (EksDiscoveryFeedDatabaseContentProvider *self)
{
  if (self->news_app_proxy != NULL)
    return TRUE;

  g_autofree gchar *object_path = object_path_from_app_id (self->application_id);
  GError *error = NULL;
  self->news_app_proxy = eks_discovery_feed_news_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                                                                         G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                         self->application_id,
                                                                         object_path,
                                                                         NULL,
                                                                         &error);
  if (error != NULL)
    {
      g_warning ("Error initializing dbus proxy: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }
  return TRUE;
}

static gboolean
ensure_video_app_proxy (EksDiscoveryFeedDatabaseContentProvider *self)
{
  if (self->video_app_proxy != NULL)
    return TRUE;

  g_autofree gchar *object_path = object_path_from_app_id (self->application_id);
  GError *error = NULL;
  self->video_app_proxy = eks_discovery_feed_video_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                           G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                                                                           G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                           self->application_id,
                                                                           object_path,
                                                                           NULL,
                                                                           &error);
  if (error != NULL)
    {
      g_warning ("Error initializing dbus proxy: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }
  return TRUE;
}

static gboolean
ensure_artwork_app_proxy (EksDiscoveryFeedDatabaseContentProvider *self)
{
  if (self->artwork_app_proxy != NULL)
    return TRUE;

  g_autofree gchar *object_path = object_path_from_app_id (self->application_id);
  GError *error = NULL;
  self->artwork_app_proxy = eks_discovery_feed_artwork_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                               G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                                                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                               self->application_id,
                                                                               object_path,
                                                                               NULL,
                                                                               &error);
  if (error != NULL)
    {
      g_warning ("Error initializing dbus proxy: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }
  return TRUE;
}

typedef struct {
  GDBusMethodInvocation *invocation;
  EksDiscoveryFeedDatabaseContentProvider  *provider;
} DiscoveryFeedQueryState;

static DiscoveryFeedQueryState *
discovery_feed_query_state_new (GDBusMethodInvocation                  *invocation,
                                EksDiscoveryFeedDatabaseContentProvider *provider)
{
  DiscoveryFeedQueryState *state = g_slice_new0 (DiscoveryFeedQueryState);
  state->invocation = g_object_ref (invocation);
  state->provider = provider;

  return state;
}

static void
discovery_feed_query_state_free (DiscoveryFeedQueryState *state)
{
  g_object_unref (state->invocation);
  g_slice_free (DiscoveryFeedQueryState, state);
}

static inline gchar *
underscorify (const gchar *string)
{
  gchar *underscored = strdup (string);
  gchar *c = underscored;
  do
    {
      if (*c == '-')
        *c = '_';
    }
  while (*c++);

  return underscored;
}

static void
add_key_value_pair_to_variant (GVariantBuilder *builder,
                               const char      *key,
                               const char      *value)
{
  g_variant_builder_open (builder, G_VARIANT_TYPE ("{ss}"));
  g_variant_builder_add (builder, "s", key);
  g_variant_builder_add (builder, "s", value);
  g_variant_builder_close (builder);
}

static void
add_key_value_pair_from_model_to_variant (EkncContentObjectModel *model,
                                          GVariantBuilder        *builder,
                                          const char             *key)
{
  g_autofree gchar *value = NULL;
  g_autofree gchar *underscore_key = underscorify (key);
  g_object_get (model, key, &value, NULL);
  add_key_value_pair_to_variant (builder, underscore_key, value);
}

static void
add_key_value_int_to_str_pair_from_model_to_variant (EkncContentObjectModel *model,
                                                     GVariantBuilder        *builder,
                                                     const char             *key)
{
  gint value;
  g_autofree gchar *str_value = malloc (sizeof (gchar) * 8);
  g_autofree gchar *underscore_key = underscorify (key);
  g_object_get (model, key, &value, NULL);
  snprintf (str_value, 8, "%i", value);
  add_key_value_pair_to_variant (builder, underscore_key, str_value);
}

static void
add_author_from_model_to_variant (EkncContentObjectModel *model,
                                  GVariantBuilder        *builder,
                                  const char             *key)
{
  g_autoptr(GVariant) authors;
  g_object_get (model, "authors", &authors, NULL);

  if (authors)
    {
      g_autoptr(GVariant) author = g_variant_get_child_value (authors, 0);
      add_key_value_pair_to_variant (builder, key, g_variant_dup_string (author, NULL));
    }
  else
    add_key_value_pair_to_variant (builder, key, "");
}

static gint
get_day_of_week (void)
{
  g_autoptr(GDateTime) datetime = g_date_time_new_now_local ();
  return g_date_time_get_day_of_week (datetime);
}

static gint
get_day_of_year (void)
{
  g_autoptr(GDateTime) datetime = g_date_time_new_now_local ();
  return g_date_time_get_day_of_year (datetime);
}

static gchar *
select_string_from_variant_from_day (GVariant *variant)
{
  gsize size = g_variant_n_children (variant);
  if (size == 0)
    return NULL;

  gint index = get_day_of_week () % size;
  /* We need to unwrap the variant and then the inner string first */
  g_autoptr(GVariant) child_variant = g_variant_get_child_value (variant, index);
  g_autoptr(GVariant) child_value = g_variant_get_variant (child_variant);

  return g_variant_dup_string (child_value, NULL);
}

typedef enum {
  DISCOVERY_FEED_NO_CUSTOM_PROPS = 0,
  DISCOVERY_FEED_SET_CUSTOM_TITLE = 1 << 0
} DiscoveryFeedCustomProps;

static gboolean
models_for_result (EkncEngine   *engine,
                   const gchar  *application_id,
                   GAsyncResult *result,
                   GSList       **models,
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

  *models = g_slist_copy_deep (eknc_query_results_get_models (results),
                               (GCopyFunc) g_object_ref,
                               NULL);

  return TRUE;
}

static gboolean
models_and_shards_for_result (EkncEngine   *engine,
                              const gchar  *application_id,
                              GAsyncResult *result,
                              GSList       **models,
                              GSList       **shards,
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

  *shards = g_slist_copy_deep (eknc_domain_get_shards (domain),
                               (GCopyFunc) g_object_ref,
                               NULL);
  *models = g_slist_copy_deep (eknc_query_results_get_models (results),
                               (GCopyFunc) g_object_ref,
                               NULL);

  return TRUE;
}

GStrv
strv_from_shard_list (GSList *string_list)
{
  GStrv strv = g_new0 (gchar *, g_slist_length (string_list));
  guint count = 0;

  for (GSList *l = string_list; l; l = l->next)
    {
      EosShardShardFile *shard = l->data;
      gchar  *shard_path = NULL;

      g_object_get (shard, "path", &shard_path, NULL);
      strv[count++] = shard_path;
    }

  return strv;
}

/* Copy properties from source object to GValue
 * and char * array. Assumes that all properties
 * are readable. */
static guint
pspecs_to_param_array (GParamSpec **pspecs,
                       GObject    *source,
                       guint      n_pspecs,
                       const gchar ***out_names,
                       GValue      **out_values)
{
  const gchar **names = g_new0 (const gchar *, n_pspecs);
  GValue *values = g_new0 (GValue, n_pspecs);
  guint i = 0;

  for (; i < n_pspecs; ++i)
    {
      names[i] = pspecs[i]->name;

      g_value_init (&values[i], pspecs[i]->value_type);
      g_object_get_property (source, names[i], &values[i]);
    }

  *out_names = names;
  *out_values = values;

  return n_pspecs;
}

/* Set the given property in the parameter array,
 * appending it if necessary. Returns the number of
 * parameters in the new arrays */
static guint
set_property_in_param_array (const gchar *name,
                             const GValue *replacement,
                             const gchar ***out_names,
                             GValue **out_values,
                             guint n_params)
{
  guint i = 0;

  /* First search for the property and try to set it directly. */
  for (; i < n_params; ++i)
    {
      if (g_strcmp0 ((*out_names)[i], name) == 0)
        {
          g_value_copy (replacement, &((*out_values)[i]));
          return n_params;
        }
    }

  /* Not found, append the property */
  *out_names = g_renew (const gchar *, *out_names, n_params + 1);
  *out_values = g_renew (GValue, *out_values, n_params + 1);

  *out_names[n_params] = name;
  g_value_copy (replacement, &((*out_values)[n_params])) ;

  return n_params + 1;
}

typedef struct _QueryPendingUpperBound {
  EkncQueryObject     *query;
  guint               offset_within_upper_bound;
  guint               wraparound_upper_bound;
  GCancellable        *cancellable;
  GAsyncReadyCallback main_query_ready_callback;
  gpointer            main_query_ready_data;
} QueryPendingUpperBound;

static QueryPendingUpperBound *
query_pending_upper_bound_new (EkncQueryObject     *query,
                               guint               offset_within_upper_bound,
                               guint               wraparound_upper_bound,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback main_query_ready_callback,
                               gpointer            main_query_ready_data)
{
  QueryPendingUpperBound *data = g_new0 (QueryPendingUpperBound, 1);
  data->query = g_object_ref (query);
  data->offset_within_upper_bound = offset_within_upper_bound;
  data->wraparound_upper_bound = wraparound_upper_bound;

  /* Keep cancellable alive if we got one, otherwise ignore it */
  data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  data->main_query_ready_callback = main_query_ready_callback;
  data->main_query_ready_data = main_query_ready_data;

  return data;
}

static void
query_pending_upper_bound_free (QueryPendingUpperBound *data)
{
  g_object_unref (data->query);
  g_clear_object (&data->cancellable);

  g_free (data);
}

/* Free an array of GValue structures, making sure to clear each value */
static void
free_gvalue_array (GValue *values, guint n_values)
{
  guint i = 0;

  for (; i < n_values; ++i)
    g_value_unset (&values[i]);

  g_free (values);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (QueryPendingUpperBound, query_pending_upper_bound_free)

static void
on_received_upper_bound_result (GObject      *source,
                                GAsyncResult *result,
                                gpointer     user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  GError *error = NULL;
  gint upper_bound;
  gint intended_limit;
  guint n_properties;
  g_auto(GValue) offset_gvalue = G_VALUE_INIT;

  /* Cannot use g_autofree with property_values as they must be cleared */
  GValue *property_values = NULL;
  g_autofree gchar  **property_names = NULL;
  g_autoptr(QueryPendingUpperBound) pending = user_data;
  g_autoptr(EkncQueryResults) results = NULL;
  g_autofree GParamSpec **copy_properties = g_object_class_list_properties (G_OBJECT_GET_CLASS (G_OBJECT (pending->query)),
                                                                            &n_properties);

  if (!(results = eknc_engine_query_finish (engine, result, &error)))
    {
      g_warning ("Unable to get upper bound on results, aborting query: %s",
                 error->message);
      return;
    }

  /* Now that we have results, we can read the upper bound and
   * determine the actual offset */
  g_object_get (results, "upper-bound", &upper_bound, NULL);
  g_object_get (pending->query, "limit", &intended_limit, NULL);

  /* EkncQueryObject is immutable, so we have to create a new one
   * and set construct properties again */
  pspecs_to_param_array (copy_properties,
                         G_OBJECT (pending->query),
                         n_properties,
                         (const gchar ***) &property_names,
                         &property_values);

  g_value_init (&offset_gvalue, G_TYPE_UINT);
  g_value_set_uint (&offset_gvalue,
                    pending->offset_within_upper_bound % (MIN(upper_bound,
                                                              pending->wraparound_upper_bound) -
                                                          intended_limit));
  n_properties = set_property_in_param_array ("offset",
                                              &offset_gvalue,
                                              (const gchar ***) &property_names,
                                              &property_values,
                                              n_properties);

  /* Get rid of the old query and construct a new one in its place */
  g_clear_object (&pending->query);
  pending->query = EKNC_QUERY_OBJECT (g_object_new_with_properties (EKNC_TYPE_QUERY_OBJECT,
                                                                    n_properties,
                                                                    (const gchar **) property_names,
                                                                    property_values));

  /* Okay, now fire off the *actual* query, passing the user data
   * and callback that we were going to pass the first time */
  eknc_engine_query (engine,
                     pending->query,
                     pending->cancellable,
                     pending->main_query_ready_callback,
                     pending->main_query_ready_data);

  free_gvalue_array (property_values, n_properties);
}

/* This function executes the given query with an offset computed
 * to be within the total number of results within the query set. The
 * caller should pass an offset that it would intend to use if
 * the query set had the number of results specified in
 * wraparound_upper_bound. The true wraparound will them be computed
 * with respect to maximum of either the number of articles in the
 * result set or the wraparound_upper_bound, accounting for the fact
 * that we may want to fetch the full window of articles specified
 * in the limit parameter to the query
 */
static gboolean
query_with_wraparound_offset (EkncEngine          *engine,
                              EkncQueryObject     *query,
                              guint               offset_within_upper_bound,
                              guint               wraparound_upper_bound,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback main_query_ready_callback,
                              gpointer            main_query_ready_data)
{
  gint intended_limit;
  guint n_properties;
  g_auto(GValue) limit_gvalue = G_VALUE_INIT;

  /* Cannot use g_autofree with property_values as they must be cleared */
  GValue *property_values = NULL;
  g_autofree gchar  **property_names = NULL;

  g_autofree GParamSpec **copy_properties = g_object_class_list_properties (G_OBJECT_GET_CLASS (G_OBJECT (query)),
                                                                            &n_properties);
  pspecs_to_param_array (copy_properties,
                         G_OBJECT (query),
                         n_properties,
                         (const gchar ***) &property_names,
                         &property_values);

  /* Override the limit, setting it to one. In the returned query we'll get
   * nothing back, but Xapian will tell us how many models matched our query
   * which we'll use later. We have to ask for at least one article
   * here, otherwise we trigger assertions in knowledge-lib. */
  g_value_init (&limit_gvalue, G_TYPE_UINT);
  g_value_set_uint (&limit_gvalue, 1);
  n_properties = set_property_in_param_array ("limit",
                                              &limit_gvalue,
                                              (const gchar ***) &property_names,
                                              &property_values,
                                              n_properties);

  g_autoptr (EkncQueryObject) truncated_query = EKNC_QUERY_OBJECT (g_object_new_with_properties (EKNC_TYPE_QUERY_OBJECT,
                                                                                                 n_properties,
                                                                                                 (const gchar **) property_names,
                                                                                                 property_values));

  /* Dispatch the query, when it comes back we'll know what to
   * set the offset to */
  eknc_engine_query (engine,
                     truncated_query,
                     cancellable,
                     on_received_upper_bound_result,
                     query_pending_upper_bound_new (query,
                                                    offset_within_upper_bound,
                                                    wraparound_upper_bound,
                                                    cancellable,
                                                    main_query_ready_callback,
                                                    main_query_ready_data));

  free_gvalue_array (property_values, n_properties);
}

static void
artwork_card_descriptions_cb (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  DiscoveryFeedQueryState *state = user_data;

  g_application_release (g_application_get_default ());

  GError *error = NULL;
  GSList *models = NULL;
  GSList *shards = NULL;

  if (!models_and_shards_for_result (engine,
                                     state->provider->application_id,
                                     result,
                                     &models,
                                     &shards,
                                     &error))
    {
      g_dbus_method_invocation_take_error (state->invocation, error);
      discovery_feed_query_state_free (state);

      /* No need to free_full the out models and shards here,
       * g_slist_copy_deep is not called if this function returns FALSE. */
      return;
    }

  g_auto(GStrv) shards_strv = strv_from_shard_list (shards);

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{ss}"));

  for (GSList *l = models; l; l = l->next)
    {
      /* Start building up object */
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));

      EkncContentObjectModel *model = l->data;
      DiscoveryFeedCustomProps flags = DISCOVERY_FEED_NO_CUSTOM_PROPS;

      /* Examine the discovery-feed-content object first and set flags
       * for things that we've overridden */
      g_autoptr(GVariant) discovery_feed_content_variant;
      g_object_get (model,
                    "discovery-feed-content",
                    &discovery_feed_content_variant,
                    NULL);

      if (discovery_feed_content_variant)
        {
          GVariantIter discovery_feed_content_iter;
          g_variant_iter_init (&discovery_feed_content_iter,
                               discovery_feed_content_variant);

          gchar *key;
          GVariant *value;

          while (g_variant_iter_loop (&discovery_feed_content_iter, "{sv}", &key, &value))
            {
              if (g_strcmp0 (key, "blurbs") == 0)
                {
                  g_autofree gchar *title = select_string_from_variant_from_day (value);

                  if (title)
                    {
                      add_key_value_pair_to_variant (&builder, "title", title);
                      add_key_value_pair_to_variant (&builder, "synopsis", "");
                      flags |= DISCOVERY_FEED_SET_CUSTOM_TITLE;
                    }
                }
            }
        }

      /* Add key-value pairs based on things we haven't addded yet */
      if (!(flags & DISCOVERY_FEED_SET_CUSTOM_TITLE))
        {
          add_key_value_pair_from_model_to_variant (model, &builder, "title");
          add_key_value_pair_from_model_to_variant (model, &builder, "synopsis");
        }

      add_key_value_pair_from_model_to_variant (model, &builder, "last-modified-date");
      add_key_value_pair_from_model_to_variant (model, &builder, "thumbnail-uri");
      add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");
      add_author_from_model_to_variant (model, &builder, "author");

      /* Stop building object */
      g_variant_builder_close (&builder);
    }

  eks_discovery_feed_artwork_complete_artwork_card_descriptions (state->provider->artwork_skeleton,
                                                                 state->invocation,
                                                                 (const gchar * const *) shards_strv,
                                                                 g_variant_builder_end (&builder));
  g_slist_free_full (models, g_object_unref);
  g_slist_free_full (shards, g_object_unref);
  discovery_feed_query_state_free (state);
}

static gboolean
handle_artwork_card_descriptions (EksDiscoveryFeedDatabaseContentProvider *skeleton,
                                  GDBusMethodInvocation                  *invocation,
                                  gpointer                                user_data)
{
    EksDiscoveryFeedDatabaseContentProvider *self = user_data;

    if (!ensure_content_app_proxy (self))
      return TRUE;

    EkncEngine *engine = eknc_engine_get_default ();

    /* Build up tags_match_any */
    GVariantBuilder tags_match_any_builder;
    g_variant_builder_init (&tags_match_any_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_any_builder, "s", "EknArticleObject");
    GVariant *tags_match_any = g_variant_builder_end (&tags_match_any_builder);

    /* Hold the application so that it doesn't go away whilst we're handling
     * the query */
    g_application_hold (g_application_get_default ());

    /* Create query and run it */
    query_with_wraparound_offset (engine,
                                  g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                "tags-match-any", tags_match_any,
                                                "sort", EKNC_QUERY_OBJECT_SORT_DATE,
                                                "order", EKNC_QUERY_OBJECT_ORDER_DESCENDING,
                                                "limit", NUMBER_OF_ARTICLES,
                                                "app-id", self->application_id,
                                                NULL),
                                  get_day_of_year (),
                                  DAYS_IN_YEAR,
                                  self->cancellable,
                                  artwork_card_descriptions_cb,
                                  discovery_feed_query_state_new (invocation, self));

    return TRUE;
}

static void
content_article_card_descriptions_cb (GObject *source,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  DiscoveryFeedQueryState *state = user_data;

  g_application_release (g_application_get_default ());

  GError *error = NULL;
  GSList *models = NULL;
  GSList *shards = NULL;

  if (!models_and_shards_for_result (engine,
                                     state->provider->application_id,
                                     result,
                                     &models,
                                     &shards,
                                     &error))
    {
      g_dbus_method_invocation_take_error (state->invocation, error);
      discovery_feed_query_state_free (state);

      /* No need to free_full the out models and shards here,
       * g_slist_copy_deep is not called if this function returns FALSE. */
      return;
    }

  g_auto(GStrv) shards_strv = strv_from_shard_list (shards);

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{ss}"));

  for (GSList *l = models; l; l = l->next)
    {
      /* Start building up object */
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));

      EkncContentObjectModel *model = l->data;
      DiscoveryFeedCustomProps flags = DISCOVERY_FEED_NO_CUSTOM_PROPS;

      /* Examine the discovery-feed-content object first and set flags
       * for things that we've overridden */
      g_autoptr(GVariant) discovery_feed_content_variant;
      g_object_get (model,
                    "discovery-feed-content",
                    &discovery_feed_content_variant,
                    NULL);

      if (discovery_feed_content_variant)
        {
          GVariantIter discovery_feed_content_iter;
          g_variant_iter_init (&discovery_feed_content_iter,
                               discovery_feed_content_variant);

          gchar *key;
          GVariant *value;

          while (g_variant_iter_loop (&discovery_feed_content_iter, "{sv}", &key, &value))
            {
              if (g_strcmp0 (key, "blurbs") == 0)
                {
                  g_autofree gchar *title = select_string_from_variant_from_day (value);

                  if (title)
                    {
                      add_key_value_pair_to_variant (&builder, "title", title);
                      add_key_value_pair_to_variant (&builder, "synopsis", "");
                      flags |= DISCOVERY_FEED_SET_CUSTOM_TITLE;
                    }
                }
            }
        }

      /* Add key-value pairs based on things we haven't addded yet */
      if (!(flags & DISCOVERY_FEED_SET_CUSTOM_TITLE))
        {
          add_key_value_pair_from_model_to_variant (model, &builder, "title");
          add_key_value_pair_from_model_to_variant (model, &builder, "synopsis");
        }

      add_key_value_pair_from_model_to_variant (model, &builder, "last-modified-date");
      add_key_value_pair_from_model_to_variant (model, &builder, "thumbnail-uri");
      add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");

      /* Stop building object */
      g_variant_builder_close (&builder);
    }

  eks_discovery_feed_content_complete_article_card_descriptions (state->provider->content_skeleton,
                                                                 state->invocation,
                                                                 (const gchar * const *) shards_strv,
                                                                 g_variant_builder_end (&builder));
  g_slist_free_full (models, g_object_unref);
  g_slist_free_full (shards, g_object_unref);
  discovery_feed_query_state_free (state);
}

static gboolean
handle_content_article_card_descriptions (EksDiscoveryFeedDatabaseContentProvider *skeleton,
                                          GDBusMethodInvocation                  *invocation,
                                          gpointer                                user_data)
{
    EksDiscoveryFeedDatabaseContentProvider *self = user_data;

    if (!ensure_content_app_proxy (self))
      return TRUE;

    EkncEngine *engine = eknc_engine_get_default ();

    /* Build up tags_match_any */
    GVariantBuilder tags_match_any_builder;
    g_variant_builder_init (&tags_match_any_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_any_builder, "s", "EknArticleObject");
    GVariant *tags_match_any = g_variant_builder_end (&tags_match_any_builder);

    GVariantBuilder tags_match_all_builder;
    g_variant_builder_init (&tags_match_all_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_all_builder, "s", "EknHasDiscoveryFeedTitle");
    GVariant *tags_match_all = g_variant_builder_end (&tags_match_all_builder);

    /* Hold the application so that it doesn't go away whilst we're handling
     * the query */
    g_application_hold (g_application_get_default ());

    /* Create query and run it */
    query_with_wraparound_offset (engine,
                                  g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                "tags-match-any", tags_match_any,
                                                "sort", EKNC_QUERY_OBJECT_SORT_DATE,
                                                "order", EKNC_QUERY_OBJECT_ORDER_DESCENDING,
                                                "limit", NUMBER_OF_ARTICLES,
                                                "app-id", self->application_id,
                                                NULL),
                                  get_day_of_year (),
                                  DAYS_IN_YEAR,
                                  self->cancellable,
                                  content_article_card_descriptions_cb,
                                  discovery_feed_query_state_new (invocation, self));

    return TRUE;
}


static void
get_word_of_the_day_content_cb (GObject *source,
                                GAsyncResult *result,
                                gpointer user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  DiscoveryFeedQueryState *state = user_data;

  g_application_release (g_application_get_default ());

  GError *error = NULL;
  GSList *models = NULL;

  if (!models_for_result (engine,
                          state->provider->application_id,
                          result,
                          &models,
                          &error))
    {
      g_dbus_method_invocation_take_error (state->invocation, error);
      discovery_feed_query_state_free (state);
      return;
    }

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));

  EkncContentObjectModel *model = g_slist_nth (models, 0)->data;

  add_key_value_pair_from_model_to_variant (model, &builder, "word");
  add_key_value_pair_from_model_to_variant (model, &builder, "definition");
  add_key_value_pair_from_model_to_variant (model, &builder, "part-of-speech");
  add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");

  eks_discovery_feed_word_complete_get_word_of_the_day (state->provider->word_skeleton,
                                                        state->invocation,
                                                        g_variant_builder_end (&builder));
  g_slist_free_full (models, g_object_unref);
  discovery_feed_query_state_free (state);
}

static gboolean
handle_get_word_of_the_day (EksDiscoveryFeedDatabaseContentProvider *skeleton,
                            GDBusMethodInvocation                   *invocation,
                            gpointer                                 user_data)
{
    EksDiscoveryFeedDatabaseContentProvider *self = user_data;

    if (!ensure_word_app_proxy (self))
      return TRUE;

    EkncEngine *engine = eknc_engine_get_default ();

    /* Build up tags_match_any */
    GVariantBuilder tags_match_any_builder;
    g_variant_builder_init (&tags_match_any_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_any_builder, "s", "EknArticleObject");
    GVariant *tags_match_any = g_variant_builder_end (&tags_match_any_builder);

    /* Hold the application so that it doesn't go away whilst we're handling
     * the query */
    g_application_hold (g_application_get_default ());

    /* Create query and run it */
    query_with_wraparound_offset (engine,
                                  g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                "tags-match-any", tags_match_any,
                                                "limit", 1,
                                                "app-id", self->application_id,
                                                NULL),
                                  get_day_of_year (),
                                  DAYS_IN_YEAR,
                                  self->cancellable,
                                  get_word_of_the_day_content_cb,
                                  discovery_feed_query_state_new (invocation, self));

    return TRUE;
}

static void
get_quote_of_the_day_content_cb (GObject *source,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  DiscoveryFeedQueryState *state = user_data;

  g_application_release (g_application_get_default ());

  GError *error = NULL;
  GSList *models = NULL;

  if (!models_for_result (engine,
                          state->provider->application_id,
                          result,
                          &models,
                          &error))
    {
      g_dbus_method_invocation_take_error (state->invocation, error);
      discovery_feed_query_state_free (state);
      return;
    }

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));

  EkncContentObjectModel *model = g_slist_nth (models, 0)->data;

  add_key_value_pair_from_model_to_variant (model, &builder, "quote");
  add_author_from_model_to_variant (model, &builder, "author");
  add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");

  eks_discovery_feed_quote_complete_get_quote_of_the_day (state->provider->quote_skeleton,
                                                          state->invocation,
                                                          g_variant_builder_end (&builder));
  g_slist_free_full (models, g_object_unref);
  discovery_feed_query_state_free (state);
}

static gboolean
handle_get_quote_of_the_day (EksDiscoveryFeedDatabaseContentProvider *skeleton,
                             GDBusMethodInvocation                   *invocation,
                             gpointer                                 user_data)
{
    EksDiscoveryFeedDatabaseContentProvider *self = user_data;

    if (!ensure_quote_app_proxy (self))
      return TRUE;

    EkncEngine *engine = eknc_engine_get_default ();

    /* Build up tags_match_any */
    GVariantBuilder tags_match_any_builder;
    g_variant_builder_init (&tags_match_any_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_any_builder, "s", "EknArticleObject");
    GVariant *tags_match_any = g_variant_builder_end (&tags_match_any_builder);

    /* Hold the application so that it doesn't go away whilst we're handling
     * the query */
    g_application_hold (g_application_get_default ());

    /* Create query and run it */
    query_with_wraparound_offset (engine,
                                  g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                "tags-match-any", tags_match_any,
                                                "limit", 1,
                                                "app-id", self->application_id,
                                                NULL),
                                  get_day_of_year (),
                                  DAYS_IN_YEAR,
                                  self->cancellable,
                                  get_quote_of_the_day_content_cb,
                                  discovery_feed_query_state_new (invocation, self));

    return TRUE;
}

static void
recent_news_articles_cb (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  DiscoveryFeedQueryState *state = user_data;

  g_application_release (g_application_get_default ());

  GError *error = NULL;
  GSList *models = NULL;
  GSList *shards = NULL;

  if (!models_and_shards_for_result (engine,
                                     state->provider->application_id,
                                     result,
                                     &models,
                                     &shards,
                                     &error))
    {
      g_dbus_method_invocation_take_error (state->invocation, error);
      discovery_feed_query_state_free (state);

      /* No need to free_full the out models and shards here,
       * g_slist_copy_deep is not called if this function returns FALSE. */
      return;
    }

  g_auto(GStrv) shards_strv = strv_from_shard_list (shards);

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{ss}"));
  for (GSList *l = models; l; l = l->next)
    {
      /* Start building up object */
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));

      EkncContentObjectModel *model = l->data;

      add_key_value_pair_from_model_to_variant (model, &builder, "title");
      add_key_value_pair_from_model_to_variant (model, &builder, "synopsis");
      add_key_value_pair_from_model_to_variant (model, &builder, "last-modified-date");
      add_key_value_pair_from_model_to_variant (model, &builder, "thumbnail-uri");
      add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");

      /* Stop building object */
      g_variant_builder_close (&builder);
    }
  eks_discovery_feed_news_complete_get_recent_news (state->provider->news_skeleton,
                                                    state->invocation,
                                                    (const gchar * const *) shards_strv,
                                                    g_variant_builder_end (&builder));
  g_slist_free_full (models, g_object_unref);
  g_slist_free_full (shards, g_object_unref);
  discovery_feed_query_state_free (state);
}

static gboolean
handle_get_recent_news (EksDiscoveryFeedDatabaseContentProvider *skeleton,
                        GDBusMethodInvocation                  *invocation,
                        gpointer                                user_data)
{
    EksDiscoveryFeedDatabaseContentProvider *self = user_data;

    if (!ensure_news_app_proxy (self))
      return TRUE;

    EkncEngine *engine = eknc_engine_get_default ();

    /* Build up tags_match_any */
    GVariantBuilder tags_match_any_builder;
    g_variant_builder_init (&tags_match_any_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_any_builder, "s", "EknArticleObject");
    GVariant *tags_match_any = g_variant_builder_end (&tags_match_any_builder);

    /* Create query and run it */
    g_autoptr(EkncQueryObject) query = g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                     "tags-match-any", tags_match_any,
                                                     "sort", EKNC_QUERY_OBJECT_SORT_DATE,
                                                     "order", EKNC_QUERY_OBJECT_ORDER_DESCENDING,
                                                     "limit", NUMBER_OF_ARTICLES,
                                                     "app-id", self->application_id,
                                                     NULL);

    /* Hold the application so that it doesn't go away whilst we're handling
     * the query */
    g_application_hold (g_application_get_default ());

    eknc_engine_query (engine,
                       query,
                       self->cancellable,
                       recent_news_articles_cb,
                       discovery_feed_query_state_new (invocation, self));

    return TRUE;
}

static void
relevant_video_cb (GObject *source,
                   GAsyncResult *result,
                   gpointer user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  DiscoveryFeedQueryState *state = user_data;

  g_application_release (g_application_get_default ());

  GError *error = NULL;
  GSList *models = NULL;
  GSList *shards = NULL;

  if (!models_and_shards_for_result (engine,
                                     state->provider->application_id,
                                     result,
                                     &models,
                                     &shards,
                                     &error))
    {
      g_dbus_method_invocation_take_error (state->invocation, error);
      discovery_feed_query_state_free (state);

      /* No need to free_full the out models and shards here,
       * g_slist_copy_deep is not called if this function returns FALSE. */
      return;
    }

  g_auto(GStrv) shards_strv = strv_from_shard_list (shards);

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{ss}"));
  for (GSList *l = models; l; l = l->next)
    {
      if (!EKNC_IS_VIDEO_OBJECT_MODEL (l->data))
        continue;

      /* Start building up object */
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));

      EkncContentObjectModel *model = l->data;

      add_key_value_pair_from_model_to_variant (model, &builder, "title");
      add_key_value_int_to_str_pair_from_model_to_variant (model, &builder, "duration");
      add_key_value_pair_from_model_to_variant (model, &builder, "thumbnail-uri");
      add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");

      /* Stop building object */
      g_variant_builder_close (&builder);
    }
  eks_discovery_feed_video_complete_get_videos (state->provider->video_skeleton,
                                                state->invocation,
                                                (const gchar * const *) shards_strv,
                                                g_variant_builder_end (&builder));
  g_slist_free_full (models, g_object_unref);
  g_slist_free_full (shards, g_object_unref);
  discovery_feed_query_state_free (state);
}

static gboolean
handle_get_videos (EksDiscoveryFeedDatabaseContentProvider *skeleton,
                   GDBusMethodInvocation                  *invocation,
                   gpointer                                user_data)
{
    EksDiscoveryFeedDatabaseContentProvider *self = user_data;

    if (!ensure_video_app_proxy (self))
      return TRUE;

    EkncEngine *engine = eknc_engine_get_default ();

    /* Build up tags_match_any */
    GVariantBuilder tags_match_any_builder;
    g_variant_builder_init (&tags_match_any_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_any_builder, "s", "EknMediaObject");
    
    GVariant *tags_match_any = g_variant_builder_end (&tags_match_any_builder);

    /* Create query and run it */
    g_autoptr(EkncQueryObject) query = g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                     "tags-match-any", tags_match_any,
                                                     "sort", EKNC_QUERY_OBJECT_SORT_DATE,
                                                     "order", EKNC_QUERY_OBJECT_ORDER_DESCENDING,
                                                     "limit", NUMBER_OF_ARTICLES,
                                                     "app-id", self->application_id,
                                                     NULL);

    /* Hold the application so that it doesn't go away whilst we're handling
     * the query */
    g_application_hold (g_application_get_default ());

    eknc_engine_query (engine,
                       query,
                       self->cancellable,
                       relevant_video_cb,
                       discovery_feed_query_state_new (invocation, self));

    return TRUE;
}

static GDBusInterfaceSkeleton *
eks_discovery_feed_database_content_provider_skeleton_for_interface (EksProvider *provider,
                                                                     const gchar *interface)
{
  EksDiscoveryFeedDatabaseContentProvider *self = EKS_DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER (provider);

  if (g_strcmp0 (interface, "com.endlessm.DiscoveryFeedContent") == 0)
      return G_DBUS_INTERFACE_SKELETON (self->content_skeleton);
  else if (g_strcmp0 (interface, "com.endlessm.DiscoveryFeedQuote") == 0)
      return G_DBUS_INTERFACE_SKELETON(self->quote_skeleton);
  else if (g_strcmp0 (interface, "com.endlessm.DiscoveryFeedWord") == 0)
      return G_DBUS_INTERFACE_SKELETON(self->word_skeleton);
  else if (g_strcmp0 (interface, "com.endlessm.DiscoveryFeedNews") == 0)
      return G_DBUS_INTERFACE_SKELETON (self->news_skeleton);
  else if (g_strcmp0 (interface, "com.endlessm.DiscoveryFeedVideo") == 0)
      return G_DBUS_INTERFACE_SKELETON (self->video_skeleton);
  else if (g_strcmp0 (interface, "com.endlessm.DiscoveryFeedArtwork") == 0)
      return G_DBUS_INTERFACE_SKELETON (self->artwork_skeleton);

  g_assert_not_reached ();
  return NULL;
}

static void
eks_discovery_feed_database_content_provider_interface_init (EksProviderInterface *iface)
{
  iface->skeleton_for_interface = eks_discovery_feed_database_content_provider_skeleton_for_interface;
}

static void
eks_discovery_feed_database_content_provider_init (EksDiscoveryFeedDatabaseContentProvider *self)
{
  self->content_skeleton = eks_discovery_feed_content_skeleton_new ();
  g_signal_connect (self->content_skeleton, "handle-article-card-descriptions",
                    G_CALLBACK (handle_content_article_card_descriptions), self);

  self->quote_skeleton = eks_discovery_feed_quote_skeleton_new ();
  g_signal_connect (self->quote_skeleton, "handle-get-quote-of-the-day",
                    G_CALLBACK (handle_get_quote_of_the_day), self);

  self->word_skeleton = eks_discovery_feed_word_skeleton_new ();
  g_signal_connect (self->word_skeleton, "handle-get-word-of-the-day",
                    G_CALLBACK (handle_get_word_of_the_day), self);

  self->news_skeleton = eks_discovery_feed_news_skeleton_new ();
  g_signal_connect (self->news_skeleton, "handle-get-recent-news",
                    G_CALLBACK (handle_get_recent_news), self);

  self->video_skeleton = eks_discovery_feed_video_skeleton_new ();
  g_signal_connect (self->video_skeleton, "handle-get-videos",
                    G_CALLBACK (handle_get_videos), self);

  self->artwork_skeleton = eks_discovery_feed_artwork_skeleton_new ();
  g_signal_connect (self->artwork_skeleton, "handle-artwork-card-descriptions",
                    G_CALLBACK (handle_artwork_card_descriptions), self);
}
