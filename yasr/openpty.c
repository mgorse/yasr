
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

/* openpty.c - replacement for openpty() */

#ifndef HAVE_OPENPTY

#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>


static int 
getpty(int *master, int *slave, char *name)
{
    int i, j;
    char ptyname[11], ttyname[11];
    char l1[] = "abcdefghijklmnopqrstuvwxyz";
    char l2[] = "0123456789abcdef";

    (void) strcpy(ttyname, "/dev/ttyxx");
    (void) strcpy(ptyname, "/dev/ptyxx");
    for (i = 0; i < 26; i++) {
        ptyname[8] = ttyname[8] = l1[i];
        for (j = 0; j < 16; j++) {
            ptyname[9] = ttyname[9] = l2[j];
            if ((*master = open(ptyname, O_RDWR)) > 0 &&
                (*slave = open(ttyname, O_RDWR)) > 0) {
                if (name != NULL && name[0] != 0) {
                    (void) strcpy(name, ttyname);
                }
                return(0);
            }
        }
    }

    return(-1);    /* Out of ttys? */
}


int 
openpty(int *master, int *slave, char *name, 
        struct termios *term, struct winsize *winsz)
{
    if (getpty(master, slave, name) == -1) {
        return(-1);
    }
    if (term) {
        (void) tcsetattr(*slave, TCSANOW, term);
    }
    if (winsz) {
        (void) ioctl(*slave, TIOCSWINSZ, winsz);
    }

    return(0);
}

#endif /*HAVE_OPENPTY*/
