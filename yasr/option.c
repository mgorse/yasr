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

/* Constants and flags for use in setting the type of an option */

#define OT_VAL      0x00
#define OT_ENUM     0x01
#define OT_STR      0x02
#define OT_TREE     0x03
#define OT_SYNTH    0x40
#define OT_BITSLICE 0x80

typedef struct
  {
    int len;
    int lastlen;
    int opt[10];
    int saychar;
    int val[10];
  }
Optqueue;

static Optqueue opq;


static int options_are_equal(int i, int j)
{
  if (i == j)
  {
    return (1);
  }

  /* Speakout's punctuation options are set using the same setting */
  if ((i >= 14 && i <= 17) && (j >= 14 && j <= 17))
  {
    return (1);
  }

  return (0);
}


void opt_queue_add(int num, int val)
{
  int i;

  for (i = 0; i < opq.len; i++)
  {
    if (options_are_equal(i, num))
    {
      break;
    }
  }
  if (i == 10)
  {
    return;
  }
  opq.opt[i] = num;
  opq.val[i] = val;
  if (i == opq.len)
  {
    opq.len++;
  }
  (void) alarm(1);
}


Opt opt[NUMOPTS];
static int curopt;
int synthopt;


void opt_add(void *ptr, int tree, char *name, int type, ...)
{
  Opt *optr;
  va_list args;
  int i;

  if (!strcasecmp(name, "synthesizer"))
    synthopt = curopt;
  optr = &opt[curopt];
  optr->ptr = ptr;
  optr->tree = tree;
  optr->name = name;
  optr->type = type;
  va_start(args, type);
  switch (type & 0x3f)
  {
  case OT_VAL:			/* Numeric value.  Expects a min and a max. */
    optr->d1 = va_arg(args, int);
    optr->max = va_arg(args, int);
    break;

  case OT_ENUM:		/* Toggle between several values. Expects the 
				 * number of values followed by a name for each.
				 */
    optr->max = va_arg(args, int) - 1;
    optr->arg = malloc((optr->max + 1) * sizeof(char *));
    for (i = 0; i <= optr->max; i++)
    {
      optr->arg[i] = strdup(va_arg(args, char *));
    }
    break;

  case 3:			/* Sub-menu -- Next option is id */
    optr->d1 = va_arg(args, int);
    break;
  }
  if (optr->type & OT_SYNTH)
  {
    optr->synth = va_arg(args, int);
    if (optr->type != (OT_TREE | OT_SYNTH))
    {
      optr->setstr = va_arg(args, char *);
    }
  } else
  {
    optr->synth = 0;
  }
  if (optr->type & OT_BITSLICE)
  {
    optr->shift = va_arg(args, int);
  }
  curopt++;
  va_end(args);
}


#define opt_ptr(x) (opt[x].type & OT_SYNTH ? \
                    (void *)(voices[opt[x].synth] + (long) opt[x].ptr) : \
                    opt[x].ptr)


int opt_getval(int num, int flag)
{
  char *p = opt_ptr(num);
  int v = opt[num].max;
  int out = 0, count = 0;
  int retval;

  if ((opt[num].type & OT_BITSLICE) && !flag)
  {				/* bitslice */
    while (v)
    {
      out += ((*p >> (opt[num].shift + count)) & 1) << count;
      count++;
      v >>= 1;
    }
    return (out);
  } else
  {
    (void) memcpy(&retval, p, sizeof(int));
    return retval;
  }
}


static void opt_setval(int num, int val)
{
  char *p = opt_ptr(num);
  int v = opt[num].max;
  int count = 0;
  int out = *p;

  if (opt[num].type & OT_BITSLICE)
  {
    while (v)
    {
      out &= ~(1 << (opt[num].shift + count));
      v >>= 1;
    }
    out |= val << opt[num].shift;
    *p = out;
  } else
  {
    (void) memcpy(p, &val, sizeof(int));
  }
}


void opt_preset(int num, char pre)
{
  char *p;

  (void) strcpy((char *) buf, opt[num].arg[(int) pre]);
  strtok((char *) buf, ":");
  while ((p = strtok(NULL, ":")))
  {
    opt_read(p, tts.synth);
  }
}


static void opt_synth_update(int num, int optval)
{
  char *p1, *p2;

  p1 = opt[num].setstr;
  if (!p1 && (opt[num].type & 0x3f) == OT_ENUM)
  {
    p1 = strstr(opt[num].arg[optval], ":") + 1;
  }

  /* This assumes that no synth will need a \ code at the beginning of a
   * setstr string.  If one does, then this code will need to be changed.
   */
  if (!strchr(p1 + 1, '\\') && !strstr(p1, "%s"))
  {
    (void) sprintf(ttsbuf, p1, optval);
  } else
  {
    p2 = ttsbuf;
    while (*p1)
    {
      switch (*p1)
      {
      case '\\':
	p1++;
	switch (*(p1++))
	{
	case 'l':
	  *(p2++) = *((char *) opt_ptr(num)) + 64;
	  break;

	case 'p':
	  *(p2++) = opt[num].arg[(int) *((char *) opt_ptr(num))][0];
	  break;

	case '?':	/* hard-coded special handling */
	  switch (num)
	  {
	  case 32:	/* bns punctuation: tbd - rework using : form */
	    switch (*(short *) opt_ptr(num))
	    {
	    case 0:
	      *(p2++) = 'Z';
	      break;
	    case 1:
	      *(p2++) = 'S';
	      break;
	    case 2:
	      *(p2++) = 'M';
	      break;
	    case 3:
	      *(p2++) = 'T';
	      break;
	    }
	  }
	  break;
	}
	break;
      case '%':
	p1++;
	switch (*(p1++))
	{
	case 's':	/* send string of numeric option */
	  strcpy(p2, opt[num].arg[optval]);
	  while (*p2) p2++;
	  break;
	default:
	  /* tbd - error */
	  break;
	}
	break;
      default:
	*(p2++) = *(p1++);
      }
    }
    *p2 = '\0';
  }
  tts_send(ttsbuf, strlen(ttsbuf));
}

void opt_queue_empty(int mode)
{
  int i;
  int max;

  max = (mode == 1 ? opq.lastlen : opq.len);
  if (!max)
  {
    return;
  }
  if (opq.saychar)
  {
    if (mode == 3)
    {
      opq.len = 0;
      return;
    }
    tts_charoff();
    opq.saychar = 0;
  }
  for (i = 0; i < max; i++)
  {
    opt_synth_update(opq.opt[i], opq.val[i]);
  }
  switch (mode)
  {
  case 3:
    opq.saychar = 1;
    tts_charon();

  case 0:
    opq.lastlen = opq.len;
    opq.len = 0;
    break;

  case 2:
    opq.len = 0;

  case 1:
    opq.lastlen = 0;
    break;
  }
}


void opt_set(int num, void *val)
{
  void *p = opt_ptr(num);
  char *p1;
  int optval = 0;		/* value to send to synth */

  switch (opt[num].type & 0x3f)
  {
  case 2:
    (void) strncpy((char *) p, val, OPT_STR_SIZE);
    break;

  case 4:
    opt_preset(num, 0);
    if (*(char *) val)
    {
      opt_preset(num, *(char *) val);
    }

  case 0:
  case 1:
    opt_setval(num, *(int *) val);
    break;
  }
  if (!(opt[num].type & OT_SYNTH) || opt[num].synth != tts.synth)
  {
    return;
  }
  if ((opt[num].type & 0x3f) != 2)
  {
    optval = opt_getval(num, 1);
  }
  p1 = opt[num].setstr;
  if (opt[num].type & OT_SYNTH)
  {
    opt_queue_add(num, optval);
  }
}


void opt_say(int num, int flag)
{
  void *p = opt_ptr(num);

  if (flag)
  {
    (void) sprintf((char *) buf, "%s... ", opt[num].name);
    tts_say((char *) buf);
  }
  switch (opt[num].type & 0x3f)
  {
  case 0:
    (void) sprintf((char *) buf, "%d", opt_getval(num, 0));
    tts_say((char *) buf);
    break;

  case 1:
    (void) strcpy((char *) buf, opt[num].arg[opt_getval(num, 0)]);
    strtok((char *) buf, ":");
    tts_say((char *) buf);
    break;

  case 2:
    tts_say((char *) p);
    break;
  }
}


#define opt_usable(x) (opt[x].tree == opt[curopt].tree && \
                      (!(opt[x].type & OT_SYNTH) || \
                      opt[x].synth == tts.synth || \
                      opt[i].synth == -1))


int optmenu(int ch)
{
  int i;
  static int state = 0;

  switch (state)
  {
  case 1:			/* User has just finished entering a number */
    if (!ui.abort)
    {
      if (ui.num >= opt[curopt].d1 && ui.num <= opt[curopt].max)
      {
	opt_set(curopt, &ui.num);
	tts_say("Value accepted.");
      } else
      {
	tts_say("Value out of range.");
      }
    }
    state = 0;
    return (1);

  case 2:			/* User has just entered a string -- not yet implemented */
    if (!ui.abort)
    {
      opt_set(curopt, &ui.buf);
      tts_say("value accepted.");
    }
    return (1);
  }

  switch (ch)
  {
  case 0:			/* initialize */
    tts_say("Setting options...");
    opt_say(curopt = 0, 1);
    break;

  case 0x1b5b41:		/* up arrow */
    for (i = curopt - 1; i >= 0; i--)
    {
      if (opt_usable(i))
      {
	curopt = i;
	break;
      }
    }
    opt_say(curopt, 1);
    break;

  case 0x1b5b42:		/* down arrow */
    for (i = curopt + 1; i < NUMOPTS; i++)
    {
      if (opt_usable(i))
      {
	curopt = i;
	break;
      }
    }
    opt_say(curopt, 1);
    break;

  case 0x1b5b44:		/* left arrow */
    if (!opt[curopt].tree)
    {
      opt_say(curopt, 1);
      break;
    }
    for (i = curopt - 1; i >= 0; i--)
    {
      if ((opt[i].type & 0x3f) == 3 &&
	  opt[i].d1 == opt[curopt].tree &&
	  (!opt[i].type & OT_SYNTH ||
	   opt[i].synth == tts.synth || opt[i].synth == -1))
      {
	curopt = i;
	break;
      }
    }
    opt_say(curopt, 1);
    break;

  case 0x1b5b43:		/* right arrow */
  case 0x0d:
  case 0x20:
    switch (opt[curopt].type & 0x3f)
    {
    case 0:
      (void) sprintf((char *) buf, "Enter a number from %d to %d.", opt[curopt].d1, opt[curopt].max);
      tts_say((char *) buf);
      ui_funcman(&ui_ennum);
      state = 1;
      break;

    case 1:
      i = (opt_getval(curopt, 0) + 1) % (opt[curopt].max + 1);
      opt_set(curopt, &i);
      opt_say(curopt, 1);
      break;

    case 2:			/* tbd -- allow the user to enter a string here */
      break;

    case 3:
      for (i = curopt + 1; i < NUMOPTS; i++)
      {
	if (opt[i].tree == opt[curopt].d1 &&
	    (!(opt[i].type & OT_SYNTH) ||
	     opt[i].synth == tts.synth || opt[i].synth == -1))
	{
	  curopt = i;
	  break;
	}
      }
      opt_say(curopt, 1);
    }
    break;

  case 27:
    tts_say("exiting options menu.");
    ui_funcman(0);
    break;

  default:
    if ((ch & 0xffffff00) || !isalpha(ch))
    {
      break;
    }
    ch |= 0x20;
    for (i = (curopt + 1) % NUMOPTS; i != curopt; i = (i + 1) % NUMOPTS)
    {
      if ((opt[i].name[0] | 0x20) == ch && opt_usable(i))
      {
	curopt = i;
	break;
      }
    }
    opt_say(curopt, 1);
  }

  return (1);
}


static char *strro(char *s, char c, int j)
{
  static char buf[500];
  char *p = s, *q = buf;

  if (j)
  {
    while (*p && *p != c)
    {
      p++;
    }
    if (!*p)
    {
      buf[0] = '\0';
      return (buf);
    }
    p++;
  }
  while (*p && *p != c)
  {
    *(q++) = *(p++);
  }
  *q = '\0';

  return (buf);
}


int opt_read(char *buf, int synth)
{
  int i, j;
  void *p;

  strtok(buf, "=");
  i = strlen(buf);
  while (i && buf[i] == ' ')
    buf[i--] = '\0';
  if (!strcasecmp(buf, "synthesizer") && cl_synth)
  {
    cl_synth = 0;
    return (0);
  }
  if (!strcasecmp(buf, "synthesizer port") && cl_synthport)
  {
    cl_synthport = 0;
    return 0;
  }
  for (i = 0; i < NUMOPTS; i++)
  {
    if (!strcasecmp(buf, opt[i].name) && opt[i].synth == synth)
    {
      break;
    }
  }
  if (i == NUMOPTS)
  {
    return (-1);
  }
  if (!(p = strtok(NULL, "")))
  {
    return (-1);
  }
  switch (opt[i].type & 0x3f)
  {
  case 0:			/* Option takes a numeric value */
    j = strtol(p, NULL, 0);
    opt_set(i, &j);
    break;

  case 1:			/* Option value has a name which we convert into a number */
    for (j = 0; j <= opt[i].max; j++)
    {
      if (!strcasecmp(p, strro(opt[i].arg[j], ':', 0)))
      {
	break;
      }
    }
    if (j > opt[i].max)
    {
      return (-1);
    }
    opt_set(i, &j);
    break;

  case 2:
    opt_set(i, p);
  }

  return (0);
}


void opt_write_single(FILE * fp, int i)
{
  switch (opt[i].type & 0x3f)
  {
  case 0:
    (void) fprintf(fp, "%s=%d\n", opt[i].name, opt_getval(i, 0));
    break;
  case 1:
    (void) fprintf(fp, "%s=%s\n", opt[i].name, strro(opt[i].arg[opt_getval(i, 0)], ':', 0));
    break;
  case 2:
    (void) fprintf(fp, "%s=%s\n", opt[i].name, (char *) opt_ptr(i));
    break;
  }
}


void opt_write(FILE * fp)
{
  int i, j;

  for (i = 0; i < NUMOPTS; i++)
  {
    if (!(opt[i].type & OT_SYNTH))
    {
      opt_write_single(fp, i);
    }
  }
  for (i = 0; i <= opt[synthopt].max; i++)
  {
    (void) fprintf(fp, "[%s]\n", opt[synthopt].arg[i]);
    for (j = 0; j < NUMOPTS; j++)
    {
      if ((opt[j].type & OT_SYNTH) && opt[j].synth == i)
      {
	opt_write_single(fp, j);
      }
    }
  }
}


void opt_init()
{
  opq.len = opq.lastlen = 0;
  opt_add(&ui.curtrack, 0, "cursor tracking", OT_ENUM, 3, "off", "arrow keys", "full");
  opt_add(&tts.synth, 0, "synthesizer", OT_ENUM, 6, "speakout", "dectalk", "Emacspeak server", "doubletalk", "bns", "apollo");
  opt_add(tts.port, 0, "synthesizer port", OT_STR);
  opt_add(&ui.kbsay, 0, "key echo", OT_ENUM, 3, "off", "keys", "words");
  opt_add(usershell, 0, "shell", OT_STR);

/* tbd - The following is a bad hack that I use to play Angband.  It should
 * be replaced with a more general facility for defining screen windows.
 */
  opt_add(&special, 255, "special", OT_ENUM, 2, "off", "on");

  opt_add(&rev.udmode, 0, "up and down arrows", OT_ENUM, 3, "speak line", "speak character", "speak word");

/* tbd - allow the user to enter 0x to indicate a Hex number, somehow */
  opt_add(&ui.disable, 255, "DisableKey", OT_VAL, 0, 0x7fffffff);

  opt_add(NULL, 0, "synthesizer options", OT_TREE | OT_SYNTH, -1, -1);

/* Speakout settings (first index is 9) */
  opt_add((void *) 4, -1, "rate", OT_VAL | OT_SYNTH, 0, 9, 0, "\005r%d");
  opt_add((void *) 8, -1, "pitch", OT_VAL | OT_SYNTH, 0, 9, 0, "\005p%d");
  opt_add((void *) 12, -1, "volume", OT_VAL | OT_SYNTH, 0, 9, 0, "\005v%d");
  opt_add((void *) 16, -1, "tone", OT_VAL | OT_SYNTH, 1, 26, 0, "\005t\\l");
  opt_add(NULL, -1, "Punctuation", OT_TREE | OT_SYNTH, -2, 0);
  opt_add(NULL, -2, "textual", OT_ENUM | OT_SYNTH | OT_BITSLICE, 2, "off", "on:!,.:;", 0, "\005y%x", 3);
  opt_add(NULL, -2, "math", OT_ENUM | OT_SYNTH | OT_BITSLICE, 2, "off", "on:%^*()/-<=>+", 0, "\005y%x", 2);
  opt_add(NULL, -2, "miscelaneous", OT_ENUM | OT_SYNTH | OT_BITSLICE, 2, "off", "on:[]{}\\|_\"'@#$&", 0, "\005y%x", 1);
  opt_add(NULL, -2, "spaces", OT_ENUM | OT_SYNTH | OT_BITSLICE, 2, "off", "on: ", 0, "\005y%x", 0);

/* DEC-Talk settings (first index is 18) */
  opt_add((void *) 4, -1, "rate", OT_VAL | OT_SYNTH, 75, 650, 1, "[:ra%d]");
  opt_add((void *) 8, -1, "volume", OT_VAL | OT_SYNTH, 0, 99, 1, "[:vol set %d]");
  opt_add((void *) 12, -1, "voice", OT_ENUM | OT_SYNTH, 10, "paul", "harry", "frank", "dennis", "betty", "ursula", "rita", "wendy", "kit", "val", 1, "[:n\\p]");
  opt_add((void *) 0, -1, "punctuation", OT_ENUM | OT_SYNTH, 3, "some", "none", "all", 1, "[:pu \\p]");

/* Emacspeak settings (first index is 22) */
  opt_add((void *) 4, -1, "rate", OT_VAL | OT_SYNTH, 0, 250, 2, "tts_set_speech_rate {%d}\r");
  opt_add((void *) 0, -1, "punctuation", OT_ENUM | OT_SYNTH, 3, "none", "some", "all", 2, "tts_set_punctuations %s\r");

/* DoubleTalk settings (first index is 23) */
  opt_add((void *) 4, -1, "rate", OT_VAL | OT_SYNTH, 0, 9, 3, "\001%ds");
  opt_add((void *) 8, -1, "pitch", OT_VAL | OT_SYNTH, 0, 99, 3, "\001%dp");
  opt_add((void *) 12, -1, "volume", OT_VAL | OT_SYNTH, 0, 9, 3, "\001%dv");
  opt_add((void *) 16, -1, "tone", OT_VAL | OT_SYNTH, 0, 2, 3, "\001%dX");
  opt_add((void *) 20, -1, "voice", OT_ENUM | OT_SYNTH, 8, "paul", "vader", "bob", "pete", "randy", "biff", "skip", "roborobert", 3, "\001%dO");
  opt_add((void *) 0, -1, "punctuation", OT_ENUM | OT_SYNTH, 4, "none:\0017b", "some:\0016b", "most:\0015b", "all:\0014b", 3, NULL);

/* Braille 'n Speak settings (first index is 29) */
  opt_add((void *) 4, -1, "rate", OT_VAL | OT_SYNTH, 1, 15, 4, "\005%dE");
  opt_add((void *) 8, -1, "pitch", OT_VAL | OT_SYNTH, 1, 63, 4, "\005%dP");
  opt_add((void *) 12, -1, "volume", OT_VAL | OT_SYNTH, 1, 15, 4, "\005%dV");
  opt_add((void *) 0, -1, "punctuation", OT_ENUM | OT_SYNTH, 4, "none", "some", "most", "all", 4, "\005\\?");

/* Apollo settings (first index is 33) */
  opt_add((void *) 0, -1, "punctuation", OT_ENUM | OT_SYNTH, 2, "off", "on", 5, "@P%d");
  opt_add((void *) 4, -1, "rate", OT_VAL | OT_SYNTH, 1, 9, 5, "@W%d");
  opt_add((void *) 8, -1, "pitch", OT_VAL | OT_SYNTH, 1, 15, 5, "@F%x");
  opt_add((void *) 12, -1, "prosody", OT_VAL | OT_SYNTH, 1, 7, 5, "@R%d");
  opt_add((void *) 16, -1, "word pause", OT_VAL | OT_SYNTH, 1, 9, 5, "@Q%d");
  opt_add((void *) 20, -1, "sentence pause", OT_VAL | OT_SYNTH, 1, 15, 5, "@D%x");
  opt_add((void *) 24, -1, "degree", OT_VAL | OT_SYNTH, 1, 8, 5, "@B%d");
  opt_add((void *) 28, -1, "volume", OT_VAL | OT_SYNTH, 1, 15, 5, "@A%x");
  opt_add((void *) 32, -1, "voice", OT_VAL | OT_SYNTH, 1, 6, 5, "@V%d");
}
