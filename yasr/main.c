/*
 * YASR ("Yet Another Screen Reader") is an attempt at a lightweight,
 * portable screen reader.
 *
 * Copyright (C) 2001-2022 by Mike Gorse. All rights reserved.
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
 * Mike Gorse <mgorse@alum.wpi.edu>
 */

/* main.c -- contains main() and the i/o-handling functions along with the
 * code to maintain our virtual window
 *
 * See COPYING for copying information.
 */

#include "yasr.h"
#include "term.h"
#include <utmp.h>
#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif
#define UTMP_HACK
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <langinfo.h>
#include <iconv.h>
iconv_t ih_inp;
static int cpid;
static int size, wsize;
static int master, slave;
char *conffile = NULL;
unsigned char buf[256];
wchar_t wide_buf[256];
char usershell[OPT_STR_SIZE];
static struct termios t;
Win *win;
static Win *winsave;
static int win_scrollmin, win_scrollmax;	/* TODO: move these back into Win */
Tts tts;
Ui ui;
Uirev rev;
static int speaking = 1;
int kbuf[100];
int kbuflen = 0;
static wchar_t okbuf[100];
static int okbuflen = 0;
static int oldcr = 0, oldcc = 0, oldch = 0;
Voices voices;
static int shell = 0;
int special = 0;
int cl_synth = 0;
int cl_synthport = 0;

static char **subprog = NULL;	/* if non-NULL, then exec it instead of shell */

static Win *wininit (int, int);
static void win_end (Win *);

extern char **environ;

#define PARM1 (parm[0] ? parm[0] : 1)
#define PARM2 (parm[1] ? parm[1] : 1)

static void
yasr_ttyname_r (int fd, char *p, int size)
{
  char *t;

  if ((t = ttyname (fd)) == NULL)
    strcpy (p, "stdin");
  else
    (void) strncpy (p, t, size);
}

static void
child ()
{
  char arg[20];
  char *cp;
  char envstr[512];

  (void) login_tty (slave);
  if (!geteuid ())
  {				/* if we're setuid root */
    yasr_ttyname_r (0, (char *) buf, 32);
    (void) chown ((char *) buf, getuid (), -1);
    (void) setuid (getuid ());
  }
  cp = usershell + strlen (usershell) - 1;
  while (*cp != '/')
  {
    cp--;
  }
  cp++;
  arg[0] = shell ? '-' : '\0';
  *(arg + 1) = '\0';
  (void) strcat (arg, cp);
  (void) sprintf (envstr, "SHELL=%s", usershell);
  (void) putenv (envstr);
  if (subprog)
  {
    char *devname = ttyname (0);

    (void) setsid ();
    (void) close (0);
    if (open (devname, O_RDWR) < 0)
    {
      perror (devname);
    }

    (void) execve (subprog[0], subprog, environ);
  }
  else
  {
    (void) execl (usershell, arg, (void *) 0);
  }
  perror ("execl");
  exit (1);
}

/* get the appropriate name for a tty from the filename */

static void
rnget (char *s, char *d)
{
  (void) strcpy (d, s + 5);
}

static void
utmpconv (char *s, char *d, int pid)
{
#ifdef UTMP_HACK
#ifdef sun
  struct utmpx *up;
  char *rs = (char *) buf, *rd = (char *) buf + 64;
  char *rstail = NULL, *rdtail = NULL;

  rnget (s, rs);
  rnget (d, rd);

  if (rs)
  {
    if (strstr (rs, "/dev/") != NULL)
    {
      rstail = strdup (rs + sizeof ("/dev/") - 1);
    }
    else
    {
      rstail = strdup (rs);
    }
  }

  if (rd)
  {
    if (strstr (rd, "/dev/") != NULL)
    {
      rdtail = strdup (rd + sizeof ("/dev/") - 1);
    }
    else
    {
      rdtail = strdup (rd);
    }
  }

  setutxent ();
  while ((up = getutxent ()) != NULL)
  {
    if (!strcmp (up->ut_line, rstail))
    {
      (void) strcpy (up->ut_line, rdtail);
      (void) time (&up->ut_tv.tv_sec);
      up->ut_pid = pid;

      (void) pututxline (up);
      updwtmpx ("wtmpx", up);
      break;
    }
  }

  endutxent ();
#else
  FILE *fp;
  fpos_t fpos;
  struct utmp u;
  char *rs = (char *) buf, *rd = (char *) buf + 64;

  rnget (s, rs);
  rnget (d, rd);
  fp = fopen ("/var/run/utmp", "r+");
  if (!fp)
    return;
  for (;;)
  {
    (void) fgetpos (fp, &fpos);
    (void) fread (&u, sizeof (struct utmp), 1, fp);
    if (feof (fp))
    {
      break;
    }
    if (!strncmp (u.ut_line, rs, sizeof (u.ut_line)))
    {
      (void) strcpy (u.ut_line, rd);
      (void) fsetpos (fp, &fpos);
      (void) fwrite (&u, sizeof (struct utmp), 1, fp);
      break;
    }
  }
  (void) fclose (fp);
#endif /*sun */
#endif
}

 /*ARGSUSED*/ static void
finish (int sig)
{
  tts_end ();
  (void) tcsetattr (0, TCSAFLUSH, &t);
  yasr_ttyname_r (slave, (char *) buf + 128, 32);
  yasr_ttyname_r (0, (char *) buf + 192, 32);
  utmpconv ((char *) buf + 128, (char *) buf + 192, getpid ());
  if (tts.pid)
  {
    (void) kill (tts.pid, 9);
  }
  if (cpid)
  {
    (void) kill (cpid, 9);
  }
  exit (0);
}

static void getoutput ();

 /*ARGSUSED*/ static void
child_finish (int sig)
{
  int pid = 0;

  while (readable (master, 0))
    getoutput ();
  (void) signal (SIGCHLD, &child_finish);
  pid = waitpid (-1, NULL, WNOHANG);
  /* It is possible for the child to die and for this handler to be called
     before fork() ever returned, in which case cpid will still hold a value
     of 0, so need to check for that. */
  if ((pid == tts.pid) && tts.reinit)
  {
    tts_reinit2 ();
  }
  else if (cpid == 0 || pid == cpid)
  {
    finish (0);
  }
}

static int
is_char (int ch)
{
#ifdef __linux__
  /* return true as long as ch isn't prefixed by <esc> */
  while (ch != (ch % 0xff))
    ch >>= 8;
  return (ch != 27);
#else
  /* assume high ascii means meta */
  return (ch >= 32 && ch <= 126);
#endif
}

static int
is_separator (int ch)
{
  int result = 0;
  return !iswalnum (ch);
  if ((ch != ch % 0xFF) || isspace (ch))
  {
    result = 1;
  }
  else if (((ch >= 0x21) && (ch <= 0x2F))
	   || ((ch >= 0x3A) && (ch <= 0x40))
	   || ((ch >= 0x5B) && (ch <= 0x60))
	   || ((ch >= 0x7B) && (ch <= 0x7E)))
  {
    result = 1;
  }
  return result;
}

/*
  I assume the buffer contains single (may be multibyte)
  character or control sequence
 * MPG: This code is wrong (ie, 1bxx can be U+1bxx or a control char, and
 * these cannot be distinguished by this code).  Key really should be a
 * struct.  Re-working this could be part of a larger re-working of
 * keybindings to make their names more intuitive in yasr.conf.
*/
static int
getkey_buf ()
{
  char *b1, *b2;
  size_t s1, s2;
  wchar_t ch;
  int key;
  int result;

  s1 = size;
  s2 = sizeof (wchar_t);
  b1 = (char *) buf;
  b2 = (char *) &ch;
  iconv (ih_inp, NULL, NULL, NULL, NULL);
  result = iconv (ih_inp, &b1, &s1, &b2, &s2);
  if (result != -1)
  {
    if (!s1)
      return ch;
  }
  key = (int) buf[0];
  if (size > 1)
  {
    key = (key << 8) + buf[1];
  }
  if (size > 2)
  {
    key = (key << 8) + buf[2];
  }
  if (size > 3)
  {
    key = (key << 8) + buf[3];
  }
  if (key >> 8 == 0x1b4f)
  {
    key += 0x000c00;
  }

  /* Convert high-bit meta keys to escape form */
#ifndef __linux__
  if (key >= 0x80 && key <= 0xff)
    key += 0x1a80;
#endif
  return key;
}

static void
getinput ()
{
  int key;

  size = read (0, buf, 255);
  if (size <= 0)
  {
    finish (0);
  }
  key = getkey_buf ();
  if (key == ui.disable)
  {
    if (ui.disabled)
    {
      tts_initsynth (NULL);
      ui.disabled = ui.silent = 0;
      tts_say (_("yasr enabled."));
    }
    else
    {
      tts_silence ();
      tts_say (_("yasr disabled."));
      ui.silent = ui.disabled = 1;
    }
    return;
  }
  else if (ui.disabled)
  {
    (void) write (master, buf, size);
    return;
  }

  tts_silence ();
  if (ui.silent == -1)
  {
    ui.silent = 0;
  }
  ui.silent = -ui.silent;
  if (ui.meta)
  {
    (void) write (master, buf, size);
    ui.meta = 0;
    return;
  }
  if (ui.kbsay == 2 && is_separator (key))
  {
    tts_out_w (okbuf, okbuflen);
    okbuflen = tts.oflag = 0;
  }
  if (!ui_keypress (key))
  {
    (void) write (master, buf, size);
  }
}

static void
wincpy (Win ** d, Win * s)
{
  int i;

  if (*d)
    win_end (*d);
  *d = wininit (s->rows, s->cols);
  (*d)->cr = s->cr;
  (*d)->cc = s->cc;
  (*d)->mode = s->mode;
  for (i = 0; i < s->rows; i++)
  {
    (void) memcpy ((*d)->row[i], s->row[i], s->cols * CHARSIZE);
  }
  (void) memcpy (&(*d)->savecp, &s->savecp, sizeof (Curpos));
}

static void
win_end (Win * win)
{
  int i;

  for (i = 0; i < win->rows; i++)
  {
    free (win->row[i]);
  }
  free (win->row);
  free (win->tab);
}

static void
win_scrollup ()
{
  int i;
  chartype *tmpc;

  if (rev.cr)
    rev.cr--;
  if (oldcr)
    oldcr--;
  tmpc = win->row[win_scrollmin];
  (void) memset (tmpc, 0, win->cols * CHARSIZE);
  for (i = win_scrollmin; i < win_scrollmax; i++)
  {
    win->row[i] = win->row[i + 1];
  }
  win->row[i] = tmpc;
  win->cr--;
}

static void
win_lf ()
{
  int i;
  chartype *tmpc;

  if (win->cr == win_scrollmax)
  {
    if (rev.cr)
      rev.cr--;
    if (oldcr)
      oldcr--;
    tmpc = win->row[win_scrollmin];
    (void) memset (tmpc, 0, win->cols * CHARSIZE);
    for (i = win_scrollmin; i < win_scrollmax; i++)
    {
      win->row[i] = win->row[i + 1];
    }
    win->row[i] = tmpc;
    win->cr--;
  }
  win->cr++;
}

static void
win_scrolldown ()
{
  int i;
  chartype *tmpc;

  tmpc = win->row[win_scrollmax];
  (void) memset (tmpc, 0, CHARSIZE * win->cols);
  for (i = win_scrollmax - 1; i >= win->cr; i--)
  {
    win->row[i + 1] = win->row[i];
  }
  win->row[win->cr] = tmpc;
}

static void
win_rlf ()
{
  if (win->cr == win_scrollmin)
    win_scrolldown ();
  else
    win->cr--;
}

int
readable (int fd, int wait)
{
  fd_set fds;
  struct timeval tv;

  tv.tv_usec = wait % 1000000;
  tv.tv_sec = wait / 1000000;
  FD_ZERO (&fds);
  FD_SET (fd, &fds);
  (void) select (fd + 1, &fds, NULL, NULL, &tv);
  return (FD_ISSET (fd, &fds));
}

#if 0
static char *
oldgulp (unsigned char *buf, int *size, unsigned char *cp, unsigned char **ep)
{
  int os = *size;
  int n;

  if (!readable (master, 1000000))
    return NULL;
  if (cp - buf >= 200)
  {
    if (ep)
    {
      n = buf + *size - *ep;
      (void) memmove (buf, *ep, 256 - n);
      *size = n + read (master, buf + n, 255 - n);
      buf[*size] = '\0';
      (void) write (1, buf + n, *size - n);
      *ep = buf;
      return ((char *) buf + n);
    }
    *size = read (master, buf, 255);
    buf[*size] = '\0';
    (void) write (1, buf, *size);
    return ((char *) buf);
  }
  *size += read (master, buf + *size, 255 - *size);
  buf[*size] = '\0';
  return ((char *) (buf + os));
}

#endif

/*!
function reads portion of data into buf and converts
to wide string, leaving 'leave' character in wide_buf;
*/
static int bytes_left;
static char *bytes_left_start;
static void
read_buf (int leave)
{
  char *b1, *b2;
  size_t s1, s2;
  if (bytes_left)
  {
    memcpy (buf, bytes_left_start, bytes_left);
  }
  size = read (master, buf + bytes_left, 255 - bytes_left - leave);
  if (size < 0)
  {
    perror ("read");
    exit (1);
  }
  write (1, buf + bytes_left, size);
  size += bytes_left;
  buf[size] = 0;
  bytes_left = 0;
  b1 = (char *) buf;
  b2 = (char *) (wide_buf + leave);
  if (leave)
    memcpy (wide_buf, wide_buf + wsize - leave,
	    sizeof (wchar_t) * (wsize - leave));
  s1 = size;
  s2 = (255 - leave) * sizeof (wchar_t);
  while (s1 > 0)
  {

    iconv (ih_inp, NULL, NULL, NULL, NULL);
    if (iconv (ih_inp, &b1, &s1, &b2, &s2) == (size_t) -1)
    {
      if (errno == EINVAL)	/* incomplete sequence at end of buffer */
      {
	break;
      }
      /* invalid multibyte sequence - should we ignore or insert
         some character meaning 'invalid'? */
      b1++;
      s1--;
    }
  }
  bytes_left = s1;
  bytes_left_start = b1;
  wsize = (wchar_t *) b2 - wide_buf;
  wide_buf[wsize] = 0;
}

static wchar_t *
gulp (wchar_t *cp, wchar_t **ep)
{
  int leave;
  if (!ep)
    leave = 0;
  else
    leave = wsize - (*ep - wide_buf);
  if (!readable (master, 1000000))
    return NULL;
  read_buf (leave);
  if (ep)
    *ep = wide_buf;
  return wide_buf;
}

static void
kbsay ()
{
  if (!ui.kbsay)
    return;
  if (buf[0] == 8 || kbuf[0] == 127)
  {
    if ((ui.kbsay == 2) && (okbuflen != 0))
    {
      okbuf[--okbuflen] = 0;
    }

    /*tts_say(_("back")); */
    return;
  }
  if (ui.kbsay == 1)
  {
    tts_saychar (kbuf[0]);
    return;
  }

  /* ui.kbsay == 2 -- handle word echo */
  if (okbuflen < sizeof (kbuf) - 1 && is_char (kbuf[0]))
  {
    okbuf[okbuflen++] = kbuf[0];
  }
}

#define MIN(a, b) ((a)>(b)? (b): (a))
static long
strwtol (wchar_t **p)
{
  long t = 0;
  while (iswdigit (**p))
  {
    t = 10 * t + *(*p)++ - '0';
  }
  return t;
}

static void
win_csi (wchar_t **pp)
{
  int parm[16], numparms = 0;
  int ignore = 0;
  wchar_t *p;
  int i;
  int x;

  p = *pp;
  if (*p == '[')
    p++;
  if (*p == '?')
    p++;
  while (!*p || iswdigit ((int) *p) || *p == ';')
  {
    if (!*p)
    {
      if (!(p = gulp (p, pp)))
      {
	return;
      }
    }
    else
    {
      p++;
    }
  }
  p = *pp;
  if (*p == '[')
  {
    ignore = 1;
    p++;
  }
  if (*p == '?')
  {
    p++;
  }
  (void) memset (&parm, 0, sizeof (int) * 16);
  while (numparms < 16 && (*p == ';' || iswdigit ((int) *p)))
  {
    parm[numparms++] = strwtol (&p);
    if (*p == ';')
      p++;
    else
      break;			/* TODO: is this redundant? */
  }

  *pp = p + 1;
  if (ignore)
  {
    return;
  }
  switch (*p)
  {
  case '@':			/* insert characters */
    (void) memmove (win->row[win->cr] + win->cc + PARM1,
		    win->row[win->cr] + win->cc,
		    (win->cols - win->cc - PARM1) * CHARSIZE);
    (void) memset (win->row[win->cr] + win->cc, 0, parm[0] * CHARSIZE);
    break;

  case 'A':			/* move up */
    win->cr -= PARM1;
    if (win->cr < 0)
    {
      win->cr = 0;
    }
    break;

  case 'B':			/* move down */
    win->cr += PARM1;
    break;

  case 'C':
  case 'a':			/* move right */
    win->cc += PARM1;
    break;

  case 'D':			/* move left */
    win->cc -= PARM1;
    break;

  case 'E':
  case 'e':			/* move down indicated number of rows */
    win->cr += PARM1;
    win->cc = 0;
    break;

  case 'F':			/* move up indicated number of rows */
    win->cr -= PARM1;
    win->cc = 0;
    break;

  case 'G':
  case '`':
    win->cc = parm[0] - 1;
    break;

  case 'H':
  case 'f':			/* move to specified row/col or top left if none given */
    if ((kbuf[0] == 8 || kbuf[0] == 127) &&
	PARM1 == win->cr + 1 && PARM2 == win->cc && !tts.oflag)
    {
      kbsay ();
      ui_saychar (win->cr, win->cc - 1);
    }
    win->cr = PARM1 - 1;
    win->cc = PARM2 - 1;
    break;

  case 'J':
    switch (parm[0])
    {
    case 0:			/* erase from cursor to end of win->creen */
      (void) memset (win->row[win->cr] + win->cc, 0,
		     CHARSIZE * (win->cols - win->cc));
      for (i = win->cr + 1; i < win->rows; i++)
      {
	(void) memset (win->row[i], 0, win->cols * CHARSIZE);
      }
      break;

    case 1:			/* erase from start to cursor */
      (void) memset (win->row[win->cr], 0, win->cc);
      for (i = 0; i < win->cr; i++)
      {
	(void) memset (win->row[i], 0, win->cols * CHARSIZE);
      }
      break;

    case 2:			/* erase whole screen */
      for (i = 0; i < win->rows; i++)
      {
	(void) memset (win->row[i], 0, win->cols * CHARSIZE);
      }
      break;
    }
    break;

  case 'K':
    switch (parm[0])
    {
    case 0:
      (void) memset (win->cr[win->row] + win->cc, 0,
		     (win->cols - win->cc) * CHARSIZE);
      break;

    case 1:
      (void) memset (win->row[win->cr], 0, (win->cc + 1) * CHARSIZE);
      break;

    case 2:
      (void) memset (win->row[win->cr], 0, win->cols * CHARSIZE);
      break;
    }
    break;

  case 'L':			/* insert rows */
    x = MIN (PARM1, win_scrollmax - win->cr);
    for (i = win_scrollmax; i >= win->cr + x; i--)
    {
      (void) memcpy (win->row[i], win->row[i - x], win->cols * CHARSIZE);
    }
    for (i = win->cr; i < win->cr + x; i++)
    {
      (void) memset (win->row[i], 0, win->cols * CHARSIZE);
    }
    break;

  case 'M':
    x = MIN (PARM1, win_scrollmax - win->cr);
    if (x + win->cr > win_scrollmax)
    {
      x = win_scrollmax - win->cr;
    }
    for (i = win->cr; i <= win_scrollmax - x; i++)
    {
      (void) memcpy (win->row[i], win->row[i + x], win->cols * CHARSIZE);
    }
    for (i = win_scrollmax - x + 1; i <= win_scrollmax; i++)
    {
      (void) memset (win->row[i], 0, win->cols * CHARSIZE);
    }
    break;

  case 'P':			/* delete characters */
    x = MIN (PARM1, win->cols - win->cc);
    (void) memmove (win->row[win->cr] + win->cc,
		    win->row[win->cr] + win->cc + x,
		    (win->cols - win->cc - x) * CHARSIZE);
    (void) memset (win->row[win->cr] + win->cols - x, 0, x * CHARSIZE);
    break;

  case 'S':			/* Scroll up */
    for (i = 0; i < PARM1; i++)
    {
      win_scrollup ();
    }
    break;

  case 'T':			/* Scroll down */
    for (i = 0; i < PARM1; i++)
    {
      win_scrolldown ();;
    }
    break;

  case 'X':			/* Erase characters */
    x = MIN (PARM1, win->cols - win->cc);
    (void) memset (win->row[win->cr] + win->cc, 0, x * CHARSIZE);
    break;

  case 'd':			/* move to indicated row */
    win->cr = PARM1 - 1;
    break;

  case 'g':
    switch (parm[0])
    {
    case 3:
      for (i = 0; i < win->cols; i++)
      {
	win->tab[i] = 0;
      }
      break;

    case 0:
      win->tab[win->cc] = 0;
      break;
    }
    break;

  case 'h':
    win->mode |= 1 << (PARM1 - 1);
    break;

  case 'l':
    win->mode &= ~(1 << (PARM1 - 1));
    break;

  case 'm':
    for (i = 0; i < numparms; i++)
    {
      switch (parm[i])
      {
      case 0:
	win->attr = 0;
	break;
      case 1:
	win->attr |= ATTR_BOLD;
	break;
      case 2:
	win->attr |= ATTR_HALFBRIGHT;
	break;
      case 4:
	win->attr |= ATTR_UNDERSCORE;
	break;
      case 5:
	win->attr |= ATTR_BLINK;
	break;
      case 7:
	win->attr |= ATTR_RV;
	break;
      case 24:
	win->attr &= ~ATTR_UNDERSCORE;
	break;
      case 25:
	win->attr &= ~ATTR_BLINK;
	break;
      case 27:
	win->attr &= ~ATTR_RV;
	break;
      }
    }
    break;

  case 'r':
    win_scrollmin = (parm[0] ? MIN (parm[0], win->rows) - 1 : 0);
    win_scrollmax = parm[1] ? MIN (parm[1], win->rows) - 1 : win->rows - 1;
    break;

  case 's':
    win->savecp.cr = win->cr;
    win->savecp.cc = win->cc;
    break;

  case 'u':
    win->cr = win->savecp.cr;
    win->cc = win->savecp.cc;
    break;
  }

  if (win->cr >= win->rows)
  {
    win->cr = win->rows - 1;
  }
  else if (win->cr < 0)
  {
    win->cr = 0;
  }
  if (win->cc >= win->cols - 1)
    win->cc = win->cols - 1;
  else if (win->cc < 0)
    win->cc = 0;
}

static void
win_addchr (wchar_t ch, int tflag)
{
  if (win->cc == win->cols)
  {
    win->cc = 0;
    win_lf ();
    win->carry++;
  }
  if (win->mode & 0x08)
  {
    (void) memmove (win->row[win->cr] + win->cc + 1,
		    win->row[win->cr] + win->cc,
		    (win->cols - win->cc - 1) * CHARSIZE);
  }
  win->row[win->cr][win->cc].attr = win->attr;
  win->row[win->cr][win->cc++].wchar = ch;
  if (tflag)
  {
    if (ui.silent != 1)
    {
      tts_addchr (ch);
    }
  }
}

wchar_t
realchar (wchar_t ch)
{
  if (!ch)
    return 32;
  return ch;
}

/* bol -- beginning of line? */
static int
bol (int cr, int cc)
{
  int i;
  chartype *rptr;

  rptr = win->row[cr];
  for (i = 0; i < cc; i++)
  {
    if (y_isblank (rptr[i].wchar))
    {
      return (0);
    }
  }

  return (1);
}

/* eol -- end of line? */

static int
eol (int cr, int cc)
{
  int i;
  chartype *rptr;

  rptr = win->row[cr];
  for (i = cc + 1; i < win->cols; i++)
  {
    if (y_isblank (rptr[i].wchar))
    {
      return (0);
    }
  }

  return (1);
}

/* firstword -- are we in the first word of the line? */

static int
firstword (int cr, int cc)
{
  chartype *rptr;
  int i;

  rptr = win->row[cr];
  i = cc;
  while (i && !y_isblank (rptr[i].wchar))
  {
    i--;
  }
  for (; i; i--)
  {
    if (!y_isblank (rptr[i].wchar))
    {
      return (0);
    }
  }

  return (1);
}

/* lastword -- are we in the last word of the line? */

 /*ARGSUSED*/ static int
lastword (int cr, int cc)
{
  chartype *rptr;
  int i = 0;

  rptr = win->row[cr];
  if (y_isblank (rptr[i].wchar))
  {
    i++;
  }
  while (i < win->cols && !y_isblank (rptr[i].wchar))
  {
    i++;
  }
  while (i < win->cols)
  {
    if (!y_isblank (rptr[i++].wchar))
    {
      return (0);
    }
  }

  return (1);
}

static void
getoutput ()
{
  wchar_t ch = 0;
  wchar_t *p;
  int i;
  int chr = 0;
  static int stathit = 0, oldoflag = 0;

  read_buf (0);
#ifdef TERMTEST
  (void) printf ("size=%d buf=%s\n", size, buf);
#endif

  if (!size)
  {
    finish (0);
  }
  p = wide_buf;

  while (p - wide_buf < wsize)
  {
    switch (ch = *p++)
    {
    case 0:
    case 7:
      break;

    case 8:
      if (win->cc)
	win->cc--;
      else if (win->carry && win->cr)
      {
	win->cr--;
	win->cc = win->cols - 1;
	win->carry--;
      }
      if ((kbuf[0] == 8 || kbuf[0] == 127) && !tts.oflag && !ui.silent)
      {
	kbsay ();
	ui_saychar (win->cr, win->cc);
      }
      if (tts.outlen)
      {
	tts.outlen--;
      }
      break;

    case 9:
      for (i = win->cc + 1; i < win->cols; i++)
      {
	if (win->tab[i])
	{
	  win->cc = i;
	  break;
	}
      }
      if (i == win->cols)
      {
	win->cc = i - 1;
      }
      break;

    case 10:
    case 11:
    case 12:
      win_lf ();
      break;

    case 13:
      win->cc = win->carry = 0;
      break;

    case 14:
    case 15:
      break;			/* may need to change in the future */

    case 27:
      if (!*p && !(p = gulp (p, NULL)))
      {
	return;
      }
      switch (*p++)
      {
      case 'D':
	win_lf ();
	break;
      case 'E':
	break;			/* FIXME -- new line */
      case 'H':
	win->tab[win->cc] = 1;
	break;
      case 'M':
	win_rlf ();
	break;
      case '7':
	wincpy (&winsave, win);
	oldcr = oldcc = 255;
	break;
      case '8':
	wincpy (&win, winsave);
	break;
      case '[':
	win_csi (&p);
	break;
      }
      break;

      /* case 155: */
      /*     win_csi(buf, &p, &size);  */
      /*     break; *//* supported by Linux console, at least */

    default:
#if 0
      if (special)
      {
	if (ch == '<')
	{
	  speaking = 0;		/* hack for medievia.com */
	}
	else if (ch == '>')
	{
	  speaking = 1;
	}
      }
#endif
      if (ch == kbuf[0] && win->cr == oldcr && win->cc == oldcc && kbuflen)
      {
/* this character was (probably) echoed as a result of a keystroke */
	kbsay ();
	win_addchr (ch, 0);
	(void) memmove (kbuf, kbuf + 1, (--kbuflen) * sizeof (int));
      }
      else
      {
	win_addchr (ch, speaking && (!special || !win->cr));
      }
      chr = 1;
    }
    if (!chr && ch != 8 && (stathit == 0 || ch < '0' || ch > '9'))
    {
      tts_flush ();
    }
    else
    {
      chr = 0;
    }
    if ((!win->cc && win->cr > win->rows - 3) || win->cr == win->rows - 1)
    {
      stathit = win->cr;
      oldoflag = tts.oflag;
    }
    if (stathit && stathit - win->cr > 1)
    {
      stathit = -1;
      tts.oflag = oldoflag;
    }
  }
  if (ch == 13 || ch == 10 || ch == 32)
  {
    tts_flush ();
  }
  if (size > 1)
  {
    if (!readable (master, 0))
      tts_flush ();
    else
      return;
  }
  else if (ch == 32 || ch == 13)
  {
    tts_flush ();
  }
  if (tts.oflag || kbuf[0] == 13 || kbuf[0] == 3 || ui.silent)
  {
    tts.oflag = stathit = 0;
    oldcr = win->cr;
    oldcc = win->cc;
    oldch = win->row[win->cr][win->cc].wchar;
    return;
  }
  stathit = tts.outlen = 0;

  /* Nothing was spoken.  See if the program moved the cursor.
   * If that's the case, read something based on how the cursor moved.
   */
  if (win->cr == oldcr)
  {
    switch (win->cc - oldcc)
    {
    case 1:			/* cursor moved right one character */
      if ((realchar (win->row[win->cr][win->cc - 1].wchar) == kbuf[0] &&
	   realchar (oldch) != kbuf[0]) ||
	  ((y_isblank (oldch) && kbuf[0] == 32)))
      {
	break;
      }
      if (kbuf[0] == 0x1b5b43 ||
	  ((ui.curtrack == 2) && (ui.kbsay != 2 || is_separator (kbuf[0]))))
      {
	ui_saychar (win->cr, win->cc);
      }
      break;

    case 0:
      break;

    case -1:
      if (kbuf[0] == 0x1b5b44 || ui.curtrack == 2)
      {
	ui_saychar (win->cr, win->cc);
      }
      break;

    default:
      if (ui.curtrack == 2)
      {
	if (eol (win->cr, win->cc))
	  ui_saychar (win->cr, win->cc);
	else
	{
	  ui_sayword (win->cr, cblank (win->cr, win->cc) ?
		      win->cc + 1 : win->cc);
	}
      }
    }
  }
  else if ((kbuf[0] == 0x1b5b43 && bol (win->cr, win->cc)) ||
	   (kbuf[0] == 0x1b5b44 && eol (win->cr, win->cc)))
  {
    ui_saychar (win->cr, win->cc);
  }
  else
  {
    switch (win->cr - oldcr)
    {
    case 1:			/* cursor moved down a line */
      if (kbuf[0] == 0x1b5b42)
      {
	ui_sayline (win->cr, 1);
	break;
      }
      if (win->cc == 0 && (oldcr == win->cols - 1 || kbuf[0] == 0x1b5b43))
      {
	ui_saychar (win->cr, win->cc);
	break;
      }
      if (ui.curtrack < 2)
      {
	break;
      }
      if (win->cc && bol (win->cr, win->cc) && lastword (oldcr, oldcc)
	  && oldcc)
      {
	ui_sayword (win->cr, win->cc);
      }
      else
      {
	ui_sayline (win->cr, 1);
      }
      break;
    case -1:			/* cursor moved up a line */
      if (kbuf[0] == 0x1b5b41)
      {
	ui_sayline (win->cr, 1);
	break;
      }
      if (ui.curtrack < 2)
	break;
      if (win->cc == win->cols - 1 && (oldcr == 0 || kbuf[0] == 0x1b5b44))
      {
	ui_saychar (win->cr, win->cc);
      }
      else if (lastword (win->cr, win->cc) &&
	       !firstword (win->cr, win->cc) &&
	       (!win->cc || cblank (win->cr, win->cc - 1)) &&
	       firstword (oldcr, oldcc))
      {
	ui_sayword (win->cr, win->cc);
      }
      else
	ui_sayline (win->cr, 1);
      break;
    }
  }
  oldcr = win->cr;
  oldcc = win->cc;
  oldch = win->row[win->cr][win->cc].wchar;
}

static void
get_tts_input ()
{
  if (!readable (tts.fd, 1))
    return;
  (void) read (tts.fd, buf, 100);
}

static void
parent ()
{
  fd_set rf;
  struct termios rt;
  int maxfd;

  (void) memcpy (&rt, &t, sizeof (struct termios));
  cfmakeraw (&rt);
  rt.c_cc[VMIN] = 1;
  rt.c_cc[VTIME] = 0;
  (void) tcsetattr (0, TCSAFLUSH, &rt);
  yasr_ttyname_r (0, (char *) (buf + 128), 32);
  yasr_ttyname_r (slave, (char *) (buf + 192), 32);
  utmpconv ((char *) (buf + 128), (char *) (buf + 192), cpid);
  maxfd = (master > tts.fd ? master : tts.fd) + 1;

  for (;;)
  {
    int result;
    FD_ZERO (&rf);
    FD_SET (master, &rf);
    FD_SET (0, &rf);
    FD_SET (tts.fd, &rf);
    result = select (maxfd, &rf, NULL, NULL, NULL);
    if (result == -1)
    {
      if (errno == EINTR)
	continue;
      else
      {
	perror ("select");
	break;
      }
    }
    if (FD_ISSET (0, &rf))
    {
      getinput ();
    }
    if (FD_ISSET (master, &rf))
    {
      getoutput ();
      kbuflen = 0;
    }
    if (FD_ISSET (tts.fd, &rf))
    {
      get_tts_input ();
    }
  }
}

#if 0
int
isctty ()
{
  ttyname_r (0, buf, 64);

  return (!strcmp (buf, "/dev/console") ||
	  (!strncmp (buf, "/dev/tty", 8) && isdigit (buf[8])));
}
#endif

char charmap[64] = "ASCII";
int
main (int argc, char *argv[])
{
  struct winsize winsz = { 0, 0 };
  int flag = 1;

  /* initialize gettext */
#ifdef ENABLE_NLS
#ifdef HAVE_LOCALE_H
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  textdomain (GETTEXT_PACKAGE);
  strcpy (charmap, nl_langinfo (CODESET));
  if (!strcmp (charmap, "ANSI_X3.4-1968"))
    strcpy (charmap, "UTF-8");

  ih_inp = iconv_open ("WCHAR_T", charmap);
  if (ih_inp == (iconv_t) - 1)
  {
    fprintf (stderr, "Codeset %s not supported\n", charmap);
    exit (1);
  }


#endif

  if (argv[0][0] == '-')
    shell = 1;
  if (isatty (0))
  {
    (void) ioctl (0, TIOCGWINSZ, &winsz);
  }
  if (!winsz.ws_row)
  {
    winsz.ws_row = 25;
    winsz.ws_col = 80;
  }
  win = wininit (winsz.ws_row, winsz.ws_col);
  win_scrollmin = 0;
  win_scrollmax = winsz.ws_row - 1;
  winsave = NULL;
  uinit ();
  (void) memset (&tts, 0, sizeof (Tts));
  opt_init ();
  (void) memset (&voices, 0, sizeof (voices));
  while (flag)
  {
    switch (getopt (argc, argv, "C:c:s:p:"))
    {
    case 'C':
      conffile = strdup (optarg);
      break;
    case 'c':
      argv[0] = "/bin/sh";
      (void) execv (argv[0], argv);
      break;
    case 's':
      (void) sprintf ((char *) buf, "synthesizer=%s", optarg);
      opt_read ((char *) buf, 0);
      cl_synth = 1;
      break;
    case 'p':
      (void) sprintf ((char *) buf, "synthesizer port=%s", optarg);
      opt_read ((char *) buf, 0);
      cl_synthport = 1;
      break;

    default:
      flag = 0;
    }
  }
  if (argv[optind])
  {
    subprog = argv + optind;
  }
  readconf ();

#if 0				/* this doesn't work */
  if (!isctty ())
  {
    cp = usershell + strlen (usershell) - 1;
    while (*cp && *cp != '/')
    {
      cp--;
    }
    cp++;
    argv[0] = cp;
    (void) execv (usershell, argv);
  }
#endif

  (void) openpty (&master, &slave, NULL, NULL, &winsz);
  (void) signal (SIGCHLD, &child_finish);
  (void) tcgetattr (0, &t);
  cpid = fork ();
  if (cpid > 0)
    parent ();
  else if (cpid == 0)
    child ();
  perror ("fork");
  return -1;
}

static Win *
wininit (int nr, int nc)
{
  int i;
  Win *win;

  win = (Win *) calloc (sizeof (Win), 1);
  if (!win)
  {
    fprintf (stderr, "wininit: cannot allocate %lu bytes\n",
	     (unsigned long) sizeof (Win));
    exit (1);
  }
  win->rows = nr;
  win->cols = nc;
  win->row = (chartype **) malloc (win->rows * sizeof (chartype *));
  win->cr = win->cc = 0;
  for (i = 0; i < win->rows; i++)
  {
    win->row[i] = (chartype *) calloc (win->cols, CHARSIZE);
    if (!win->row[i])
    {
      fprintf (stderr, "wininit: cannot allocate row %d\n", i);
      exit (1);
    }
  }
  win->savecp.cr = win->savecp.cc = 0;
  win->tab = (char *) calloc (win->cols, 1);
  if (!win->tab)
  {
    fprintf (stderr, "wininit: cannot allocate win->tab\n");
    exit (1);
  }
  for (i = 0; i < win->cols; i += 8)
    win->tab[i] = 1;
  return (win);
}


void
w_speak (wchar_t *ibuf, int len)
{
  char obuf[1024];
  int olen;
  wchar_t *wstart;
  int lc = 0, nc = 0;
  int len1;
  int i;

  if (!len)
    len = wcslen (ibuf);
  len1 = len - 1;
  wstart = ibuf;
  olen = 0;
  for (i = 0; i < len; i++)
  {
    if (ibuf[i] == lc && ++nc == ui.minrc - 1)
    {
      olen -= nc;
      if (olen)
      {
	tts_out_w (wstart, olen);
      }
      olen = 0;
      while (i < len1 && ibuf[i + 1] == lc)
      {
	i++;
	nc++;
      }
      tts_saychar (lc);
      tts_out ((unsigned char *) obuf,
	       sprintf (obuf, "%s %d %s\r", _("repeats"), nc + 1,
			_("times")));
      nc = 0;
    }
    else
    {
      if (!olen)
	wstart = ibuf + i;
      olen++;
      if (ibuf[i] != lc)
      {
	nc = 0;
      }
      if (!iswalnum (ibuf[i]) &&
	  ibuf[i] != 32 && ibuf[i] != '=' && ibuf[i] >= 0)
      {
	lc = ibuf[i];
      }
      else
	lc = 0;
    }
  }
  if (olen)
  {
    tts_out_w (wstart, olen);
  }
}

#if 0
void
speak (char *ibuf, int len)
{
  char obuf[1024];
  int lc = 0, nc = 0;
  int len1, olen = 0;
  int i;

  if (!len)
    len = strlen (ibuf);
  len1 = len - 1;

  for (i = 0; i < len; i++)
  {
    if (ibuf[i] == lc && ++nc == ui.minrc - 1)
    {
      olen -= nc;
      if (olen)
      {
	tts_out ((unsigned char *) obuf, olen);
      }
      olen = 0;
      while (i < len1 && ibuf[i + 1] == lc)
      {
	i++;
	nc++;
      }
      tts_saychar (lc);
      tts_out ((unsigned char *) obuf,
	       sprintf (obuf, "%s %d %s\r", _("repeats"), nc + 1,
			_("times")));
      olen = nc = 0;
    }
    else
    {
      obuf[olen++] = ibuf[i];
      if (ibuf[i] != lc)
      {
	nc = 0;
      }
      if (!isalpha ((int) ibuf[i]) && !isdigit ((int) ibuf[i]) &&
	  ibuf[i] != 32 && ibuf[i] != '=' && ibuf[i] >= 0)
      {
	lc = ibuf[i];
      }
      else
	lc = 0;
      if (olen > 250 && !nc)
      {
	tts_out ((unsigned char *) obuf, olen);
	olen = 0;
      }
    }
  }
  if (olen)
  {
    tts_out ((unsigned char *) obuf, olen);
  }
}
#endif
