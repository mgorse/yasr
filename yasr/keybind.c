
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

int 
kb_add(Keymap *map, int k, int i, int na, int *a, int flag)
{
    int v = 0;
    Keybind *kp, *tmpk;

    if ((v = kb_search(map, k)) == -1) {
        v = map->numkeys;
    }
    kp = map->kb + v;
    if (v < map->numkeys) {
        switch (flag) {
            case 0: 
                return(-1);  /* error */

            case 1:          /* add at the beginning */
                tmpk = kp->next; 
                kp->next = malloc(sizeof(Keybind));
                (void) memcpy(kp->next, kp, sizeof(Keybind)); 
                kp->next->next = tmpk;
                kp->index = i;
                kp->argp = a;
                kp->numargs = na;
                return(0);

            case 2:          /* add after */
                tmpk = kp;
                while (tmpk->next) {
                    tmpk = tmpk->next;
                }
                tmpk->next = malloc(sizeof(Keybind));
                tmpk = tmpk->next;
                tmpk->key = k; 
                tmpk->index = i; 
                tmpk->argp = a; 
                tmpk->numargs = na;
                return(0);
        }
    }
    if (++(map->numkeys) % 32 == 1) {
        map->kb = realloc(map->kb, (map->numkeys + 31) * sizeof(Keybind));
        kp = map->kb + v;
    }
    kp->key = k; 
    kp->index = i; 
    kp->argp = a; 
    kp->numargs = na;

    return(0);
}


int 
kb_search(Keymap *map, int k)
{
    int v;

    for (v = 0; v < map->numkeys; v++)  {
        if (map->kb[v].key == k) {
            return v;
        }
    }

    return(-1);
}


#ifdef USE_KBWIZ
void 
kb_del(Keymap *map, int key)
{
    int v;
    Keybind *kp, *tmpk;

    v = kb_search(map, key);
    if (v == -1) {
        return;
    }
    kp = map->kb + v;
    tmpk = kp;
    while ((tmpk = tmpk->next) != NULL) {
        free(tmpk);
    }
    (void) memmove(kp, kp + 1, (map->numkeys - v - 1) * sizeof(Keybind));
}
#endif /*USE_KBWIZ*/


/*ARGSUSED*/
int 
kbwiz(int key)
{
#ifdef USE_KBWIZ
    static int state = 0;
    static Keymap *map;
    static int sourcekey;
    Keybind *kb;

    if (!key) {     /* initialize */
        if (ui.revmode) {
            map = &rev.keymap;
            tts_say("Editing review keymap. Enter command.");
        } else {
            map = &ui.keymap;
            tts_say("Editing normal keymap. Enter command.");
        }
        return(1);
    }

    switch (state) {
        case 0:
            switch (key) {
                case 'c': 
                case 'C': 
                    state = 1; 
                    tts_say("Copy which key?"); 
                    break;

                case 'd': 
                case 'D': 
                    state = 3; 
                    tts_say("Delete which key?"); 
                    break;

                case 'm': 
                case 'M': 
                    state = 5; 
                    tts_say("Move which key?"); 
                    break;

                case 27: 
                    tts_say("exit keyboard wizard."); 
                    ui_funcman(0); 
                    break;

                default: 
                    tts_say("c to copy, d to delete, m to move."); 
                    break;
            }
            break;

        case 1: 
        case 3: 
        case 5:
            if (key == 27) { 
                state = 0; 
                tts_say("Command aborted."); 
                break;
            }
            if (kb_search(map, key) == -1) { 
                tts_say("Key not defined."); 
                break;
            }
            if (state == 3) {
                kb_del(map, key);
                state = 0;
                tts_say("Key deleted.");
            } else { 
                state++; 
                tts_say("To which key?"); 
                sourcekey = key;
            }
            break;

        case 2: 
        case 6:
            if (kb_search(map, key) != -1) { 
                tts_say("Keystroke already defined. Aborting."); 
                state = 0; 
                break; 
            }
            kb = map->kb + kb_search(map, sourcekey);
            (void) kb_add(map, key, kb->index, kb->numargs, kb->argp, 0);
            if (state == 6) {
                kb_del(map, sourcekey);
                tts_say("key moved.");
            } else {
                tts_say("Key copied.");
            }
            state = 0;
            break;
    }

    return(1);
}
#else
    tts_say("Not implemented");
    ui_funcman(0);
    return(1);
}
#endif
