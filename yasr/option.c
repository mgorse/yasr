/*
 * YASR ("Yet Another Screen Reader") is an attempt at a lightweight,
 * portable screen reader.
 *
 * Copyright (C) 2001-2003 by Michael P. Gorse. All rights reserved.
 *
 * YASR comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU Lesser General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
 *
 * Web Page: http://yasr.sf.net
 *
 * This software is maintained by:
 * Michael P. Gorse <mgorse@users.sourceforge.net>
 */

#include "yasr.h"

/* Constants and flags for use in setting the type of an option */

typedef struct
{
  int len;
  int lastlen;
  int opt[10];
  int saychar;
  union
  {
    int val;
    double val_float;
  } v[10];
} Optqueue;

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

static void opt_queue_add(int num, ...)
{
  int i;
  va_list args;

  va_start(args, num);
  for (i = 0; i < opq.len; i++)
  {
    if (options_are_equal(i, num)) break;
  }
  if (i == 10)
  {
    /* tbc - give some kind of warning */
  }
  else
  {
    opq.opt[i] = num;
    if ((opt[num].type & 0x3f) == OT_FLOAT)
    {
      opq.v[i].val_float = va_arg(args, double);
    }
    else opq.v[i].val = va_arg(args, int);
    if (i == opq.len) opq.len++;
    (void) alarm(1);
  }
  va_end(args);
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
  optr->internal_name = name;
  optr->localized_name = _(name);
  optr->type = type;
  va_start(args, type);
  switch (type & 0x3f)
  {
  case OT_INT:			/* Numeric value.  Expects a min and a max. */
    optr->v.val_int.min = va_arg(args, int);
    optr->v.val_int.max = va_arg(args, int);
    break;
  case OT_FLOAT:			/* Numeric value.  Expects a min and a max. */
    optr->v.val_float.min = va_arg(args, double);
    optr->v.val_float.max = va_arg(args, double);
    break;

  case OT_ENUM:		/* Toggle between several values. Expects the 
				 * number of values followed by a name for each.
				 */
    optr->v.enum_max = va_arg(args, int) - 1;
    optr->arg = malloc((optr->v.enum_max + 1) * sizeof(char *));
    for (i = 0; i <= optr->v.enum_max; i++)
    {
      optr->arg[i] = strdup(va_arg(args, char *));
    }
    break;

  case OT_TREE:			/* Sub-menu -- Next option is id */
    optr->v.submenu = va_arg(args, int);
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

#define data_size(x) (((x)->type & 0x3f) == OT_FLOAT? sizeof(double): sizeof(int))


int opt_getval(int num, int flag)
{
  char *p = opt_ptr(num);
  int out = 0, count = 0;
  int retval;

  if ((opt[num].type & OT_BITSLICE) && !flag)
  {				/* bitslice */
    int v = opt[num].v.enum_max;	/* assuming OT_ENUM */
    while (v)
    {
      /* hmm; will this code work for enum_max > 1? */
      out += ((*p >> (opt[num].shift + count)) & 1) << count;
      count++;
      v >>= 1;
    }
    return (out);
  } else
  {
    (void) memcpy(&retval, p, data_size(&opt[num]));
    return retval;
  }
}

double opt_getval_float(int num, int flag)
{
  char *p = opt_ptr(num);
  double retval;

  (void) memcpy(&retval, p, data_size(&opt[num]));
  return retval;
}

static void opt_setval(int num, void *val)
{
  char *p = opt_ptr(num);
  int count = 0;
  int out = *p;

  if (opt[num].type & OT_BITSLICE)
  {
    int v = opt[num].v.enum_max;
    int val_int = *(int *)val;
    while (v)
    {
      out &= ~(1 << (opt[num].shift + count));
      v >>= 1;
    }
    out |= val_int << opt[num].shift;
    *p = out;
  }
  else (void) memcpy(p, val, data_size(&opt[num]));
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
  }
  else
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

static void opt_synth_update_float(int num, double optval)
{
  int i;
  
  (void) sprintf(ttsbuf, opt[num].setstr, optval);
  for (i=0; i<strlen(ttsbuf); i++)
    {
      if ((ttsbuf[i])==',')
        ttsbuf[i]='.';
    }

  tts_send(ttsbuf, strlen(ttsbuf));
}

void opt_queue_empty(int mode)
{
  int i;
  int max;

  max = (mode == 1 ? opq.lastlen : opq.len);
  if (!max) return;
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
    if ((opt[opq.opt[i]].type & 0x3f) == OT_FLOAT)
    {
      opt_synth_update_float(opq.opt[i], opq.v[i].val_float);
    }
    else
    {
      opt_synth_update(opq.opt[i], opq.v[i].val);
    }
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

  switch (opt[num].type & 0x3f)
  {
  case OT_STR:
    (void) strncpy((char *) p, val, OPT_STR_SIZE);
    break;

  case OT_PRESET:
    opt_preset(num, 0);
    if (*(char *) val)
    {
      opt_preset(num, *(char *) val);
    }

  case OT_INT:
  case OT_ENUM:
  case OT_FLOAT:
    opt_setval(num, val);
    break;
  }
  if (!(opt[num].type & OT_SYNTH) || opt[num].synth != tts.synth) return;
  switch (opt[num].type & 0x3f)
  {
  case OT_INT:
  case OT_ENUM:
    opt_queue_add(num, opt_getval(num, 1));
    break;
  case OT_FLOAT:
    opt_queue_add(num, opt_getval_float(num, 1));
    break;
  }
}

void opt_say(int num, int flag)
{
  void *p = opt_ptr(num);

  if (flag)
  {
    (void) sprintf((char *) buf, "%s... ", opt[num].localized_name);
    tts_say((char *) buf);
  }
  switch (opt[num].type & 0x3f)
  {
  case OT_INT:
    (void) sprintf((char *) buf, "%d", opt_getval(num, 0));
    tts_say((char *) buf);
    break;
  case OT_FLOAT:
    (void) sprintf((char *) buf, "%f", opt_getval_float(num, 0));
    tts_say((char *) buf);
    break;
  case OT_ENUM:
    (void) strcpy((char *) buf, opt[num].arg[opt_getval(num, 0)]);
    strtok((char *) buf, ":");
    tts_say(_((char *) buf));
    break;
  case OT_STR:
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
  case 1:			/* User has just finished entering an integer */
    if (!ui.abort)
    {
      int num = atoi(ui.str);
      if (num >= opt[curopt].v.val_int.min && num <= opt[curopt].v.val_int.max)
      {
	opt_set(curopt, &num);
	tts_say(_("Value accepted."));
      } else
      {
	tts_say(_("Value out of range."));
      }
    }
    state = 0;
    return (1);

  case 2:			/* User has just finished entering a float */
    if (!ui.abort)
    {
      double num = atof(ui.str);
      if (num >= opt[curopt].v.val_float.min && num <= opt[curopt].v.val_float.max)
      {
	opt_set(curopt, &num);
	tts_say(_("Value accepted."));
      } else
      {
	tts_say(_("Value out of range."));
      }
    }
    state = 0;
    return (1);

  case 3:			/* User has just entered a string -- not yet implemented */
    if (!ui.abort)
    {
      opt_set(curopt, &ui.buf);
      tts_say(_("value accepted."));
    }
    return (1);
  }

  switch (ch)
  {
  case 0:			/* initialize */
    tts_say(_("Setting options..."));
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
      if ((opt[i].type & 0x3f) == OT_TREE &&
	  opt[i].v.submenu == opt[curopt].tree &&
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
    case OT_INT:
      (void) sprintf((char *) buf, "%s %d %s %d.",_("Enter a number from"), opt[curopt].v.val_int.min, _("to"), opt[curopt].v.val_int.max);
      tts_say((char *) buf);
      ui_funcman(&ui_build_str);
      state = 1;
      break;
    case OT_FLOAT:
      (void) sprintf((char *) buf, "%s %lf %s %lf.",_("Enter a number from"), opt[curopt].v.val_float.min,_("to"),  opt[curopt].v.val_float.max);
      tts_say((char *) buf);
      ui_funcman(&ui_build_str);
      state = 2;
      break;

    case OT_ENUM:
      i = (opt_getval(curopt, 0) + 1) % (opt[curopt].v.enum_max + 1);
      opt_set(curopt, &i);
      opt_say(curopt, 1);
      break;

    case OT_STR:			/* tbd -- allow the user to enter a string here */
      break;

    case OT_TREE:
      for (i = curopt + 1; i < NUMOPTS; i++)
      {
	if (opt[i].tree == opt[curopt].v.submenu &&
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
    tts_say(_("exiting options menu."));
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
      if ((opt[i].localized_name[0] | 0x20) == ch && opt_usable(i))
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
  float float_val;

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
    if (!strcasecmp(buf, opt[i].internal_name) && opt[i].synth == synth)
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
  case OT_INT:
    j = strtol(p, NULL, 0);
    opt_set(i, &j);
    break;

  case OT_FLOAT:
    sscanf(p, "%f", &float_val);
    opt_set(i, &float_val);
    break;

  case OT_ENUM:
    for (j = 0; j <= opt[i].v.enum_max; j++)
    {
      if (!strcasecmp(p, strro(opt[i].arg[j], ':', 0)))
      {
	break;
      }
    }
    if (j > opt[i].v.enum_max)
    {
      return (-1);
    }
    opt_set(i, &j);
    break;
  case OT_STR:
    opt_set(i, p);
    break;
  }

  return (0);
}

void opt_write_single(FILE * fp, int i)
{
  switch (opt[i].type & 0x3f)
  {
  case OT_INT:
    (void) fprintf(fp, "%s=%d\n", opt[i].internal_name, opt_getval(i, 0));
    break;
  case OT_FLOAT:
    (void) fprintf(fp, "%s=%lf\n", opt[i].internal_name, opt_getval_float(i, 0));
    break;
  case OT_ENUM:
    (void) fprintf(fp, "%s=%s\n",
	opt[i].internal_name, strro(opt[i].arg[opt_getval(i, 0)], ':', 0));
    break;
  case OT_STR:
    (void) fprintf(fp, "%s=%s\n", opt[i].internal_name, (char *) opt_ptr(i));
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
  for (i = 0; i <= opt[synthopt].v.enum_max; i++)
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
  opt_add(&ui.curtrack, 0, N_("cursor tracking"), OT_ENUM, 3, N_("off"), N_("arrow keys"), N_("full"));
  opt_add(&tts.synth, 0, N_("synthesizer"), OT_ENUM, 9, N_("speakout"), N_("dectalk"), N_("Emacspeak server"), N_("doubletalk"), N_("bns"), N_("apollo"), N_("festival"), N_("Ciber232"), N_("speech dispatcher"));
  opt_add(tts.port, 0, N_("synthesizer port"), OT_STR);
  opt_add(&ui.kbsay, 0, N_("key echo"), OT_ENUM, 3, N_("off"), N_("keys"), N_("words"));
  opt_add(usershell, 0, N_("shell"), OT_STR);

/* tbd - The following is a bad hack that I use to play Angband.  It should
 * be replaced with a more general facility for defining screen windows.
 */
  opt_add(&special, 255, "special", OT_ENUM, 2, "off", "on");

  opt_add(&rev.udmode, 0, N_("up and down arrows"), OT_ENUM, 3, N_("speak line"), N_("speak character"), N_("speak word"));

/* tbd - allow the user to enter 0x to indicate a Hex number, somehow */
  opt_add(&ui.disable, 255, N_("DisableKey"), OT_INT, 0, 0x7fffffff);

  opt_add(NULL, 0, N_("synthesizer options"), OT_TREE | OT_SYNTH, -1, -1);

/* Speakout settings (first index is 9) */
  opt_add((void *) 4, -1,N_( "rate"), OT_INT | OT_SYNTH, 0, 9, 0, "\005r%d");
  opt_add((void *) 8, -1, N_("pitch"), OT_INT | OT_SYNTH, 0, 9, 0, "\005p%d");
  opt_add((void *) 12, -1, N_("volume"), OT_INT | OT_SYNTH, 0, 9, 0, "\005v%d");
  opt_add((void *) 16, -1, N_("tone"), OT_INT | OT_SYNTH, 1, 26, 0, "\005t\\l");
  opt_add(NULL, -1, N_("Punctuation"), OT_TREE | OT_SYNTH, -2, 0);
  opt_add(NULL, -2, N_("textual"), OT_ENUM | OT_SYNTH | OT_BITSLICE, 2, N_("off"), N_("on")":!,.:;", 0, "\005y%x", 3);
  opt_add(NULL, -2, N_("math"), OT_ENUM | OT_SYNTH | OT_BITSLICE, 2, N_("off"), N_("on")":%^*()/-<=>+", 0, "\005y%x", 2);
  opt_add(NULL, -2, N_("miscelaneous"), OT_ENUM | OT_SYNTH | OT_BITSLICE, 2, N_("off"), N_("on")":[]{}\\|_\"'@#$&", 0, "\005y%x", 1);
  opt_add(NULL, -2, N_("spaces"), OT_ENUM | OT_SYNTH | OT_BITSLICE, 2, N_("off"), N_("on")": ", 0, "\005y%x", 0);

/* DEC-Talk settings (first index is 18) */
  opt_add((void *) 4, -1, N_("rate"), OT_INT | OT_SYNTH, 75, 650, 1, "[:ra%d]");
  opt_add((void *) 8, -1, N_("volume"), OT_INT | OT_SYNTH, 0, 99, 1, "[:vol set %d]");
  opt_add((void *) 12, -1, N_("voice"), OT_ENUM | OT_SYNTH, 10, N_("paul"), N_("harry"), N_("frank"), N_("dennis"), N_("betty"), N_("ursula"), N_("rita"), N_("wendy"), N_("kit"), N_("val"), 1, "[:n\\p]");
  opt_add((void *) 0, -1, N_("punctuation"), OT_ENUM | OT_SYNTH, 3, N_("some"), N_("none"), N_("all"), 1, "[:pu \\p]");

/* Emacspeak settings (first index is 22) */
  opt_add((void *) 4, -1, N_("rate"), OT_INT | OT_SYNTH, 0, 999, 2, "tts_set_speech_rate {%d}\r");
  opt_add((void *) 0, -1, N_("punctuation"), OT_ENUM | OT_SYNTH, 3, N_("none"), N_("some"), N_("all"), 2, "tts_set_punctuations %s\r");

/* DoubleTalk settings (first index is 23) */
  opt_add((void *) 4, -1, N_("rate"), OT_INT | OT_SYNTH, 0, 9, 3, "\001%ds");
  opt_add((void *) 8, -1, N_("pitch"), OT_INT | OT_SYNTH, 0, 99, 3, "\001%dp");
  opt_add((void *) 12, -1, N_("volume"), OT_INT | OT_SYNTH, 0, 9, 3, "\001%dv");
  opt_add((void *) 16, -1, N_("tone"), OT_INT | OT_SYNTH, 0, 2, 3, "\001%dX");
  opt_add((void *) 20, -1, N_("voice"), OT_ENUM | OT_SYNTH, 8, N_("paul"), N_("vader"), N_("bob"), N_("pete"), N_("randy"), N_("biff"), N_("skip"), N_("roborobert"), 3, "\001%dO");
  opt_add((void *) 0, -1, N_("punctuation"), OT_ENUM | OT_SYNTH, 4, N_("none")":\0017b", N_("some")":\0016b", N_("most")":\0015b", N_("all")":\0014b", 3, NULL);

/* Braille 'n Speak settings (first index is 29) */
  opt_add((void *) 4, -1, N_("rate"), OT_INT | OT_SYNTH, 1, 15, 4, "\005%dE");
  opt_add((void *) 8, -1, N_("pitch"), OT_INT | OT_SYNTH, 1, 63, 4, "\005%dP");
  opt_add((void *) 12, -1, N_("volume"), OT_INT | OT_SYNTH, 1, 15, 4, "\005%dV");
  opt_add((void *) 0, -1, N_("punctuation"), OT_ENUM | OT_SYNTH, 4, N_("none"), N_("some"), N_("most"), N_("all"), 4, "\005\\?");

/* Apollo settings (first index is 33) */
  opt_add((void *) 0, -1, N_("punctuation"), OT_ENUM | OT_SYNTH, 2, N_("off"), N_("on"), 5, "@P%d");
  opt_add((void *) 4, -1, N_("rate"), OT_INT | OT_SYNTH, 1, 9, 5, "@W%d");
  opt_add((void *) 8, -1, N_("pitch"), OT_INT | OT_SYNTH, 1, 15, 5, "@F%x");
  opt_add((void *) 12, -1, N_("prosody"), OT_INT | OT_SYNTH, 1, 7, 5, "@R%d");
  opt_add((void *) 16, -1, N_("word pause"), OT_INT | OT_SYNTH, 1, 9, 5, "@Q%d");
  opt_add((void *) 20, -1, N_("sentence pause"), OT_INT | OT_SYNTH, 1, 15, 5, "@D%x");
  opt_add((void *) 24, -1, N_("degree"), OT_INT | OT_SYNTH, 1, 8, 5, "@B%d");
  opt_add((void *) 28, -1, N_("volume"), OT_INT | OT_SYNTH, 1, 15, 5, "@A%x");
  opt_add((void *) 32, -1, N_("voice"), OT_INT | OT_SYNTH, 1, 6, 5, "@V%d");

  /* festival settings (first index is 43) */
  opt_add((void *)4, -1, N_("rate"), OT_FLOAT|OT_SYNTH, (double)0.4, (double)2.5, 6, "(Parameter.set 'Duration_Stretch %e)");

/* Ciber232 settings  (first index 44)*/
  opt_add((void *) 4, -1, N_("rate"), OT_INT | OT_SYNTH, 0, 9, 7, "@b%d");
  opt_add((void *) 8, -1, N_("volume"), OT_INT | OT_SYNTH, 0, 9, 7, "@d%d");
  opt_add((void *) 12, -1, N_("pitch"), OT_INT | OT_SYNTH, 0, 9, 7, "@a%d");
  opt_add((void *) 16, -1, N_("sex"), OT_INT | OT_SYNTH, 0, 1, 7, "@c%d");
  opt_add((void *) 20, -1, N_("Voice"), OT_INT | OT_SYNTH, 0, 1, 7, "@q%d");
  opt_add((void *) 24, -1, N_("entonation"), OT_INT | OT_SYNTH, 0, 9, 7, "@r%d");
  opt_add((void *) 28, -1, N_("caseonwarning"), OT_INT | OT_SYNTH, 0, 1, 7, "@n%d");
  opt_add((void *) 32, -1, N_("deviceonwarning"), OT_INT | OT_SYNTH, 0, 1, 7, "@t%d");

}
