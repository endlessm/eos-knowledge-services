/* Copyright 2016 Endless Mobile, Inc. */

#include "eks-discovery-feed-provider.h"
#include "eks-provider-iface.h"

#include "eks-knowledge-app-dbus.h"
#include "eks-discovery-feed-provider-dbus.h"
#include "eks-query-util.h"

#include <eos-knowledge-content.h>
#include <eos-shard/eos-shard-shard-file.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUMBER_OF_ARTICLES 5
#define DAYS_IN_YEAR 365
// In SDK2, the default limit is 0 in SDK3, the default limit is all matches
#define SENSIBLE_QUERY_LIMIT 500

struct _EksDiscoveryFeedProvider
{
  GObject parent_instance;

  gchar *application_id;
  EksDiscoveryFeedContent *content_skeleton;
  EksDiscoveryFeedQuote *quote_skeleton;
  EksDiscoveryFeedWord *word_skeleton;
  EksDiscoveryFeedNews *news_skeleton;
  EksDiscoveryFeedVideo *video_skeleton;
  EksDiscoveryFeedArtwork *artwork_skeleton;
  GCancellable *cancellable;
};

static void eks_discovery_feed_provider_interface_init (EksProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EksDiscoveryFeedProvider,
                         eks_discovery_feed_provider,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EKS_TYPE_PROVIDER,
                                                eks_discovery_feed_provider_interface_init))

enum {
  PROP_0,
  PROP_APPLICATION_ID,
  NPROPS
};

static GParamSpec *eks_discovery_feed_provider_props [NPROPS] = { NULL, };

static void
eks_discovery_feed_provider_get_property (GObject    *object,
                                          unsigned    prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  EksDiscoveryFeedProvider *self = EKS_DISCOVERY_FEED_PROVIDER (object);

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
eks_discovery_feed_provider_set_property (GObject      *object,
                                          unsigned      prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  EksDiscoveryFeedProvider *self = EKS_DISCOVERY_FEED_PROVIDER (object);

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
eks_discovery_feed_provider_finalize (GObject *object)
{
  EksDiscoveryFeedProvider *self = EKS_DISCOVERY_FEED_PROVIDER (object);

  g_clear_pointer (&self->application_id, g_free);
  g_clear_object (&self->content_skeleton);
  g_clear_object (&self->quote_skeleton);
  g_clear_object (&self->word_skeleton);
  g_clear_object (&self->news_skeleton);
  g_clear_object (&self->video_skeleton);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (eks_discovery_feed_provider_parent_class)->finalize (object);
}

static void
eks_discovery_feed_provider_class_init (EksDiscoveryFeedProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = eks_discovery_feed_provider_get_property;
  object_class->set_property = eks_discovery_feed_provider_set_property;
  object_class->finalize = eks_discovery_feed_provider_finalize;

  eks_discovery_feed_provider_props[PROP_APPLICATION_ID] =
    g_param_spec_string ("application-id", "Application Id", "Application Id",
      "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     NPROPS,
                                     eks_discovery_feed_provider_props);
}

typedef struct {
  GDBusMethodInvocation *invocation;
  EksDiscoveryFeedProvider *provider;
} DiscoveryFeedQueryState;

static DiscoveryFeedQueryState *
discovery_feed_query_state_new (GDBusMethodInvocation                  *invocation,
                                EksDiscoveryFeedProvider *provider)
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
add_author_from_model_to_variant (EkncArticleObjectModel *model,
                                  GVariantBuilder        *builder,
                                  const char             *key)
{
  char * const *authors = eknc_article_object_model_get_authors (model);
  add_key_value_pair_to_variant (builder, key, authors ? authors[0] : "");
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

static const char *
select_string_from_array_from_day (JsonArray *array)
{
  guint size = json_array_get_length (array);
  if (size == 0)
    return NULL;

  int ix = get_day_of_week () % size;
  return json_array_get_string_element (array, ix);
}

typedef enum {
  DISCOVERY_FEED_NO_CUSTOM_PROPS = 0,
  DISCOVERY_FEED_SET_CUSTOM_TITLE = 1 << 0
} DiscoveryFeedCustomProps;

typedef struct _QueryPendingUpperBound {
  EkncQueryObject       *query;
  guint                 offset_within_upper_bound;
  guint                 wraparound_upper_bound;
  GCancellable          *cancellable;
  GDBusMethodInvocation *invocation;
  GAsyncReadyCallback   main_query_ready_callback;
  gpointer              main_query_ready_data;
  GDestroyNotify        main_query_ready_destroy;
} QueryPendingUpperBound;

static QueryPendingUpperBound *
query_pending_upper_bound_new (EkncQueryObject       *query,
                               guint                  offset_within_upper_bound,
                               guint                  wraparound_upper_bound,
                               GDBusMethodInvocation *invocation,
                               GCancellable          *cancellable,
                               GAsyncReadyCallback    main_query_ready_callback,
                               gpointer               main_query_ready_data,
                               GDestroyNotify         main_query_ready_destroy)
{
  QueryPendingUpperBound *data = g_new0 (QueryPendingUpperBound, 1);
  data->query = g_object_ref (query);
  data->offset_within_upper_bound = offset_within_upper_bound;
  data->wraparound_upper_bound = wraparound_upper_bound;

  /* Keep cancellable alive if we got one, otherwise ignore it */
  data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  data->invocation = g_object_ref (invocation);
  data->main_query_ready_callback = main_query_ready_callback;
  data->main_query_ready_data = main_query_ready_data;
  data->main_query_ready_destroy = main_query_ready_destroy;

  return data;
}

static void
query_pending_upper_bound_free (QueryPendingUpperBound *data)
{
  g_object_unref (data->query);
  g_clear_object (&data->cancellable);
  g_object_unref (data->invocation);
  g_clear_pointer (&data->main_query_ready_data, data->main_query_ready_destroy);

  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (QueryPendingUpperBound, query_pending_upper_bound_free)

static void
on_received_upper_bound_result (GObject      *source,
                                GAsyncResult *result,
                                gpointer     user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  g_autoptr(QueryPendingUpperBound) pending = user_data;
  g_autoptr(GError) error = NULL;

  g_autoptr(EkncQueryResults) results = eknc_engine_query_finish (engine, result, &error);

  if (error != NULL)
    {
      g_warning ("Unable to get upper bound on results, aborting query: %s",
                 error->message);
      g_dbus_method_invocation_take_error (pending->invocation,
                                           g_steal_pointer (&error));

      /* No need to free_full the out models and shards here,
       * g_slist_copy_deep is not called if this function returns early. */
      return;
    }

  /* Now that we have results, we can read the upper bound and
   * determine the actual offset */
  guint intended_limit;
  g_object_get (pending->query, "limit", &intended_limit, NULL);
  gint upper_bound = eknc_query_results_get_upper_bound (results);
  guint offset = pending->offset_within_upper_bound % (MIN (upper_bound,
                                                            pending->wraparound_upper_bound) -
                                                       intended_limit);

  /* Get rid of the old query and construct a new one in its place */
  EkncQueryObject *query = eknc_query_object_new_from_object (pending->query,
                                                              "offset", offset,
                                                              NULL);
  g_set_object (&pending->query, query);

  /* Okay, now fire off the *actual* query, passing the user data
   * and callback that we were going to pass the first time */
  eknc_engine_query (engine,
                     pending->query,
                     pending->cancellable,
                     pending->main_query_ready_callback,
                     g_steal_pointer (&pending->main_query_ready_data));
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
static void
query_with_wraparound_offset (EkncEngine            *engine,
                              EkncQueryObject       *query,
                              guint                  offset_within_upper_bound,
                              guint                  wraparound_upper_bound,
                              GDBusMethodInvocation *invocation,
                              GCancellable          *cancellable,
                              GAsyncReadyCallback    main_query_ready_callback,
                              gpointer               main_query_ready_data,
                              GDestroyNotify         main_query_ready_destroy)
{
  /* Override the limit, setting it to one. In the returned query we'll get
   * nothing back, but Xapian will tell us how many models matched our query
   * which we'll use later. We have to ask for at least one article
   * here, otherwise we trigger assertions in knowledge-lib. */
  g_autoptr (EkncQueryObject) truncated_query = eknc_query_object_new_from_object (query,
                                                                                   "limit", 1,
                                                                                   NULL);

  /* Dispatch the query, when it comes back we'll know what to
   * set the offset to */
  eknc_engine_query (engine,
                     truncated_query,
                     cancellable,
                     on_received_upper_bound_result,
                     query_pending_upper_bound_new (query,
                                                    offset_within_upper_bound,
                                                    wraparound_upper_bound,
                                                    invocation,
                                                    cancellable,
                                                    main_query_ready_callback,
                                                    main_query_ready_data,
                                                    main_query_ready_destroy));
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
                                     NULL,
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

      add_key_value_pair_from_model_to_variant (model, &builder, "title");
      add_key_value_pair_from_model_to_variant (model, &builder, "synopsis");
      add_key_value_pair_from_model_to_variant (model, &builder, "last-modified-date");
      add_key_value_pair_from_model_to_variant (model, &builder, "thumbnail-uri");
      add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");
      add_author_from_model_to_variant (EKNC_ARTICLE_OBJECT_MODEL (model),
                                        &builder, "author");

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
handle_artwork_card_descriptions (EksDiscoveryFeedProvider *skeleton,
                                  GDBusMethodInvocation    *invocation,
                                  gpointer                  user_data)
{
    EksDiscoveryFeedProvider *self = user_data;
    EkncEngine *engine = eknc_engine_get_default ();
    const char *tags_match_any[] = { "EknArticleObject", NULL };

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
                                  invocation,
                                  self->cancellable,
                                  artwork_card_descriptions_cb,
                                  discovery_feed_query_state_new (invocation, self),
                                  (GDestroyNotify) discovery_feed_query_state_free);

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
                                     NULL,
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

      /* Examine the discovery-feed-content string first and set flags
       * for things that we've overridden */
      g_autoptr(JsonObject) discovery_feed_content = NULL;
      g_object_get (model,
                    "discovery-feed-content", &discovery_feed_content,
                    NULL);

      if (discovery_feed_content != NULL &&
          json_object_has_member (discovery_feed_content, "blurbs"))
        {
          JsonArray *blurbs = json_object_get_array_member (discovery_feed_content,
                                                            "blurbs");
          const char *title = select_string_from_array_from_day (blurbs);

          if (title)
            {
              add_key_value_pair_to_variant (&builder, "title", title);
              add_key_value_pair_to_variant (&builder, "synopsis", "");
              flags |= DISCOVERY_FEED_SET_CUSTOM_TITLE;
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
handle_content_article_card_descriptions (EksDiscoveryFeedProvider *skeleton,
                                          GDBusMethodInvocation    *invocation,
                                          gpointer                  user_data)
{
    EksDiscoveryFeedProvider *self = user_data;
    EkncEngine *engine = eknc_engine_get_default ();
    const char *tags_match_any[] = { "EknArticleObject", NULL };

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
                                  invocation,
                                  self->cancellable,
                                  content_article_card_descriptions_cb,
                                  discovery_feed_query_state_new (invocation, self),
                                  (GDestroyNotify) discovery_feed_query_state_free);

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
                          NULL,
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
handle_get_word_of_the_day (EksDiscoveryFeedProvider *skeleton,
                            GDBusMethodInvocation    *invocation,
                            gpointer                  user_data)
{
    EksDiscoveryFeedProvider *self = user_data;
    EkncEngine *engine = eknc_engine_get_default ();
    const char *tags_match_any[] = { "EknArticleObject", NULL };

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
                                  invocation,
                                  self->cancellable,
                                  get_word_of_the_day_content_cb,
                                  discovery_feed_query_state_new (invocation, self),
                                  (GDestroyNotify) discovery_feed_query_state_free);

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
                          NULL,
                          &error))
    {
      g_dbus_method_invocation_take_error (state->invocation, error);
      discovery_feed_query_state_free (state);
      return;
    }

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));

  EkncContentObjectModel *model = g_slist_nth (models, 0)->data;

  add_key_value_pair_from_model_to_variant (model, &builder, "title");
  add_author_from_model_to_variant (EKNC_ARTICLE_OBJECT_MODEL (model), &builder,
                                    "author");
  add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");

  eks_discovery_feed_quote_complete_get_quote_of_the_day (state->provider->quote_skeleton,
                                                          state->invocation,
                                                          g_variant_builder_end (&builder));
  g_slist_free_full (models, g_object_unref);
  discovery_feed_query_state_free (state);
}

static gboolean
handle_get_quote_of_the_day (EksDiscoveryFeedProvider *skeleton,
                             GDBusMethodInvocation    *invocation,
                             gpointer                  user_data)
{
    EksDiscoveryFeedProvider *self = user_data;
    EkncEngine *engine = eknc_engine_get_default ();
    const char *tags_match_any[] = { "EknArticleObject", NULL };

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
                                  invocation,
                                  self->cancellable,
                                  get_quote_of_the_day_content_cb,
                                  discovery_feed_query_state_new (invocation, self),
                                  (GDestroyNotify) discovery_feed_query_state_free);

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
                                     NULL,
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
handle_get_recent_news (EksDiscoveryFeedProvider *skeleton,
                        GDBusMethodInvocation    *invocation,
                        gpointer                  user_data)
{
    EksDiscoveryFeedProvider *self = user_data;
    EkncEngine *engine = eknc_engine_get_default ();
    const char *tags_match_any[] = { "EknArticleObject", NULL };

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
                                     NULL,
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
  gint videos_found = 0;
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

      videos_found += 1;
      if (videos_found == NUMBER_OF_ARTICLES)
        break;
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
handle_get_videos (EksDiscoveryFeedProvider *skeleton,
                   GDBusMethodInvocation    *invocation,
                   gpointer                  user_data)
{
    EksDiscoveryFeedProvider *self = user_data;
    EkncEngine *engine = eknc_engine_get_default ();
    const char *tags_match_any[] = { "EknMediaObject", NULL };

    /* Create query and run it */
    g_autoptr(EkncQueryObject) query = g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                     "tags-match-any", tags_match_any,
                                                     "sort", EKNC_QUERY_OBJECT_SORT_DATE,
                                                     "order", EKNC_QUERY_OBJECT_ORDER_DESCENDING,
                                                     "limit", SENSIBLE_QUERY_LIMIT,
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
eks_discovery_feed_provider_skeleton_for_interface (EksProvider *provider,
                                                    const char  *interface)
{
  EksDiscoveryFeedProvider *self = EKS_DISCOVERY_FEED_PROVIDER (provider);

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
eks_discovery_feed_provider_interface_init (EksProviderInterface *iface)
{
  iface->skeleton_for_interface = eks_discovery_feed_provider_skeleton_for_interface;
}

static void
eks_discovery_feed_provider_init (EksDiscoveryFeedProvider *self)
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
