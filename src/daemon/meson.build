fildemd = executable(
  'fildemd',
  'fildemd.c',
  include_directories: include_directories('../core'),
  dependencies: [glib_dep, gio_dep, gobject_dep, fildem_core_dep],
  link_with: [fildem_core],
  install: true,
)
