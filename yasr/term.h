
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

/* term.h -- terminal header file for yasr */

#define CHARSIZE sizeof(chartype)

#define ATTR_BOLD 0x01
#define ATTR_HALFBRIGHT 0x02
#define ATTR_UNDERSCORE 0x04
#define ATTR_BLINK 0x08
#define ATTR_RV 0x10
