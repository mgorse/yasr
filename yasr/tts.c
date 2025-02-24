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

#include "yasr.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef ENABLE_NLS
#include <iconv.h>
#include <langinfo.h>
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 1
#endif

char ttsbuf[80];

static Tts_synth synth[] = {
  {"", "%s\r", "\030", "\005c3", "\005c0", TRUE, "", ""},	/* speakout */
  {"", "%s[:syn]", "\003", "[:sa le]", "[:sa c]", FALSE, "[]{}\\|_@#^*<>\"`~", ""},	/* DECtalk */
  {"s\r", "q {%s}\rd\r", "s\r", "l {%c}\r", NULL, FALSE, "[]{}\\|_@#^*<>\"`~^", "exit"},	/* emacspeak server */
  {"", "%s", "\030", "\001c", "\001t", TRUE, "", ""},	/* doubletalk */
  {"", "%s\r\r", "\030\030\r\r", NULL, NULL, TRUE, "", ""},	/* BNS */
  {"", "%s\r\n", "\030", "@P1", "@P0", TRUE, "@", ""},	/* Apollo */
  {"(audio_mode 'async)\n",
   "(SayText \"%s\")\n",
   "(audio_mode 'shutup)\n",
   "(SayText '%c)\n",
   NULL,
   TRUE,
   "",
   "\003\")\n"},		/* festival */
  {"", "%s\r\n", "\033", "%c\r", NULL, TRUE, "@", ""},	/* Ciber232 */
/* speech dispatcher (note: various things handled with custom code) */
  {NULL, NULL, "CANCEL SELF\r\n", NULL, NULL, FALSE, "", "quit\r\n"},
};

static char *dict[256];
static int tts_flushed = 0;


int
dict_read (char *buf)
{
  int c;
  char *p;

  if (isdigit ((int) buf[0]))
  {
    c = strtol (buf, &p, 0);
    if (c > 255)
    {
      return (1);
    }
    p++;
  }
  else
  {
    c = buf[0];
    p = buf + 2;
  }
  dict[c] = strdup (p);

  return (0);
}


void
dict_write (FILE * fp)
{
  int i;

  for (i = 0; i < 256; i++)
  {
    if (dict[i])
    {
      if (i > 31 && i < 127 && i != 35 && i != 91)
      {
	(void) fprintf (fp, "%c=%s\n", i, dict[i]);
      }
      else
      {
	(void) fprintf (fp, "0x%.2x=%s\n", i, dict[i]);
      }
    }
  }
}


/* Search for a program on the path */

void
tts_flush ()
{
  if (tts.outlen)
  {
    w_speak (tts.buf, tts.outlen);
  }

  tts.outlen = 0;

}

#if 0
static void
tts_wait (int usecs)
{
  char buf[100];

  if (usecs != -1 && !readable (tts.fd, usecs))
    return;
  while (readable (tts.fd, 0))
  {
    read (tts.fd, buf, sizeof (buf));
  }
}
#endif

void
tts_silence ()
{
  char tmp[1] = { 0 };

  if (tts_flushed)
  {
    return;
  }
  tts.obufhead = tts.obuftail = tts.flood = 0;
#ifdef ENABLE_SPEECHD
  if (tts.synth == TTS_SPEECHD)
  {
    spd_cancel (tts.speechd);
    return;
  }
#endif
  (void) tcflush (tts.fd, TCOFLUSH);
  opt_queue_empty (1);
  tts_send (synth[tts.synth].flush, strlen (synth[tts.synth].flush));
  tts.oflag = 0;
  tts_flushed = 1;

  if (tts.synth == TTS_DECTALK)
  {
    do
    {
      if (!readable (tts.fd, 50000))
	break;
      if (read (tts.fd, tmp, 1) == -1)
      {
	perror ("tts_silence");
	break;
      }
    }
    while (tmp[0] != 1 && tmp[0] != 0);
    /* Following is a hack; without it, [:sa c] could have been sent and
       lost by the DEC-talk if a silence happens immediately after it */
    tts_send ("[:sa c]", 7);
    tts.oflag = 0;		/* pretend we didn't just do that */
  }
}


static void
tts_obufpack ()
{
  (void) memmove (tts.obuf, tts.obuf + tts.obufhead,
		  tts.obuftail - tts.obufhead);
  tts.obuftail -= tts.obufhead;
  tts.obufhead = 0;
}


#ifdef TTSLOG
static int ofd;
#endif


 /*ARGSUSED*/ static void
tts_obufout (int x)
{
  int len, len2;
  int oldoflag;

  if (!tts.flood)
  {
    opt_queue_empty (2);
  }
  oldoflag = tts.oflag;
  while (tts.obufhead < tts.obuftail)
  {
    len = strlen (tts.obuf + tts.obufhead);
    if (len > 1024)
    {
      len = 1024;
    }
    len2 = write (tts.fd, tts.obuf + tts.obufhead, len);
#ifdef TTSLOG
    (void) write (ofd, tts.obuf + tts.obufhead, len2);
#endif
    tts.obufhead += len2;
    if (len2 < len)
    {
      tts_checkreset ();
      tts.oflag = oldoflag;
      (void) signal (SIGALRM, &tts_obufout);
      alarm (1);
      return;
    }
    while (tts.obufhead < tts.obuftail && !tts.obuf[tts.obufhead])
    {
      tts.obufhead++;
    }
  }
  tts.flood = 0;
  tts.oflag = oldoflag;
  (void) signal (SIGALRM, &tts_obufout);
}


static void
tts_addbuf (char *buf, int len, int len2)
{
  char *ptr;

  tts.flood = 1;
  if (len2 == -1)
  {
    ptr = buf;
  }
  else
  {
    ptr = buf + len2;
    len -= len2;
  }
  if (tts.obuftail + len > tts.obuflen)
  {
    if ((tts.obuftail - tts.obufhead) + len > tts.obuflen)
    {
      tts.obuflen += 1024 + len - (len % 1024);
      tts.obuf = realloc (tts.obuf, tts.obuflen);
    }
    else
    {
      tts_obufpack ();
    }
  }
  (void) memcpy (tts.obuf + tts.obuftail, ptr, len);
  tts.obuftail += len;
  tts.obuf[tts.obuftail++] = 0;
  (void) alarm (1);
}


/* Low-level function to send data to the synthesizer. */
void
tts_send (char *buf, int len)
{
  int len2;

  if (!len)
  {
    return;
  }
  if (tts_flushed)
  {
    tts_flushed = 0;
  }
#ifndef SILENT
  if (!tts.flood)
  {
    len2 = write (tts.fd, buf, len);

#ifdef TTSLOG
    (void) write (ofd, buf, len2);
#endif

    if (len2 < len)
    {
      tts_addbuf (buf, len, len2);
    }
  }
  else
    tts_addbuf (buf, len, 0);
#endif

  tts.oflag = 1;
}


/* Returns TRUE if the given character can't be sent directly to the
 * synthesizer */
static int
unspeakable (unsigned char ch)
{
  char *p = synth[tts.synth].unspeakable;

  if (ch < 32)
    return 1;
  while (*p)
  {
    if (*p++ == ch)
      return 1;
  }
  return (0);
}

void
tts_end ()
{
  tts_send (synth[tts.synth].end, strlen (synth[tts.synth].end));
}

#ifdef ENABLE_NLS

int
is_unicode (unsigned char *buf, int len)
{
  int n;
  while (len)
  {
    if (!(*buf & 0x80))
    {
      buf++;
      len--;
      continue;
    }
    if (((*buf) & 0xe0) == 0xc0)
      n = 1;
    else if (((*buf) & 0xf0) == 0xe0)
      n = 2;
    else if (((*buf) & 0xf8) == 0xf0)
      n = 3;
    else if (((*buf) & 0xfc) == 0xf8)
      n = 4;
    else if (((*buf) & 0xfe) == 0xfc)
      n = 5;
    else
      return 0;
    buf++;
    len--;
    while (n--)
    {
      if (!len)
	return 0;
      if (((*buf) & 0xc0) != 0x80)
	return 0;
      len--;
      buf++;
    }
  }
  return 1;
}

void
tts_send_iso (char *buf, int len)
{
  char outbuf[1024], *o;
  iconv_t cd;
  size_t l1, l2;
  if (is_unicode ((unsigned char *) buf, len))
  {
    tts_send (buf, len);
    return;
  }
  l1 = len;
  cd = iconv_open ("UTF-8", nl_langinfo (CODESET));
  if (cd == (iconv_t) - 1)
    return;
  while (l1)
  {
    l2 = 1024;
    o = outbuf;
    iconv (cd, &buf, &l1, &o, &l2);
    if (l2 == 1024)
      break;
    tts_send (outbuf, o - outbuf);
  }
  iconv_close (cd);
}

#else

#define tts_send_iso tts_send

#endif


/* High level function to speak the given string of wide characters. */
void
tts_out_w (wchar_t *buf, int len)
{
  char obuf[1024];
  int obo = 0;
  char *p;
  int i;
  int xml = 0;			/* what's this? */

  if (!len)
    return;
  if (tts.synth == TTS_SPEECHD)
  {
    while (len > 0)
    {
      if (*buf < 0x80)
      {
	obuf[obo++] = *buf;
      }
      else if (*buf < 0x800)
      {
	obuf[obo++] = 0xc0 | (*buf >> 6);
	obuf[obo++] = 0x80 | (*buf & 0x3f);
      }
      else if (*buf < 0x10000)
      {
	obuf[obo++] = 0xe0 | (*buf >> 12);
	obuf[obo++] = 0x80 | ((*buf >> 6) & 0x3f);
	obuf[obo++] = 0x80 | (*buf & 0x3f);
      }
      else
	obuf[obo++] = ' ';
      if (obo >= 1020)
      {
	tts_out ((unsigned char *) obuf, obo);
	obo = 0;
      }
      buf++;
      len--;
    }
    if (obo)
      tts_out ((unsigned char *) obuf, obo);
    return;
  }
  opt_queue_empty (0);
  /* for other synthesizers we assume they works internally in
     ISO-8859-1 */
  p = synth[tts.synth].say;
  opt_queue_empty (0);
  while (*p)
  {
    if (*p == '%')
    {
      p++;
      for (i = 0; i < len; i++)
      {
	if (buf[i] > 255 || unspeakable ((unsigned char) buf[i]))
	{
	  if (obo > 0 && obuf[obo - 1] != ' ')
	  {
	    obuf[obo++] = ' ';
	    tts_send (obuf, obo);
	    obo = 0;
	  }
	  else if (buf[i] < 256 && dict[buf[i]])
	  {
	    (void) strcpy (obuf + obo, dict[buf[i]]);
	    obo = strlen (obuf);
	  }
	  obuf[obo++] = 32;
	}
	else if (xml)
	{
	  switch (buf[i])
	  {
	    /* For some reason, &lt; does not work for me with Voxeo */
	  case '<':
	    strcpy (obuf + obo, " less than ");
	    obo += 11;
	    break;
	  case '>':
	    strcpy (obuf + obo, "&gt;");
	    obo += 4;
	    break;
	  case '&':
	    strcpy (obuf + obo, "&amp;");
	    obo += 5;
	    break;
	  default:
	    obuf[obo++] = buf[i];
	    break;
	  }
	}
	else
	{
	  if (ui.split_caps && i > 0 && iswlower (buf[i - 1])
	      && iswupper (buf[i]))
	  {
	    obuf[obo++] = ' ';
	  }
	  obuf[obo++] = buf[i];
	}
	if (obo > (sizeof (obuf) / sizeof (obuf[0])) - 6)
	{
	  tts_send (obuf, obo);
	  obo = 0;
	}
      }
    }
    else
    {
      obuf[obo++] = *p;
    }
    p++;
  }
  tts_send (obuf, obo);
}

/* High level function to speak the given text. Takes a buffer and a length. */
void
tts_out (unsigned char *buf, int len)
{
  char obuf[1024];
  char *p;
  int obo = 0;			/* current offset into obuf */
  int i;
  int xml = 0;

  if (!len)
    return;
  opt_queue_empty (0);
  if (tts.synth == TTS_SPEECHD)
  {
#ifdef ENABLE_SPEECHD
    p = strndup ((char *) buf, len);
    spd_say (tts.speechd, SPD_MESSAGE, p);
    free (p);
#else
    char *q;
    tts_send ("SPEAK\r\n", 7);
    if (buf[0] == '.' && buf[1] == '\r')
      tts_send (".", 1);
    for (p = (char *) buf;
	 (q = strstr (p + 1, "\r.\r")) && q < (char *) buf + len; p = q)
    {
      tts_send_iso (p, q - p);
    }
    tts_send_iso (p, (long) buf + len - (long) p);
    tts_send ("\r\n.\r\n", 5);
#endif
    return;
  }
  p = synth[tts.synth].say;
  opt_queue_empty (0);
  while (*p)
  {
    if (*p == '%')
    {
      p++;
      for (i = 0; i < len; i++)
      {
	if (unspeakable ((unsigned char) buf[i]))
	{
	  if (obo > 0 && obuf[obo - 1] != ' ')
	  {
	    obuf[obo++] = ' ';
	    tts_send (obuf, obo);
	    obo = 0;
	  }
	  if (dict[buf[i]])
	  {
	    (void) strcpy (obuf + obo, dict[buf[i]]);
	    obo = strlen (obuf);
	  }
	  obuf[obo++] = 32;
	}
	else if (xml)
	{
	  switch (buf[i])
	  {
	    /* For some reason, &lt; does not work for me with Voxeo */
	  case '<':
	    strcpy (obuf + obo, " less than ");
	    obo += 11;
	    break;
	  case '>':
	    strcpy (obuf + obo, "&gt;");
	    obo += 4;
	    break;
	  case '&':
	    strcpy (obuf + obo, "&amp;");
	    obo += 5;
	    break;
	  default:
	    obuf[obo++] = buf[i];
	    break;
	  }
	}
	else
	{
	  if (ui.split_caps && i > 0 && islower (buf[i - 1])
	      && isupper (buf[i]))
	  {
	    obuf[obo++] = ' ';
	  }
	  obuf[obo++] = buf[i];
	}
	if (obo > (sizeof (obuf) / sizeof (obuf[0])) - 6)
	{
	  tts_send (obuf, obo);
	  obo = 0;
	}
      }
    }
    else
    {
      obuf[obo++] = *p;
    }
    p++;
  }
  tts_send (obuf, obo);
}

/* High level function to speak the given text. */
void
tts_say (char *buf)
{
  tts_out ((unsigned char *) buf, strlen (buf));
}


void
tts_say_printf (char *fmt, ...)
{
  va_list arg;
  char buf[200];

  va_start (arg, fmt);
  (void) vsnprintf (buf, sizeof (buf), fmt, arg);
  tts_say (buf);
  va_end (arg);
}

/* Speaks a description for the given wide character. */
void
tts_saychar (wchar_t ch)
{
  int i, j = 0;
  int stack[10];
#ifdef ENABLE_SPEECHD
  char buf[8];
#endif

  if (!ch)
  {
    ch = 32;
  }
  if (tts.synth == TTS_SPEECHD)
  {
#ifdef ENABLE_SPEECHD
    /* spd_wchar is buggy through 0.11.1, so we cannot use it */
    memset (buf, 0, sizeof (buf));
    wcrtomb (buf, ch, NULL);
    spd_char (tts.speechd, SPD_MESSAGE, buf);
#else
    if (ch == 32 || ch == 160)
      tts_send ("CHAR space\r\n", 12);
#ifndef ENABLE_NLS
    else
      tts_printf_ll ("CHAR %c\r\n", ch);
#else
    else if (ch < 0x80)
    {
      tts_printf_ll ("CHAR %c\r\n", ch);
    }
    else
    {
      char buf[8], *cin, *cout;
      size_t l1, l2;
      iconv_t cd;
      cd = iconv_open ("UTF-8", "WCHAR_T");
      if (cd == (iconv_t) - 1)
	return;
      l1 = sizeof (wchar_t);
      l2 = 7;
      cin = (char *) &ch;
      cout = buf;
      iconv (cd, &cin, &l1, &cout, &l2);
      iconv_close (cd);
      *cout = 0;
      tts_printf_ll ("CHAR %s\r\n", buf);
    }
#endif
#endif
    return;
  }
  if (unspeakable (ch) && ch < 128 && dict[ch])
  {
    tts_say (dict[ch]);
    return;
  }
  if (!synth[tts.synth].charoff)
  {				/* assume on string does everything */
    (void) sprintf (ttsbuf, synth[tts.synth].charon, ch);
    tts_send (ttsbuf, strlen (ttsbuf));
    return;
  }

  for (i = 0; i < NUMOPTS; i++)
  {
    if ((opt[i].type & 0x3f) != OT_TREE && !opt[i].ptr
	&& opt[i].synth == tts.synth)
    {
      stack[j++] = i;
      stack[j++] = opt_getval (i, 0);
      opt_set (i, &opt[i].v.enum_max);
    }
  }
  ttsbuf[1] = 0;
  opt_queue_empty (3);
  ttsbuf[0] = ch;
  tts_send (ttsbuf, 1);
  if (synth[tts.synth].saychar_needs_flush)
  {
    tts_send (synth[tts.synth].say + 2, strlen (synth[tts.synth].say) - 2);
  }
  while (j)
  {
    j -= 2;
    opt_set (stack[j], stack + j + 1);
  }
}


static char *ph_alph[] = {
  "alpha", "bravo", "charlie", "delta", "echo",
  "foxtrot", "golf", "hotel", "india", "juliet",
  "kilo", "lima", "mike", "november", "oscar",
  "papa", "quebec", "romeo", "sierra", "tango",
  "uniform", "victor", "whisky", "x ray",
  "yankee", "zulu"
};

char *
get_alph (wchar_t ch)
{
  char *c, buf[8];
  if (!iswalpha (ch))
    return NULL;
  ch = towlower (ch);
  c = NULL;
  if (ch >= 'a' && ch <= 'z')
    c = ph_alph[ch - 'a'];
  if (tts.synth != TTS_SPEECHD)
    return c;
  if (c)
    return _(c);
  sprintf (buf, "#%x", (unsigned int) ch);
  c = _(buf);
  if (!c || *c == '#')
    return NULL;
  return c;
}

void
tts_sayphonetic (wchar_t ch)
{
  char *c;
  c = get_alph (ch);
  if (!c)
  {
    tts_saychar (ch);
    return;
  }
  if (iswupper (ch))
  {
    char buf[64];
    sprintf (buf, "%s %s", _("cap"), c);
    tts_say (buf);
    return;
  }
  tts_say (c);
}

static int
open_tcp (char *port)
{
  char host[1024];
  int portnum;
  int fd;
  struct sockaddr_in servaddr;
  char *p;
  int len;

  p = strstr (port, ":");
  portnum = atoi (p + 1);
  len = p - port;
  if (len > 1023)
  {
    fprintf (stderr, "synthesizer port too long\n");
    exit (1);
  }
  memcpy (host, port, len);
  host[len] = '\0';
  if ((fd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror ("socket");
    exit (1);
  }
  memset (&servaddr, 0, sizeof (servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons (portnum);

#ifdef HAVE_INET_PTON
  if (inet_pton (AF_INET, host, &servaddr.sin_addr) <= 0)
  {
    perror ("inet_pton");
    exit (1);
  }
#else
  servaddr.sin_addr.s_addr = inet_addr (host);
#endif

  if (connect (fd, (struct sockaddr *) &servaddr, sizeof (servaddr)) < 0)
  {
    perror ("connect");
    exit (1);
  }
  return fd;
}

#ifdef ENABLE_SPEECHD
static void
refresh_speechd_option (const char *name, char **values)
{
  Opt *optr = opt;
  int i, count;

  while (optr->synth != TTS_SPEECHD
	 || strcmp (optr->internal_name, name) != 0)
    optr++;

  for (i = 0; i <= optr->v.enum_max; i++)
    free (optr->arg[i]);
  free (optr->arg);

  count = 0;
  while (values[count])
    count++;

  optr->v.enum_max = count - 1;
  optr->arg = (char **) malloc (count * sizeof (char *));
  for (i = 0; i < count; i++)
    optr->arg[i] = strdup (values[i]);
}

static void
tts_speechd_refresh_voices ()
{
  if (tts.speechd_voices)
    free_spd_modules (tts.speechd_voices);
  tts.speechd_voices = spd_list_voices (tts.speechd);
  refresh_speechd_option ("voice", tts.speechd_voices);
}
#endif

int
tts_init (int first_call)
{
  struct termios t;
  char *arg[8];
  int i;
  char *portname;
  int mode = O_RDWR | O_NONBLOCK;
  char buf[100];

  portname = getfn (tts.port);
  tts.pid = 0;
  tts.reinit = !first_call;

  (void) signal (SIGALRM, &tts_obufout);

#ifdef TTSLOG
  ofd = open ("tts.log", O_WRONLY | O_CREAT);
  if (ofd == -1)
  {
    perror ("open");
  }
#endif

#ifdef ENABLE_SPEECHD
  if (tts.synth == TTS_SPEECHD)
  {
    tts.speechd = spd_open ("yar", "main", NULL, SPD_MODE_THREADED);
    if (!tts.speechd)
    {
      fprintf (stderr, "Couldn't initialize speech-dispatcher\n");
      return -1;
    }

    if (tts.speechd_modules)
      free_spd_modules (tts.speechd_modules);
    tts.speechd_modules = spd_list_modules (tts.speechd);
    refresh_speechd_option ("module", tts.speechd_modules);

    tts_speechd_refresh_voices ();

    return 0;
  }
#endif

  if (first_call && tts.port[0] != '|' && strstr (tts.port, ":"))
  {
    tts.fd = open_tcp (tts.port);
  }
  else if (tts.port[0] != '|')
  {
    if (tts.synth == TTS_DECTALK)
    {
      mode = O_NOCTTY | O_RDWR;
    }
    else if (tts.synth == TTS_EMACSPEAK_SERVER)
    {
      mode = O_WRONLY;
    }
    if (first_call)
    {
      tts.fd = open (portname, mode);
    }
    if (tts.fd == -1)
    {
      perror ("tts");
      exit (1);
    }
    (void) tcgetattr (tts.fd, &t);
    if (tts.synth == TTS_DECTALK)
    {
      /* When sending a ^C, make sure we send a ^C and not a break */
      cfmakeraw (&t);
      /* following two lines match the Emacspeak server */
      t.c_iflag |= IXON | IXOFF;
      t.c_lflag |= IEXTEN;
      /* When the DEC-talk powers on, it sends several ^A's.  Discard them */
      tcflush (tts.fd, TCIFLUSH);
    }
    t.c_cflag |= CRTSCTS | CLOCAL;
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 10;
    cfsetospeed (&t, B9600);
    (void) tcsetattr (tts.fd, 0, &t);
  }
  else
  {				/* a pipe */
    (void) strcpy (buf, tts.port + 1);
    arg[i = 0] = strtok (buf, " ");
    while (i < 7)
    {
      if (!(arg[++i] = strtok (NULL, " ")))
      {
	break;
      }
    }

    if (first_call)
    {
      if (openpty (&tts.fd, &tts.fd_slave, NULL, NULL, NULL) == -1)
      {
	perror ("openpty");
	exit (1);
      }
    }

    if (!(tts.pid = fork ()))
    {
      /* child -- set up tty */
      (void) dup2 (tts.fd_slave, 0);
      (void) dup2 (tts.fd_slave, 1);
      (void) dup2 (tts.fd_slave, 2);
      /* tts.fd_slave is not closed */
    }
    if (!tts.pid)
    {
      (void) execvp (arg[0], arg);
      perror ("execpv");
    }
    (void) usleep (10000);
    if (tts.pid == -1)
    {
      perror ("forkpty");
    }
  }
  if (tts.synth == TTS_SPEECHD)
  {
    char *logname = getenv ("LOGNAME");
    if (logname == NULL)
      logname = getlogin ();
    tts_printf_ll ("SET self CLIENT_NAME %s:yasr:tts\r\n", logname);
  }
  else
    tts_send (synth[tts.synth].init, strlen (synth[tts.synth].init));

  /* init is now finished */
  tts.reinit = 0;

  return (0);
}

int
tts_reinit2 ()
{
  tts_init (0);
  tts_initsynth (NULL);
  return (0);
}

/* Reinitializes the speech synthesizer if the synthesizer process has died.
 * If the synthesizer process is still alive, then do nothing. */
void
tts_checkreset ()
{
  int status;

  /*If we don't have a child, do nothing */
  if (tts.pid <= 0)
    return;

  status = kill (tts.pid, 0);
  if (!status)
    return;			/*There's a process, it's good */

  /*There's no process, so let's fork a new synth engine */
  close (tts.fd);
  tts_init (FALSE);
}

/* Adds the given wide character to the buffer of characters to be sent to the
 * synthesizer. If the buffer is full, then flush it. */
void
tts_addchr (wchar_t ch)
{
  tts.buf[tts.outlen++] = ch;
  if (tts.outlen > 250)
  {
    tts_flush ();
  }
}


 /*ARGSUSED*/ void
tts_initsynth (int *argp)
{
  int i, v;

  for (i = 0; i < NUMOPTS; i++)
  {
    if ((opt[i].type & 0x40) &&
	opt[i].synth == tts.synth && (opt[i].type & 0x3f) != 0x03)
    {
      v = opt_getval (i, 0);
      opt_set (i, &v);
    }
  }
  if (!ui.silent)
  {
    tts_say (_("Synthesizer reinitialized."));
  }
}



 /*ARGSUSED*/ void
tts_reinit (int *argp)
{
  int pid = tts.pid;

  if (pid == 0)
  {
    return;
  }

  tts.reinit = 1;		/* Start reinit */

  tts_silence ();
  usleep (500000);

  if (kill (pid, SIGTERM) != 0)
  {
    if (errno == ESRCH)
    {
      tts_reinit2 ();
    }
    else
    {
      kill (pid, SIGKILL);
    }
  }

  /* wait init completion (tts.fd must be available) */
  while (tts.reinit)
  {
    usleep (100000);
  }
}


void
tts_charon ()
{
  ttssend (synth[tts.synth].charon);
}


void
tts_charoff ()
{
  ttssend (synth[tts.synth].charoff);
}


void
tts_printf_ll (const char *str, ...)
{
  char buf[200];
  va_list args;

  va_start (args, str);
  vsprintf (buf, str, args);
  tts_send (buf, strlen (buf));
}

#ifdef ENABLE_SPEECHD
void
tts_update_speechd (int num, int optval)
{
  const char *name = opt[num].internal_name;

  if (!strcmp (name, "module"))
  {
    spd_set_output_module (tts.speechd, opt[num].arg[optval]);
    tts_speechd_refresh_voices ();
  }
  else if (!strcmp (name, "voice"))
    spd_set_synthesis_voice (tts.speechd, opt[num].arg[optval]);
  else if (!strcmp (name, "rate"))
    spd_set_voice_rate (tts.speechd, optval);
  else if (!strcmp (name, "pitch"))
    spd_set_voice_pitch (tts.speechd, optval);
  else if (!strcmp (name, "volume"))
    spd_set_volume (tts.speechd, optval);
  else if (!strcmp (name, "punctuation"))
    spd_set_punctuation (tts.speechd, optval);
}
#endif
