
bin_PROGRAMS = yasr

noinst_PROGRAMS = keytest

yasr_SOURCES = \
	cfmakeraw.c \
	config.c \
	debug.c \
	forkpty.c \
	getfn.c \
	keybind.c \
	login_tty.c \
	main.c \
	openpty.c \
	option.c \
	tts.c \
	ui.c \
	term.h \
	tts.h \
	yasr.h

keytest_SOURCES = \
	keytest.c \
	cfmakeraw.c

man1_MANS = \
	yasr.1

EXTRA_DIST = \
	$(man1_MANS)
