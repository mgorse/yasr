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
#include <sys/param.h>

static void conf_write(int *);

Func funcs[] = {
  {&rev_line, "say line"},	/* 0 */
  {&rev_ch, "say character"},	/* 1 */
  {&rev_curpos, "say review cursor"},	/* 2 */
  {&rev_find, "find"},		/* 3 */
  {&ui_silence, "flush"},	/* 4 */
  {&ui_revtog, "toggle review mode"},	/* 5 */
  {&ui_curpos, "say cursor"},	/* 6 */
  {&rev_rttc, "read top to cursor"},	/* 7 */
  {&rev_rctb, "read cursor to bottom"},	/* 8 */
  {&rev_rs, "read screen"},	/* 9 */
  {&rev_searchtocursor, "search back"},	/* 10 */
  {&rev_searchtoend, "search forward"},	/* 11 */
  {&ui_bypass, "bypass"},	/* 12 */
  {&rev_word, "say word"},	/* 13 */
  {&ui_optmenu, "options menu"},	/* 14 */
  {&ui_kbwiz, "keyboard wizard"},	/* 15 */
  {&conf_write, "write configuration"},	/* 16 */
  {&ui_opt_set, "set option"},	/* 17 */
  {&ui_bol, "beginning of line"},	/* 18 */
  {&ui_eol, "end of line"},	/* 19 */
  {&tts_initsynth, "reinitialize"},	/* 20 */
  {&ui_detachtog, "toggle cursor detachment"},	/* 21 */
  {&rev_toline, "Move review cursor to line"},	/* 22 */
  {&rev_tocol, "move review cursor to column"},	/* 23 */
  {&ui_routerc, "route review cursor"},	/* 24 */
  {&ui_sayascii, "ascii"},	/* 25 */
  {&rev_nextpar, "next paragraph"},	/* 26 */
  {&rev_prevpar, "previous paragraph"},	/* 27 */
  {NULL, NULL}			/* required terminator */
};


void readconf()
{
  FILE *fp;
  int args, arg[16], *argp;
  int i, key, ln = 0, mode = 0;
  char *home, *ptr, *s;
  char confname[MAXPATHLEN];

  if ((home = getenv("HOME")) != NULL)
  {
    (void) strcpy((char *) buf, home);
    (void) strcat((char *) buf, "/.yasr.conf");
  }
  (void) sprintf(confname, "%s/yasr.conf", PACKAGE_DATA_DIR);

  if (!(fp = fopen(conffile, "r")) &&
      !(fp = fopen((char *) buf, "r")) && !(fp = fopen(confname, "r")))
  {
    (void) printf("yasr: unable to find %s or ~/.yasr.conf or %s.\n", conffile, confname);
    exit(1);
  }

  for (;;)
  {
    ln++;
    (void) fgets((char *) buf, 200, fp);
    if (feof(fp))
    {
      break;
    }
    ptr = (char *) buf;
    while (*ptr == 32 || *ptr == 9)
    {
      ptr++;
    }
    if (ptr[0] == '#')
    {
      continue;
    }
    buf[strlen((char *) buf) - 1] = 0;
    if (buf[strlen((char *) buf) - 1] == '\r')
    {
      buf[strlen((char *) buf) - 1] = 0;
    }
    if (!strlen(ptr))
    {
      continue;
    }
    if (!strcasecmp(ptr, "[reviewkeys]"))
    {
      mode = 1;
    }
    else if (!strcasecmp(ptr, "[normalkeys]"))
    {
      mode = 2;
    }
    else if (!strcasecmp((char *) buf, "[options]"))
    {
      mode = 3;
    }
    else if (!strcasecmp((char *) buf, "[dict]"))
    {
      mode = 4;
    }
    else if (buf[0] == '[')
    {
      mode = 0;
      buf[strlen((char *) buf) - 1] = '\0';
      for (i = 0; i <= opt[synthopt].v.enum_max; i++)
      {
	if (!strcasecmp((char *) buf + 1, opt[synthopt].arg[i]))
	{
	  mode = -i - 1;
	}
      }
    }
    else
    {
      switch (mode)
      {
      case 1:
      case 2:			/* keybind */
	key = strtol(ptr, &s, 0);
	s++;
	ptr = strtok(s, ":");
	if (atoi(s)) i = atoi(s);
	else
	{
	  for (i = 0; funcs[i].desc; i++)
	  {
	    if (!strcasecmp(s, funcs[i].desc))
	    {
	      break;
	    }
	  }
	}
	if (!funcs[i].desc)
	{
	  printf("Syntax error on line %d of configuration file.\n", ln);
	  exit(1);
	}
	args = 0;
	while ((ptr = strtok(NULL, ":")))
	{
	  /* tbc -- is allowing numbers okay? */
	  arg[args++] = atoi(ptr);
	}
	if (args)
	{
	  argp = (int *) malloc(args * sizeof(int));
	  (void) memcpy(argp, arg, args * sizeof(int));
	}
	else argp = NULL;
	if (mode == 1)
	{
	  (void) kb_add(&rev.keymap, key, i, args, argp, 2);
	}
	else
	{
	  (void) kb_add(&ui.keymap, key, i, args, argp, 2);
	}
	break;

      case 4:
	(void) dict_read((char *) buf);
	break;

      default:			/* option */
	if (opt_read((char *) buf, mode < 0 ? -mode - 1 : 0))
	{
	  printf
	    ("Error on line %d of configuration file: invalid option\n", ln);
	  exit(1);
	}
	if (!strcasecmp((char *) buf, "synthesizer port"))
	{
	  (void) tts_init();
	}
	break;
      }
    }
  }
  (void) fclose(fp);
}


static void kb_write(FILE * fp, Keymap * map)
{
  int i, j;
  int *argp;
  char tmpstr[12];
  Keybind *kb;

  for (i = 0, kb = map->kb; i < map->numkeys; i++, kb++)
  {
    (void) sprintf((char *) buf, "0x%x:%s", kb->key, funcs[kb->index].desc);
    argp = kb->argp;
    for (j = 0; j < kb->numargs; j++)
    {
      (void) sprintf(tmpstr, ":%d", *(argp++));
      (void) strcat((char *) buf, tmpstr);
    }
    (void) fprintf(fp, "%s\n", buf);
  }
}


 /*ARGSUSED*/ static void conf_write(int *argp)
{
  FILE *fp;
  char filename[200];

  (void) strcpy(filename, getenv("HOME"));
  (void) strcat(filename, "/.yasr.conf");
  fp = fopen(filename, "w");
  if (!fp)
  {
    tts_say_printf("Unable to write configuration file %s.", filename);
    return;
  }
  (void) fprintf(fp, "[normalkeys]\n");
  kb_write(fp, &ui.keymap);
  (void) fprintf(fp, "[reviewkeys]\n");
  kb_write(fp, &rev.keymap);
  (void) fprintf(fp, "[options]\n");
  opt_write(fp);
  (void) fprintf(fp, "[dict]\n");
  dict_write(fp);
  (void) fclose(fp);
  tts_say_printf("Configuration saved to %s.", filename);
}
