/* Copyright 2018 Endless Mobile, Inc. */

#include <dmodel.h>
#include "dm-enums.h"

#include "eks-errors.h"
#include "eks-provider-iface.h"
#include "eks-query-util.h"

#include "eks-knowledge-app-dbus.h"
#include "eks-metadata-provider.h"
#include "eks-metadata-provider-dbus.h"

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

static void add_dbus_friendly_keys_recurse_into_json_structure (JsonBuilder  *builder,
                                                                JsonNode     *node,
                                                                JsonNodeType  node_type);

static void
copy_non_null_json_object_key (JsonObject  *source_object,
                               const gchar *member_name,
                               JsonNode    *member_node,
                               gpointer     user_data)
{
  JsonBuilder *builder = user_data;
  JsonNodeType member_node_type = json_node_get_node_type (member_node);

  switch (member_node_type)
    {
      case JSON_NODE_OBJECT:
      case JSON_NODE_ARRAY:
        json_builder_set_member_name (builder, member_name);
        add_dbus_friendly_keys_recurse_into_json_structure (builder,
                                                            member_node,
                                                            member_node_type);
        break;
      case JSON_NODE_VALUE:
        json_builder_set_member_name (builder, member_name);
        json_builder_add_value (builder, json_node_ref (member_node));
        break;
      case JSON_NODE_NULL:
        break;
    }
}

static void
build_dbus_friendly_json_object_from_object (JsonBuilder *builder,
                                             JsonObject  *object)
{
  json_builder_begin_object (builder);
  json_object_foreach_member (object,
                              copy_non_null_json_object_key,
                              builder);
  json_builder_end_object (builder);
}

static void
copy_non_null_json_array_element (JsonArray  *source_array,
                                  guint       member_index,
                                  JsonNode   *member_node,
                                  gpointer    user_data)
{
  JsonBuilder *builder = user_data;
  JsonNodeType member_node_type = json_node_get_node_type (member_node);

  switch (member_node_type)
    {
      case JSON_NODE_OBJECT:
      case JSON_NODE_ARRAY:
        add_dbus_friendly_keys_recurse_into_json_structure (builder,
                                                            member_node,
                                                            member_node_type);
        break;
      case JSON_NODE_VALUE:
        json_builder_add_value (builder, json_node_ref (member_node));
        break;
      case JSON_NODE_NULL:
        break;
    }
}

static void
build_dbus_friendly_json_array_from_array (JsonBuilder *builder,
                                           JsonArray   *array)
{
  json_builder_begin_array (builder);
  json_array_foreach_element (array,
                              copy_non_null_json_array_element,
                              builder);
  json_builder_end_array (builder);
}

static void
add_dbus_friendly_keys_recurse_into_json_structure (JsonBuilder  *builder,
			                            JsonNode     *node,
			                            JsonNodeType  node_type)
{
  switch (node_type)
    {
      case JSON_NODE_OBJECT:
        build_dbus_friendly_json_object_from_object (builder,
                                                     json_node_get_object (node));
        break;
      case JSON_NODE_ARRAY:
        build_dbus_friendly_json_array_from_array (builder,
                                                   json_node_get_array (node));
        break;
      default:
        g_assert_not_reached();
    }
}

/* The d-bus wire protocol doesn't support maybe types,
 * but GVariant does. The way that this is handled in consuming
 * applications is to check if the property exists on the vardict,
 * so remove all NULL-valued properties from this object recursively. */
static JsonNode *
json_node_from_object_with_nulls_recursively_removed (JsonObject *object)
{
  g_autoptr(JsonBuilder) builder = json_builder_new ();

  build_dbus_friendly_json_object_from_object (builder, object);
  return json_builder_get_root (builder);
}

static gboolean
gvalue_to_variant_internal (GValue              *value,
                            const GVariantType  *expected_type,
                            GVariant           **out_variant,
                            GError             **error)
{
  g_return_val_if_fail (out_variant != NULL, FALSE);

  /* Special case: if expected_type is G_VARIANT_TYPE_VARDICT then we need
   * to deserialize from JsonObject. */
  if (expected_type == G_VARIANT_TYPE_VARDICT)
    {
      JsonObject *object = g_value_get_boxed (value);

      if (object == NULL)
        return TRUE;

      g_autoptr(JsonNode) node =
        json_node_from_object_with_nulls_recursively_removed (object);
      *out_variant = json_gvariant_deserialize (node,
                                                NULL,
                                                error);
      return *out_variant != NULL;
    }

  *out_variant = g_dbus_gvalue_to_gvariant (value, expected_type);
  return *out_variant != NULL;
}

static gboolean
maybe_add_key_value_pair_from_model_to_variant (DmContent           *model,
                                                GVariantBuilder     *builder,
                                                const char          *key,
                                                const GVariantType  *expected_type,
                                                GError             **error)
{
  g_auto(GValue) value = G_VALUE_INIT;
  GParamSpec *pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (model),
                                                    key);
  g_autoptr(GVariant) converted = NULL;

  if (pspec == NULL)
    return TRUE;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (G_OBJECT (model), key, &value);

  if (!gvalue_to_variant_internal (&value, expected_type, &converted, error))
    return FALSE;

  /* If we got NULL here it just means that the source property was NULL,
   * so don't add it. */
  if (converted == NULL)
    return TRUE;

  add_key_value_pair_to_variant (builder, key, g_steal_pointer (&converted));
  return TRUE;
}

typedef struct _ModelVariantTypes {
  const gchar        *prop_name;
  const GVariantType *variant_type;
} ModelVariantTypes;

static const ModelVariantTypes model_variant_types[] = {
  { "child_tags", G_VARIANT_TYPE_STRING_ARRAY },
  { "content_type", G_VARIANT_TYPE_STRING },
  { "copyright_holder", G_VARIANT_TYPE_STRING },
  { "discovery_feed_content", G_VARIANT_TYPE_VARDICT },
  { "id", G_VARIANT_TYPE_STRING },
  { "language", G_VARIANT_TYPE_STRING },
  { "last_modified_date", G_VARIANT_TYPE_STRING },
  { "license", G_VARIANT_TYPE_STRING },
  { "original_title", G_VARIANT_TYPE_STRING },
  { "original_uri", G_VARIANT_TYPE_STRING },
  { "tags", G_VARIANT_TYPE_STRING_ARRAY },
  { "temporal_coverage", G_VARIANT_TYPE_STRING_ARRAY },
  { "title", G_VARIANT_TYPE_STRING },
  { "thumbnail_uri", G_VARIANT_TYPE_STRING }
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
      DmContent *model = l->data;
      gsize i = 0;

      g_variant_builder_open (&builder, G_VARIANT_TYPE_VARDICT);

      for (; i < model_variant_types_n; ++i)
        {
          const ModelVariantTypes *model_prop = &model_variant_types[i];

          if (!maybe_add_key_value_pair_from_model_to_variant (model,
                                                               &builder,
                                                               model_prop->prop_name,
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
  DmEngine *engine = DM_ENGINE (source);
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
  models_variant = g_variant_ref_sink (build_models_variants (models, &error));

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
                                      GArray     *values_array,
                                      GPtrArray  *props_array)
{
  guint index = values_array->len;
  GValue *value = NULL;

  /* Need to be careful to set value *after* the array has been resized */
  g_array_set_size (values_array, values_array->len + 1);
  value = &(g_array_index (values_array, GValue, index));
  g_value_init (value, G_TYPE_STRING);
  g_value_set_string (value, str);
  g_ptr_array_add (props_array, g_strdup (key));
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
                                       GArray                       *values_array,
                                       GPtrArray                    *props_array,
                                       VariantToValueTransformFunc   transform,
                                       gpointer                      user_data,
                                       GError                      **error)
{
  guint index = values_array->len;
  GValue *value = NULL;

  /* Need to be careful to set value *after* the array has been resized */
  g_array_set_size (values_array, values_array->len + 1);
  value = &(g_array_index (values_array, GValue, index));

  /* If we return false from here, this should be treated as an
   * unrecoverable error for the call */
  if (!transform (variant, value, user_data, error))
    return FALSE;

  g_ptr_array_add (props_array, g_strdup (key));

  return TRUE;
}

static gboolean
append_construction_prop_from_variant_dbus_transform (const char  *key,
                                                      GVariant    *variant,
                                                      GArray      *values_array,
                                                      GPtrArray   *props_array,
                                                      gpointer     extra_data,
                                                      GError     **error)
{
  return append_construction_prop_from_variant (key,
                                                variant,
                                                values_array,
                                                props_array,
                                                translate_gvariant_to_gvalue,
                                                extra_data,
                                                error);
}

static gboolean
append_construction_prop_from_variant_enum_transform (const char  *key,
                                                      GVariant    *variant,
                                                      GArray      *values_array,
                                                      GPtrArray   *props_array,
                                                      gpointer     extra_data,
                                                      GError     **error)
{
  return append_construction_prop_from_variant (key,
                                                variant,
                                                values_array,
                                                props_array,
                                                translate_gvariant_to_gvalue_parse_enum,
                                                extra_data,
                                                error);
}

typedef gboolean (*AppendConstructionPropFromVariantWithTransformFunc) (const char  *key,
                                                                        GVariant    *variant,
                                                                        GArray      *values_array,
                                                                        GPtrArray   *props_array,
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
                       value_translation_info_new (append_construction_prop_from_variant_dbus_transform,
                                                   NULL,
                                                   NULL));
  g_hash_table_insert (table,
                       g_strdup ("tags-match-any"),
                       value_translation_info_new (append_construction_prop_from_variant_dbus_transform,
                                                   NULL,
                                                   NULL));
  g_hash_table_insert (table,
                       g_strdup ("tags-match-all"),
                       value_translation_info_new (append_construction_prop_from_variant_dbus_transform,
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
                                                   alloc_for_gtype (DM_TYPE_QUERY_SORT),
                                                   g_free));
  g_hash_table_insert (table,
                       g_strdup ("order"),
                       value_translation_info_new (append_construction_prop_from_variant_enum_transform,
                                                   alloc_for_gtype (DM_TYPE_QUERY_ORDER),
                                                   g_free));

  return g_steal_pointer (&table);
}

static DmQuery *
create_query_from_dbus_query_parameters (GVariant     *query_parameters,
                                         const char   *application_id,
                                         GHashTable   *translation_infos,
                                         GError      **error)
{
  GVariantIter iter;
  char *iter_key;
  GVariant *iter_value;

  g_autoptr(GArray) values_array = g_array_new (FALSE, TRUE, sizeof (GValue));
  g_autoptr(GPtrArray) props_array = g_ptr_array_new_with_free_func (g_free);

  /* Always add the app-id to the query */
  append_construction_prop_from_string ("app-id",
                                        application_id,
                                        values_array,
                                        props_array);

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
                                          values_array,
                                          props_array,
                                          translation_info->user_data,
                                          error))
        return NULL;
    }

  return DM_QUERY (g_object_new_with_properties (DM_TYPE_QUERY,
                                                 props_array->len,
                                                 (const char **) props_array->pdata,
                                                 (const GValue *) values_array->data));
}

static gboolean
handle_query (EksContentMetadata    *skeleton,
              GDBusMethodInvocation *invocation,
              GVariant              *queries,
              gpointer               user_data)
{
  EksMetadataProvider *self = user_data;
  DmEngine *engine = dm_engine_get_default ();
  g_autoptr(GError) local_error = NULL;
  g_autoptr(DmQuery) query = NULL;
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

  dm_engine_query (engine,
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
  DmEngine *engine = dm_engine_get_default ();
  g_autoptr(GError) error = NULL;
  DmDomain *domain = dm_engine_get_domain_for_app (engine,
                                                   self->application_id,
                                                   &error);
  g_auto(GStrv) shards_strv = NULL;

  if (domain == NULL)
    {
      g_dbus_method_invocation_take_error (invocation,
                                           eks_map_error_to_eks_error (error));
      return TRUE;
    }

  shards_strv = strv_from_shard_list (dm_domain_get_shards (domain));
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
