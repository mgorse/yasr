
bin_PROGRAMS = yasr

noinst_PROGRAMS = keytest

yasr_SOURCES = \
	config.c \
	debug.c \
	getfn.c \
	keybind.c \
	main.c \
	option.c \
	tts.c \
	ui.c \
	gettext.h \
	term.h \
	tts.h \
	yasr.h

yasr_LDADD = $(LIBOBJS) $(INTLLIBS)

keytest_SOURCES = \
	keytest.c

keytest_LDADD = $(LIBOBJS)

man1_MANS = \
	yasr.1

EXTRA_DIST = \
	$(man1_MANS)

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
