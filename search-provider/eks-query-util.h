/* Copyright 2018 Endless Mobile, Inc. */

#pragma once

#include <eos-knowledge-content.h>
#include <eos-shard/eos-shard-shard-file.h>

#include <gio/gio.h>

gboolean models_for_result (EkncEngine    *engine,
                            const gchar   *application_id,
                            GAsyncResult  *result,
                            GSList       **models,
                            gint          *upper_bound,
                            GError       **error);

gboolean models_and_shards_for_result (EkncEngine    *engine,
                                       const gchar   *application_id,
                                       GAsyncResult  *result,
                                       GSList       **models,
                                       GSList       **shards,
                                       gint          *upper_bound,
                                       GError       **error);

GStrv strv_from_shard_list (GSList *string_list);

