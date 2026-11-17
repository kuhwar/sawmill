project('sawmill', 'c',
  version: '0.1.0',
  meson_version: '>= 1.1',
  default_options: ['warning_level=2', 'c_std=gnu11'],
)

cc = meson.get_compiler('c')
add_project_arguments(
  '-Wno-unused-parameter',
  '-Wno-missing-field-initializers',
  '-DSAWMILL_CATALOG_URL="@0@"'.format(get_option('catalog_url')),
  language: 'c',
)

deps = [
  dependency('gtk4', version: '>= 4.16'),
  dependency('libadwaita-1', version: '>= 1.6'),
  dependency('libsoup-3.0', version: '>= 3.0'),
  cc.find_library('m', required: false),
]

gnome = import('gnome')
resources = gnome.compile_resources('sawmill-resources', 'data/sawmill.gresource.xml',
  source_dir: 'data',
  c_name: 'sawmill',
)

executable('sawmill',
  resources,
  'src/main.c',
  'src/store-window.c',
  'src/store-catalog.c',
  'src/store-sync.c',
  'src/store-images.c',
  'src/store-art.c',
  dependencies: deps,
  install: true,
)

install_data('data/io.sawmill.Store.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data('data/icons/io.sawmill.Store.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '512x512' / 'apps',
)
