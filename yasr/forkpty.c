
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

/* forkpty.c - replacement for forkpty */

#ifndef HAVE_FORKPTY

#include "yasr.h"
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

int 
forkpty(int *master, char *name, struct termios *term, struct winsize *winsz)
{
    int slave;
    int pid;

    if (openpty(master, &slave, name, term, winsz) == -1) {
        return(-1);
    }
    if ((pid = fork())) {
        return(pid);
    }

    /* child -- set up tty */
    (void) dup2(slave, 0);
    (void) dup2(slave, 1);
    (void) dup2(slave, 2);

    return(pid);
}

#endif /*HAVE_FORKPTY*/
