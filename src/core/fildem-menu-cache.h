#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define FILDEM_TYPE_MENU_CACHE (fildem_menu_cache_get_type())

G_DECLARE_FINAL_TYPE(FildemMenuCache, fildem_menu_cache, FILDEM, MENU_CACHE, GObject)

FildemMenuCache *fildem_menu_cache_new(void);

void fildem_menu_cache_set_tree(FildemMenuCache *self,
                                const gchar *window_uid,
                                GVariant *nodes);
GVariant *fildem_menu_cache_get_tree(FildemMenuCache *self,
                                     const gchar *window_uid);

void fildem_menu_cache_invalidate(FildemMenuCache *self,
                                  const gchar *window_uid);
guint64 fildem_menu_cache_generation(FildemMenuCache *self,
                                     const gchar *window_uid);

G_END_DECLS
