/* Copyright 2018 Endless Mobile, Inc. */

#pragma once

#include <eos-knowledge-content.h>

#include <gio/gio.h>

gboolean eks_ensure_app_shards_are_symlinked_to_home_directory (EkncEngine   *engine,
                                                                const gchar  *application_id,
                                                                GError      **error);

