#include <gio/gio.h>
#include <gtk/gtk.h>

#include "fildem-core.h"

typedef struct {
  GtkApplication *app;
  GtkWidget *window;
  GtkSearchEntry *search_entry;
  GtkStringList *string_list;
  GPtrArray *entry_ids;
  GDBusProxy *window_proxy;
  GDBusProxy *hud_proxy;
  gchar *window_uid;
} HudState;

static void
refresh_results(HudState *state);

static void
clear_results(HudState *state)
{
  guint n_items = g_list_model_get_n_items(G_LIST_MODEL(state->string_list));

  while (n_items > 0) {
    gtk_string_list_remove(state->string_list, n_items - 1);
    n_items--;
  }
  g_ptr_array_set_size(state->entry_ids, 0);
}

static void
append_result(HudState *state, const gchar *id, const gchar *label)
{
  gtk_string_list_append(state->string_list, label);
  g_ptr_array_add(state->entry_ids, g_strdup(id));
}

static const gchar *
resolve_window_uid(HudState *state)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  GVariant *context = NULL;
  const gchar *window_uid = NULL;

  reply = g_dbus_proxy_call_sync(state->window_proxy,
                                 "GetActiveWindow",
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
  if (reply == NULL) {
    g_warning("fildemhud GetActiveWindow failed: %s", error->message);
    return "";
  }

  g_variant_get(reply, "(@a{sv})", &context);
  g_variant_lookup(context, "window_uid", "&s", &window_uid);

  g_free(state->window_uid);
  state->window_uid = g_strdup(window_uid == NULL ? "" : window_uid);
  g_variant_unref(context);

  return state->window_uid;
}

static void
refresh_results(HudState *state)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) results = NULL;
  GVariantIter iter;
  GVariant *item = NULL;
  const gchar *term;
  const gchar *window_uid;

  term = gtk_editable_get_text(GTK_EDITABLE(state->search_entry));
  window_uid = resolve_window_uid(state);

  reply = g_dbus_proxy_call_sync(state->hud_proxy,
                                 "Query",
                                 g_variant_new("(ss)", window_uid, term),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
  if (reply == NULL) {
    g_warning("fildemhud Query failed: %s", error->message);
    return;
  }

  g_variant_get(reply, "(@aa{sv})", &results);

  clear_results(state);

  g_variant_iter_init(&iter, results);
  while ((item = g_variant_iter_next_value(&iter)) != NULL) {
    const gchar *id = NULL;
    const gchar *label = NULL;

    g_variant_lookup(item, "id", "&s", &id);
    g_variant_lookup(item, "label", "&s", &label);

    if (id != NULL && label != NULL) {
      append_result(state, id, label);
    }

    g_variant_unref(item);
  }
}

static void
on_window_destroy(GtkWidget *widget, gpointer user_data)
{
  HudState *state = user_data;
  state->window = NULL;
  state->search_entry = NULL;
  (void)widget;
}

static void
on_search_changed(GtkEditable *editable, gpointer user_data)
{
  HudState *state = user_data;
  refresh_results(state);
  (void)editable;
}

static void
on_list_activated(GtkListView *list_view, guint position, gpointer user_data)
{
  HudState *state = user_data;
  const gchar *entry_id;
  const gchar *window_uid;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ignored = NULL;

  if (position >= state->entry_ids->len) {
    return;
  }

  entry_id = g_ptr_array_index(state->entry_ids, position);
  window_uid = resolve_window_uid(state);

  ignored = g_dbus_proxy_call_sync(state->hud_proxy,
                                   "Execute",
                                   g_variant_new("(ss)", window_uid, entry_id),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  if (ignored == NULL) {
    g_warning("fildemhud Execute failed: %s", error->message);
    return;
  }

  gtk_window_close(GTK_WINDOW(state->window));
  (void)list_view;
}

static void
on_list_item_setup(GtkSignalListItemFactory *factory,
                   GtkListItem *item,
                   gpointer user_data)
{
  GtkWidget *label = gtk_label_new(NULL);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
  gtk_list_item_set_child(item, label);

  (void)factory;
  (void)user_data;
}

static void
on_list_item_bind(GtkSignalListItemFactory *factory,
                  GtkListItem *item,
                  gpointer user_data)
{
  GtkWidget *label = gtk_list_item_get_child(item);
  GObject *obj = gtk_list_item_get_item(item);

  gtk_label_set_label(GTK_LABEL(label), gtk_string_object_get_string(GTK_STRING_OBJECT(obj)));

  (void)factory;
  (void)user_data;
}

static GtkWidget *
create_list_view(HudState *state)
{
  GtkSelectionModel *selection;
  GtkListItemFactory *factory;

  selection = GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(state->string_list)));
  factory = gtk_signal_list_item_factory_new();

  g_signal_connect(factory,
                   "setup",
                   G_CALLBACK(on_list_item_setup),
                   NULL);

  g_signal_connect(factory,
                   "bind",
                   G_CALLBACK(on_list_item_bind),
                   NULL);

  return gtk_list_view_new(selection, factory);
}

static void
on_activate(GtkApplication *app, gpointer user_data)
{
  HudState *state = user_data;
  GtkWidget *box;
  GtkWidget *list_view;

  if (state->window != NULL) {
    refresh_results(state);
    gtk_window_present(GTK_WINDOW(state->window));
    return;
  }

  state->window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(state->window), "Fildem HUD");
  gtk_window_set_default_size(GTK_WINDOW(state->window), 640, 420);

  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top(box, 12);
  gtk_widget_set_margin_bottom(box, 12);
  gtk_widget_set_margin_start(box, 12);
  gtk_widget_set_margin_end(box, 12);

  state->search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
  gtk_editable_set_text(GTK_EDITABLE(state->search_entry), "");
  g_signal_connect(state->search_entry, "changed", G_CALLBACK(on_search_changed), state);

  list_view = create_list_view(state);
  g_signal_connect(list_view, "activate", G_CALLBACK(on_list_activated), state);

  gtk_box_append(GTK_BOX(box), GTK_WIDGET(state->search_entry));
  gtk_box_append(GTK_BOX(box), list_view);

  gtk_window_set_child(GTK_WINDOW(state->window), box);
  g_signal_connect(state->window, "destroy", G_CALLBACK(on_window_destroy), state);
  gtk_window_present(GTK_WINDOW(state->window));

  refresh_results(state);
}

static void
on_hud_proxy_signal(GDBusProxy *proxy,
                    const gchar *sender_name,
                    const gchar *signal_name,
                    GVariant *parameters,
                    gpointer user_data)
{
  HudState *state = user_data;
  const gchar *window_uid = NULL;

  if (g_strcmp0(signal_name, "HudRequested") != 0) {
    return;
  }

  g_variant_get(parameters, "(&s)", &window_uid);
  (void)window_uid;

  if (state->window != NULL) {
    refresh_results(state);
    gtk_window_present(GTK_WINDOW(state->window));
    return;
  }

  g_application_activate(G_APPLICATION(state->app));

  (void)proxy;
  (void)sender_name;
}

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
    g_error("failed to create proxy for %s: %s", interface_name, error->message);
  }

  return proxy;
}

int
main(int argc, char **argv)
{
  HudState state = {0};
  int status;

  state.app = gtk_application_new("org.fildem.hud", G_APPLICATION_DEFAULT_FLAGS);
  state.string_list = gtk_string_list_new(NULL);
  state.entry_ids = g_ptr_array_new_with_free_func(g_free);

  state.window_proxy = new_proxy("org.fildem.v1.Window");
  state.hud_proxy = new_proxy("org.fildem.v1.Hud");
  g_signal_connect(state.hud_proxy, "g-signal", G_CALLBACK(on_hud_proxy_signal), &state);

  g_signal_connect(state.app, "activate", G_CALLBACK(on_activate), &state);

  status = g_application_run(G_APPLICATION(state.app), argc, argv);

  g_clear_object(&state.window_proxy);
  g_clear_object(&state.hud_proxy);
  g_clear_object(&state.string_list);
  g_clear_object(&state.app);
  g_free(state.window_uid);
  g_ptr_array_free(state.entry_ids, TRUE);

  return status;
}
