#include "fildem-serializer.h"

#include <string.h>

static const gchar *FILDEM_BUS_NAME = "org.fildem";
static const gchar *FILDEM_OBJECT_PATH = "/org/fildem/v1";

const gchar *
fildem_bus_name(void)
{
  return FILDEM_BUS_NAME;
}

const gchar *
fildem_object_path(void)
{
  return FILDEM_OBJECT_PATH;
}

static gboolean
validate_dict_array_with_keys(GVariant *items,
                              const gchar *signature,
                              const gchar *required_key_1,
                              const gchar *required_key_2,
                              GError **error)
{
  GVariantIter iter;
  GVariant *item = NULL;

  if (!g_variant_is_of_type(items, G_VARIANT_TYPE(signature))) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Expected variant type %s",
                signature);
    return FALSE;
  }

  g_variant_iter_init(&iter, items);
  while ((item = g_variant_iter_next_value(&iter)) != NULL) {
    const gchar *v1 = NULL;
    const gchar *v2 = NULL;
    gboolean ok;

    ok = fildem_variant_lookup_string_required(item, required_key_1, &v1, error) &&
         fildem_variant_lookup_string_required(item, required_key_2, &v2, error);

    g_variant_unref(item);

    if (!ok) {
      return FALSE;
    }
  }

  return TRUE;
}

gboolean
fildem_variant_lookup_string_required(GVariant *dict,
                                      const gchar *key,
                                      const gchar **value_out,
                                      GError **error)
{
  if (!g_variant_is_of_type(dict, G_VARIANT_TYPE("a{sv}"))) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Expected dictionary type a{sv}");
    return FALSE;
  }

  if (!g_variant_lookup(dict, key, "&s", value_out)) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Missing required key %s",
                key);
    return FALSE;
  }

  if (*value_out == NULL || strlen(*value_out) == 0) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Required key %s cannot be empty",
                key);
    return FALSE;
  }

  return TRUE;
}

gboolean
fildem_validate_window_context(GVariant *context, GError **error)
{
  const gchar *window_uid = NULL;
  const gchar *app_id = NULL;

  if (!g_variant_is_of_type(context, G_VARIANT_TYPE(FILDEM_ACTIVE_WINDOW_SIGNATURE))) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "WindowContext must have signature %s",
                FILDEM_ACTIVE_WINDOW_SIGNATURE);
    return FALSE;
  }

  if (!fildem_variant_lookup_string_required(context, "window_uid", &window_uid, error)) {
    return FALSE;
  }

  if (!fildem_variant_lookup_string_required(context, "app_id", &app_id, error)) {
    return FALSE;
  }

  return TRUE;
}

gboolean
fildem_validate_top_level_items(GVariant *items, GError **error)
{
  return validate_dict_array_with_keys(items,
                                       FILDEM_TOP_LEVEL_SIGNATURE,
                                       "id",
                                       "label",
                                       error);
}

gboolean
fildem_validate_menu_tree(GVariant *nodes, GError **error)
{
  return validate_dict_array_with_keys(nodes,
                                       FILDEM_MENU_TREE_SIGNATURE,
                                       "id",
                                       "label",
                                       error);
}

GVariant *
fildem_empty_top_level_items(void)
{
  return g_variant_new_array(G_VARIANT_TYPE("a{sv}"), NULL, 0);
}

GVariant *
fildem_empty_menu_tree(void)
{
  return g_variant_new_array(G_VARIANT_TYPE("a{sv}"), NULL, 0);
}

GVariant *
fildem_empty_actions(void)
{
  return g_variant_new_strv(NULL, 0);
}
