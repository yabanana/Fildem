#include <gio/gio.h>
#include <glib.h>

typedef struct {
  const gchar *file_name;
  const gchar *interface_name;
} InterfaceExpectation;

static const InterfaceExpectation EXPECTED_INTERFACES[] = {
  {"org.fildem.v1.Window.xml", "org.fildem.v1.Window"},
  {"org.fildem.v1.TopLevel.xml", "org.fildem.v1.TopLevel"},
  {"org.fildem.v1.MenuTree.xml", "org.fildem.v1.MenuTree"},
  {"org.fildem.v1.Activation.xml", "org.fildem.v1.Activation"},
  {"org.fildem.v1.WindowActions.xml", "org.fildem.v1.WindowActions"},
  {"org.fildem.v1.Hud.xml", "org.fildem.v1.Hud"},
};

static gboolean
interface_has_method(GDBusInterfaceInfo *interface_info, const gchar *method_name)
{
  guint i;

  if (interface_info == NULL || interface_info->methods == NULL) {
    return FALSE;
  }

  for (i = 0; interface_info->methods[i] != NULL; i++) {
    if (g_strcmp0(interface_info->methods[i]->name, method_name) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

static void
introspection_xml_is_valid(void)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS(EXPECTED_INTERFACES); i++) {
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path = NULL;
    g_autofree gchar *contents = NULL;
    g_autoptr(GDBusNodeInfo) node = NULL;

    path = g_build_filename(FILDEM_SOURCE_INTERFACES_DIR,
                            EXPECTED_INTERFACES[i].file_name,
                            NULL);
    g_assert_true(g_file_test(path, G_FILE_TEST_EXISTS));

    g_assert_true(g_file_get_contents(path, &contents, NULL, &error));
    g_assert_no_error(error);

    node = g_dbus_node_info_new_for_xml(contents, &error);
    g_assert_no_error(error);
    g_assert_nonnull(node);
    g_assert_nonnull(node->interfaces);
    g_assert_nonnull(node->interfaces[0]);
    g_assert_cmpstr(node->interfaces[0]->name,
                    ==,
                    EXPECTED_INTERFACES[i].interface_name);

    if (g_strcmp0(node->interfaces[0]->name, "org.fildem.v1.Hud") == 0) {
      g_assert_true(interface_has_method(node->interfaces[0], "RequestHud"));
      g_assert_true(interface_has_method(node->interfaces[0], "Query"));
      g_assert_true(interface_has_method(node->interfaces[0], "Execute"));
    }
  }
}

int
main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/fildem/contract/introspection-xml-is-valid", introspection_xml_is_valid);
  return g_test_run();
}
