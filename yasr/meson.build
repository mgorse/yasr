yasr_sources = [
  'config.c',
  'debug.c',
  'getfn.c',
  'keybind.c',
  'main.c',
  'option.c',
  'tts.c',
  'ui.c',
]

yasr_headers = [
  'gettext.h',
  'term.h',
  'tts.h',
  'yasr.h'
]

executable('yasr',
  yasr_sources,
  dependencies: [ iconv_dep, libintl_dep, openpty_dep, speechd_dep ],
  include_directories: extra_includes,
  install: true)

install_man('yasr.1')

  install_data('yasr.conf',
               install_dir: pkgdatadir)
