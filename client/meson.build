project('sawmill', 'c',
  version: '0.1.0',
  meson_version: '>= 0.62',
  default_options: ['warning_level=2', 'c_std=gnu11'],
)

cc = meson.get_compiler('c')
add_project_arguments('-Wno-unused-parameter', '-Wno-missing-field-initializers', language: 'c')

deps = [
  dependency('gtk4', version: '>= 4.16'),
  dependency('libadwaita-1', version: '>= 1.6'),
  cc.find_library('m', required: false),
]

executable('sawmill',
  'src/main.c',
  'src/store-window.c',
  'src/store-data.c',
  'src/store-art.c',
  dependencies: deps,
  install: true,
)

install_data('data/io.sawmill.Store.desktop',
  install_dir: get_option('datadir') / 'applications',
)
