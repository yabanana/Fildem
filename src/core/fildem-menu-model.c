#include "fildem-menu-model.h"

static gboolean
match_term(const gchar *candidate, const gchar *term)
{
  g_autofree gchar *candidate_fold = NULL;
  g_autofree gchar *term_fold = NULL;

  if (term == NULL || *term == 0) {
    return TRUE;
  }

  if (candidate == NULL) {
    return FALSE;
  }

  candidate_fold = g_utf8_casefold(candidate, -1);
  term_fold = g_utf8_casefold(term, -1);

  return g_strstr_len(candidate_fold, -1, term_fold) != NULL;
}

static void
append_result(GVariantBuilder *array_builder,
              const gchar *id,
              const gchar *label,
              const gchar *kind)
{
  GVariantBuilder item_builder;

  g_variant_builder_init(&item_builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&item_builder, "{sv}", "id", g_variant_new_string(id));
  g_variant_builder_add(&item_builder, "{sv}", "label", g_variant_new_string(label));
  g_variant_builder_add(&item_builder, "{sv}", "kind", g_variant_new_string(kind));

  g_variant_builder_add_value(array_builder, g_variant_builder_end(&item_builder));
}

GVariant *
fildem_menu_model_query(GVariant *menu_nodes,
                        GVariant *window_actions,
                        const gchar *term)
{
  GVariantBuilder results;
  GVariantIter node_iter;
  GVariantIter actions_iter;
  GVariant *item = NULL;
  const gchar *action = NULL;

  g_variant_builder_init(&results, G_VARIANT_TYPE("aa{sv}"));

  if (menu_nodes != NULL && g_variant_is_of_type(menu_nodes, G_VARIANT_TYPE("aa{sv}"))) {
    g_variant_iter_init(&node_iter, menu_nodes);
    while ((item = g_variant_iter_next_value(&node_iter)) != NULL) {
      const gchar *id = NULL;
      const gchar *label = NULL;
      gboolean enabled = TRUE;
      gboolean visible = TRUE;
      gboolean is_separator = FALSE;

      g_variant_lookup(item, "id", "&s", &id);
      g_variant_lookup(item, "label", "&s", &label);
      g_variant_lookup(item, "enabled", "b", &enabled);
      g_variant_lookup(item, "visible", "b", &visible);
      g_variant_lookup(item, "is_separator", "b", &is_separator);

      if (id != NULL &&
          label != NULL &&
          enabled &&
          visible &&
          !is_separator &&
          match_term(label, term)) {
        append_result(&results, id, label, "menu");
      }

      g_variant_unref(item);
    }
  }

  if (window_actions != NULL && g_variant_is_of_type(window_actions, G_VARIANT_TYPE("as"))) {
    g_variant_iter_init(&actions_iter, window_actions);
    while (g_variant_iter_next(&actions_iter, "&s", &action)) {
      g_autofree gchar *action_id = NULL;
      if (match_term(action, term)) {
        action_id = g_strdup_printf("action:%s", action);
        append_result(&results, action_id, action, "window-action");
      }
    }
  }

  return g_variant_builder_end(&results);
}
