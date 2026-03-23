#include <glib.h>

#include "fildem-core.h"

static GVariant *
new_window_context(const gchar *window_uid, const gchar *app_id)
{
  GVariantBuilder builder;

  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{sv}", "window_uid", g_variant_new_string(window_uid));
  g_variant_builder_add(&builder, "{sv}", "app_id", g_variant_new_string(app_id));

  return g_variant_builder_end(&builder);
}

static GVariant *
new_menu_nodes(void)
{
  GVariantBuilder items;
  GVariantBuilder item;

  g_variant_builder_init(&items, G_VARIANT_TYPE("aa{sv}"));

  g_variant_builder_init(&item, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&item, "{sv}", "id", g_variant_new_string("menu.open"));
  g_variant_builder_add(&item, "{sv}", "label", g_variant_new_string("Open"));
  g_variant_builder_add(&item, "{sv}", "enabled", g_variant_new_boolean(TRUE));
  g_variant_builder_add(&item, "{sv}", "visible", g_variant_new_boolean(TRUE));
  g_variant_builder_add(&item, "{sv}", "is_separator", g_variant_new_boolean(FALSE));
  g_variant_builder_add_value(&items, g_variant_builder_end(&item));

  g_variant_builder_init(&item, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&item, "{sv}", "id", g_variant_new_string("menu.separator"));
  g_variant_builder_add(&item, "{sv}", "label", g_variant_new_string(""));
  g_variant_builder_add(&item, "{sv}", "enabled", g_variant_new_boolean(TRUE));
  g_variant_builder_add(&item, "{sv}", "visible", g_variant_new_boolean(TRUE));
  g_variant_builder_add(&item, "{sv}", "is_separator", g_variant_new_boolean(TRUE));
  g_variant_builder_add_value(&items, g_variant_builder_end(&item));

  g_variant_builder_init(&item, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&item, "{sv}", "id", g_variant_new_string("menu.hidden"));
  g_variant_builder_add(&item, "{sv}", "label", g_variant_new_string("Hidden"));
  g_variant_builder_add(&item, "{sv}", "enabled", g_variant_new_boolean(TRUE));
  g_variant_builder_add(&item, "{sv}", "visible", g_variant_new_boolean(FALSE));
  g_variant_builder_add(&item, "{sv}", "is_separator", g_variant_new_boolean(FALSE));
  g_variant_builder_add_value(&items, g_variant_builder_end(&item));

  g_variant_builder_init(&item, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&item, "{sv}", "id", g_variant_new_string("menu.disabled"));
  g_variant_builder_add(&item, "{sv}", "label", g_variant_new_string("Disabled"));
  g_variant_builder_add(&item, "{sv}", "enabled", g_variant_new_boolean(FALSE));
  g_variant_builder_add(&item, "{sv}", "visible", g_variant_new_boolean(TRUE));
  g_variant_builder_add(&item, "{sv}", "is_separator", g_variant_new_boolean(FALSE));
  g_variant_builder_add_value(&items, g_variant_builder_end(&item));

  return g_variant_builder_end(&items);
}

static void
validate_window_context_ok(void)
{
  g_autoptr(GError) error = NULL;
  GVariant *ctx = new_window_context("w-1", "org.gnome.Nautilus.desktop");

  g_assert_true(fildem_validate_window_context(ctx, &error));
  g_assert_no_error(error);
}

static void
validate_window_context_missing_key(void)
{
  g_autoptr(GError) error = NULL;
  GVariantBuilder builder;
  GVariant *ctx;

  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{sv}", "window_uid", g_variant_new_string("w-1"));
  ctx = g_variant_builder_end(&builder);

  g_assert_false(fildem_validate_window_context(ctx, &error));
  g_assert_error(error, FILDEM_ERROR, FILDEM_ERROR_INVALID_ARGUMENT);
}

static void
menu_model_query_filters_results(void)
{
  g_autoptr(GVariant) items = new_menu_nodes();
  g_autoptr(GVariant) actions = g_variant_new_strv((const gchar *[]) {"Minimize", "Close", NULL}, -1);
  g_autoptr(GVariant) results = fildem_menu_model_query(items, actions, "clo");
  GVariant *entry = NULL;
  const gchar *id = NULL;
  const gchar *kind = NULL;

  g_assert_cmpuint(g_variant_n_children(results), ==, 1);
  entry = g_variant_get_child_value(results, 0);
  g_variant_lookup(entry, "id", "&s", &id);
  g_variant_lookup(entry, "kind", "&s", &kind);
  g_assert_cmpstr(id, ==, "action:Close");
  g_assert_cmpstr(kind, ==, "window-action");
  g_variant_unref(entry);
}

static gboolean
results_have_id(GVariant *results, const gchar *expected_id)
{
  GVariantIter iter;
  GVariant *item = NULL;

  g_variant_iter_init(&iter, results);
  while ((item = g_variant_iter_next_value(&iter)) != NULL) {
    const gchar *id = NULL;
    gboolean match = FALSE;

    g_variant_lookup(item, "id", "&s", &id);
    match = g_strcmp0(id, expected_id) == 0;
    g_variant_unref(item);

    if (match) {
      return TRUE;
    }
  }

  return FALSE;
}

static void
menu_model_query_skips_invisible_and_separators(void)
{
  g_autoptr(GVariant) items = new_menu_nodes();
  g_autoptr(GVariant) actions = g_variant_new_strv((const gchar *[]) {"Close", NULL}, -1);
  g_autoptr(GVariant) results = fildem_menu_model_query(items, actions, "");

  g_assert_cmpuint(g_variant_n_children(results), ==, 2);
  g_assert_true(results_have_id(results, "menu.open"));
  g_assert_true(results_have_id(results, "action:Close"));
  g_assert_false(results_have_id(results, "menu.hidden"));
  g_assert_false(results_have_id(results, "menu.disabled"));
  g_assert_false(results_have_id(results, "menu.separator"));
}

int
main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/fildem/serializer/window-context-ok", validate_window_context_ok);
  g_test_add_func("/fildem/serializer/window-context-missing-key", validate_window_context_missing_key);
  g_test_add_func("/fildem/model/query-filters", menu_model_query_filters_results);
  g_test_add_func("/fildem/model/query-skips-invisible-and-separators",
                  menu_model_query_skips_invisible_and_separators);

  return g_test_run();
}
