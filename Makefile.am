
if YASR_SUN
yasr_DDIRS= yasr acclogin
else
yasr_DDIRS= yasr
endif

SUBDIRS = \
	$(yasr_DDIRS) \
	po \
	intl

pkgdata_DATA = \
	yasr.conf

EXTRA_DIST = \
	$(pkgdata_DATA) \
	autogen.sh \
	depcomp \
	BUGS \
	CREDITS
