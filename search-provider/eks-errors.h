/* Copyright 2018 Endless Mobile, Inc. */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/**
 * EksError:
 * @EKS_ERROR_APP_NOT_FOUND: App or app file was not found
 * @EKS_ERROR_UNSUPPORTED_VERSION: Unsupported version of content found
 * @EKS_ERROR_ID_NOT_FOUND: Requested ID not found
 * @EKS_ERROR_MALFORMED_APP: App was not well-formed
 * @EKS_ERROR_INVALID_REQUEST: Caller made a malformed request
 *
 * Error enumeration for domain related errors.
 */
typedef enum {
  EKS_ERROR_APP_NOT_FOUND,
  EKS_ERROR_UNSUPPORTED_VERSION,
  EKS_ERROR_ID_NOT_FOUND,
  EKS_ERROR_MALFORMED_APP,
  EKS_ERROR_INVALID_REQUEST
} EksError;

#define EKS_ERROR eks_error_quark ()
GQuark eks_error_quark (void);

GError *eks_map_error_to_eks_error (const GError *error);

G_END_DECLS
