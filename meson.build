project('yasr', 'c',
        version: '0.7.0',
        license: 'LGPLv2+',
        default_options: [
          'buildtype=debugoptimized',
          'warning_level=1',
          'c_std=gnu99',
        ],
        meson_version: '>= 0.47.0')

cc = meson.get_compiler('c')
host_system = host_machine.system()

conf = configuration_data()
conf.set('GETTEXT_PACKAGE', '"yasr"')

add_global_arguments([
  '-I' + meson.build_root(), # config.h
], language: 'c')

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
conf.set_quoted('PACKAGE_DATA_DIR', pkgdatadir)
localedir = join_paths(prefix, get_option('localedir'))
conf.set_quoted('LOCALEDIR', localedir)

foreach h : [ 'fcntl.h', 'sys/ioctl.h', 'sys/time.h', 'unistd.h',  'utmpx.h',
  'pty.h', 'locale.h']
  if cc.check_header(h)
    conf.set('HAVE_@0@'.format(h.underscorify().to_upper()), 1)
  endif
endforeach

openpty_dep = cc.find_library('util')

i18n = import('i18n')

xgettext = find_program('xgettext', required : false)
if xgettext.found()
  conf.set('ENABLE_NLS', 1)
endif

  speechd_dep = dependency('speech-dispatcher', required: false)
if speechd_dep.found()
  conf.set('ENABLE_SPEECHD', 1)
endif

configure_file(output: 'config.h', configuration: conf)

subdir('yasr')
subdir('tools')

if xgettext.found()
  subdir('po')
endif
