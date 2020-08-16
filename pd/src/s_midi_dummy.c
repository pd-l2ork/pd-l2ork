/* Copyright (c) 1997-2003 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.

   this file is a dummy for systems without any MIDI support

*/

#ifdef __EMSCRIPTEN__
#include <stdio.h>
#include <stdlib.h>
#include "m_pd.h"
#include "s_stuff.h"

static int web_nmidiin;
static int web_midiinfd[MAXMIDIINDEV];
static int web_nmidiout;
static int web_midioutfd[MAXMIDIOUTDEV];
void __Pd_openMidi(int nmidiin, int *midiinvec, int nmidiout, int *midioutvec);
const char *__Pd_getMidiInDeviceName(int devno);
const char *__Pd_getMidiOutDeviceName(int devno);
#endif

void sys_do_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec)
{
#ifdef __EMSCRIPTEN__
    if (nmidiin > MAXMIDIINDEV)
        nmidiin = MAXMIDIINDEV;
    for (int i = 0; i < nmidiin; i++)
        web_midiinfd[i] = midiinvec[i];
    web_nmidiin = nmidiin;

    if (nmidiout > MAXMIDIOUTDEV)
        nmidiout = MAXMIDIOUTDEV;
    for (int i = 0; i < nmidiout; i++)
        web_midioutfd[i] = midioutvec[i];
    web_nmidiout = nmidiout;

    __Pd_openMidi(nmidiin, midiinvec, nmidiout, midioutvec);
#endif
}

void sys_close_midi( void)
{
}

void sys_putmidimess(int portno, int a, int b, int c)
{
}

void sys_putmidibyte(int portno, int byte)
{
}

void sys_poll_midi(void)
{
}

void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
#ifdef __EMSCRIPTEN__
    int i, ndev;
    if ((ndev = web_nmidiin) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++) {
        int devno = web_midiinfd[i];
        const char *name = __Pd_getMidiInDeviceName(devno);
        sprintf(indevlist + i * devdescsize, name);
        free(name);
    }
    *nindevs = ndev;

    if ((ndev = web_nmidiout) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++) {
        int devno = web_midioutfd[i];
        const char *name = __Pd_getMidiOutDeviceName(devno);
        sprintf(outdevlist + i * devdescsize, name);
        free(name);
    }
    *noutdevs = ndev;
#endif
}
