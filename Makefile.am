
if YASR_SUN
yasr_DDIRS= yasr acclogin
else
yasr_DDIRS= yasr
endif

SUBDIRS = \
	$(yasr_DDIRS) \
m4 \
	po

pkgdata_DATA = \
	yasr.conf

EXTRA_DIST = config.rpath  config.rpath  \
	$(pkgdata_DATA) \
	autogen.sh \
	depcomp \
	BUGS \
	CREDITS \
	README-br \
	README-de \
	README-es \
	README-fr \
	README-ru

ACLOCAL_AMFLAGS = -I m4
