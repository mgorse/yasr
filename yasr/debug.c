
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
#include <string.h>
#include <stdarg.h>

#define DEBUG     1 
#define DEBUGFILE "/tmp/yasr.debug"

#ifdef DEBUG
static FILE *debugfp;
#endif /*DEBUG*/


void
open_debug()
{
#ifdef DEBUG
    char filename[1024];

    (void) sprintf(filename, "%s%1d", DEBUGFILE, getpid());
    if ((debugfp = fopen(filename, "w+")) == NULL) {
        fprintf(stderr, "Couldn't open debug file %s\n", DEBUGFILE);
    }
#endif /*DEBUG*/
}


void
debug(char *format, ...)
{
#ifdef DEBUG
    va_list argp;
    va_start(argp, format);

    vfprintf(debugfp, format, argp);
    (void) fflush(debugfp);

    va_end(argp);
#endif /*DEBUG*/
}


void
close_debug()
{
#ifdef DEBUG
    (void) fclose(debugfp);
#endif /*DEBUG*/
}
