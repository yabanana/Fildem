#include "fildem-menu-registry.h"

#include "fildem-serializer.h"

typedef struct {
  GHashTable *top_level_map;
  GHashTable *actions_map;
} FildemMenuRegistryPrivate;

struct _FildemMenuRegistry {
  GObject parent_instance;
};

G_DEFINE_TYPE_WITH_PRIVATE(FildemMenuRegistry, fildem_menu_registry, G_TYPE_OBJECT)

static void
fildem_menu_registry_dispose(GObject *object)
{
  FildemMenuRegistry *self = FILDEM_MENU_REGISTRY(object);
  FildemMenuRegistryPrivate *priv = fildem_menu_registry_get_instance_private(self);

  g_clear_pointer(&priv->top_level_map, g_hash_table_unref);
  g_clear_pointer(&priv->actions_map, g_hash_table_unref);

  G_OBJECT_CLASS(fildem_menu_registry_parent_class)->dispose(object);
}

static void
fildem_menu_registry_class_init(FildemMenuRegistryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = fildem_menu_registry_dispose;
}

static void
fildem_menu_registry_init(FildemMenuRegistry *self)
{
  FildemMenuRegistryPrivate *priv = fildem_menu_registry_get_instance_private(self);

  priv->top_level_map = g_hash_table_new_full(g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               (GDestroyNotify)g_variant_unref);
  priv->actions_map = g_hash_table_new_full(g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             (GDestroyNotify)g_variant_unref);
}

FildemMenuRegistry *
fildem_menu_registry_new(void)
{
  return g_object_new(FILDEM_TYPE_MENU_REGISTRY, NULL);
}

void
fildem_menu_registry_set_top_level(FildemMenuRegistry *self,
                                   const gchar *window_uid,
                                   GVariant *items)
{
  FildemMenuRegistryPrivate *priv;

  g_return_if_fail(FILDEM_IS_MENU_REGISTRY(self));
  g_return_if_fail(window_uid != NULL);
  g_return_if_fail(items != NULL);

  priv = fildem_menu_registry_get_instance_private(self);
  g_hash_table_replace(priv->top_level_map,
                       g_strdup(window_uid),
                       g_variant_ref_sink(items));
}

GVariant *
fildem_menu_registry_get_top_level(FildemMenuRegistry *self,
                                   const gchar *window_uid)
{
  FildemMenuRegistryPrivate *priv;
  GVariant *value;

  g_return_val_if_fail(FILDEM_IS_MENU_REGISTRY(self), NULL);
  g_return_val_if_fail(window_uid != NULL, NULL);

  priv = fildem_menu_registry_get_instance_private(self);
  value = g_hash_table_lookup(priv->top_level_map, window_uid);

  if (value == NULL) {
    return g_variant_ref_sink(fildem_empty_top_level_items());
  }

  return g_variant_ref(value);
}

void
fildem_menu_registry_set_actions(FildemMenuRegistry *self,
                                 const gchar *window_uid,
                                 GVariant *actions)
{
  FildemMenuRegistryPrivate *priv;

  g_return_if_fail(FILDEM_IS_MENU_REGISTRY(self));
  g_return_if_fail(window_uid != NULL);
  g_return_if_fail(actions != NULL);

  priv = fildem_menu_registry_get_instance_private(self);
  g_hash_table_replace(priv->actions_map,
                       g_strdup(window_uid),
                       g_variant_ref_sink(actions));
}

GVariant *
fildem_menu_registry_get_actions(FildemMenuRegistry *self,
                                 const gchar *window_uid)
{
  FildemMenuRegistryPrivate *priv;
  GVariant *value;

  g_return_val_if_fail(FILDEM_IS_MENU_REGISTRY(self), NULL);
  g_return_val_if_fail(window_uid != NULL, NULL);

  priv = fildem_menu_registry_get_instance_private(self);
  value = g_hash_table_lookup(priv->actions_map, window_uid);

  if (value == NULL) {
    return g_variant_ref_sink(fildem_empty_actions());
  }

  return g_variant_ref(value);
}
