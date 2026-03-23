project(
  'fildem-rewrite',
  'c',
  version: '1.0.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
  ],
)

glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
gobject_dep = dependency('gobject-2.0')
gtk4_dep = dependency('gtk4')
glib_compile_schemas = find_program('glib-compile-schemas')

source_interfaces_dir = join_paths(meson.project_source_root(), 'interfaces')
installed_interfaces_dir = join_paths(get_option('prefix'),
                                      get_option('datadir'),
                                      'fildem',
                                      'interfaces')

add_project_arguments(
  '-DFILDEM_SOURCE_INTERFACES_DIR="' + source_interfaces_dir + '"',
  '-DFILDEM_INSTALLED_INTERFACES_DIR="' + installed_interfaces_dir + '"',
  language: 'c',
)

subdir('src')
subdir('tests')

install_subdir(
  'interfaces',
  install_dir: join_paths(get_option('datadir'), 'fildem'),
)

install_subdir(
  'src/extension/fildem-shell@fildem.org',
  install_dir: join_paths(get_option('datadir'), 'gnome-shell', 'extensions'),
)

extension_schema_compiled = custom_target(
  'fildem-shell-compiled-schema',
  input: 'src/extension/fildem-shell@fildem.org/schemas/org.fildem.shell.gschema.xml',
  output: 'gschemas.compiled',
  command: [
    glib_compile_schemas,
    '--targetdir', '@OUTDIR@',
    meson.current_source_dir() + '/src/extension/fildem-shell@fildem.org/schemas',
  ],
  build_by_default: true,
  install: true,
  install_dir: join_paths(get_option('datadir'),
                          'gnome-shell',
                          'extensions',
                          'fildem-shell@fildem.org',
                          'schemas'),
)

install_data(
  'data/org.fildem.hud.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

install_data(
  'packaging/systemd/fildemd.service',
  install_dir: join_paths(get_option('prefix'), 'lib', 'systemd', 'user'),
)
