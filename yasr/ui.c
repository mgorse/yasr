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

#include "yasr.h"

static void ui_saypos(int, int);

void ui_funcman(int (*f) (int))
{
  static void *fstack[8];	/* crappy stack, but who cares. */
  static int nf = 0;

  if (f)
  {
    fstack[nf++] = (void *) ui.func;
    ui.func = f;
    (*ui.func) (0);
  } else
  {
    ui.func = (int (*)(int)) fstack[--nf];
  }
}


 /*ARGSUSED*/ void rev_rttc(int *argp)
{
  int cr, cc;
  int i;

  if (ui.revmode)
  {
    cr = rev.cr;
    cc = rev.cc;
  } else
  {
    cr = win->cr;
    cc = win->cc;
  }
  for (i = 0; i < cr; i++)
  {
    ui_sayline(i, 0);
  }
  ui_saylinepart(i, 0, cc, 0);
}


 /*ARGSUSED*/ void rev_rctb(int *argp)
{
  int i;
  int cr, cc;

  if (ui.revmode)
  {
    cr = rev.cr;
    cc = rev.cc;
  } else
  {
    cr = win->cr;
    cc = win->cc;
  }
  ui_saylinepart(cr, cc, -1, 0);
  for (i = cr + 1; i < win->rows; i++)
  {
    ui_sayline(i, 0);
  }
}


void rev_rs(int *argp)
{
  int i;
  int a, b;

  if (argp)
  {
    a = *argp;
    b = *(argp + 1);
  } else
  {
    a = 0;
    b = win->rows - 1;
  }

  for (i = a; i <= b; i++)
  {
    ui_sayline(i, 0);
  }
}


 /*ARGSUSED*/ void rev_curpos(int *argp)
{
  ui_saypos(rev.cr, rev.cc);
}


void ui_sayword(int cr, int cc)
{
  chartype *c;
  int cp;
  int i = 0;

  c = win->row[cr] + cc;
  cp = cc;
  while (cp && (*c & 0xff) > 32)
  {
    c--;
    cp--;
  }
  if ((*c & 0xff) <= 32)
  {
    c++;
    cp++;
  }
  while (cp < win->cols && (*c & 0xff) > 32)
  {
    cp++;
    buf[i++] = realchar(*c++);
  }
  if (i)
  {
    speak((char *) buf, i);
  }
}


void rev_line(int *argp)
{
  int nr, nc;

  if (ui.revmode)
  {
    nr = rev.cr;
    nc = rev.cc;
  } else
  {
    nr = win->cr;
    nc = win->cc;
  }
  if (argp && (*argp & 0xff00) == 0x0100)
  {
    nr = *argp & 0xff;
  } else if (argp)
  {
    nr += (*argp * (ui.num ? ui.num : 1));
  } else if (ui.num)
  {
    nr = ui.num - 1;
  }

  if (nr < 0)
  {
    nr = 0;
    tts_say("top");
  } else if (nr >= win->rows)
  {
    nr = win->rows - 1;
    tts_say("bottom");
  }
  if (ui.revmode)
  {
    rev.cr = nr;
  }

  if (!argp || !rev.udmode || !(*(argp + 1)))
  {
    ui_sayline(nr, 1);
    return;
  }

  switch (rev.udmode)
  {
  case 1:
    ui_saychar(nr, nc);
    break;
  case 2:
    ui_sayword(nr, nc);
    break;
  }
}


void rev_word(int *argp)
{
  int nw;
  int nr, nc;

  if (ui.revmode)
  {
    nr = rev.cr;
    nc = rev.cc;
  } else
  {
    nr = win->cr;
    nc = win->cc;
  }
  nw = argp ? *argp : 0;

  if (nw > 0)
  {
    while (!cblank(nr, nc) && nc < win->cols)
    {
      nc++;
    }
  }
  if (nw < 0)
  {
    while (nc && !cblank(nr, nc))
    {
      nc--;
    }
  }

  while (nw > 0)
  {
    nw--;
    for (;;)
    {
      if (++nc >= win->cols)
      {
	if (++nr == win->rows)
	{
	  tts_say("bottom right");
	  nr--;
	  nc--;
	  break;
	}
	nc = 0;
      }
      if (!cblank(nr, nc))
	break;
    }
  }

  while (nw < 0)
  {
    nw++;
    for (;;)
    {
      if (--nc < 0)
      {
	if (--nr < 0)
	{
	  tts_say("top left");
	  nr = nc = 0;
	  break;
	}
	nc = win->cols - 1;
      }
      if (!cblank(nr, nc))
      {
	while (nc && !cblank(nr, nc - 1))
	{
	  nc--;
	}
	break;
      }
    }
  }
  ui_sayword(nr, nc);
  if (ui.revmode)
  {
    rev.cr = nr;
    rev.cc = nc;
  }
}


static void ui_sayphonetic(int row, int col)
{
  tts_sayphonetic((char) win->row[row][col]);
}


void rev_ch(int *argp)
{
  int nr, nc;

  if (ui.revmode)
  {
    nr = rev.cr;
    nc = rev.cc;
  } else
  {
    nr = win->cr;
    nc = win->cc;
  }

  if (argp)
  {
    nc += (*argp * (ui.num ? ui.num : 1));
  } else if (ui.num)
  {
    nc = ui.num - 1;
  }
  while (nc < 0)
  {
    nr--;
    nc += win->cols;
  }
  while (nc >= win->cols)
  {
    nr++;
    nc -= win->cols;
  }
  if (nr < 0)
  {
    nr = nc = 0;
    tts_say("top left");
  } else if (nr >= win->rows)
  {
    nr = win->rows - 1;
    nc = win->cols - 1;
    tts_say("bottom right");
  }
  (rev.repeat & 1 && !argp ? ui_sayphonetic : ui_saychar) (nr, nc);
  if (ui.revmode)
  {
    rev.cr = nr;
    rev.cc = nc;
  }
}


int ui_ennum(int ch)
{
  if (!ch)
  {
    ui.num = ui.abort = 0;
    return (1);
  } else if (ch < 0x0100 && isdigit(ch))
  {
    ui.num = ui.num * 10 + (ch - 48);
    return (1);
  }
  ui_funcman(0);
  if (ch == 27)
  {
    tts_say("Aborting.");
    ui.abort = 1;
    ui.num = 0;
  }

  return (2);
}

int ui_build_str(int ch)
{
  switch (ch)
  {
  case 0:
    ui.strlen = ui.abort = 0;
    return 1;
  case 0x08:
  case 0x7f:
    if (ui.strlen)
    {
      tts_saychar(ui.str[--ui.strlen]);
    }
    return 1;
  case 27:	/* escape */
    tts_say("aborting");
    ui_funcman(0);
    return 1;
  case 13:
  case 10:
    ui.str[ui.strlen] = 0;
    ui_funcman(0);
    return 1;
  default:
    if (ui.strlen < sizeof(ui.str) - 1)	/* ascii dep. */
    {
      ui.str[ui.strlen++] = ch;
    }
    return 1;
  }
}


static int rev_searchline(int l, int c1, int c2, int reverse)
{
  int i, j, len;
  int a, b, step;
  chartype *cp;

  len = strlen(rev.findbuf);
  if (c2 == -1)
  {
    c2 = win->cols - len;
  }
  if (c2 < c1)
  {
    return (0);
  }
  if (reverse)
  {
    a = c2;
    b = c1 - 1;
    step = -1;
  } else
  {
    a = c1;
    b = c2 + 1;
    step = 1;
  }
  for (i = a, cp = win->row[l] + a; i != b; i += step, cp += step)
  {
    for (j = 0; j < len; j++)
    {
      if (i + j < win->cols &&
	  toupper(realchar(*(cp + j))) != toupper(rev.findbuf[j]))
      {
	break;
      }
    }
    if (j == len)
    {
      rev.cr = l;
      rev.cc = i;
      ui_saypos(rev.cr, rev.cc);
      return (1);
    }
  }

  return (0);
}


 /*ARGSUSED*/ void rev_searchtocursor(int *argp)
{
  int i;

  if (rev.cc && rev_searchline(rev.cr, 0, rev.cc - 1, 1))
  {
    return;
  }
  for (i = rev.cr - 1; i >= 0; i--)
  {
    if (rev_searchline(i, 0, -1, 1))
    {
      return;
    }
  }
  tts_say("not found");
}


 /*ARGSUSED*/ void rev_searchtoend(int *argp)
{
  int i;

  if (rev_searchline(rev.cr, rev.cc + 1, -1, 0))
  {
    return;
  }
  for (i = rev.cr + 1; i < win->rows; i++)
  {
    if (rev_searchline(i, 0, -1, 0))
    {
      return;
    }
  }
  tts_say("not found");
}


 /*ARGSUSED*/ static void rev_searchall(int *argp)
{
  int i;

  for (i = 0; i < win->rows; i++)
  {
    if (rev_searchline(i, 0, -1, 0))
    {
      return;
    }
  }
  tts_say("not found");
}


static int rev_find_aux(int ch)
{
  if (!rev.meta)
  {
    switch (ch)
    {
    case 0:			/* initialize */
      rev.findbuflen = rev.meta = 0;
      tts_say("Enter pattern to find");
      return (1);

    case '>':
      rev.findbuf[rev.findbuflen] = 0;
      ui_funcman(0);
      rev_searchtoend(NULL);
      return (1);

    case '<':
      rev.findbuf[rev.findbuflen] = 0;
      ui_funcman(0);
      rev_searchtocursor(NULL);
      return (1);

    case '\\':
      rev.meta = 1;
      return (1);

    case 0x08:
    case 0x7f:
      if (rev.findbuflen)
      {
	tts_saychar(rev.findbuf[--rev.findbuflen]);
      }
      return (1);

    case 27:			/* escape */
      tts_say("aborting");
      ui_funcman(0);
      return 1;

    case 13:
    case 10:
      rev.findbuf[rev.findbuflen] = 0;
      ui_funcman(0);
      rev_searchall(NULL);
      return (1);
    }
  }
  rev.meta = 0;
  if (rev.findbuflen >= win->cols)
  {
    return (1);
  }
  rev.findbuf[rev.findbuflen++] = (char) ch;

  return (1);
}


 /*ARGSUSED*/ void rev_find(int *argp)
{
  ui_funcman(&rev_find_aux);
}


void rev_toline(int *argp)
{
  int arg1 = *argp, arg2 = *(argp + 1), reading = 0;

  if (arg1 < 0)
  {
    if (arg1 <= -win->rows)
    {
      arg1 = -win->rows + 1;
    }
    arg1 = win->rows + arg1;
  } else
  {
    if (!arg1 || arg1 > win->rows)
    {
      arg1 = win->rows;
    }
  }

  rev.cr = arg1 - 1;
  if (!arg2)
  {
    return;
  }

  if (arg2 == 1 || arg2 == 3)
  {
    if (!rev.cr)
    {
      tts_say("top");
    } else if (rev.cr == win->rows - 1)
    {
      tts_say("bottom");
    }
  }

  if (arg2 > 1)
  {
    reading = 1;
  }
  ui_sayline(rev.cr, reading);
}


void rev_tocol(int *argp)
{
  int arg1 = *argp, arg2 = *(argp + 1), i, c = -1;

  if (arg1 > 0xff)
  {
    arg1 -= 256;
    c = 0;
    for (i = 0; i <= win->cols; i++)
    {
      if (win->row[rev.cr][i] > ' ')
      {
	c = i;
      }
    }
  }
  if (arg1 < 0)
  {
    if (arg1 <= -win->cols)
    {
      arg1 = -win->cols + 1;
    }
    arg1 = win->cols + arg1;
  } else if (!arg1 || arg1 > win->cols)
  {
    arg1 = win->cols;
  }

  if (c > 0 && arg1 > c)
  {
    rev.cc = c;
  } else
  {
    rev.cc = arg1 - 1;
  }
  if (!arg2)
  {
    return;
  }

  if (arg2 >= 2)
  {
    if (!rev.cc)
    {
      tts_say("left");
    } else if (rev.cc == win->cols - 1 || rev.cc == c)
    {
      tts_say("right");
    }
  }
  ui_saychar(rev.cr, rev.cc);
}


static int rev_main(int ch)
{
  int index;
  Keybind *kf;

  if (ch < 0x100 && isdigit(ch))
  {
    ui_funcman(&ui_ennum);
    return (2);
  }
  index = kb_search(&rev.keymap, ch);
  if (index == -1)
  {				/* invalid key */
    rev.used = 0;
    return (0);
  }
  kf = &rev.keymap.kb[index];
  while (kf)
  {
    (*funcs[kf->index].f) (kf->argp);
    kf = kf->next;
  }
  ui.num = 0;

  return (1);
}

static int line_is_blank(int row, int c1, int c2)
{
  int i;
  int len;
  chartype *rptr;

  len = c2 - c1;
  rptr = win->row[row] + c1;
  for (i = 0; i < len; i++)
  {
    if (!y_isblank(*rptr++))
      return 0;
  }
  return 1;
}

void rev_nextpar(int *argp)
{
  int i;

  for (i = rev.cr; i < win->rows && !line_is_blank(i, 0, win->cols); i++);
  for (; i < win->rows && line_is_blank(i, 0, win->cols); i++);
  if (i == win->rows)
    return;
  rev.cr = i;
  rev.cc = 0;
  rev_rctb(NULL);
}

void rev_prevpar(int *argp)
{
  int i;

  if (rev.cr == 0)
    return;
  for (i = rev.cr; i >= 0 && !line_is_blank(i, 0, win->cols); i--);
  if (i >= rev.cr - 1)
  {
    for (; i >= 0 && line_is_blank(i, 0, win->cols); i--);
    for (; i >= 0 && !line_is_blank(i, 0, win->cols); i--);
    if (i < 0)
    {
      for (i = 0; i < win->rows && line_is_blank(i, 0, win->cols); i++);
      if (i >= rev.cr)
	return;
    } else
      i++;
  } else
    i++;
  rev.cr = i;
  rev.cc = 0;
  rev_rctb(NULL);
}

 /*ARGSUSED*/ void ui_bypass(int *argp)
{
  ui.meta = 1;
}


 /*ARGSUSED*/ void ui_revtog(int *argp)
{
  ui.revmode ^= 1;
  if (ui.revmode == 0)
  {
    tts_say("exit");
    ui.func = NULL;
    return;
  }
  tts_say("review");
  ui.func = &rev_main;
  rev.findbuflen = rev.meta = ui.num = 0;
  if (!ui.rc_detached)
  {
    rev.cr = win->cr;
    rev.cc = win->cc;
  }
}


 /*ARGSUSED*/ void ui_detachtog(int *argp)
{
  ui.rc_detached ^= 1;
  if (ui.rc_detached)
  {
    tts_say("Detached");
    return;
  }
  rev.cr = win->cr;
  rev.cc = win->cc;
  tts_say("Attached");
}


void ui_routerc(int *argp)
{
  rev.cr = win->cr;
  rev.cc = win->cc;
  if (*argp)
  {
    if (*argp > 1)
    {
      tts_say("Cursor routed");
    }
    if (*argp == 1 || *argp == 3)
    {
      ui_saypos(rev.cr, rev.cc);
    }
  }
}

/* Returns non-zero if the key has been processed. */
int ui_keypress(int key)
{
  Keybind *kf;
  int used = 0;
  int index;

  if (ui.func)
  {
    while ((used = (*ui.func) (key)) == 2)
    {
      continue;
    }
  }
  if (used)
  {
    return (used);
  }
  index = kb_search(&ui.keymap, key);
  if (index == -1)
  {
    if (!ui.revmode && kbuflen < 99)
      kbuf[kbuflen++] = key;
    return (ui.revmode);
  }
  kf = &ui.keymap.kb[index];
  if (key == rev.lastkey)
  {
    rev.repeat++;
  } else
  {
    rev.repeat = 0;
  }
  while (kf)
  {
    (*funcs[kf->index].f) (kf->argp);
    kf = kf->next;
  }
  ui.num = 0;
  rev.lastkey = key;

  return (1);
}


void ui_saychar(int row, int col)
{
  tts_saychar((char) win->row[row][col]);
}


static void ui_sayblankline()
{
  tts_say("blank");
}

void ui_saylinepart(int row, int c1, int c2, int say_blank)
{
  int blank = 1;
  int len;
  chartype *rptr;
  int i;

  if (c2 == -1)
  {
    c2 = win->cols;
  }
  len = c2 - c1;
  rptr = win->row[row] + c1;
  for (i = 0; i < len; i++)
  {
    buf[i] = realchar(*(rptr++));
    if (buf[i] != 32)
      blank = 0;
  }
  if (blank)
  {
    if (say_blank)
    {
      ui_sayblankline();
    }
    return;
  }
  speak((char *) buf, len);
}

void ui_sayline(int row, int say_blank)
{
  ui_saylinepart(row, 0, win->cols, say_blank);
}

 /*ARGSUSED*/ void ui_curpos(int *argp)
{
  ui_saypos(win->cr, win->cc);
}


void ui_silence(int *argp)
{
  tts_silence();
  if (argp && *argp)
  {
    ui.silent++;
  }
}


static void ui_saypos(int row, int col)
{
  char buf[20];

  (void) sprintf(buf, "c%dl%d", col + 1, row + 1);
  tts_say(buf);
}


void uinit()
{
  (void) memset(&ui, 0, sizeof(ui));
  ui.minrc = 4;
  ui.curtrack = 2;
}


 /*ARGSUSED*/ void ui_kbwiz(int *argp)
{
  ui_funcman(&kbwiz);
}


 /*ARGSUSED*/ void ui_optmenu(int *argp)
{
  ui_funcman(&optmenu);
}


int ui_addstr(int ch)
{
  switch (ch)
  {
  case 27:
    tts_say("Aborting.");
    ui_funcman(0);
   /*FALLTHROUGH*/ case 0:
    ui.buflen = 0;
    break;

  case 13:
    ui_funcman(0);
    return (2);

  default:
    if (ui.buflen < 99)
    {
      ui.buf[ui.buflen++] = ch;
    }
  }

  return (1);
}


void ui_opt_set(int *argp)
{
  int t;

  if (!argp)
  {
    tts_say("error, fix keybinding.");
    return;
  }
  switch (opt[*argp].type)
  {
  case OT_STR:
    opt_set(*argp, argp + 1);
    break;

  case OT_INT:
    t = (opt_getval(*argp, 0) + 1) % (opt[*argp].v.val_int.max + 1);
    opt_set(*argp, &t);
    break;
  case OT_ENUM:
    t = (opt_getval(*argp, 0) + 1) % (opt[*argp].v.enum_max + 1);
    opt_set(*argp, &t);
    break;
  default:
    tts_say("error in keybinding: unsupported option type");
    break;
  }
  opt_say(*argp, 0);
}


 /*ARGSUSED*/ void ui_bol(int *argp)
{
  for (rev.cc = 0; rev.cc < win->cols; rev.cc++)
  {
    if (!cblank(rev.cr, rev.cc))
    {
      break;
    }
  }
  if (rev.cc == win->cols)
  {
    rev.cc = 0;
  }
  if (!cblank(rev.cr, rev.cc))
  {
    ui_sayword(rev.cr, rev.cc);
  }
}


 /*ARGSUSED*/ void ui_eol(int *argp)
{
  for (rev.cc = win->cols - 1; rev.cc; rev.cc--)
  {
    if (!cblank(rev.cr, rev.cc))
    {
      break;
    }
  }
  if (!cblank(rev.cr, rev.cc))
  {
    ui_sayword(rev.cr, rev.cc);
  }
}


 /*ARGSUSED*/ void ui_sayascii(int *argp)
{
  int cr;
  int cc;

  if (ui.revmode)
  {
    cr = rev.cr;
    cc = rev.cc;
  } else
  {
    cr = win->cr;
    cc = win->cc;
  }
  tts_say_printf("%d", win->row[cr][cc] & 0xff);
}
