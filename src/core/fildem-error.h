#pragma once

#include <glib.h>

typedef enum {
  FILDEM_ERROR_INVALID_ARGUMENT,
  FILDEM_ERROR_NOT_FOUND,
} FildemError;

#define FILDEM_ERROR fildem_error_quark()

GQuark fildem_error_quark(void);
