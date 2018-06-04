/* Copyright 2018 Endless Mobile, Inc. */

#include "eknc-enums.h"

#include "eks-errors.h"
#include "eks-provider-iface.h"
#include "eks-query-util.h"
#include "eks-shards-util.h"

#include "eks-knowledge-app-dbus.h"
#include "eks-metadata-provider.h"
#include "eks-metadata-provider-dbus.h"

#include <eos-knowledge-content.h>

#include <eos-shard/eos-shard-shard-file.h>

#include <json-glib/json-glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _EksMetadataProvider
{
  GObject parent_instance;

  char *application_id;
  EksContentMetadata *skeleton;
  GHashTable *translation_infos;
};

static void eks_metadata_provider_interface_init (EksProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EksMetadataProvider,
                         eks_metadata_provider,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EKS_TYPE_PROVIDER,
                                                eks_metadata_provider_interface_init))

enum {
  PROP_0,
  PROP_APPLICATION_ID,
  NPROPS
};

static GParamSpec *eks_metadata_provider_props [NPROPS] = { NULL, };

static void
eks_metadata_provider_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EksMetadataProvider *self = EKS_METADATA_PROVIDER (object);

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
eks_metadata_provider_set_property (GObject      *object,
                                    unsigned      prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EksMetadataProvider *self = EKS_METADATA_PROVIDER (object);

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
eks_metadata_provider_finalize (GObject *object)
{
  EksMetadataProvider *self = EKS_METADATA_PROVIDER (object);

  g_clear_pointer (&self->application_id, g_free);
  g_clear_object (&self->skeleton);
  g_clear_pointer (&self->translation_infos, g_hash_table_unref);

  G_OBJECT_CLASS (eks_metadata_provider_parent_class)->finalize (object);
}

static void
eks_metadata_provider_class_init (EksMetadataProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = eks_metadata_provider_get_property;
  object_class->set_property = eks_metadata_provider_set_property;
  object_class->finalize = eks_metadata_provider_finalize;

  eks_metadata_provider_props[PROP_APPLICATION_ID] =
    g_param_spec_string ("application-id", "Application Id", "Application Id",
      "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     NPROPS,
                                     eks_metadata_provider_props);
}

typedef struct _MetadataQueryState {
  EksMetadataProvider   *provider;
  GDBusMethodInvocation *invocation;
} MetadataQueryState;

static MetadataQueryState *
metadata_query_state_new (EksMetadataProvider   *provider,
                          GDBusMethodInvocation *invocation)
{
  MetadataQueryState *state = g_new0 (MetadataQueryState, 1);
  state->provider = provider;
  state->invocation = g_object_ref (invocation);

  return state;
}

static void
metadata_query_state_free (MetadataQueryState *state)
{
  g_clear_object (&state->invocation);

  g_free (state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MetadataQueryState,
                               metadata_query_state_free)

static void
add_key_value_pair_to_variant (GVariantBuilder *builder,
                               const char      *key,
                               GVariant        *value)
{
  g_variant_builder_open (builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (builder, "s", key);
  g_variant_builder_add (builder, "v", value);
  g_variant_builder_close (builder);
}

static gboolean
gvalue_to_variant_internal (GValue              *value,
                            const GVariantType  *expected_type,
                            GVariant           **out_variant,
                            GError             **error)
{
  g_return_val_if_fail (out_variant != NULL, FALSE);

  /* Special case: if expected_type is G_VARIANT_TYPE_VARDICT then we need
   * to dup the variant directly. */
  if (expected_type == G_VARIANT_TYPE_VARDICT)
    {
      *out_variant = g_value_dup_variant (value);
      return TRUE;
    }

  *out_variant = g_dbus_gvalue_to_gvariant (value, expected_type);
  return *out_variant != NULL;
}

static gboolean
maybe_add_key_value_pair_from_model_to_variant (EkncContentObjectModel  *model,
                                                GVariantBuilder         *builder,
                                                const char              *property_name,
                                                const char              *model_key,
                                                const GVariantType      *expected_type,
                                                GError                 **error)
{
  g_auto(GValue) value = G_VALUE_INIT;
  GParamSpec *pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (model),
                                                    property_name);
  g_autoptr(GVariant) converted = NULL;

  if (pspec == NULL)
    return TRUE;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (G_OBJECT (model), property_name, &value);

  /* Special case for SDK1: If the expected type is G_VARIANT_TYPE_STRING_ARRAY
   * we need to read for a G_VALUE_TYPE_VARIANT and then deserialize as opposed
   * to trying to read from a GStrv */
  if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_STRING_ARRAY))
    converted = g_value_dup_variant (&value);
  else
    {
      if (!gvalue_to_variant_internal (&value, expected_type, &converted, error))
        return FALSE;
    }

  /* If we got NULL here it just means that the source property was NULL,
   * so don't add it. */
  if (converted == NULL)
    return TRUE;

  /* Need to use a different name here as the name between the interface
   * and the internal model key can vary */
  add_key_value_pair_to_variant (builder, model_key, g_steal_pointer (&converted));
  return TRUE;
}

typedef struct _ModelVariantTypes {
  const gchar        *prop_name;
  const gchar        *model_prop_name;
  const GVariantType *variant_type;
} ModelVariantTypes;

static const ModelVariantTypes model_variant_types[] = {
  { "child_tags", "child_tags", G_VARIANT_TYPE_STRING_ARRAY },
  { "content_type", "content_type", G_VARIANT_TYPE_STRING },
  { "copyright_holder", "copyright_holder", G_VARIANT_TYPE_STRING },
  { "discovery_feed_content", "discovery_feed_content", G_VARIANT_TYPE_VARDICT },
  { "ekn_id", "id", G_VARIANT_TYPE_STRING },
  { "language", "language", G_VARIANT_TYPE_STRING },
  { "last_modified_date", "last_modified_date", G_VARIANT_TYPE_STRING },
  { "license", "license", G_VARIANT_TYPE_STRING },
  { "original_title", "original_title", G_VARIANT_TYPE_STRING },
  { "original_uri", "original_uri", G_VARIANT_TYPE_STRING },
  { "tags", "tags", G_VARIANT_TYPE_STRING_ARRAY },
  { "title", "title", G_VARIANT_TYPE_STRING },
  { "thumbnail_uri", "thumbnail_uri", G_VARIANT_TYPE_STRING }
};
static const gsize model_variant_types_n = G_N_ELEMENTS (model_variant_types);

static GVariant *
build_models_variants (GSList  *models,
                       GError **error)
{
  g_auto(GVariantBuilder) builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (GSList *l = models; l; l = l->next)
    {
      EkncContentObjectModel *model = l->data;
      gsize i = 0;

      g_variant_builder_open (&builder, G_VARIANT_TYPE_VARDICT);

      for (; i < model_variant_types_n; ++i)
        {
          const ModelVariantTypes *model_prop = &model_variant_types[i];

          if (!maybe_add_key_value_pair_from_model_to_variant (model,
                                                               &builder,
                                                               model_prop->prop_name,
                                                               model_prop->model_prop_name,
                                                               model_prop->variant_type,
                                                               error))
            return NULL;
        }

      g_variant_builder_close (&builder);
    }

  return g_variant_builder_end (&builder);
}

static void
on_received_query_results (GObject      *source,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  g_autoptr(MetadataQueryState) state = user_data;

  /* Careful here, these two need to be cleaned up manually */
  GSList *models = NULL;
  GSList *shards = NULL;

  g_auto(GStrv) shards_strv = NULL;
  GVariant *results_tuple_array[1];
  g_autoptr(GVariant) models_variant = NULL;
  g_auto(GVariantDict) result_metadata;
  gint upper_bound = 0;
  g_autoptr(GError) error = NULL;

  /* Make sure to init the vardict first before any return path
   * otherwise g_auto will attempt to clear uninitialized memory */
  g_variant_dict_init (&result_metadata, NULL);

  g_application_release (g_application_get_default ());

  if (!models_and_shards_for_result (engine,
                                     state->provider->application_id,
                                     result,
                                     &models,
                                     &shards,
                                     &upper_bound,
                                     &error))
    {
      g_dbus_method_invocation_take_error (state->invocation,
                                           eks_map_error_to_eks_error (error));
      g_slist_free_full (models, g_object_unref);
      g_slist_free_full (shards, g_object_unref);
      return;
    }

  shards_strv = strv_from_shard_list (shards);
  models_variant = build_models_variants (models, &error);

  if (models_variant == NULL)
    {
      g_dbus_method_invocation_take_error (state->invocation,
                                           eks_map_error_to_eks_error (error));
      g_slist_free_full (models, g_object_unref);
      g_slist_free_full (shards, g_object_unref);
      return;
    }

  g_variant_dict_insert (&result_metadata, "upper_bound", "i", upper_bound);

  /* Easier than using GVariantBuilder. Note that if a child
   * has a floating reference the container takes ownership of
   * them via g_variant_ref_sink, so we need to steal the pointer */
  results_tuple_array[0] = g_variant_new ("(@a{sv}@aa{sv})",
                                          g_variant_dict_end (&result_metadata),
                                          g_steal_pointer (&models_variant));

  eks_content_metadata_complete_query (state->provider->skeleton,
                                       state->invocation,
                                       (const char * const *) shards_strv,
                                       g_variant_new_array (G_VARIANT_TYPE ("(a{sv}aa{sv})"),
                                                            results_tuple_array,
                                                            1));

  g_slist_free_full (models, g_object_unref);
  g_slist_free_full (shards, g_object_unref);
}

static void
append_construction_prop_from_string (const char *key,
                                      const char *str,
                                      GArray     *parameters_array)
{
  guint index = parameters_array->len;
  GParameter *parameter = NULL;
  GValue *value = NULL;

  /* Need to be careful to set parameter and value *after*
   * the array has been resized */
  g_array_set_size (parameters_array, parameters_array->len + 1);
  parameter = &(g_array_index (parameters_array, GParameter, index));
  value = &parameter->value;

  g_value_init (value, G_TYPE_STRING);
  g_value_set_string (value, str);

  /* Copying even though the member is const */
  parameter->name = g_strdup (key);
}

typedef gboolean (*VariantToValueTransformFunc) (GVariant  *variant,
                                                 GValue    *value,
                                                 gpointer   user_data,
                                                 GError   **error);

static gboolean
translate_gvariant_to_gvalue (GVariant  *variant,
                              GValue    *value,
                              gpointer   user_data,
                              GError   **error)
{
  g_dbus_gvariant_to_gvalue (variant, value);
  return TRUE;
}

static gboolean
translate_gvariant_to_gvalue_passthrough (GVariant  *variant,
                                          GValue    *value,
                                          gpointer   user_data,
                                          GError   **error)
{
  g_value_init (value, G_TYPE_VARIANT);
  g_value_set_variant (value, variant);
  return TRUE;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GEnumClass, g_type_class_unref)

static gboolean
translate_gvariant_to_gvalue_parse_enum (GVariant  *variant,
                                         GValue    *value,
                                         gpointer   user_data,
                                         GError   **error)
{
  GType enum_type = *((GType *) (user_data));
  const char *str = g_variant_get_string (variant, NULL);
  g_autoptr(GEnumClass) klass = g_type_class_ref (enum_type);
  GEnumValue *enum_value = g_enum_get_value_by_nick (klass, str);

  if (enum_value == NULL)
    {
      g_set_error (error,
                   EKS_ERROR,
                   EKS_ERROR_INVALID_REQUEST,
                   "Couldn't translate value '%s' to a valid enum",
                   str);
      return FALSE;
    }

  g_value_init (value, enum_type);
  g_value_set_enum (value, enum_value->value);
  return TRUE;
}

static gboolean
append_construction_prop_from_variant (const char                   *key,
                                       GVariant                     *variant,
                                       GArray                       *parameters_array,
                                       VariantToValueTransformFunc   transform,
                                       gpointer                      user_data,
                                       GError                      **error)
{
  guint index = parameters_array->len;
  GParameter *parameter = NULL;
  GValue *value = NULL;

  /* Need to be careful to set parameter *after* the array has been resized */
  g_array_set_size (parameters_array, parameters_array->len + 1);
  parameter = &g_array_index (parameters_array, GParameter, index);
  value = &parameter->value;

  /* If we return false from here, this should be treated as an
   * unrecoverable error for the call */
  if (!transform (variant, value, user_data, error))
    return FALSE;

  /* Copying into name here even though the name is const */
  parameter->name = g_strdup (key);

  return TRUE;
}

static gboolean
append_construction_prop_from_variant_dbus_transform (const char  *key,
                                                      GVariant    *variant,
                                                      GArray      *parameters_array,
                                                      gpointer     extra_data,
                                                      GError     **error)
{
  return append_construction_prop_from_variant (key,
                                                variant,
                                                parameters_array,
                                                translate_gvariant_to_gvalue,
                                                extra_data,
                                                error);
}

static gboolean
append_query_construction_prop_from_variant_dbus_transform (const char  *key,
                                                            GVariant    *variant,
                                                            GArray      *parameters_array,
                                                            gpointer     extra_data,
                                                            GError     **error)
{
  /* We need to use a custom transform here that hardcodes "query" as the
   * key name that is going to be used to construct the GObject, even though
   * the key we receive over DBus is called "search-terms".
   * EkncQueryObject changed this API in later versions.
   */
  return append_construction_prop_from_variant ("query",
                                                variant,
                                                parameters_array,
                                                translate_gvariant_to_gvalue,
                                                extra_data,
                                                error);
}

static gboolean
append_construction_prop_from_variant_enum_transform (const char  *key,
                                                      GVariant    *variant,
                                                      GArray      *parameters_array,
                                                      gpointer     extra_data,
                                                      GError     **error)
{
  return append_construction_prop_from_variant (key,
                                                variant,
                                                parameters_array,
                                                translate_gvariant_to_gvalue_parse_enum,
                                                extra_data,
                                                error);
}

static gboolean
append_construction_prop_from_variant_passthrough_transform (const char  *key,
                                                             GVariant    *variant,
                                                             GArray      *parameters_array,
                                                             gpointer     extra_data,
                                                             GError     **error)
{
  return append_construction_prop_from_variant (key,
                                                variant,
                                                parameters_array,
                                                translate_gvariant_to_gvalue_passthrough,
                                                extra_data,
                                                error);
}

typedef gboolean (*AppendConstructionPropFromVariantWithTransformFunc) (const char  *key,
                                                                        GVariant    *variant,
                                                                        GArray      *parameters_array,
                                                                        gpointer     extra_data,
                                                                        GError     **error);

typedef struct _ValueTranslationInfo {
  AppendConstructionPropFromVariantWithTransformFunc append_func;
  gpointer                                           user_data;
  GDestroyNotify                                     user_data_destroy;
} ValueTranslationInfo;

static ValueTranslationInfo *
value_translation_info_new (AppendConstructionPropFromVariantWithTransformFunc append_func,
                            gpointer                                           user_data,
                            GDestroyNotify                                     user_data_destroy)
{
  ValueTranslationInfo *info = g_new0 (ValueTranslationInfo, 1);

  info->append_func = append_func;
  info->user_data = user_data;
  info->user_data_destroy = user_data_destroy;

  return info;
}

static void
value_translation_info_free (ValueTranslationInfo *info)
{
  if (info == NULL)
    return;

  if (info->user_data_destroy)
    g_clear_pointer (&info->user_data, info->user_data_destroy);

  g_free (info);
}

/* Note that since GType can be 64-bits, we need to allocate
 * in order to pass it as user_data. */
static GType *
alloc_for_gtype (GType type)
{
  GType *typep = g_new0 (GType, 1);
  *typep = type;

  return typep;
}

static GHashTable *
article_metadata_query_construction_props_translation_table (void)
{
  g_autoptr(GHashTable) table = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       (GDestroyNotify) value_translation_info_free);

  g_hash_table_insert (table,
                       g_strdup ("search-terms"),
                       value_translation_info_new (append_query_construction_prop_from_variant_dbus_transform,
                                                   NULL,
                                                   NULL));
  g_hash_table_insert (table,
                       g_strdup ("tags-match-any"),
                       value_translation_info_new (append_construction_prop_from_variant_passthrough_transform,
                                                   NULL,
                                                   NULL));
  g_hash_table_insert (table,
                       g_strdup ("tags-match-all"),
                       value_translation_info_new (append_construction_prop_from_variant_passthrough_transform,
                                                   NULL,
                                                   NULL));
  g_hash_table_insert (table,
                       g_strdup ("limit"),
                       value_translation_info_new (append_construction_prop_from_variant_dbus_transform,
                                                   NULL,
                                                   NULL));
  g_hash_table_insert (table,
                       g_strdup ("offset"),
                       value_translation_info_new (append_construction_prop_from_variant_dbus_transform,
                                                   NULL,
                                                   NULL));
  g_hash_table_insert (table,
                       g_strdup ("sort"),
                       value_translation_info_new (append_construction_prop_from_variant_enum_transform,
                                                   alloc_for_gtype (EKNC_TYPE_QUERY_OBJECT_SORT),
                                                   g_free));
  g_hash_table_insert (table,
                       g_strdup ("order"),
                       value_translation_info_new (append_construction_prop_from_variant_enum_transform,
                                                   alloc_for_gtype (EKNC_TYPE_QUERY_OBJECT_ORDER),
                                                   g_free));

  return g_steal_pointer (&table);
}

static void
clear_gparameter_with_allocated_name (GParameter *parameter)
{
  g_clear_pointer (&parameter->name, g_free);
  g_value_unset (&parameter->value);
}

static EkncQueryObject *
create_query_from_dbus_query_parameters (GVariant     *query_parameters,
                                         const char   *application_id,
                                         GHashTable   *translation_infos,
                                         GError      **error)
{
  GVariantIter iter;
  char *iter_key;
  GVariant *iter_value;

  g_autoptr(GArray) parameters_array = g_array_new (FALSE, TRUE, sizeof (GParameter));
  g_array_set_clear_func (parameters_array, (GDestroyNotify) clear_gparameter_with_allocated_name);

  /* Always add the app-id to the query */
  append_construction_prop_from_string ("app-id",
                                        application_id,
                                        parameters_array);

  g_variant_iter_init (&iter, query_parameters);
  while (g_variant_iter_next (&iter, "{sv}", &iter_key, &iter_value))
    {
      g_autofree char *key = iter_key;
      g_autoptr(GVariant) variant = iter_value;
      ValueTranslationInfo *translation_info = g_hash_table_lookup (translation_infos,
                                                                    key);
      
      if (translation_info == NULL)
        {
          g_set_error (error,
                       EKS_ERROR,
                       EKS_ERROR_INVALID_REQUEST,
                       "Invalid query parameter: %s",
                       key);
          return NULL;
        }

      if (!translation_info->append_func (key,
                                          variant,
                                          parameters_array,
                                          translation_info->user_data,
                                          error))
        return NULL;
    }

  return EKNC_QUERY_OBJECT (g_object_newv (EKNC_TYPE_QUERY_OBJECT,
                                           parameters_array->len,
                                           (GParameter *) parameters_array->data));
}

static gboolean
handle_query (EksContentMetadata    *skeleton,
              GDBusMethodInvocation *invocation,
              GVariant              *queries,
              gpointer               user_data)
{
  EksMetadataProvider *self = user_data;
  EkncEngine *engine = eknc_engine_get_default ();
  g_autoptr(GError) local_error = NULL;
  g_autoptr(EkncQueryObject) query = NULL;
  g_autoptr(GVariant) first_child = NULL;
  guint n_children = g_variant_n_children (queries);

  /* XXX: For now, we only support one query in the array, but the
   * interface contract may be extended to support multiple queries. */
  if (n_children != 1)
    {
      g_dbus_method_invocation_take_error (invocation,
                                           g_error_new (G_IO_ERROR,
                                                        G_IO_ERROR_FAILED,
                                                        "This version of eks-search-provider "
                                                        "can only perform one query, not %u",
                                                        n_children));
      return TRUE;
    }

  if (!eks_ensure_app_shards_are_symlinked_to_home_directory (engine,
                                                              self->application_id,
                                                              &local_error))
    {
      g_dbus_method_invocation_take_error (invocation,
                                           g_steal_pointer (&local_error));
      return TRUE;
    }

  first_child = g_variant_get_child_value (queries, 0);
  query =
    create_query_from_dbus_query_parameters (first_child,
                                             self->application_id,
                                             self->translation_infos,
                                             &local_error);

  if (query == NULL)
    {
      g_dbus_method_invocation_take_error (invocation,
                                           g_steal_pointer (&local_error));
      return TRUE;
    }

  /* Hold the application so that it doesn't go away whilst we're handling
   * the query */
  g_application_hold (g_application_get_default ());

  eknc_engine_query (engine,
                   query,
                   NULL,
                   on_received_query_results,
                   metadata_query_state_new (self, invocation));
  return TRUE;
}

static gboolean
handle_shards (EksContentMetadata    *skeleton,
               GDBusMethodInvocation *invocation,
               gpointer               user_data)
{
  EksMetadataProvider *self = user_data;
  EkncEngine *engine = eknc_engine_get_default ();
  g_autoptr(GError) error = NULL;
  EkncDomain *domain = eknc_engine_get_domain_for_app (engine,
                                                       self->application_id,
                                                       &error);
  g_auto(GStrv) shards_strv = NULL;

  if (domain == NULL)
    {
      g_dbus_method_invocation_take_error (invocation,
                                           eks_map_error_to_eks_error (error));
      return TRUE;
    }

  shards_strv = strv_from_shard_list (eknc_domain_get_shards (domain));
  eks_content_metadata_complete_shards (skeleton,
                                        invocation,
                                        (const char * const *) shards_strv);

  return TRUE;
}

static GDBusInterfaceSkeleton *
eks_metadata_provider_skeleton_for_interface (EksProvider *provider,
                                              const char  *interface)
{
  EksMetadataProvider *self = EKS_METADATA_PROVIDER (provider);

  if (g_strcmp0 (interface, "com.endlessm.ContentMetadata") == 0)
      return G_DBUS_INTERFACE_SKELETON (self->skeleton);

  g_assert_not_reached ();
  return NULL;
}

static void
eks_metadata_provider_interface_init (EksProviderInterface *iface)
{
  iface->skeleton_for_interface = eks_metadata_provider_skeleton_for_interface;
}

static void
eks_metadata_provider_init (EksMetadataProvider *self)
{
  self->skeleton = eks_content_metadata_skeleton_new ();
  self->translation_infos = article_metadata_query_construction_props_translation_table ();

  g_signal_connect (self->skeleton, "handle-query",
                    G_CALLBACK (handle_query), self);
  g_signal_connect (self->skeleton, "handle-shards",
                    G_CALLBACK (handle_shards), self);
}
