#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define FILDEM_TYPE_MENU_REGISTRY (fildem_menu_registry_get_type())

G_DECLARE_FINAL_TYPE(FildemMenuRegistry, fildem_menu_registry, FILDEM, MENU_REGISTRY, GObject)

FildemMenuRegistry *fildem_menu_registry_new(void);

void fildem_menu_registry_set_top_level(FildemMenuRegistry *self,
                                        const gchar *window_uid,
                                        GVariant *items);
GVariant *fildem_menu_registry_get_top_level(FildemMenuRegistry *self,
                                             const gchar *window_uid);

void fildem_menu_registry_set_actions(FildemMenuRegistry *self,
                                      const gchar *window_uid,
                                      GVariant *actions);
GVariant *fildem_menu_registry_get_actions(FildemMenuRegistry *self,
                                           const gchar *window_uid);

G_END_DECLS
