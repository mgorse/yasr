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

/* getfn.c -- get an actual port name from the value of "synthesizer port"
 *
 * This function simply returns the inputted value if it is not passed one
 * of the special sequences it recognizes (ie, Sn or Ln for serial and parallel
 * ports, whos names are very system-dependent).
 */

#include "yasr.h"

static char newname[40];

static char *serial_name(int port)
{
#if defined(__linux__)
  int fd;

  /* Try devfs name first */
  (void) sprintf(newname, "/dev/tts/%d", port);
  if ((fd = open(newname, O_RDONLY)) > 0)
  {
    close(fd);
    return (newname);
  }
  (void) sprintf(newname, "/dev/ttyS%d", port);
#elif defined(__OpenBSD__)
  (void) sprintf(newname, "/dev/tty0%d", port);
#elif defined(__FreeBSD__)
  (void) sprintf(newname, "/dev/cuaa%d", port);	/* tbd - is this right? */
#elif defined(__NetBSD__)
  (void) sprintf(newname, "/dev/tty0%d", port);
#elif defined(sun)
  (void) sprintf(newname, "/dev/cua/%c", (char) port + 65);	/*tbd- is this right? */
#else
  return (NULL);		/* Unsupported os. */
#endif
  return (newname);
}


 /*ARGSUSED*/ static char *parallel_name(int port)
{
#if defined(__linux__)
  int fd;

  /* Try devfs name first */
  (void) sprintf(newname, "/dev/printers/%d", port);
  if ((fd = open(newname, O_RDONLY)) > 0)
  {
    close(fd);
    return (newname);
  }
  (void) sprintf(newname, "/dev/lp%d", port);
  return (newname);
#elif defined(__OpenBSD__)
  (void) sprintf(newname, "/dev/lpt%d", port);
  return (newname);
#else
  return (NULL);		/* Unsupported os */
#endif
}


char *getfn(char *name)
{
  if (!isdigit((int) name[1]) || name[2])
  {
    return name;
  }
  switch (name[0])
  {
  case 'S':
  case 's':
    return (serial_name(name[1] - 48));

  case 'L':
  case 'l':
    return (parallel_name(name[1] - 48));

  default:
    return (name);
  }
}
