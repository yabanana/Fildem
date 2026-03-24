#pragma once

#include <gio/gio.h>

#include "fildem-error.h"

#define FILDEM_ACTIVE_WINDOW_SIGNATURE "a{sv}"
#define FILDEM_TOP_LEVEL_SIGNATURE "aa{sv}"
#define FILDEM_MENU_TREE_SIGNATURE "aa{sv}"

G_BEGIN_DECLS

const gchar *fildem_bus_name(void);
const gchar *fildem_object_path(void);

gboolean fildem_validate_window_context(GVariant *context, GError **error);
gboolean fildem_validate_top_level_items(GVariant *items, GError **error);
gboolean fildem_validate_menu_tree(GVariant *nodes, GError **error);

gboolean fildem_variant_lookup_string_required(GVariant *dict,
                                               const gchar *key,
                                               const gchar **value_out,
                                               GError **error);

GVariant *fildem_empty_top_level_items(void);
GVariant *fildem_empty_menu_tree(void);
GVariant *fildem_empty_actions(void);

G_END_DECLS
