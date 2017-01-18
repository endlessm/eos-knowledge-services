/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copyright 2016 Endless Mobile, Inc. */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define EKS_TYPE_SUBTREE_DISPATCHER eks_subtree_dispatcher_get_type ()
G_DECLARE_FINAL_TYPE(EksSubtreeDispatcher, eks_subtree_dispatcher, EKS, SUBTREE_DISPATCHER, GObject)

/**
 * EksSubtreeDispatcher:
 *
 * This class structure contains no public members.
 */
struct _EksSubtreeDispatcher
{
  GObject parent;
};

struct _EksSubtreeDispatcherClass
{
  GObjectClass parent_class;
};

gboolean eks_subtree_dispatcher_register (EksSubtreeDispatcher *self,
                                          GDBusConnection *connection,
                                          const char *subtree_path,
                                          GError **error);

void eks_subtree_dispatcher_unregister (EksSubtreeDispatcher *self);

G_END_DECLS
