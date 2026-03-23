test_core = executable(
  'test-core',
  'test-core.c',
  include_directories: include_directories('../src/core'),
  dependencies: [glib_dep, gio_dep, gobject_dep, fildem_core_dep],
  link_with: [fildem_core],
)

test('core', test_core)

test_contract = executable(
  'test-contract',
  'test-contract.c',
  dependencies: [glib_dep, gio_dep, gobject_dep],
)

test('dbus-contract', test_contract)
