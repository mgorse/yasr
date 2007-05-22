
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

EXTRA_DIST = config.rpath  \
	$(pkgdata_DATA) \
	autogen.sh \
	depcomp \
	BUGS \
	CREDITS

ACLOCAL_AMFLAGS = -I m4
