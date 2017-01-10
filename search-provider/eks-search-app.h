/* Copyright 2016 Endless Mobile, Inc. */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define EKS_TYPE_SEARCH_APP eks_search_app_get_type ()
G_DECLARE_FINAL_TYPE (EksSearchApp, eks_search_app, EKS, SEARCH_APP, GApplication)

G_END_DECLS
