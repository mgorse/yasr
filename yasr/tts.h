
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

/* tts.h -- defines Tts_synth */

#define TTS_SPEAKOUT         0
#define TTS_DECTALK          1
#define TTS_EMACSPEAK_SERVER 2
#define TTS_DOUBLETALK       3
#define TTS_BNS              4
#define TTS_APOLLO           5

typedef struct Tts_synth Tts_synth;

struct Tts_synth
{
    char *init;
    char *say;
    char *flush;
    char *charon;
    char *charoff;
    int saychar_needs_flush;
    char *unspeakable;
    char *end;
};
