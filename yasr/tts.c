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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tts.h"

char ttsbuf[80];

static Tts_synth synth[] = {
  {"", "%s\r", "\030", "\005c3", "\005c0", 1, "", ""},	/* speakout */
  {"", "%s[:syn]", "\003", "[:sa le]", "[:sa c]", 0, "[]{}\\|_@#^*<>\"`~", ""},	/* DECtalk */
  {"s\r", "q {%s}\rd\r", "s\r", "l %c\r", NULL, 0, "[]{}\\|_@#^*<>\"`~^", "exit"},	/* emacspeak server */
  {"", "%s", "\030", "\001c", "\001t", 1, "", ""},	/* doubletalk */
  {"", "%s\r\r", "\030\030\r\r", NULL, NULL, 1, NULL, NULL},	/* BNS */
  {"", "%s\r\n", "\030", "@P1", "@P0", 1, "@", ""},	/* Apollo */
  {"(audio_mode 'async)\n(parameter.set 'Audio_Method 'netaudio)\n",
   "(SayText \"%s\")\n",
   "(audio_mode 'shutup)\n",
   "(SayText '%c)\n",
   NULL,
   1,
   "",
   "\003\")\n",} /* festival */
};

static char *dict[256];
static int tts_flushed = 0;


int dict_read(char *buf)
{
  int c;
  char *p;

  if (isdigit((int) buf[0]))
  {
    c = strtol(buf, &p, 0);
    if (c > 255)
    {
      return (1);
    }
    p++;
  } else
  {
    c = buf[0];
    p = buf + 2;
  }
  dict[c] = strdup(p);

  return (0);
}


void dict_write(FILE * fp)
{
  int i;

  for (i = 0; i < 256; i++)
  {
    if (dict[i])
    {
      if (i > 31 && i < 127 && i != 35 && i != 91)
      {
	(void) fprintf(fp, "%c=%s\n", i, dict[i]);
      } else
      {
	(void) fprintf(fp, "0x%.2x=%s\n", i, dict[i]);
      }
    }
  }
}


/* Search for a program on the path */

void tts_flush()
{
  if (tts.outlen)
  {
    speak(tts.buf, tts.outlen);
  }
  tts.outlen = 0;
}


void tts_silence()
{
  char tmp[1] = { 0 };

  if (tts_flushed)
  {
    return;
  }
  (void) tcflush(tts.fd, TCOFLUSH);
  tts.obufhead = tts.obuftail = tts.flood = 0;
  opt_queue_empty(1);
  tts_send(synth[tts.synth].flush, strlen(synth[tts.synth].flush));
  tts.oflag = 0;
  tts_flushed = 1;

/* XXX: note that this code hangs on the read on the Solaris platform ,
 *      so it's #ifdef'ed out for now. This might hang on other platforms
 *      to, in which case a better solution might be to remove it entirely.
 */

#ifndef sun
  if (tts.synth == TTS_DECTALK)
  {
    if (read(tts.fd, tmp, 1) == -1)
    {
      perror("tts_silence");
    }
  }
#endif /*!sun */
}


static void tts_obufpack()
{
  (void) memmove(tts.obuf, tts.obuf + tts.obufhead,
		 tts.obuftail - tts.obufhead);
  tts.obuftail -= tts.obufhead;
  tts.obufhead = 0;
}


#ifdef TTSLOG
static int ofd;
#endif


 /*ARGSUSED*/ static void tts_obufout(int x)
{
  int len, len2;
  int oldoflag;

  if (!tts.flood)
  {
    opt_queue_empty(2);
  }
  oldoflag = tts.oflag;
  sighit = 1;
  while (tts.obufhead < tts.obuftail)
  {
    len = strlen(tts.obuf + tts.obufhead);
    if (len > 1024)
    {
      len = 1024;
    }
    len2 = write(tts.fd, tts.obuf + tts.obufhead, len);
#ifdef TTSLOG
    (void) write(ofd, tts.obuf + tts.obufhead, len2);
#endif
    tts.obufhead += len2;
    if (len2 < len)
    {
      tts.oflag = oldoflag;
      (void) signal(SIGALRM, &tts_obufout);
      alarm(1);
      return;
    }
    while (tts.obufhead < tts.obuftail && !tts.obuf[tts.obufhead])
    {
      tts.obufhead++;
    }
  }
  tts.flood = 0;
  tts.oflag = oldoflag;
  (void) signal(SIGALRM, &tts_obufout);
}


static void tts_addbuf(char *buf, int len, int len2)
{
  char *ptr;

  tts.flood = 1;
  if (len2 == -1)
  {
    ptr = buf;
  } else
  {
    ptr = buf + len2;
    len -= len2;
  }
  if (tts.obuftail + len > tts.obuflen)
  {
    if ((tts.obuftail - tts.obufhead) + len > tts.obuflen)
    {
      tts.obuflen += 1024 + len - (len % 1024);
      tts.obuf = realloc(tts.obuf, tts.obuflen);
    } else
    {
      tts_obufpack();
    }
  }
  (void) memcpy(tts.obuf + tts.obuftail, ptr, len);
  tts.obuftail += len;
  tts.obuf[tts.obuftail++] = 0;
  (void) alarm(1);
}


void tts_send(char *buf, int len)
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
    len2 = write(tts.fd, buf, len);

#ifdef TTSLOG
    (void) write(ofd, buf, len2);
#endif

    if (len2 < len)
    {
      tts_addbuf(buf, len, len2);
    }
  } else
    tts_addbuf(buf, len, 0);
#endif

  tts.oflag = 1;
}


static int unspeakable(unsigned char ch)
{
  char *p = synth[tts.synth].unspeakable;

  if (ch < 32)
  {
    return (1);
  }
  while (*p)
  {
    if (*p++ == ch)
    {
      return (1);
    }
  }

  return (0);
}


void tts_end()
{
  tts_send(synth[tts.synth].end, strlen(synth[tts.synth].end));
}


void tts_out(unsigned char *buf, int len)
{
  char obuf[1024];
  char *p;
  int obp = 0;
  int i;

  if (!len)
    return;
  opt_queue_empty(0);
  p = synth[tts.synth].say;
  opt_queue_empty(0);
  while (*p)
  {
    if (*p == '%')
    {
      p++;
      for (i = 0; i < len; i++)
      {
	if (unspeakable((unsigned char)buf[i]))
	{
	  if (obp && obuf[obp - 1] != 32)
	  {
	    obuf[obp++] = ' ';
	  }
	  if (dict[buf[i]])
	  {
	    (void) strcpy(obuf + obp, dict[buf[i]]);
	    obp = strlen(obuf);
	  }
	  obuf[obp++] = 32;
	} else
	{
	  obuf[obp++] = buf[i];
	}
      }
    } else
    {
      obuf[obp++] = *p;
    }
    p++;
  }
  tts_send(obuf, obp);
}


void tts_say(char *buf)
{
  tts_out((unsigned char *) buf, strlen(buf));
}


void tts_say_printf(char *fmt, ...)
{
  va_list arg;
  char buf[200];

  va_start(arg, fmt);
  (void) vsnprintf(buf, sizeof(buf), fmt, arg);
  tts_say(buf);
  va_end(arg);
}


void tts_saychar(unsigned char ch)
{
  int i, j = 0;
  int stack[10];

  if (!ch)
  {
    ch = 32;
  }
  if (unspeakable(ch) && dict[ch])
  {
    tts_say(dict[ch]);
    return;
  }
  if (!synth[tts.synth].charoff)
  {				/* assume on string does everything */
    (void) sprintf(ttsbuf, synth[tts.synth].charon, ch);
    tts_send(ttsbuf, strlen(ttsbuf));
    return;
  }

  for (i = 0; i < NUMOPTS; i++)
  {
    if ((opt[i].type & 0x3f) != OT_TREE && !opt[i].ptr && opt[i].synth == tts.synth)
    {
      stack[j++] = i;
      stack[j++] = opt_getval(i, 0);
      opt_set(i, &opt[i].v.enum_max);
    }
  }
  ttsbuf[1] = 0;
  opt_queue_empty(3);
  ttsbuf[0] = ch;
  tts_send(ttsbuf, 1);
  if (synth[tts.synth].saychar_needs_flush)
  {
    tts_send(synth[tts.synth].say + 2, strlen(synth[tts.synth].say) - 2);
  }
  while (j)
  {
    j -= 2;
    opt_set(stack[j], stack + j + 1);
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


void tts_sayphonetic(unsigned char ch)
{
  if (!isalpha(ch))
  {
    tts_saychar(ch);
    return;
  }
  if (isupper(ch))
  {
    tts_say("cap");
  } else
  {
    ch = toupper(ch);
  }
  tts_say(ph_alph[--ch & 0x3f]);
}

static int open_tcp(char *port)
{
  char host[1024];
  int portnum;
  int fd;
  struct sockaddr_in servaddr;
  char *p;
  int len;

  p = strstr(port, ":");
  portnum = atoi(p + 1);
  len = p - port;
  if (len > 1023)
  {
    fprintf(stderr, "synthesizer port too long\n");
    exit(1);
  }
  memcpy(host, port, len);
  host[len] = '\0';
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("socket");
    exit(1);
  }
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = portnum;

  if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0)
  {
    perror("inet_pton");
    exit(1);
  }

  if (connect(fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
  {
    perror("connect");
    exit(1);
  }
  return fd;
}

int tts_init()
{
  struct termios t;
  char *arg[8];
  int i;
  char *portname;
  int mode = O_RDWR | O_NONBLOCK;
  char buf[100];

  portname = getfn(tts.port);
  tts.pid = 0;
  (void) memset(dict, 0, sizeof(dict));
  (void) signal(SIGALRM, &tts_obufout);

#ifdef TTSLOG
  ofd = open("tts.log", O_WRONLY | O_CREAT);
  if (ofd == -1)
  {
    perror("open");
  }
#endif
  if (tts.port[0] != '|' && strstr(tts.port, ":"))
  {
    tts.fd = open_tcp(tts.port);
  } else if (tts.port[0] != '|')
  {
    if (tts.synth == TTS_DECTALK)
    {
      mode = O_NOCTTY | O_RDWR;
    } else if (tts.synth == TTS_EMACSPEAK_SERVER)
    {
      mode = O_WRONLY;
    }
    tts.fd = open(portname, mode);
    if (tts.fd == -1)
    {
      perror("tts");
      exit(1);
    }
    (void) tcgetattr(tts.fd, &t);
    t.c_cflag |= CRTSCTS | CLOCAL;
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 10;
    cfsetospeed(&t, B9600);
    (void) tcsetattr(tts.fd, 0, &t);
  } else
  {				/* a pipe */
    (void) strcpy(buf, tts.port + 1);
    arg[i = 0] = strtok(buf, " ");
    while (i < 7)
    {
      if (!(arg[++i] = strtok(NULL, " ")))
      {
	break;
      }
    }
    if (!(tts.pid = forkpty(&tts.fd, NULL, NULL, NULL)))
    {
      (void) execvp(arg[0], arg);
      perror("execpv");
    }
    (void) usleep(10000);
    if (tts.pid == -1)
    {
      perror("forkpty");
    }
  }
  tts_send(synth[tts.synth].init, strlen(synth[tts.synth].init));

  return (0);
}


void tts_addchr(char ch)
{
  tts.buf[tts.outlen++] = ch;
  if (tts.outlen > 250)
  {
    tts_flush();
  }
}


 /*ARGSUSED*/ void tts_initsynth(int *argp)
{
  int i, v;

  for (i = 0; i < NUMOPTS; i++)
  {
    if ((opt[i].type & 0x40) &&
	opt[i].synth == tts.synth && (opt[i].type & 0x3f) != 0x03)
    {
      v = opt_getval(i, 0);
      opt_set(i, &v);
    }
  }
  if (!ui.silent)
  {
    tts_say("Synthesizer reinitialized.");
  }
}


void tts_charon()
{
  ttssend(synth[tts.synth].charon);
}


void tts_charoff()
{
  ttssend(synth[tts.synth].charoff);
}


#if DEBUG
void tts_printf(char *str, ...)
{
  char buf[200];
  va_list args;

  va_start(args, str);
  vsprintf(buf, str, args);
  tts_say(buf);
}
#endif
