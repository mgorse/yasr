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
  dependencies: [ openpty_dep, speechd_dep ],
  install: true)

install_man('yasr.1')
