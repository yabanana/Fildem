#include <gio/gio.h>
#include <glib.h>

#include "fildem-core.h"

static GDBusProxy *
new_proxy(const gchar *interface_name)
{
  g_autoptr(GError) error = NULL;
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        NULL,
                                        fildem_bus_name(),
                                        fildem_object_path(),
                                        interface_name,
                                        NULL,
                                        &error);
  if (proxy == NULL) {
    g_error("cannot connect to %s: %s", interface_name, error->message);
  }

  return proxy;
}

static gint
call_void_method(const gchar *interface_name,
                 const gchar *method_name,
                 GVariant *parameters)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ignored = NULL;

  proxy = new_proxy(interface_name);
  ignored = g_dbus_proxy_call_sync(proxy,
                                   method_name,
                                   parameters,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  if (ignored == NULL) {
    g_printerr("error: %s\n", error->message);
    return 1;
  }

  return 0;
}

static gint
print_active_window(void)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  GVariant *context = NULL;
  const gchar *window_uid = "";
  const gchar *app_id = "";
  const gchar *title = "";

  proxy = new_proxy("org.fildem.v1.Window");
  reply = g_dbus_proxy_call_sync(proxy,
                                 "GetActiveWindow",
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
  if (reply == NULL) {
    g_printerr("error: %s\n", error->message);
    return 1;
  }

  g_variant_get(reply, "(@a{sv})", &context);
  g_variant_lookup(context, "window_uid", "&s", &window_uid);
  g_variant_lookup(context, "app_id", "&s", &app_id);
  g_variant_lookup(context, "title", "&s", &title);

  g_print("window_uid=%s\napp_id=%s\ntitle=%s\n", window_uid, app_id, title);
  g_variant_unref(context);
  return 0;
}

static gint
print_top_level(const gchar *window_uid)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  GVariant *items = NULL;
  GVariantIter iter;
  GVariant *item = NULL;
  const gchar *id;
  const gchar *label;

  proxy = new_proxy("org.fildem.v1.TopLevel");
  reply = g_dbus_proxy_call_sync(proxy,
                                 "GetTopLevel",
                                 g_variant_new("(s)", window_uid),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
  if (reply == NULL) {
    g_printerr("error: %s\n", error->message);
    return 1;
  }

  g_variant_get(reply, "(@aa{sv})", &items);
  g_variant_iter_init(&iter, items);
  while ((item = g_variant_iter_next_value(&iter)) != NULL) {
    id = "";
    label = "";
    g_variant_lookup(item, "id", "&s", &id);
    g_variant_lookup(item, "label", "&s", &label);
    g_print("%s\t%s\n", id, label);
    g_variant_unref(item);
  }

  g_variant_unref(items);
  return 0;
}

static gint
print_menu_tree(const gchar *window_uid)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  GVariant *nodes = NULL;
  GVariantIter iter;
  GVariant *item = NULL;

  proxy = new_proxy("org.fildem.v1.MenuTree");
  reply = g_dbus_proxy_call_sync(proxy,
                                 "GetMenuTree",
                                 g_variant_new("(s)", window_uid),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
  if (reply == NULL) {
    g_printerr("error: %s\n", error->message);
    return 1;
  }

  g_variant_get(reply, "(@aa{sv})", &nodes);
  g_variant_iter_init(&iter, nodes);
  while ((item = g_variant_iter_next_value(&iter)) != NULL) {
    const gchar *id = "";
    const gchar *parent_id = "";
    const gchar *label = "";
    gboolean enabled = TRUE;
    gboolean visible = TRUE;

    g_variant_lookup(item, "id", "&s", &id);
    g_variant_lookup(item, "parent_id", "&s", &parent_id);
    g_variant_lookup(item, "label", "&s", &label);
    g_variant_lookup(item, "enabled", "b", &enabled);
    g_variant_lookup(item, "visible", "b", &visible);

    g_print("%s\tparent=%s\tenabled=%d\tvisible=%d\t%s\n",
            id,
            parent_id,
            enabled ? 1 : 0,
            visible ? 1 : 0,
            label);

    g_variant_unref(item);
  }

  g_variant_unref(nodes);
  return 0;
}

static gint
print_window_actions(const gchar *window_uid)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  GVariant *actions = NULL;
  GVariantIter iter;
  const gchar *action = NULL;

  proxy = new_proxy("org.fildem.v1.WindowActions");
  reply = g_dbus_proxy_call_sync(proxy,
                                 "ListWindowActions",
                                 g_variant_new("(s)", window_uid),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
  if (reply == NULL) {
    g_printerr("error: %s\n", error->message);
    return 1;
  }

  g_variant_get(reply, "(@as)", &actions);
  g_variant_iter_init(&iter, actions);
  while (g_variant_iter_next(&iter, "&s", &action)) {
    g_print("%s\n", action);
  }

  g_variant_unref(actions);
  return 0;
}

static gint
query_hud(const gchar *window_uid, const gchar *term)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  GVariant *results = NULL;
  GVariantIter iter;
  GVariant *item = NULL;

  proxy = new_proxy("org.fildem.v1.Hud");
  reply = g_dbus_proxy_call_sync(proxy,
                                 "Query",
                                 g_variant_new("(ss)", window_uid, term),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
  if (reply == NULL) {
    g_printerr("error: %s\n", error->message);
    return 1;
  }

  g_variant_get(reply, "(@aa{sv})", &results);
  g_variant_iter_init(&iter, results);
  while ((item = g_variant_iter_next_value(&iter)) != NULL) {
    const gchar *id = "";
    const gchar *label = "";
    const gchar *kind = "";

    g_variant_lookup(item, "id", "&s", &id);
    g_variant_lookup(item, "label", "&s", &label);
    g_variant_lookup(item, "kind", "&s", &kind);

    g_print("%s\t%s\t%s\n", id, kind, label);
    g_variant_unref(item);
  }

  g_variant_unref(results);
  return 0;
}

static void
print_usage(void)
{
  g_printerr("usage:\n");
  g_printerr("  fildemctl status\n");
  g_printerr("  fildemctl top-level <window_uid>\n");
  g_printerr("  fildemctl menu-tree <window_uid>\n");
  g_printerr("  fildemctl actions <window_uid>\n");
  g_printerr("  fildemctl activate-top-level <window_uid> <top_level_id>\n");
  g_printerr("  fildemctl activate-item <window_uid> <item_id>\n");
  g_printerr("  fildemctl activate-action <window_uid> <action_id>\n");
  g_printerr("  fildemctl hud-request <window_uid>\n");
  g_printerr("  fildemctl hud-query <window_uid> <term>\n");
  g_printerr("  fildemctl hud-exec <window_uid> <entry_id>\n");
}

int
main(int argc, char **argv)
{
  if (argc == 1 || g_strcmp0(argv[1], "status") == 0 || g_strcmp0(argv[1], "window") == 0) {
    return print_active_window();
  }

  if (g_strcmp0(argv[1], "top-level") == 0) {
    if (argc < 3) {
      g_printerr("usage: fildemctl top-level <window_uid>\n");
      return 2;
    }
    return print_top_level(argv[2]);
  }

  if (g_strcmp0(argv[1], "menu-tree") == 0) {
    if (argc < 3) {
      print_usage();
      return 2;
    }
    return print_menu_tree(argv[2]);
  }

  if (g_strcmp0(argv[1], "actions") == 0) {
    if (argc < 3) {
      print_usage();
      return 2;
    }
    return print_window_actions(argv[2]);
  }

  if (g_strcmp0(argv[1], "activate-top-level") == 0) {
    if (argc < 4) {
      print_usage();
      return 2;
    }
    return call_void_method("org.fildem.v1.Activation",
                            "ActivateTopLevel",
                            g_variant_new("(ss)", argv[2], argv[3]));
  }

  if (g_strcmp0(argv[1], "activate-item") == 0) {
    if (argc < 4) {
      print_usage();
      return 2;
    }
    return call_void_method("org.fildem.v1.Activation",
                            "ActivateMenuItem",
                            g_variant_new("(ss)", argv[2], argv[3]));
  }

  if (g_strcmp0(argv[1], "activate-action") == 0) {
    if (argc < 4) {
      print_usage();
      return 2;
    }
    return call_void_method("org.fildem.v1.WindowActions",
                            "ActivateWindowAction",
                            g_variant_new("(ss)", argv[2], argv[3]));
  }

  if (g_strcmp0(argv[1], "hud-query") == 0) {
    if (argc < 4) {
      print_usage();
      return 2;
    }
    return query_hud(argv[2], argv[3]);
  }

  if (g_strcmp0(argv[1], "hud-request") == 0) {
    if (argc < 3) {
      print_usage();
      return 2;
    }
    return call_void_method("org.fildem.v1.Hud",
                            "RequestHud",
                            g_variant_new("(s)", argv[2]));
  }

  if (g_strcmp0(argv[1], "hud-exec") == 0) {
    if (argc < 4) {
      print_usage();
      return 2;
    }
    return call_void_method("org.fildem.v1.Hud",
                            "Execute",
                            g_variant_new("(ss)", argv[2], argv[3]));
  }

  print_usage();
  return 2;
}
