
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

#include <stdio.h>
#include <unistd.h>
#include "yasr.h"
#include <string.h>
#include <fcntl.h>
#include <termios.h>

int
main(int argc, char *argv[])
{
    struct termios t, rt;
    char buf[5];
    int len;
    int val;

    /* Set raw mode */
    (void) tcgetattr(0, &t);
    (void) memcpy(&rt, &t, sizeof(struct termios));
    cfmakeraw(&rt);
    (void) tcsetattr(0, TCSAFLUSH, &rt);

    (void) printf("%s now running. Q quits.\n", argv[0]);

    /* Enter loop */
    while ((len = read(0, buf, 5))) {
        val = buf[0];
        if (len > 1) {
            val = (val << 8) + buf[1];
        }
        if (len > 2) {
            val = (val << 8) + buf[2];
        }
        if (val == 81 || val == 113) {
            break;
        }
        (void) printf("0x%x\n", val);
    }

    /* Reset terminal */
    (void) tcsetattr(0, TCSAFLUSH, &t);
    (void) printf("Exiting.\n");

    return(0);
}
