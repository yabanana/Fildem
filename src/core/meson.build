fildem_core_sources = files(
  'fildem-error.c',
  'fildem-serializer.c',
  'fildem-menu-registry.c',
  'fildem-menu-cache.c',
  'fildem-menu-model.c',
)

fildem_core_headers = files(
  'fildem-core.h',
  'fildem-error.h',
  'fildem-serializer.h',
  'fildem-menu-registry.h',
  'fildem-menu-cache.h',
  'fildem-menu-model.h',
)

fildem_core_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: [glib_dep, gio_dep, gobject_dep],
)

fildem_core = library(
  'fildem-core',
  fildem_core_sources,
  include_directories: include_directories('.'),
  dependencies: [glib_dep, gio_dep, gobject_dep],
  install: true,
)

install_headers(fildem_core_headers, subdir: 'fildem')
