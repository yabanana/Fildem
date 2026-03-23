#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

GVariant *fildem_menu_model_query(GVariant *menu_nodes,
                                  GVariant *window_actions,
                                  const gchar *term);

G_END_DECLS
