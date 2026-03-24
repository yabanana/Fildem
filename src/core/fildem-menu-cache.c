#include "fildem-menu-cache.h"

#include "fildem-serializer.h"

typedef struct {
  GHashTable *tree_map;
  GHashTable *generation_map;
} FildemMenuCachePrivate;

struct _FildemMenuCache {
  GObject parent_instance;
};

G_DEFINE_TYPE_WITH_PRIVATE(FildemMenuCache, fildem_menu_cache, G_TYPE_OBJECT)

static void
fildem_menu_cache_dispose(GObject *object)
{
  FildemMenuCache *self = FILDEM_MENU_CACHE(object);
  FildemMenuCachePrivate *priv = fildem_menu_cache_get_instance_private(self);

  g_clear_pointer(&priv->tree_map, g_hash_table_unref);
  g_clear_pointer(&priv->generation_map, g_hash_table_unref);

  G_OBJECT_CLASS(fildem_menu_cache_parent_class)->dispose(object);
}

static void
fildem_menu_cache_class_init(FildemMenuCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = fildem_menu_cache_dispose;
}

static void
fildem_menu_cache_init(FildemMenuCache *self)
{
  FildemMenuCachePrivate *priv = fildem_menu_cache_get_instance_private(self);

  priv->tree_map = g_hash_table_new_full(g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          (GDestroyNotify)g_variant_unref);
  priv->generation_map = g_hash_table_new_full(g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                NULL);
}

FildemMenuCache *
fildem_menu_cache_new(void)
{
  return g_object_new(FILDEM_TYPE_MENU_CACHE, NULL);
}

void
fildem_menu_cache_set_tree(FildemMenuCache *self,
                           const gchar *window_uid,
                           GVariant *nodes)
{
  FildemMenuCachePrivate *priv;

  g_return_if_fail(FILDEM_IS_MENU_CACHE(self));
  g_return_if_fail(window_uid != NULL);
  g_return_if_fail(nodes != NULL);

  priv = fildem_menu_cache_get_instance_private(self);
  g_hash_table_replace(priv->tree_map,
                       g_strdup(window_uid),
                       g_variant_ref_sink(nodes));
}

GVariant *
fildem_menu_cache_get_tree(FildemMenuCache *self,
                           const gchar *window_uid)
{
  FildemMenuCachePrivate *priv;
  GVariant *value;

  g_return_val_if_fail(FILDEM_IS_MENU_CACHE(self), NULL);
  g_return_val_if_fail(window_uid != NULL, NULL);

  priv = fildem_menu_cache_get_instance_private(self);
  value = g_hash_table_lookup(priv->tree_map, window_uid);

  if (value == NULL) {
    return g_variant_ref_sink(fildem_empty_menu_tree());
  }

  return g_variant_ref(value);
}

void
fildem_menu_cache_invalidate(FildemMenuCache *self,
                             const gchar *window_uid)
{
  FildemMenuCachePrivate *priv;
  gpointer raw_value;
  guint64 generation = 0;

  g_return_if_fail(FILDEM_IS_MENU_CACHE(self));
  g_return_if_fail(window_uid != NULL);

  priv = fildem_menu_cache_get_instance_private(self);
  raw_value = g_hash_table_lookup(priv->generation_map, window_uid);

  if (raw_value != NULL) {
    generation = GPOINTER_TO_UINT(raw_value);
  }

  generation++;
  g_hash_table_replace(priv->generation_map,
                       g_strdup(window_uid),
                       GUINT_TO_POINTER((guint)generation));
}

guint64
fildem_menu_cache_generation(FildemMenuCache *self,
                             const gchar *window_uid)
{
  FildemMenuCachePrivate *priv;
  gpointer raw_value;

  g_return_val_if_fail(FILDEM_IS_MENU_CACHE(self), 0);
  g_return_val_if_fail(window_uid != NULL, 0);

  priv = fildem_menu_cache_get_instance_private(self);
  raw_value = g_hash_table_lookup(priv->generation_map, window_uid);
  return raw_value == NULL ? 0 : (guint64)GPOINTER_TO_UINT(raw_value);
}
