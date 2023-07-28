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

#include "js.hpp"
#include <emscripten.h>

// EM_JS functions are exposed to the global scope in javascript
// we use a double underscore prefix to prevent potential namespace collision
EM_JS(void, __mainInit, (void), {
    if (typeof Module.mainInit === "function") {
        Module.mainInit();
    }
    else {
        console.error("couldn't find the javascript function 'Module.mainInit'");
    }
});

EM_JS(void, __mainLoop, (void), {
    if (typeof Module.mainLoop === "function") {
        Module.mainLoop();
    }
    else {
        console.error("couldn't find the javascript function 'Module.mainLoop'");
    }
});

EM_JS(void, __mainExit, (void), {
    if (typeof Module.mainExit === "function") {
        Module.mainExit();
    }
    else {
        console.error("couldn't find the javascript function 'Module.mainExit'");
    }
});

EM_JS(void, __Pd_reinit, (int newinchan, int newoutchan, int newrate), {
    if (typeof Module.Pd.reinit === "function") {
        Module.Pd.reinit(newinchan, newoutchan, newrate);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.reinit'");
    }
});

EM_JS(void, __Pd_openMidi, (int nmidiin, int *midiinvec,
                            int nmidiout, int *midioutvec), {
    if (typeof Module.Pd.openMidi === "function") {
        var midiinarr = [];
        for (let i = 0; i < nmidiin; i++) {
            midiinarr.push(HEAP32[(midiinvec >> 2) + i]);
        }
        var midioutarr = [];
        for (let i = 0; i < nmidiout; i++) {
            midioutarr.push(HEAP32[(midioutvec >> 2) + i]);
        }
        Module.Pd.openMidi(midiinarr, midioutarr);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.openMidi'");
    }
});

EM_JS(const char *, __Pd_getMidiInDeviceName, (int devno), {
    if (typeof Module.Pd.getMidiInDeviceName === "function") {
        return Module.Pd.getMidiInDeviceName(devno);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.getMidiInDeviceName'");
    }
});

EM_JS(const char *, __Pd_getMidiOutDeviceName, (int devno), {
    if (typeof Module.Pd.getMidiOutDeviceName === "function") {
        return Module.Pd.getMidiOutDeviceName(devno);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.getMidiOutDeviceName'");
    }
});

EM_JS(int, __Pd_loadLib, (const char *filename, const char *symname), {
    return Asyncify.handleAsync(async () => {
        try {
            await loadDynamicLibrary(UTF8ToString(filename), {loadAsync: true, global: true, nodelete: true, fs: FS});
            var makeout = Module['_' + UTF8ToString(symname)];
            if (typeof makeout === "function") {
                makeout();
                return 1; // success
            }
            else {
                return -1; // couldn't find the function
            }
        }
        catch (error) {
            console.error(error);
            return 0; // couldn't load the external
        }
    });
});

EM_JS(void, __Pd_receiveCommandBuffer, (char *buf), {
    if (typeof Module.Pd.receiveCommandBuffer === "function") {
        Module.Pd.receiveCommandBuffer(intArrayFromString(UTF8ToString(buf), true));
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveCommandBuffer'");
    }
});

EM_JS(void, __Pd_receivePrint, (const char *s), {
    if (typeof Module.Pd.receivePrint === "function") {
        Module.Pd.receivePrint(UTF8ToString(s));
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receivePrint'");
    }
});

EM_JS(void, __Pd_receiveBang, (const char *source), {
    if (typeof Module.Pd.receiveBang === "function") {
        Module.Pd.receiveBang(UTF8ToString(source));
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveBang'");
    }
});

EM_JS(void, __Pd_receiveFloat, (const char *source, float value), {
    if (typeof Module.Pd.receiveFloat === "function") {
        Module.Pd.receiveFloat(UTF8ToString(source), value);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveFloat'");
    }
});

EM_JS(void, __Pd_receiveSymbol, (const char *source, const char *symbol), {
    if (typeof Module.Pd.receiveSymbol === "function") {
        Module.Pd.receiveSymbol(UTF8ToString(source), UTF8ToString(symbol));
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveSymbol'");
    }
});

EM_JS(void, __Pd_receiveList, (const char *source, const char *list), {
    if (typeof Module.Pd.receiveList === "function") {
        var str = UTF8ToString(list);
        var arr = str.split(",").map(item => isNaN(+item) ? item : +item);
        Module.Pd.receiveList(UTF8ToString(source), arr);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveList'");
    }
});

EM_JS(void, __Pd_receiveMessage, (const char *source, const char *symbol,
                                  const char *list), {
    if (typeof Module.Pd.receiveMessage === "function") {
        var str = UTF8ToString(list);
        var arr = str.split(",").map(item => isNaN(+item) ? item : +item);
        Module.Pd.receiveMessage(UTF8ToString(source), UTF8ToString(symbol), arr);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveMessage'");
    }
});

EM_JS(void, __Pd_receiveNoteOn, (int channel, int pitch, int velocity), {
    if (typeof Module.Pd.receiveNoteOn === "function") {
        Module.Pd.receiveNoteOn(channel, pitch, velocity);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveNoteOn'");
    }
});

EM_JS(void, __Pd_receiveControlChange, (int channel, int controller, int value), {
    if (typeof Module.Pd.receiveControlChange === "function") {
        Module.Pd.receiveControlChange(channel, controller, value);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveControlChange'");
    }
});

EM_JS(void, __Pd_receiveProgramChange, (int channel, int value), {
    if (typeof Module.Pd.receiveProgramChange === "function") {
        Module.Pd.receiveProgramChange(channel, value);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveProgramChange'");
    }
});

EM_JS(void, __Pd_receivePitchBend, (int channel, int value), {
    if (typeof Module.Pd.receivePitchBend === "function") {
        Module.Pd.receivePitchBend(channel, value);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receivePitchBend'");
    }
});

EM_JS(void, __Pd_receiveAftertouch, (int channel, int value), {
    if (typeof Module.Pd.receiveAftertouch === "function") {
        Module.Pd.receiveAftertouch(channel, value);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveAftertouch'");
    }
});

EM_JS(void, __Pd_receivePolyAftertouch, (int channel, int pitch, int value), {
    if (typeof Module.Pd.receivePolyAftertouch === "function") {
        Module.Pd.receivePolyAftertouch(channel, pitch, value);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receivePolyAftertouch'");
    }
});

EM_JS(void, __Pd_receiveMidiByte, (int port, int byte), {
    if (typeof Module.Pd.receiveMidiByte === "function") {
        Module.Pd.receiveMidiByte(port, byte);
    }
    else {
        console.error("couldn't find the javascript function 'Module.Pd.receiveMidiByte'");
    }
});
