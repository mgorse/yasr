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
 * Web Page: http://yasr.sf.net
 *
 * This software is maintained by:
 * Michael P. Gorse <mgorse@users.sourceforge.net>
 */

#include "yasr.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
  {"", "%s\r\r", "\030\030\r\r", NULL, NULL, TRUE, NULL, NULL},	/* BNS */
  {"", "%s\r\n", "\030", "@P1", "@P0", TRUE, "@", ""},	/* Apollo */
  {"(audio_mode 'async)\n",
   "(SayText \"%s\")\n",
   "(audio_mode 'shutup)\n",
   "(SayText '%c)\n",
   NULL,
   TRUE,
   "",
   "\003\")\n"}, /* festival */
  {"", "%s\r\n", "\033", "%c\r", NULL, TRUE, "@", ""},	/* Ciber232 */
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

  if (tts.synth == TTS_DECTALK)
  {
    do
    {
      if (!readable(tts.fd, 50000)) break;
      if (read(tts.fd, tmp, 1) == -1)
      {
	perror("tts_silence");
	break;
      }
    } while (tmp[0] != 1 && tmp[0] != 0);
    /* Following is a hack; without it, [:sa c] could have been sent and
       lost by the DEC-talk if a silence happens immediately after it */
    tts_send("[:sa c]", 7);
    tts.oflag = 0;	/* pretend we didn't just do that */
  }
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

  if (ch < 32) return 1;
  while (*p)
  {
    if (*p++ == ch) return 1;
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
  int obo = 0;	/* current offset into obuf */
  int i;
  int xml = 0;

  if (!len) return;
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
	  if (obo > 0 && obuf[obo - 1] != ' ')
	  {
	    obuf[obo++] = ' ';
	    tts_send(obuf, obo);
	    obo = 0;
	  }
	  if (dict[buf[i]])
	  {
	    (void) strcpy(obuf + obo, dict[buf[i]]);
	    obo = strlen(obuf);
	  }
	  obuf[obo++] = 32;
	}
	else if (xml)
	{
	  switch (buf[i])
	  {
	  /* For some reason, &lt; does not work for me with Voxeo */
	  case '<': strcpy(obuf + obo, " less than "); obo += 11; break;
	  case '>': strcpy(obuf + obo, "&gt;"); obo += 4; break;
	  case '&': strcpy(obuf + obo, "&amp;"); obo += 5; break;
	  default: obuf[obo++] = buf[i]; break;
	  }
	}
	else
	{
	  obuf[obo++] = buf[i];
	}
	if (obo > (sizeof(obuf) / sizeof(obuf[0])) - 6)
	{
	  tts_send(obuf, obo);
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
  tts_send(obuf, obo);
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
    tts_say(_("cap"));
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
  servaddr.sin_port = htons(portnum);

#ifdef HAVE_INET_PTON
  if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0)
  {
    perror("inet_pton");
    exit(1);
  }
#else
  servaddr.sin_addr.s_addr = inet_addr(host);
#endif

  if (connect(fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
  {
    perror("connect");
    exit(1);
  }
  return fd;
}

int tts_init( int first_call)
{
  struct termios t;
  char *arg[8];
  int i;
  char *portname;
  int mode = O_RDWR | O_NONBLOCK;
  char buf[100];

  portname = getfn(tts.port);
  tts.pid = 0;
  tts.reinit = !first_call; 

  (void) memset(dict, 0, sizeof(dict));
  (void) signal(SIGALRM, &tts_obufout);

#ifdef TTSLOG
  ofd = open("tts.log", O_WRONLY | O_CREAT);
  if (ofd == -1)
  {
    perror("open");
  }
#endif
  if (first_call && tts.port[0] != '|' && strstr(tts.port, ":"))
  {
    tts.fd = open_tcp(tts.port);
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
      tts.fd = open(portname, mode);
    }
    if (tts.fd == -1)
    {
      perror("tts");
      exit(1);
    }
    (void) tcgetattr(tts.fd, &t);
    if (tts.synth == TTS_DECTALK)
    {
      /* When sending a ^C, make sure we send a ^C and not a break */
      cfmakeraw(&t);
      /* When the DEC-talk powers on, it sends several ^A's.  Discard them */
      tcflush(tts.fd, TCIFLUSH);
    }
    t.c_cflag |= CRTSCTS | CLOCAL;
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 10;
    cfsetospeed(&t, B9600);
    (void) tcsetattr(tts.fd, 0, &t);
  }
  else
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

    if (first_call)
    {
      if (openpty(&tts.fd, &tts.fd_slave, NULL, NULL, NULL) == -1)
      {
	perror("openpty");
	exit(1);
      }
    } 

    if (!(tts.pid = fork()))
    {
      /* child -- set up tty */
      (void) dup2(tts.fd_slave, 0);
      (void) dup2(tts.fd_slave, 1);
      (void) dup2(tts.fd_slave, 2);
      /* tts.fd_slave is not closed */
    }
    if (!tts.pid)
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

  /* init is now finished */
  tts.reinit = 0;
  
  return (0);
}

int tts_reinit2()
{
  tts_init(0);
  tts_initsynth(NULL);
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
    tts_say(_("Synthesizer reinitialized."));
  }
}



 /*ARGSUSED*/ void tts_reinit(int *argp)
{
  int pid=tts.pid;

  if (pid==0)
  {
      return;
  }
  
  tts.reinit=1; /* Start reinit */

  tts_silence();
  usleep(500000);

  if (kill (pid, SIGTERM)!=0)
  {
    if (errno==ESRCH)
    {
      tts_reinit2();
    }
    else
    {
      kill (pid, SIGKILL);
    }
  }

  /* wait init completion (tts.fd must be available) */
  while(tts.reinit)
  {
      usleep(100000);
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
