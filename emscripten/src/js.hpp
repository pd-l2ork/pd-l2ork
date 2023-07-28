/*
* Copyright (c) 2020 Zack Lee (cuinjune@gmail.com)
* Copyright (c) 2020 Purr Data team
*
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.
*
* See https://git.purrdata.net/jwilkes/purr-data for documentation
*
*/

#ifdef __cplusplus
extern "C" {
#endif

extern void __mainInit(void);
extern void __mainLoop(void);
extern void __mainExit(void);
extern void __Pd_reinit(int newinchan, int newoutchan, int newrate);
extern void __Pd_openMidi(int nmidiin, int *midiinvec,
                          int nmidiout, int *midioutvec);
extern const char *__Pd_getMidiInDeviceName(int devno);
extern const char *__Pd_getMidiOutDeviceName(int devno);
extern int __Pd_loadLib(const char *filename, const char *symname);
extern void __Pd_receiveCommandBuffer(char *buf);
extern void __Pd_receivePrint(const char *s);
extern void __Pd_receiveBang(const char *source);
extern void __Pd_receiveFloat(const char *source, float value);
extern void __Pd_receiveSymbol(const char *source, const char *symbol);
extern void __Pd_receiveList(const char *source, const char *list);
extern void __Pd_receiveMessage(const char *source, const char *symbol,
                                const char *list);
extern void __Pd_receiveNoteOn(int channel, int pitch, int velocity);
extern void __Pd_receiveControlChange(int channel, int controller,
                                        int value);
extern void __Pd_receiveProgramChange(int channel, int value);
extern void __Pd_receivePitchBend(int channel, int value);
extern void __Pd_receiveAftertouch(int channel, int value);
extern void __Pd_receivePolyAftertouch(int channel, int pitch, int value);
extern void __Pd_receiveMidiByte(int port, int byte);

#ifdef __cplusplus
}
#endif
