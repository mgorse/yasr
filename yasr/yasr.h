
/*
 * YASR ("Yet Another Screen Reader") is an attempt at a lightweight,
 * portable screen reader.
 *
 * Copyright (C) 2001-2002 by Michael P. Gorse. All rights reserved.
 *
 * YASR comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU Lesser General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
 *
 * Web Page: http://mgorse.dhs.org:8000/yasr/
 *
 * This software is maintained by:
 * Michael P. Gorse <mgorse@users.sourceforge.net>
 */

/* yasr.h -- header file for yasr */

#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>

#ifdef HAVE_UTIL_H
#include <util.h>
#elif defined(HAVE_LIBUTIL_H)
#include <libutil.h>
#elif defined(HAVE_PTY_H)
#include <pty.h>
#endif

#include <errno.h>

typedef short chartype;

typedef unsigned char uchar;

typedef struct Curpos Curpos;
struct Curpos
{
    int cr;
    int cc;
};

typedef struct Win Win;
struct Win
{
    int carry;
    int cc;
    int cols;
    int cr;
    int mode;			/* flags changed by h/l csi sequences */
    char attr;			/* flags changed by the m csi sequences */
    Curpos savecp;
    chartype **row;
    int rows;
    char *tab;
    Win *next;
};

typedef struct Func Func;
struct Func
{
    void (*f) ();
    char *desc;
};

typedef struct Keybind Keybind;
struct Keybind
{
    int key;
    int index;
    int numargs;
    int *argp;
    Keybind *next;
};

typedef struct Keymap Keymap;
struct Keymap
{
    int numkeys;
    Keybind *kb;
};

typedef enum mpunct mpunct;
enum mpunct
{
    PUNCT_NONE,
    PUNCT_SOME,
    PUNCT_MOST,
    PUNCT_ALL
};

#define OPT_STR_SIZE 80

typedef struct Tts Tts;
struct Tts
{
    int fd;
    int flood;
    char *obuf;
    int obufhead, obuflen, obuftail;
    int oflag;			/* set to 1 every time tts_send is called */
    int outlen;
    char buf[256];
    int synth;
    pid_t pid;
    char port[OPT_STR_SIZE];
};

typedef struct Uirev Uirev;
struct Uirev
{
    int cc;
    int cr;
    int lastkey;
    int meta;
    int repeat;
    int udmode;
    char findbuf[200];
    int findbuflen;
    int used;
    Keymap keymap;
};

typedef struct Ui Ui;
struct Ui
{
    int abort;			/* set if the user aborts entering something */
    char buf[100];
    int buflen;
    int curtrack;		/* 0 = none, 1 = with cursor keys, 2 = always */
    int disable;		/* key to disable */
    int disabled;		/* true if disabled */
    int (*func) (int);
    int kbsay;
    int num;	/* number that the user is entering */
    char str[100];	/* string that the user is entering */
    int strlen;
    int (*oldfunc) (int);
    int meta;
    int minrc;	/* min. repeat count */
    int revmode;
    int silent;
    int rc_detached;
    Keymap keymap;
};

typedef struct Opt Opt;
struct Opt
{
    int *ptr;
    char *name;
    char *setstr;
    int type;
    int shift;			/* # bits to shift, if type & 0x80 */
    int tree;
    union
    {
      struct { int min; int max; } val_int;
      struct { double min; double max; } val_float;
      int enum_max;
      int submenu;
    } v;
    int synth;
    char **arg;
};

extern Tts tts;
extern Ui ui;
extern Uirev rev;
extern Win *win;
extern int cl_synth;
extern int cl_synthport;

extern Opt opt[];
extern int synthopt;
extern int sighit;
extern char *conffile;
extern unsigned char buf[256];
extern int kbuf[100];
extern int kbuflen;
extern char usershell[OPT_STR_SIZE];
extern char ttsbuf[40];
extern char voices[6][64];
extern int special;

extern Func funcs[];

/* ui.c prototypes */
extern void rev_rttc(int *argp);
extern void rev_rctb(int *argp);
extern void rev_rs(int *argp);
extern void rev_line(int *argp);
extern void rev_word(int *argp);
extern void rev_ch(int *argp);
extern void rev_curpos(int *argp);
extern void rev_toline(int *argp);
extern void rev_tocol(int *argp);
extern void rev_searchtocursor(int *argp);
extern void rev_searchtoend(int *argp);
extern void rev_find(int *argp);
extern int ui_ennum(int ch);
extern int ui_build_str(int ch);
extern void ui_funcman(int (*f) (int));
extern void ui_kbwiz(int *argp);
extern void ui_optmenu(int *argp);
extern void ui_saychar(int row, int col);
extern void ui_sayword(int cr, int cc);
extern void ui_saylinepart(int row, int c1, int c2, int say_blank);
extern void ui_sayline(int row, int say_blank);
extern void ui_bypass(int *argp);
extern void ui_curpos(int *argp);
extern void ui_revtog(int *argp);
extern void ui_detachtog(int *argp);
extern void ui_routerc(int *argp);
extern void ui_silence(int *argp);
extern void ui_opt_set(int *argp);
extern void ui_bol(int *argp);
extern void ui_eol(int *argp);
extern int ui_keypress(int key);
extern void ui_sayascii(int *argp);
extern void rev_nextpar(int *argp);
extern void rev_prevpar(int *argp);
extern void uinit();

/* tts.c prototypes */
extern void tts_charoff();
extern void tts_charon();
extern void tts_addchr(char ch);
extern void tts_out(unsigned char *ibuf, int len);
extern void tts_say(char *buf);
extern void tts_saychar(unsigned char ch);
extern void tts_sayphonetic(unsigned char ch);
extern void tts_send(char *buf, int len);
extern void tts_flush();
extern void tts_silence();
extern void tts_end();
extern int tts_init();
extern void tts_say_printf(char *fmt, ...);
extern void tts_initsynth(int *argp);
extern int dict_read(char *buf);
extern void dict_write(FILE * fp);

/* config.c prototypes */
extern void readconf();

/* main.c prototypes */
void speak(char *ibuf, int len);
extern char realchar(chartype ch);

/* debug.c prototypes */
extern void open_debug(char *);
extern void debug(char *format, ...);
extern void close_debug();

/* getfn.c prototypes */
char *getfn(char *name);

/* keybind.c prototypes */
/* Undefine this to remove the keyboard wizard and make yasr slightly smaller*/
#define USE_KBWIZ

extern int kb_search(Keymap * map, int k);
extern int kb_add(Keymap * map, int k, int i, int na, int *a, int flag);
extern int kbwiz(int ch);

/* option.c prototypes */
extern int optmenu(int ch);
extern int opt_getval(int menu, int flag);
extern void opt_init();
extern int opt_read(char *buf, int synth);
extern void opt_say(int num, int flag);
extern void opt_set(int num, void *val);
extern void opt_queue_empty(int ll);
extern void opt_write(FILE * fp);

/* openpty.c prototypes */
extern int openpty(int *, int *, char *, struct termios *, struct winsize *);

/* cfmakeraw.c prototypes */
extern void cfmakeraw(struct termios *);

/* login_tty.c prototypes */
extern int login_tty(int);

/* forkpty.c prototypes */
extern int forkpty(int *, char *, struct termios *, struct winsize *);

/* tbc - Would it be more efficient to ensure that "blank" grids always held
   ascii 0x20 rather than ascii 0x00? */
#define y_isblank(ch) ((ch & 0xdf) == 0)
#define cblank(r, c)  ((win->row[r][c] & 0xdf) == 0)
#define ttssend(x)    if (x) tts_send(x, strlen(x))

#define NUMOPTS 44

/* Option types */
#define OT_INT      0x00
#define OT_FLOAT    0x01
#define OT_ENUM     0x02
#define OT_STR      0x03
#define OT_TREE     0x04
#define OT_PRESET   0x04
#define OT_SYNTH    0x40
#define OT_BITSLICE 0x80

#if DEBUG
#include "debug.h"
#endif
