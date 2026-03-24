#include <gio/gio.h>
#include <glib.h>

#include <errno.h>
#include <string.h>

#include "fildem-core.h"

typedef enum {
  PROVIDER_NONE,
  PROVIDER_GTK,
  PROVIDER_DBUSMENU,
} ProviderKind;

typedef struct {
  const gchar *name;
  const gchar *xml_file;
  GDBusNodeInfo *node_info;
  guint registration_id;
} InterfaceRegistration;

typedef struct {
  GMainLoop *loop;
  GDBusConnection *connection;
  guint owner_id;
  GVariant *active_window;
  FildemMenuRegistry *registry;
  FildemMenuCache *cache;
  InterfaceRegistration interfaces[6];
} FildemDaemon;

static const gchar *INTERFACE_FILES[6][2] = {
  {"org.fildem.v1.Window", "org.fildem.v1.Window.xml"},
  {"org.fildem.v1.TopLevel", "org.fildem.v1.TopLevel.xml"},
  {"org.fildem.v1.MenuTree", "org.fildem.v1.MenuTree.xml"},
  {"org.fildem.v1.Activation", "org.fildem.v1.Activation.xml"},
  {"org.fildem.v1.WindowActions", "org.fildem.v1.WindowActions.xml"},
  {"org.fildem.v1.Hud", "org.fildem.v1.Hud.xml"},
};

static const gchar *DBUSMENU_REGISTRAR_BUS = "com.canonical.AppMenu.Registrar";
static const gchar *DBUSMENU_REGISTRAR_PATH = "/com/canonical/AppMenu/Registrar";
static const gchar *DBUSMENU_IFACE = "com.canonical.dbusmenu";

static GVariant *
empty_window_context(void)
{
  GVariantBuilder builder;

  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  return g_variant_builder_end(&builder);
}

static GVariant *
empty_dict_variant(void)
{
  GVariantBuilder builder;

  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  return g_variant_builder_end(&builder);
}

static gchar *
lookup_interface_file_path(const gchar *name)
{
  g_autofree gchar *installed = NULL;

  installed = g_build_filename(FILDEM_INSTALLED_INTERFACES_DIR, name, NULL);
  if (g_file_test(installed, G_FILE_TEST_EXISTS)) {
    return g_steal_pointer(&installed);
  }

  return g_build_filename(FILDEM_SOURCE_INTERFACES_DIR, name, NULL);
}

static gboolean
load_interface_info(InterfaceRegistration *entry, GError **error)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *contents = NULL;

  path = lookup_interface_file_path(entry->xml_file);

  if (!g_file_get_contents(path, &contents, NULL, error)) {
    return FALSE;
  }

  entry->node_info = g_dbus_node_info_new_for_xml(contents, error);
  if (entry->node_info == NULL || entry->node_info->interfaces == NULL ||
      entry->node_info->interfaces[0] == NULL) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Invalid introspection xml for %s",
                entry->name);
    return FALSE;
  }

  return TRUE;
}

static void
return_dbus_error(GDBusMethodInvocation *invocation,
                  GError *error,
                  const gchar *fallback)
{
  if (error != NULL) {
    g_dbus_method_invocation_return_dbus_error(invocation,
                                               "org.fildem.Error.InvalidArgument",
                                               error->message);
    g_error_free(error);
    return;
  }

  g_dbus_method_invocation_return_dbus_error(invocation,
                                             "org.fildem.Error.InvalidArgument",
                                             fallback);
}

static void
emit_signal(FildemDaemon *daemon,
            const gchar *interface_name,
            const gchar *signal_name,
            GVariant *parameters)
{
  g_dbus_connection_emit_signal(daemon->connection,
                                NULL,
                                fildem_object_path(),
                                interface_name,
                                signal_name,
                                parameters,
                                NULL);
}

static void
emit_activation_requested(FildemDaemon *daemon,
                          const gchar *window_uid,
                          const gchar *target_id)
{
  emit_signal(daemon,
              "org.fildem.v1.Activation",
              "ActivationRequested",
              g_variant_new("(&s&s)", window_uid, target_id));
}

static gboolean
variant_as_int64(GVariant *value, gint64 *out)
{
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT16)) {
    *out = g_variant_get_int16(value);
    return TRUE;
  }

  if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16)) {
    *out = g_variant_get_uint16(value);
    return TRUE;
  }

  if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
    *out = g_variant_get_int32(value);
    return TRUE;
  }

  if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
    *out = g_variant_get_uint32(value);
    return TRUE;
  }

  if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64)) {
    *out = g_variant_get_int64(value);
    return TRUE;
  }

  if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64)) {
    *out = g_variant_get_uint64(value);
    return TRUE;
  }

  if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
    const gchar *raw = g_variant_get_string(value, NULL);
    gchar *end = NULL;
    gint64 parsed;

    errno = 0;
    parsed = g_ascii_strtoll(raw, &end, 10);
    if (errno == 0 && end != NULL && *end == '\0') {
      *out = parsed;
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
variant_as_boolean(GVariant *value, gboolean *out)
{
  gint64 int_value;

  if (g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
    *out = g_variant_get_boolean(value);
    return TRUE;
  }

  if (variant_as_int64(value, &int_value)) {
    *out = int_value != 0;
    return TRUE;
  }

  if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
    const gchar *raw = g_variant_get_string(value, NULL);

    if (g_ascii_strcasecmp(raw, "true") == 0) {
      *out = TRUE;
      return TRUE;
    }

    if (g_ascii_strcasecmp(raw, "false") == 0) {
      *out = FALSE;
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
context_lookup_string(GVariant *context,
                      const gchar *key,
                      const gchar **out)
{
  return g_variant_lookup(context, key, "&s", out) && *out != NULL && (*out)[0] != '\0';
}

static gboolean
context_lookup_int64(GVariant *context,
                     const gchar *key,
                     gint64 *out)
{
  g_autoptr(GVariant) value = NULL;

  value = g_variant_lookup_value(context, key, NULL);
  if (value == NULL) {
    return FALSE;
  }

  return variant_as_int64(value, out);
}

static gboolean
dict_lookup_string(GVariant *dict,
                   const gchar *key,
                   const gchar **out)
{
  return g_variant_lookup(dict, key, "&s", out) && *out != NULL;
}

static gboolean
dict_lookup_boolean(GVariant *dict,
                    const gchar *key,
                    gboolean default_value)
{
  g_autoptr(GVariant) value = NULL;
  gboolean out;

  value = g_variant_lookup_value(dict, key, NULL);
  if (value == NULL) {
    return default_value;
  }

  if (!variant_as_boolean(value, &out)) {
    return default_value;
  }

  return out;
}

static gint64
dict_lookup_int64(GVariant *dict,
                  const gchar *key,
                  gint64 default_value)
{
  g_autoptr(GVariant) value = NULL;
  gint64 out;

  value = g_variant_lookup_value(dict, key, NULL);
  if (value == NULL) {
    return default_value;
  }

  if (!variant_as_int64(value, &out)) {
    return default_value;
  }

  return out;
}

static void
normalize_label_and_mnemonic(const gchar *raw,
                             gchar **label_out,
                             gchar **mnemonic_out)
{
  GString *clean;
  gchar *mnemonic = NULL;
  gsize i;

  if (raw == NULL) {
    *label_out = g_strdup("");
    *mnemonic_out = g_strdup("");
    return;
  }

  clean = g_string_new(NULL);
  for (i = 0; raw[i] != '\0'; i++) {
    if (raw[i] == '_' && raw[i + 1] != '\0') {
      if (mnemonic == NULL) {
        mnemonic = g_strndup(&raw[i + 1], 1);
      }
      continue;
    }

    g_string_append_c(clean, raw[i]);
  }

  if (mnemonic == NULL) {
    mnemonic = g_strdup("");
  }

  *label_out = g_string_free(clean, FALSE);
  *mnemonic_out = mnemonic;
}

static GVariant *
build_top_level_item(const gchar *id,
                     const gchar *label,
                     gboolean enabled,
                     gboolean visible,
                     const gchar *mnemonic)
{
  GVariantBuilder dict;

  g_variant_builder_init(&dict, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&dict, "{sv}", "id", g_variant_new_string(id));
  g_variant_builder_add(&dict, "{sv}", "label", g_variant_new_string(label));
  g_variant_builder_add(&dict, "{sv}", "enabled", g_variant_new_boolean(enabled));
  g_variant_builder_add(&dict, "{sv}", "visible", g_variant_new_boolean(visible));
  g_variant_builder_add(&dict, "{sv}", "mnemonic", g_variant_new_string(mnemonic));

  return g_variant_builder_end(&dict);
}

static GVariant *
build_menu_node(const gchar *id,
                const gchar *parent_id,
                const gchar *label,
                gboolean enabled,
                gboolean visible,
                const gchar *toggle_type,
                gboolean toggle_state,
                const gchar *accel,
                gboolean is_separator,
                guint position)
{
  GVariantBuilder dict;

  g_variant_builder_init(&dict, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&dict, "{sv}", "id", g_variant_new_string(id));
  g_variant_builder_add(&dict, "{sv}", "parent_id", g_variant_new_string(parent_id));
  g_variant_builder_add(&dict, "{sv}", "label", g_variant_new_string(label));
  g_variant_builder_add(&dict, "{sv}", "enabled", g_variant_new_boolean(enabled));
  g_variant_builder_add(&dict, "{sv}", "visible", g_variant_new_boolean(visible));
  g_variant_builder_add(&dict, "{sv}", "toggle_type", g_variant_new_string(toggle_type));
  g_variant_builder_add(&dict, "{sv}", "toggle_state", g_variant_new_boolean(toggle_state));
  g_variant_builder_add(&dict, "{sv}", "accel", g_variant_new_string(accel));
  g_variant_builder_add(&dict, "{sv}", "is_separator", g_variant_new_boolean(is_separator));
  g_variant_builder_add(&dict, "{sv}", "position", g_variant_new_int32((gint32)position));

  return g_variant_builder_end(&dict);
}

static void
publish_menu_state(FildemDaemon *daemon,
                   const gchar *window_uid,
                   GVariant *top_level,
                   GVariant *menu_tree)
{
  fildem_menu_registry_set_top_level(daemon->registry, window_uid, g_variant_ref(top_level));
  fildem_menu_cache_set_tree(daemon->cache, window_uid, g_variant_ref(menu_tree));

  emit_signal(daemon,
              "org.fildem.v1.TopLevel",
              "TopLevelChanged",
              g_variant_new("(&s@aa{sv})", window_uid, g_variant_ref(top_level)));
  emit_signal(daemon,
              "org.fildem.v1.MenuTree",
              "MenuTreeChanged",
              g_variant_new("(&s)", window_uid));
}

static GDBusProxy *
new_proxy(const gchar *bus_name,
          const gchar *object_path,
          const gchar *interface_name,
          GError **error)
{
  return g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       NULL,
                                       bus_name,
                                       object_path,
                                       interface_name,
                                       NULL,
                                       error);
}

static gboolean
dbusmenu_get_menu_proxy(GVariant *context, GDBusProxy **proxy_out, GError **error)
{
  g_autoptr(GDBusProxy) registrar = NULL;
  g_autoptr(GVariant) reply = NULL;
  const gchar *menu_bus = NULL;
  const gchar *menu_path = NULL;
  gint64 xid = 0;

  if (!context_lookup_int64(context, "xid", &xid) || xid <= 0) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Missing valid xid in active window context");
    return FALSE;
  }

  registrar = new_proxy(DBUSMENU_REGISTRAR_BUS,
                        DBUSMENU_REGISTRAR_PATH,
                        DBUSMENU_REGISTRAR_BUS,
                        error);
  if (registrar == NULL) {
    return FALSE;
  }

  reply = g_dbus_proxy_call_sync(registrar,
                                 "GetMenuForWindow",
                                 g_variant_new("(u)", (guint32)xid),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 error);
  if (reply == NULL) {
    return FALSE;
  }

  g_variant_get(reply, "(&s&o)", &menu_bus, &menu_path);
  if (menu_bus == NULL || menu_path == NULL) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_NOT_FOUND,
                "Menu bus/path not found for xid=%" G_GINT64_FORMAT,
                xid);
    return FALSE;
  }

  *proxy_out = new_proxy(menu_bus, menu_path, DBUSMENU_IFACE, error);
  return *proxy_out != NULL;
}

static gchar *
dbusmenu_shortcut_to_string(GVariant *value)
{
  GString *out;
  GVariant *first = NULL;
  GVariantIter token_iter;
  const gchar *token = NULL;
  GPtrArray *tokens;
  guint i;

  if (value == NULL) {
    return g_strdup("");
  }

  if (!g_variant_is_of_type(value, G_VARIANT_TYPE("aas"))) {
    return g_strdup("");
  }

  if (g_variant_n_children(value) == 0) {
    return g_strdup("");
  }

  first = g_variant_get_child_value(value, 0);
  if (first == NULL || !g_variant_is_of_type(first, G_VARIANT_TYPE("as"))) {
    g_clear_pointer(&first, g_variant_unref);
    return g_strdup("");
  }

  tokens = g_ptr_array_new_with_free_func(g_free);
  g_variant_iter_init(&token_iter, first);
  while (g_variant_iter_next(&token_iter, "&s", &token)) {
    g_ptr_array_add(tokens, g_strdup(token));
  }

  out = g_string_new(NULL);
  for (i = 0; i < tokens->len; i++) {
    const gchar *entry = g_ptr_array_index(tokens, i);

    if (i + 1 < tokens->len) {
      g_string_append_printf(out, "<%s>", entry);
    } else {
      g_string_append(out, entry);
    }
  }

  g_ptr_array_free(tokens, TRUE);
  g_variant_unref(first);

  return g_string_free(out, FALSE);
}

static gboolean
variant_child_as_int64(GVariant *container,
                       guint index,
                       gint64 *out)
{
  g_autoptr(GVariant) child = NULL;

  if (g_variant_n_children(container) <= index) {
    return FALSE;
  }

  child = g_variant_get_child_value(container, index);
  return variant_as_int64(child, out);
}

static void
dbusmenu_collect_children(GVariant *children,
                          const gchar *parent_id,
                          gboolean top_level,
                          guint *position,
                          GVariantBuilder *top_builder,
                          GVariantBuilder *tree_builder);

static void
dbusmenu_collect_node(GVariant *node,
                      const gchar *parent_id,
                      gboolean top_level,
                      guint *position,
                      GVariantBuilder *top_builder,
                      GVariantBuilder *tree_builder)
{
  g_autoptr(GVariant) id_value = NULL;
  g_autoptr(GVariant) props = NULL;
  g_autoptr(GVariant) children = NULL;
  g_autoptr(GVariant) shortcut = NULL;
  gint64 node_numeric_id = 0;
  const gchar *raw_label = NULL;
  const gchar *raw_type = NULL;
  const gchar *raw_toggle_type = "";
  gboolean enabled;
  gboolean visible;
  gboolean separator;
  gboolean toggle_state;
  gchar *label = NULL;
  gchar *mnemonic = NULL;
  g_autofree gchar *accel = NULL;
  g_autofree gchar *node_id = NULL;
  const gchar *next_parent = parent_id == NULL ? "" : parent_id;

  if (g_variant_n_children(node) < 3) {
    return;
  }

  id_value = g_variant_get_child_value(node, 0);
  if (!variant_as_int64(id_value, &node_numeric_id)) {
    return;
  }

  props = g_variant_get_child_value(node, 1);
  children = g_variant_get_child_value(node, 2);
  if (!g_variant_is_of_type(props, G_VARIANT_TYPE("a{sv}"))) {
    return;
  }

  dict_lookup_string(props, "label", &raw_label);
  dict_lookup_string(props, "type", &raw_type);
  dict_lookup_string(props, "toggle-type", &raw_toggle_type);

  enabled = dict_lookup_boolean(props, "enabled", TRUE);
  visible = dict_lookup_boolean(props, "visible", TRUE);
  toggle_state = dict_lookup_int64(props, "toggle-state", 0) != 0;
  separator = g_strcmp0(raw_type, "separator") == 0;

  shortcut = g_variant_lookup_value(props, "shortcut", NULL);
  accel = dbusmenu_shortcut_to_string(shortcut);

  node_id = g_strdup_printf("%" G_GINT64_FORMAT, node_numeric_id);

  if (raw_label != NULL) {
    normalize_label_and_mnemonic(raw_label, &label, &mnemonic);
  } else {
    label = g_strdup("");
    mnemonic = g_strdup("");
  }

  if (separator || *label != '\0') {
    g_variant_builder_add_value(tree_builder,
                                build_menu_node(node_id,
                                                next_parent,
                                                label,
                                                enabled,
                                                visible,
                                                raw_toggle_type,
                                                toggle_state,
                                                accel,
                                                separator,
                                                (*position)++));

    if (top_level && !separator && *label != '\0') {
      g_variant_builder_add_value(top_builder,
                                  build_top_level_item(node_id,
                                                       label,
                                                       enabled,
                                                       visible,
                                                       mnemonic));
    }

    next_parent = node_id;
  }

  if (children != NULL && g_variant_is_of_type(children, G_VARIANT_TYPE("av"))) {
    dbusmenu_collect_children(children,
                              next_parent,
                              FALSE,
                              position,
                              top_builder,
                              tree_builder);
  }

  g_free(label);
  g_free(mnemonic);
}

static void
dbusmenu_collect_children(GVariant *children,
                          const gchar *parent_id,
                          gboolean top_level,
                          guint *position,
                          GVariantBuilder *top_builder,
                          GVariantBuilder *tree_builder)
{
  GVariantIter iter;
  GVariant *child = NULL;

  g_variant_iter_init(&iter, children);
  while ((child = g_variant_iter_next_value(&iter)) != NULL) {
    GVariant *tuple = NULL;

    if (g_variant_is_of_type(child, G_VARIANT_TYPE_VARIANT)) {
      tuple = g_variant_get_variant(child);
    } else {
      tuple = g_variant_ref(child);
    }

    dbusmenu_collect_node(tuple,
                          parent_id,
                          top_level,
                          position,
                          top_builder,
                          tree_builder);

    g_variant_unref(tuple);
    g_variant_unref(child);
  }
}

static gboolean
fetch_from_dbusmenu(GVariant *context,
                    GVariant **top_level_out,
                    GVariant **menu_tree_out,
                    GError **error)
{
  g_autoptr(GDBusProxy) menu_proxy = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) root = NULL;
  g_autoptr(GVariant) children = NULL;
  GVariantBuilder top_builder;
  GVariantBuilder tree_builder;
  guint position = 0;

  if (!dbusmenu_get_menu_proxy(context, &menu_proxy, error)) {
    return FALSE;
  }

  reply = g_dbus_proxy_call_sync(menu_proxy,
                                 "GetLayout",
                                 g_variant_new("(ii@as)",
                                               0,
                                               -1,
                                               g_variant_new_strv(NULL, 0)),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 error);
  if (reply == NULL) {
    return FALSE;
  }

  if (g_variant_n_children(reply) < 2) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Invalid GetLayout reply from dbusmenu provider");
    return FALSE;
  }

  root = g_variant_get_child_value(reply, 1);
  if (g_variant_n_children(root) < 3) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Invalid dbusmenu root node");
    return FALSE;
  }

  children = g_variant_get_child_value(root, 2);

  g_variant_builder_init(&top_builder, G_VARIANT_TYPE("aa{sv}"));
  g_variant_builder_init(&tree_builder, G_VARIANT_TYPE("aa{sv}"));

  if (children != NULL && g_variant_is_of_type(children, G_VARIANT_TYPE("av"))) {
    dbusmenu_collect_children(children,
                              "",
                              TRUE,
                              &position,
                              &top_builder,
                              &tree_builder);
  }

  *top_level_out = g_variant_ref_sink(g_variant_builder_end(&top_builder));
  *menu_tree_out = g_variant_ref_sink(g_variant_builder_end(&tree_builder));

  return TRUE;
}

static gchar *
gtk_section_key_from_variant(GVariant *section)
{
  gint64 first = 0;
  gint64 second = 0;

  if (!variant_child_as_int64(section, 0, &first) ||
      !variant_child_as_int64(section, 1, &second)) {
    return NULL;
  }

  return g_strdup_printf("%" G_GINT64_FORMAT ":%" G_GINT64_FORMAT, first, second);
}

static void
gtk_collect_section(GHashTable *section_map,
                    const gchar *section_key,
                    const gchar *parent_id,
                    guint depth,
                    guint *position,
                    GVariantBuilder *top_builder,
                    GVariantBuilder *tree_builder)
{
  GVariant *entries = NULL;
  GVariantIter iter;
  GVariant *entry = NULL;
  guint index = 0;

  if (depth > 64) {
    return;
  }

  entries = g_hash_table_lookup(section_map, section_key);
  if (entries == NULL || !g_variant_is_of_type(entries, G_VARIANT_TYPE_ARRAY)) {
    return;
  }

  g_variant_iter_init(&iter, entries);
  while ((entry = g_variant_iter_next_value(&iter)) != NULL) {
    GVariant *dict = NULL;
    const gchar *raw_label = NULL;
    const gchar *action = NULL;
    gchar *label = NULL;
    gchar *mnemonic = NULL;
    gchar *next_parent = NULL;
    gboolean enabled = TRUE;
    gboolean visible = TRUE;

    if (g_variant_is_of_type(entry, G_VARIANT_TYPE_VARIANT)) {
      dict = g_variant_get_variant(entry);
    } else {
      dict = g_variant_ref(entry);
    }

    if (!g_variant_is_of_type(dict, G_VARIANT_TYPE("a{sv}"))) {
      g_variant_unref(dict);
      g_variant_unref(entry);
      index++;
      continue;
    }

    dict_lookup_string(dict, "label", &raw_label);
    dict_lookup_string(dict, "action", &action);
    enabled = dict_lookup_boolean(dict, "enabled", TRUE);
    visible = dict_lookup_boolean(dict, "visible", TRUE);

    if (raw_label != NULL && raw_label[0] != '\0') {
      g_autofree gchar *node_id = NULL;

      normalize_label_and_mnemonic(raw_label, &label, &mnemonic);

      if (action != NULL && action[0] != '\0') {
        node_id = g_strdup(action);
      } else {
        node_id = g_strdup_printf("gtk:%s:%u", section_key, index);
      }

      g_variant_builder_add_value(tree_builder,
                                  build_menu_node(node_id,
                                                  parent_id == NULL ? "" : parent_id,
                                                  label,
                                                  enabled,
                                                  visible,
                                                  "",
                                                  FALSE,
                                                  "",
                                                  FALSE,
                                                  (*position)++));

      if (depth == 0) {
        g_variant_builder_add_value(top_builder,
                                    build_top_level_item(node_id,
                                                         label,
                                                         enabled,
                                                         visible,
                                                         mnemonic));
      }

      next_parent = g_strdup(node_id);
      g_free(label);
      g_free(mnemonic);
    } else {
      next_parent = g_strdup(parent_id == NULL ? "" : parent_id);
    }

    {
      g_autoptr(GVariant) submenu = g_variant_lookup_value(dict, ":submenu", NULL);
      if (submenu != NULL) {
        g_autofree gchar *sub_key = gtk_section_key_from_variant(submenu);
        if (sub_key != NULL) {
          gtk_collect_section(section_map,
                              sub_key,
                              next_parent,
                              depth + 1,
                              position,
                              top_builder,
                              tree_builder);
        }
      }
    }

    {
      g_autoptr(GVariant) section = g_variant_lookup_value(dict, ":section", NULL);
      if (section != NULL) {
        g_autofree gchar *sub_key = gtk_section_key_from_variant(section);
        if (sub_key != NULL) {
          gtk_collect_section(section_map,
                              sub_key,
                              next_parent,
                              depth + 1,
                              position,
                              top_builder,
                              tree_builder);
        }
      }
    }

    g_free(next_parent);
    g_variant_unref(dict);
    g_variant_unref(entry);
    index++;
  }
}

static gboolean
fetch_from_gtk_exporter(GVariant *context,
                        GVariant **top_level_out,
                        GVariant **menu_tree_out,
                        GError **error)
{
  const gchar *bus_name = NULL;
  const gchar *menu_path = NULL;
  g_autoptr(GDBusProxy) menus_proxy = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) sections = NULL;
  GHashTable *section_map;
  GVariantIter iter;
  GVariant *section = NULL;
  GVariantBuilder ids_builder;
  GVariantBuilder top_builder;
  GVariantBuilder tree_builder;
  guint i;
  guint position = 0;

  if (!context_lookup_string(context, "gtk_unique_bus_name", &bus_name)) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Missing gtk_unique_bus_name in active window context");
    return FALSE;
  }

  if (!context_lookup_string(context, "gtk_menubar_object_path", &menu_path) &&
      !context_lookup_string(context, "gtk_app_menu_object_path", &menu_path)) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Missing gtk_menubar_object_path in active window context");
    return FALSE;
  }

  menus_proxy = new_proxy(bus_name, menu_path, "org.gtk.Menus", error);
  if (menus_proxy == NULL) {
    return FALSE;
  }

  g_variant_builder_init(&ids_builder, G_VARIANT_TYPE("ai"));
  for (i = 0; i < 256; i++) {
    g_variant_builder_add(&ids_builder, "i", (gint32)i);
  }

  reply = g_dbus_proxy_call_sync(menus_proxy,
                                 "Start",
                                 g_variant_new("(@ai)", g_variant_builder_end(&ids_builder)),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 error);
  if (reply == NULL) {
    return FALSE;
  }

  /* Best effort; ignored when unsupported by provider */
  g_dbus_proxy_call_sync(menus_proxy,
                         "End",
                         g_variant_new("(@ai)", g_variant_new_array(G_VARIANT_TYPE_INT32, NULL, 0)),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         NULL);

  if (g_variant_n_children(reply) == 0) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Invalid Start reply from gtk menu exporter");
    return FALSE;
  }

  sections = g_variant_get_child_value(reply, 0);
  if (!g_variant_is_of_type(sections, G_VARIANT_TYPE_ARRAY)) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "GTK Start reply has invalid sections type");
    return FALSE;
  }

  section_map = g_hash_table_new_full(g_str_hash,
                                      g_str_equal,
                                      g_free,
                                      (GDestroyNotify)g_variant_unref);

  g_variant_iter_init(&iter, sections);
  while ((section = g_variant_iter_next_value(&iter)) != NULL) {
    g_autofree gchar *key = NULL;
    g_autoptr(GVariant) entries = NULL;

    key = gtk_section_key_from_variant(section);
    if (key == NULL || g_variant_n_children(section) < 3) {
      g_variant_unref(section);
      continue;
    }

    entries = g_variant_get_child_value(section, 2);
    if (entries != NULL && g_variant_is_of_type(entries, G_VARIANT_TYPE_ARRAY)) {
      g_hash_table_replace(section_map, g_steal_pointer(&key), g_variant_ref(entries));
    }

    g_variant_unref(section);
  }

  g_variant_builder_init(&top_builder, G_VARIANT_TYPE("aa{sv}"));
  g_variant_builder_init(&tree_builder, G_VARIANT_TYPE("aa{sv}"));

  gtk_collect_section(section_map,
                      "0:0",
                      "",
                      0,
                      &position,
                      &top_builder,
                      &tree_builder);

  g_hash_table_unref(section_map);

  *top_level_out = g_variant_ref_sink(g_variant_builder_end(&top_builder));
  *menu_tree_out = g_variant_ref_sink(g_variant_builder_end(&tree_builder));

  return TRUE;
}

static ProviderKind
choose_provider(GVariant *context)
{
  const gchar *dummy = NULL;
  gint64 xid = 0;

  if (context_lookup_string(context, "gtk_unique_bus_name", &dummy) &&
      (context_lookup_string(context, "gtk_menubar_object_path", &dummy) ||
       context_lookup_string(context, "gtk_app_menu_object_path", &dummy))) {
    return PROVIDER_GTK;
  }

  if (context_lookup_int64(context, "xid", &xid) && xid > 0) {
    return PROVIDER_DBUSMENU;
  }

  return PROVIDER_NONE;
}

static gboolean
activate_gtk_action(GVariant *context,
                    const gchar *full_action,
                    GError **error)
{
  const gchar *bus_name = NULL;
  const gchar *path_key = NULL;
  const gchar *object_path = NULL;
  const gchar *action_name = NULL;
  g_autoptr(GDBusProxy) actions_proxy = NULL;
  g_autoptr(GVariant) reply = NULL;

  if (!context_lookup_string(context, "gtk_unique_bus_name", &bus_name)) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Missing gtk_unique_bus_name in active window context");
    return FALSE;
  }

  if (g_str_has_prefix(full_action, "app.")) {
    path_key = "gtk_application_object_path";
    action_name = full_action + 4;
  } else if (g_str_has_prefix(full_action, "win.")) {
    path_key = "gtk_window_object_path";
    action_name = full_action + 4;
  } else if (g_str_has_prefix(full_action, "unity.")) {
    path_key = "gtk_menubar_object_path";
    action_name = full_action + 6;
  } else {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_NOT_FOUND,
                "Action '%s' is not a GTK action id",
                full_action);
    return FALSE;
  }

  if (!context_lookup_string(context, path_key, &object_path)) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_INVALID_ARGUMENT,
                "Missing %s in active window context",
                path_key);
    return FALSE;
  }

  actions_proxy = new_proxy(bus_name, object_path, "org.gtk.Actions", error);
  if (actions_proxy == NULL) {
    return FALSE;
  }

  reply = g_dbus_proxy_call_sync(actions_proxy,
                                 "Activate",
                                 g_variant_new("(s@av@a{sv})",
                                               action_name,
                                               g_variant_new_array(G_VARIANT_TYPE("v"), NULL, 0),
                                               empty_dict_variant()),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 error);

  return reply != NULL;
}

static gboolean
activate_dbusmenu_item(GVariant *context,
                       const gchar *target_id,
                       gboolean is_top_level,
                       GError **error)
{
  g_autoptr(GDBusProxy) menu_proxy = NULL;
  g_autoptr(GVariant) reply = NULL;
  gchar *end = NULL;
  gint64 parsed_id;
  guint32 timestamp;

  parsed_id = g_ascii_strtoll(target_id, &end, 10);
  if (end == NULL || *end != '\0') {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_NOT_FOUND,
                "Target '%s' is not a dbusmenu item id",
                target_id);
    return FALSE;
  }

  if (!dbusmenu_get_menu_proxy(context, &menu_proxy, error)) {
    return FALSE;
  }

  timestamp = (guint32)(g_get_real_time() / 1000);
  reply = g_dbus_proxy_call_sync(menu_proxy,
                                 "Event",
                                 g_variant_new("(isvu)",
                                               (gint32)parsed_id,
                                               is_top_level ? "opened" : "clicked",
                                               g_variant_new_int32(0),
                                               timestamp),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 error);
  return reply != NULL;
}

static gboolean
active_window_uid_matches(FildemDaemon *daemon,
                          const gchar *window_uid)
{
  const gchar *active_uid = NULL;

  if (daemon->active_window == NULL ||
      !context_lookup_string(daemon->active_window, "window_uid", &active_uid)) {
    return FALSE;
  }

  return g_strcmp0(active_uid, window_uid) == 0;
}

static gboolean
activate_target(FildemDaemon *daemon,
                const gchar *window_uid,
                const gchar *target_id,
                gboolean is_top_level,
                GError **error)
{
  ProviderKind provider;

  if (!active_window_uid_matches(daemon, window_uid)) {
    g_set_error(error,
                FILDEM_ERROR,
                FILDEM_ERROR_NOT_FOUND,
                "Window %s is not active",
                window_uid);
    return FALSE;
  }

  if (g_str_has_prefix(target_id, "action:")) {
    emit_activation_requested(daemon, window_uid, target_id);
    return TRUE;
  }

  provider = choose_provider(daemon->active_window);

  if (provider == PROVIDER_GTK) {
    if (activate_gtk_action(daemon->active_window, target_id, error)) {
      return TRUE;
    }

    g_clear_error(error);
    return activate_dbusmenu_item(daemon->active_window, target_id, is_top_level, error);
  }

  if (provider == PROVIDER_DBUSMENU) {
    return activate_dbusmenu_item(daemon->active_window, target_id, is_top_level, error);
  }

  g_set_error(error,
              FILDEM_ERROR,
              FILDEM_ERROR_NOT_FOUND,
              "No menu provider available for active window");
  return FALSE;
}

static void
refresh_window_menu_state(FildemDaemon *daemon)
{
  const gchar *window_uid = NULL;
  g_autoptr(GVariant) top_level = NULL;
  g_autoptr(GVariant) menu_tree = NULL;
  g_autoptr(GError) error = NULL;
  ProviderKind provider;

  if (daemon->active_window == NULL ||
      !context_lookup_string(daemon->active_window, "window_uid", &window_uid)) {
    return;
  }

  provider = choose_provider(daemon->active_window);

  if (provider == PROVIDER_GTK) {
    if (!fetch_from_gtk_exporter(daemon->active_window, &top_level, &menu_tree, &error)) {
      g_debug("gtk menu provider unavailable: %s", error->message);
      g_clear_error(&error);
      fetch_from_dbusmenu(daemon->active_window, &top_level, &menu_tree, &error);
    }
  } else if (provider == PROVIDER_DBUSMENU) {
    fetch_from_dbusmenu(daemon->active_window, &top_level, &menu_tree, &error);
  }

  if (top_level == NULL || menu_tree == NULL) {
    if (error != NULL) {
      g_debug("menu provider fetch failed for window %s: %s", window_uid, error->message);
      g_clear_error(&error);
    }

    top_level = g_variant_ref_sink(fildem_empty_top_level_items());
    menu_tree = g_variant_ref_sink(fildem_empty_menu_tree());
  }

  publish_menu_state(daemon, window_uid, top_level, menu_tree);
}

static void
handle_window_methods(FildemDaemon *daemon,
                      const gchar *method_name,
                      GVariant *parameters,
                      GDBusMethodInvocation *invocation)
{
  if (g_strcmp0(method_name, "UpdateActiveWindow") == 0) {
    GVariant *context = NULL;
    g_autoptr(GError) error = NULL;

    g_variant_get(parameters, "(@a{sv})", &context);
    if (!fildem_validate_window_context(context, &error)) {
      g_variant_unref(context);
      return_dbus_error(invocation, g_steal_pointer(&error), "invalid window context");
      return;
    }

    g_clear_pointer(&daemon->active_window, g_variant_unref);
    daemon->active_window = g_variant_ref_sink(context);

    emit_signal(daemon,
                "org.fildem.v1.Window",
                "ActiveWindowChanged",
                g_variant_new("(@a{sv})", g_variant_ref(daemon->active_window)));

    refresh_window_menu_state(daemon);

    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  if (g_strcmp0(method_name, "GetActiveWindow") == 0) {
    GVariant *context = daemon->active_window == NULL ?
      empty_window_context() :
      g_variant_ref(daemon->active_window);

    g_dbus_method_invocation_return_value(invocation,
                                          g_variant_new("(@a{sv})", context));
    return;
  }

  g_dbus_method_invocation_return_dbus_error(invocation,
                                             "org.fildem.Error.UnknownMethod",
                                             "Unknown method on org.fildem.v1.Window");
}

static void
handle_top_level_methods(FildemDaemon *daemon,
                         const gchar *method_name,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation)
{
  if (g_strcmp0(method_name, "SetTopLevel") == 0) {
    const gchar *window_uid = NULL;
    GVariant *items = NULL;
    g_autoptr(GError) error = NULL;

    g_variant_get(parameters, "(&s@aa{sv})", &window_uid, &items);
    if (!fildem_validate_top_level_items(items, &error)) {
      g_variant_unref(items);
      return_dbus_error(invocation, g_steal_pointer(&error), "invalid top-level payload");
      return;
    }

    fildem_menu_registry_set_top_level(daemon->registry, window_uid, items);

    emit_signal(daemon,
                "org.fildem.v1.TopLevel",
                "TopLevelChanged",
                g_variant_new("(&s@aa{sv})", window_uid, g_variant_ref(items)));

    g_variant_unref(items);
    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  if (g_strcmp0(method_name, "GetTopLevel") == 0) {
    const gchar *window_uid = NULL;
    GVariant *items;

    g_variant_get(parameters, "(&s)", &window_uid);
    items = fildem_menu_registry_get_top_level(daemon->registry, window_uid);

    g_dbus_method_invocation_return_value(invocation,
                                          g_variant_new("(@aa{sv})", items));
    return;
  }

  g_dbus_method_invocation_return_dbus_error(invocation,
                                             "org.fildem.Error.UnknownMethod",
                                             "Unknown method on org.fildem.v1.TopLevel");
}

static void
handle_menu_tree_methods(FildemDaemon *daemon,
                         const gchar *method_name,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation)
{
  if (g_strcmp0(method_name, "SetMenuTree") == 0) {
    const gchar *window_uid = NULL;
    GVariant *nodes = NULL;
    g_autoptr(GError) error = NULL;

    g_variant_get(parameters, "(&s@aa{sv})", &window_uid, &nodes);
    if (!fildem_validate_menu_tree(nodes, &error)) {
      g_variant_unref(nodes);
      return_dbus_error(invocation, g_steal_pointer(&error), "invalid menu tree payload");
      return;
    }

    fildem_menu_cache_set_tree(daemon->cache, window_uid, nodes);

    emit_signal(daemon,
                "org.fildem.v1.MenuTree",
                "MenuTreeChanged",
                g_variant_new("(&s)", window_uid));

    g_variant_unref(nodes);
    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  if (g_strcmp0(method_name, "GetMenuTree") == 0) {
    const gchar *window_uid = NULL;
    GVariant *nodes;

    g_variant_get(parameters, "(&s)", &window_uid);
    nodes = fildem_menu_cache_get_tree(daemon->cache, window_uid);
    g_dbus_method_invocation_return_value(invocation,
                                          g_variant_new("(@aa{sv})", nodes));
    return;
  }

  if (g_strcmp0(method_name, "InvalidateMenuTree") == 0) {
    const gchar *window_uid = NULL;

    g_variant_get(parameters, "(&s)", &window_uid);
    fildem_menu_cache_invalidate(daemon->cache, window_uid);

    emit_signal(daemon,
                "org.fildem.v1.MenuTree",
                "MenuTreeChanged",
                g_variant_new("(&s)", window_uid));

    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  g_dbus_method_invocation_return_dbus_error(invocation,
                                             "org.fildem.Error.UnknownMethod",
                                             "Unknown method on org.fildem.v1.MenuTree");
}

static void
handle_activation_methods(FildemDaemon *daemon,
                          const gchar *method_name,
                          GVariant *parameters,
                          GDBusMethodInvocation *invocation)
{
  const gchar *window_uid = NULL;
  const gchar *target_id = NULL;
  gboolean is_top_level;
  g_autoptr(GError) error = NULL;

  if (g_strcmp0(method_name, "ActivateTopLevel") != 0 &&
      g_strcmp0(method_name, "ActivateMenuItem") != 0) {
    g_dbus_method_invocation_return_dbus_error(invocation,
                                               "org.fildem.Error.UnknownMethod",
                                               "Unknown method on org.fildem.v1.Activation");
    return;
  }

  g_variant_get(parameters, "(&s&s)", &window_uid, &target_id);
  is_top_level = g_strcmp0(method_name, "ActivateTopLevel") == 0;

  if (!activate_target(daemon, window_uid, target_id, is_top_level, &error)) {
    return_dbus_error(invocation, g_steal_pointer(&error), "activation failed");
    return;
  }

  g_dbus_method_invocation_return_value(invocation, NULL);
}

static void
handle_window_actions_methods(FildemDaemon *daemon,
                              const gchar *method_name,
                              GVariant *parameters,
                              GDBusMethodInvocation *invocation)
{
  if (g_strcmp0(method_name, "SetWindowActions") == 0) {
    const gchar *window_uid = NULL;
    GVariant *actions = NULL;

    g_variant_get(parameters, "(&s@as)", &window_uid, &actions);
    fildem_menu_registry_set_actions(daemon->registry, window_uid, actions);

    emit_signal(daemon,
                "org.fildem.v1.WindowActions",
                "WindowActionsChanged",
                g_variant_new("(&s@as)", window_uid, g_variant_ref(actions)));

    g_variant_unref(actions);
    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  if (g_strcmp0(method_name, "ListWindowActions") == 0) {
    const gchar *window_uid = NULL;
    GVariant *actions;

    g_variant_get(parameters, "(&s)", &window_uid);
    actions = fildem_menu_registry_get_actions(daemon->registry, window_uid);

    g_dbus_method_invocation_return_value(invocation,
                                          g_variant_new("(@as)", actions));
    return;
  }

  if (g_strcmp0(method_name, "ActivateWindowAction") == 0) {
    const gchar *window_uid = NULL;
    const gchar *action_id = NULL;
    g_autofree gchar *entry_id = NULL;

    g_variant_get(parameters, "(&s&s)", &window_uid, &action_id);
    entry_id = g_strdup_printf("action:%s", action_id);
    emit_activation_requested(daemon, window_uid, entry_id);

    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  g_dbus_method_invocation_return_dbus_error(invocation,
                                             "org.fildem.Error.UnknownMethod",
                                             "Unknown method on org.fildem.v1.WindowActions");
}

static void
handle_hud_methods(FildemDaemon *daemon,
                   const gchar *method_name,
                   GVariant *parameters,
                   GDBusMethodInvocation *invocation)
{
  if (g_strcmp0(method_name, "RequestHud") == 0) {
    const gchar *window_uid = "";

    g_variant_get(parameters, "(&s)", &window_uid);
    emit_signal(daemon,
                "org.fildem.v1.Hud",
                "HudRequested",
                g_variant_new("(&s)", window_uid));

    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  if (g_strcmp0(method_name, "Query") == 0) {
    const gchar *window_uid = NULL;
    const gchar *term = NULL;
    GVariant *nodes;
    GVariant *actions;
    GVariant *results;

    g_variant_get(parameters, "(&s&s)", &window_uid, &term);

    nodes = fildem_menu_cache_get_tree(daemon->cache, window_uid);
    actions = fildem_menu_registry_get_actions(daemon->registry, window_uid);
    results = fildem_menu_model_query(nodes, actions, term);

    g_variant_unref(nodes);
    g_variant_unref(actions);

    g_dbus_method_invocation_return_value(invocation,
                                          g_variant_new("(@aa{sv})", results));
    return;
  }

  if (g_strcmp0(method_name, "Execute") == 0) {
    const gchar *window_uid = NULL;
    const gchar *entry_id = NULL;
    g_autoptr(GError) error = NULL;

    g_variant_get(parameters, "(&s&s)", &window_uid, &entry_id);

    if (!activate_target(daemon, window_uid, entry_id, FALSE, &error)) {
      return_dbus_error(invocation, g_steal_pointer(&error), "execute failed");
      return;
    }

    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  g_dbus_method_invocation_return_dbus_error(invocation,
                                             "org.fildem.Error.UnknownMethod",
                                             "Unknown method on org.fildem.v1.Hud");
}

static void
handle_method_call(GDBusConnection *connection,
                   const gchar *sender,
                   const gchar *object_path,
                   const gchar *interface_name,
                   const gchar *method_name,
                   GVariant *parameters,
                   GDBusMethodInvocation *invocation,
                   gpointer user_data)
{
  FildemDaemon *daemon = user_data;

  if (g_strcmp0(interface_name, "org.fildem.v1.Window") == 0) {
    handle_window_methods(daemon, method_name, parameters, invocation);
    return;
  }

  if (g_strcmp0(interface_name, "org.fildem.v1.TopLevel") == 0) {
    handle_top_level_methods(daemon, method_name, parameters, invocation);
    return;
  }

  if (g_strcmp0(interface_name, "org.fildem.v1.MenuTree") == 0) {
    handle_menu_tree_methods(daemon, method_name, parameters, invocation);
    return;
  }

  if (g_strcmp0(interface_name, "org.fildem.v1.Activation") == 0) {
    handle_activation_methods(daemon, method_name, parameters, invocation);
    return;
  }

  if (g_strcmp0(interface_name, "org.fildem.v1.WindowActions") == 0) {
    handle_window_actions_methods(daemon, method_name, parameters, invocation);
    return;
  }

  if (g_strcmp0(interface_name, "org.fildem.v1.Hud") == 0) {
    handle_hud_methods(daemon, method_name, parameters, invocation);
    return;
  }

  g_dbus_method_invocation_return_dbus_error(invocation,
                                             "org.fildem.Error.UnknownInterface",
                                             "Unknown interface");

  (void)connection;
  (void)sender;
  (void)object_path;
}

static const GDBusInterfaceVTable interface_vtable = {
  .method_call = handle_method_call,
  .get_property = NULL,
  .set_property = NULL,
};

static gboolean
register_interfaces(FildemDaemon *daemon, GError **error)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS(INTERFACE_FILES); i++) {
    GDBusInterfaceInfo *interface_info;

    daemon->interfaces[i].name = INTERFACE_FILES[i][0];
    daemon->interfaces[i].xml_file = INTERFACE_FILES[i][1];

    if (!load_interface_info(&daemon->interfaces[i], error)) {
      return FALSE;
    }

    interface_info = daemon->interfaces[i].node_info->interfaces[0];
    daemon->interfaces[i].registration_id = g_dbus_connection_register_object(
      daemon->connection,
      fildem_object_path(),
      interface_info,
      &interface_vtable,
      daemon,
      NULL,
      error);

    if (daemon->interfaces[i].registration_id == 0) {
      return FALSE;
    }
  }

  return TRUE;
}

static void
unregister_interfaces(FildemDaemon *daemon)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS(daemon->interfaces); i++) {
    if (daemon->connection != NULL && daemon->interfaces[i].registration_id != 0) {
      g_dbus_connection_unregister_object(daemon->connection,
                                          daemon->interfaces[i].registration_id);
      daemon->interfaces[i].registration_id = 0;
    }

    if (daemon->interfaces[i].node_info != NULL) {
      g_dbus_node_info_unref(daemon->interfaces[i].node_info);
      daemon->interfaces[i].node_info = NULL;
    }
  }
}

static void
on_bus_acquired(GDBusConnection *connection,
                const gchar *name,
                gpointer user_data)
{
  FildemDaemon *daemon = user_data;
  g_autoptr(GError) error = NULL;

  daemon->connection = g_object_ref(connection);

  if (!register_interfaces(daemon, &error)) {
    g_critical("failed to register dbus interfaces: %s", error->message);
    g_main_loop_quit(daemon->loop);
    return;
  }

  g_message("fildemd bus acquired: %s", name);
}

static void
on_name_acquired(GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
  g_message("fildemd name acquired: %s", name);
  (void)connection;
  (void)user_data;
}

static void
on_name_lost(GDBusConnection *connection,
             const gchar *name,
             gpointer user_data)
{
  FildemDaemon *daemon = user_data;
  g_warning("fildemd lost bus name: %s", name);
  g_main_loop_quit(daemon->loop);
  (void)connection;
}

int
main(void)
{
  FildemDaemon daemon = {0};

  daemon.registry = fildem_menu_registry_new();
  daemon.cache = fildem_menu_cache_new();
  daemon.loop = g_main_loop_new(NULL, FALSE);

  daemon.owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                   fildem_bus_name(),
                                   G_BUS_NAME_OWNER_FLAGS_NONE,
                                   on_bus_acquired,
                                   on_name_acquired,
                                   on_name_lost,
                                   &daemon,
                                   NULL);

  g_main_loop_run(daemon.loop);

  if (daemon.owner_id != 0) {
    g_bus_unown_name(daemon.owner_id);
  }

  unregister_interfaces(&daemon);
  g_clear_object(&daemon.connection);
  g_clear_pointer(&daemon.active_window, g_variant_unref);
  g_clear_object(&daemon.registry);
  g_clear_object(&daemon.cache);
  g_clear_pointer(&daemon.loop, g_main_loop_unref);

  return 0;
}
