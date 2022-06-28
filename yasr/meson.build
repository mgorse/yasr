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

keytest_sources = [
	'keytest.c'
]

executable('yasr',
  yasr_sources,
  dependencies: openpty_dep,
  install: true)

executable('keytest',
  keytest_sources,
  install: false)

install_man('yasr.1')
