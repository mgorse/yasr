
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
 * Web Page: http://mgorse.dhs.org:8000/yasr/
 *
 * This software is maintained by:
 * Mike Gorse <mgorse@alum.wpi.edu>
 */

/* tts.h -- defines Tts_synth */

#define TTS_SPEAKOUT         0
#define TTS_DECTALK          1
#define TTS_EMACSPEAK_SERVER 2
#define TTS_DOUBLETALK       3
#define TTS_BNS              4
#define TTS_APOLLO           5
#define TTS_FESTIVAL	     6
#define TTS_Ciber232         7
#define TTS_SPEECHD          8
#define TTS_SYNTH_COUNT      9

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
