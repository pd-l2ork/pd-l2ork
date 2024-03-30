const isMobile = (typeof window.orientation !== "undefined") || (navigator.userAgent.indexOf('IEMobile') !== -1);
const isFirefox = navigator.userAgent.toLowerCase().includes('firefox');
const loading = document.getElementById("loading");
const filter = document.getElementById("filter");
let currentFile = "";
let subscribedData = {};
let searchPaths = ['/'];
let fileCache = {};

//Files that need to be short-circuited for cyclone
//If they are symlinked during the build process, emscripten bundler resolves the symlinks and 
//copies them into main.data multiple times, adding ~120MB to main.data and making the page take 
//a long time to load. Instead, the files are listed here, and are symlinked using the FS.symlink
//function in emscriptens internal filesystem. The real cyclone files are Hammer.wasm and Sickle.wasm.
//All of these files will be symlinked to those two files.
let hammerFiles = ['Append.wasm','bangbang.wasm','match.wasm','spell.wasm','Borax.wasm','bondo.wasm','maximum.wasm','split.wasm','Bucket.wasm','buddy.wasm','mean.wasm','spray.wasm','Clip.wasm','capture.wasm','midiflush.wasm','sprintf.wasm','Decode.wasm','cartopol.wasm','midiformat.wasm','substitute.wasm','Histo.wasm','coll.wasm','midiparse.wasm','sustain.wasm','comment.wasm','minimum.wasm','switch.wasm','cosh.wasm','mousefilter.wasm','tanh.wasm','counter.wasm','mtr.wasm','testmess.wasm','cycle.wasm','next.wasm','thresh.wasm','MouseState.wasm','dbtoa.wasm','offer.wasm','tosymbol.wasm','Peak.wasm','decide.wasm','onebang.wasm','universal.wasm','Table.wasm','drunk.wasm','past.wasm','urn.wasm','TogEdge.wasm','flush.wasm','xbendin.wasm','Trough.wasm','forward.wasm','poltocar.wasm','xbendin2.wasm','Uzi.wasm','fromsymbol.wasm','pong.wasm','xbendout.wasm','accum.wasm','funbuff.wasm','prepend.wasm','xbendout2.wasm','acos.wasm','funnel.wasm','prob.wasm','xnotein.wasm','active.wasm','gate.wasm','pv.wasm','xnoteout.wasm','allhammers.wasm','grab.wasm','round.wasm','zl.wasm','anal.wasm','hammer.wasm','seq.wasm','asin.wasm','iter.wasm','sinh.wasm','loadmess.wasm','speedlim.wasm'];
let sickleFiles = ['abs~.wasm', 'bitand~.wasm', 'cosx~.wasm', 'kink~.wasm', 'minmax~.wasm', 'rampsmooth~.wasm', 'Snapshot~.wasm', 'acos~.wasm', 'bitnot~.wasm', 'count~.wasm', 'Line~.wasm', 'mstosamps~.wasm', 'rand~.wasm', 'spike~.wasm', 'acosh~.wasm', 'bitor~.wasm', 'curve~.wasm', 'linedrive~.wasm', 'onepole~.wasm', 'record~.wasm', 'svf~.wasm', 'allpass~.wasm', 'bitshift~.wasm', 'log~.wasm', 'overdrive~.wasm', 'reson~.wasm', 'tanh~.wasm', 'allsickles~.wasm', 'bitxor~.wasm', 'cycle~.wasm', 'lookup~.wasm', 'peakamp~.wasm', 'round~.wasm', 'tanx~.wasm', 'asin~.wasm', 'buffir~.wasm', 'dbtoa~.wasm', 'lores~.wasm', 'peek~.wasm', 'sah~.wasm', 'train~.wasm', 'asinh~.wasm', 'capture~.wasm', 'delay~.wasm', 'phasewrap~.wasm', 'sampstoms~.wasm', 'trapezoid~.wasm', 'atan2~.wasm', 'cartopol~.wasm', 'delta~.wasm', 'pink~.wasm', 'Scope~.wasm', 'triangle~.wasm', 'atan~.wasm', 'change~.wasm', 'deltaclip~.wasm', 'play~.wasm', 'trunc~.wasm', 'atanh~.wasm', 'click~.wasm', 'edge~.wasm', 'poke~.wasm', 'sickle~.wasm', 'vectral~.wasm', 'atodb~.wasm', 'Clip~.wasm', 'frameaccum~.wasm', 'matrix~.wasm', 'poltocar~.wasm', 'sinh~.wasm', 'wave~.wasm', 'average~.wasm', 'comb~.wasm', 'framedelta~.wasm', 'maximum~.wasm', 'pong~.wasm', 'sinx~.wasm', 'zerox~.wasm', 'avg~.wasm', 'cosh~.wasm', 'index~.wasm', 'minimum~.wasm', 'pow~.wasm', 'slide~.wasm'];

//CONSTANTS IMPORTED FROM g_vumeter.c, lines 25-61
let vu_colors = [
    16579836, 10526880, 4210752, 16572640, 16572608,
    16579784, 14220504, 14220540, 14476540, 16308476,
    14737632, 8158332, 2105376, 16525352, 16559172,
    15263784, 1370132, 2684148, 3952892, 16003312,
    12369084, 6316128, 0, 9177096, 5779456,
    7874580, 2641940, 17488, 5256, 5767248
];
let vu_valmap =
[
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9,10,10,10,10,10,
    11,11,11,11,11,12,12,12,12,12,
    13,13,13,13,14,14,14,14,15,15,
    15,15,16,16,16,16,17,17,17,18,
    18,18,19,19,19,20,20,20,21,21,
    22,22,23,23,24,24,25,26,27,28,
    29,30,31,32,33,33,34,34,35,35,
    36,36,37,37,37,38,38,38,39,39,
    39,39,39,39,40,40
];
let vu_colmap =
[
    0,17,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    15,15,15,15,15,15,15,15,15,15,14,14,13,13,13,13,13,13,13,13,13,13,13,19,19,19
];
let vu_scale_str = [
    "<-99", "-50", "-30", "-20", "-12", "-6",
      "-2", "-0dB",  "+2",  "+6",">+12",  "",
];
let object_types = [
    '#X obj',
    '#N canvas',
    '#X text',
    '#X msg',
    '#X floatatom',
    '#X symbolatom',
    '#X dropdown'
];
let known_objects = [
    'adc~',
    'bng',
    'tgl',
    'vsl',
    'hsl',
    'vradio',
    'hradio',
    'flatgui/knob',
    'vu',
    'pddplink',
    'pddp/pddplink',
    'nbx',
    'cnv',
    'key',
    'keyup',
    'keyname',
    'legacy_mousemotion',
    'legacy_mouseclick'
  ]
//END CONSTANTS



//For Arrays/Graphs/Data Structures
//
//low - The coordinate on the low end of screenspace (top or left)
//high - The coordinate on the high end of screenspace (bottom or right)
//span - The distance in screenspace between low and high (width or height)
//coord/screen - the coordinate to convert (x or y)
function coordToScreen(low, high, span, coord) {
    let raw = (coord - Math.min(low, high)) * span / Math.abs(low - high);
    return Math.round(high > low ? raw : span - raw);
}
function screenToCoord(low, high, span, screen) {
    let raw = high > low ? screen : span - screen;
    return (raw) * Math.abs(low - high) / span + Math.min(low, high);   
}

// create an AudioContext
const audioContextList = [];
(function () {
    var AudioContext = self.AudioContext || self.webkitAudioContext || false;
    if (AudioContext) {
        self.AudioContext = new Proxy(AudioContext, {
            construct(target, args) {
                const result = new target(...args);
                audioContextList.push(result);
                return result;
            }
        });
    }
    else {
        alert("The Web Audio API is not supported in this browser.");
    }
})();

function resumeAudio() {
    audioContextList.forEach(ctx => {
        if (ctx.state !== "running") { ctx.resume(); }
    });
}

function suspendAudio() {
    audioContextList.forEach(ctx => {
        if (ctx.state !== "suspended") { ctx.suspend(); }
    });
}

// ["click", "contextmenu", "auxclick", "dblclick"
//     , "mousedown", "mouseup", "pointerup", "touchend"
//     , "keydown", "keyup"
// ].forEach(name => document.addEventListener(name, resumeAudio));

//--------------------- emscripten ----------------------------
var Module = {
    preRun: []
    , postRun: []
    , print: function (e) {
        1 < arguments.length && (e = Array.prototype.slice.call(arguments).join(" "));
        console.log(e);
    }
    , printErr: function (e) {
        1 < arguments.length && (e = Array.prototype.slice.call(arguments).join(" "));
        console.error(e)
    }
    , pd: {} // make pd object accessible from outside of the scope
    , mainInit: function () { // called after Module is ready
        Module.pd = new Module.Pd(); // instantiate Pd object
        if (typeof Module.pd != "object") {
            alert("Pd-l2Ork: failed to instantiate pd object");
            console.error("Pd-l2Ork: failed to instantiate pd object");
            Module.mainExit();
            return;
        }
        var pd = Module.pd;
        pd.setNoGui(true); // set to true if you don't use the pd's gui

        // create an AudioContext
        var isWebAudioSupported = false;
        var audioContextList = [];
        (function () {
            var AudioContext = self.AudioContext || self.webkitAudioContext || false;
            if (AudioContext) {
                isWebAudioSupported = true;
                self.AudioContext = new Proxy(AudioContext, {
                    construct(target, args) {
                        var result = new target(...args);
                        audioContextList.push(result);
                        return result;
                    }
                });
            }
        })();
        if (isWebAudioSupported) {
            console.log("Audio: successfully enabled");
        }
        else {
            alert("The Web Audio API is not supported in this browser.");
            console.error("Audio: failed to use the web audio");
            Module.mainExit();
            return;
        }

        // check if the web midi library exists and is supported
        var isWebMidiSupported = false;
        if (typeof WebMidi != "object") {
            alert("Midi: failed to find the 'WebMidi' object");
            console.error("Midi: failed to find the 'WebMidi' object");
            Module.mainExit();
            return;
        }

        // array of enabled midi device ids (without duplicates)
        var midiInIds = [];
        var midiOutIds = [];

        // 10 input, 10 output device numbers to send with "pd midi-dialog"
        // 0: no device, 1: first available device, 2: second available device...
        var midiarr = [];

        // enable midi
        WebMidi.enable(function (err) {
            if (err) {
                // if the browser doesn't support web midi, one can still use pd without it
                // alert("The Web MIDI API is not supported in this browser.\nPlease check: https://github.com/djipco/webmidi#browser-support");
                console.error("Midi: failed to enable midi", err);
            }
            else {
                isWebMidiSupported = true;
                console.log("Midi: successfully enabled");

                // select all available input/output devices as default
                midiInIds = [];
                midiOutIds = [];
                for (var i = 0; i < WebMidi.inputs.length; i++) {
                    midiInIds.push(WebMidi.inputs[i].id);
                }
                for (var i = 0; i < WebMidi.outputs.length; i++) {
                    midiOutIds.push(WebMidi.outputs[i].id);
                }
                midiarr = [];
                for (var i = 0; i < 10; i++) {
                    var devno = i < midiInIds.length ? i + 1 : 0;
                    midiarr.push(devno);
                }
                for (var i = 0; i < 10; i++) {
                    var devno = i < midiOutIds.length ? i + 1 : 0;
                    midiarr.push(devno);
                }
                // called whenever input/output devices connection status changes
                function onConnectionChanged() {
                    console.log("Midi: connection status changed");
                    pdsend("pd midi-dialog", midiarr.join(" ")); // send message to pd
                }
                // make sure we get only one callback at a time
                var timerId;
                WebMidi.addListener("connected", function (e) {
                    clearTimeout(timerId);
                    timerId = setTimeout(() => onConnectionChanged(), 100);
                });
                WebMidi.addListener("disconnected", function (e) {
                    clearTimeout(timerId);
                    timerId = setTimeout(() => onConnectionChanged(), 100);
                });
            }
        }, false); // not use sysex

        // reinit pd (called by "pd audio-dialog" message)
        Module.Pd.reinit = function (newinchan, newoutchan, newrate) {
            if (pd.init(newinchan, newoutchan, newrate, pd.getTicksPerBuffer())) {

                // print obtained settings
                console.log("Pd-l2Ork: successfully reinitialized");
                console.log("Pd-l2Ork: audio input channels: " + pd.getNumInChannels());
                console.log("Pd-l2Ork: audio output channels: " + pd.getNumOutChannels());
                console.log("Pd-l2Ork: audio sample rate: " + pd.getSampleRate());
                console.log("Pd-l2Ork: audio ticks per buffer: " + pd.getTicksPerBuffer());
            }
            else {
                // failed to reinit pd
                alert("Pd-l2Ork: failed to reinitialize pd");
                console.error("Pd-l2Ork: failed to reinitialize pd");
                Module.mainExit();
            }
        }

        // open midi (called by "pd midi-dialog" message)
        // receives input/output arrays of only selected devices
        // 0: first available device, 1: second available device...
        Module.Pd.openMidi = function (midiinarr, midioutarr) {
            if (!isWebMidiSupported)
                return;

            // if the selected device doesn't exist, use first available device instead
            midiinarr = midiinarr.map(item => item >= WebMidi.inputs.length || item < 0 ? 0 : item);
            midioutarr = midioutarr.map(item => item >= WebMidi.outputs.length || item < 0 ? 0 : item);

            // save this settings so we can check again later when connection status changes 
            midiarr = [];
            for (var i = 0; i < 10; i++) {
                var devno = i < midiinarr.length ? midiinarr[i] + 1 : 0;
                midiarr.push(devno);
            }
            for (var i = 0; i < 10; i++) {
                var devno = i < midioutarr.length ? midioutarr[i] + 1 : 0;
                midiarr.push(devno);
            }
            // remove duplicates and convert device numbers to ids
            midiinarr = Array.from(new Set(midiinarr));
            midioutarr = Array.from(new Set(midioutarr));
            midiInIds = midiinarr.map(item => WebMidi.inputs[item].id);
            midiOutIds = midioutarr.map(item => WebMidi.outputs[item].id);

            // print all selected devices to the console
            for (var i = 0; i < midiInIds.length; i++) {
                var input = WebMidi.getInputById(midiInIds[i]);
                console.log("Midi: input" + (i + 1) + ": " + input.name);
            }
            for (var i = 0; i < midiOutIds.length; i++) {
                var output = WebMidi.getOutputById(midiOutIds[i]);
                console.log("Midi: output" + (i + 1) + ": " + output.name);
            }
            // receive midi messages from WebMidi and forward them to pd input
            function receiveNoteOn(e) {
                pd.sendNoteOn(e.channel, e.note.number, e.rawVelocity);
            }

            function receiveNoteOff(e) {
                pd.sendNoteOn(e.channel, e.note.number, 0);
            }

            function receiveControlChange(e) {
                pd.sendControlChange(e.channel, e.controller.number, e.value);
            }

            function receiveProgramChange(e) {
                pd.sendProgramChange(e.channel, e.value + 1);
            }

            function receivePitchBend(e) {
                // [bendin] takes 0 - 16383 while [bendout] returns -8192 - 8192
                pd.sendPitchBend(e.channel, e.value * 8192 + 8192);
            }

            function receiveAftertouch(e) {
                pd.sendAftertouch(e.channel, e.value * 127);
            }

            function receivePolyAftertouch(e) {
                pd.sendPolyAftertouch(e.channel, e.note.number, e.value * 127);
            }

            for (var i = 0; i < midiInIds.length; i++) {
                var input = WebMidi.getInputById(midiInIds[i]);
                if (input) {
                    input.removeListener(); // remove all added listeners
                    input.addListener("noteon", "all", receiveNoteOn);
                    input.addListener("noteoff", "all", receiveNoteOff);
                    input.addListener("controlchange", "all", receiveControlChange);
                    input.addListener("programchange", "all", receiveProgramChange);
                    input.addListener("pitchbend", "all", receivePitchBend);
                    input.addListener("channelaftertouch", "all", receiveAftertouch);
                    input.addListener("keyaftertouch", "all", receivePolyAftertouch);
                }
            }
        }

        // get midi in device name
        Module.Pd.getMidiInDeviceName = function (devno) {
            if (!isWebMidiSupported)
                return;
            if (devno >= WebMidi.inputs.length || devno < 0) {
                devno = 0;
            }
            var name = WebMidi.inputs[devno].name;
            var lengthBytes = lengthBytesUTF8(name) + 1;
            var stringOnWasmHeap = _malloc(lengthBytes);
            stringToUTF8(name, stringOnWasmHeap, lengthBytes);
            return stringOnWasmHeap;
        }

        // get midi out device name
        Module.Pd.getMidiOutDeviceName = function (devno) {
            if (!isWebMidiSupported)
                return;
            if (devno >= WebMidi.inputs.length || devno < 0) {
                devno = 0;
            }
            var name = WebMidi.outputs[devno].name;
            var lengthBytes = lengthBytesUTF8(name) + 1;
            var stringOnWasmHeap = _malloc(lengthBytes);
            stringToUTF8(name, stringOnWasmHeap, lengthBytes);
            return stringOnWasmHeap;
        }

        // receive gui commands (only called in gui mode)
        Module.Pd.receiveCommandBuffer = function (data) {
            var command_buffer = {
                next_command: ""
            };
            perfect_parser(data, command_buffer);
        }

        // receive print messages (only called in no gui mode)
        let buf = '';
        Module.Pd.receivePrint = function (s) {
            let lines = s.split('\n');
            for(let i = 0; i < lines.length - 1; i++) {
                buf += lines[i];
                console.log(buf);
                buf = '';
            }
            buf += lines[lines.length - 1];
        }

        // receive from pd's subscribed sources
        Module.Pd.receiveBang = function (source) {
            if (source in subscribedData) {
                for (const data of subscribedData[source]) {
                    switch (data.type) {
                        case "bng":
                            gui_bng_update_circle(data);
                            gui_send('Bang', data.send)
                            break;
                        case "tgl":
                            data.value = data.value ? 0 : data.default_value;
                            gui_tgl_update_cross(data);
                            gui_send('Float', data.send, data.value);
                            break;
                        case "vsl":
                        case "hsl":
                            gui_slider_bang(data);
                            break;
                        case "vradio":
                        case "hradio":
                        case "floatatom":
                        case "nbx":
                            gui_send('Float', data.send, data.value);
                            break;
                        case "symbolatom":
                            gui_send('Symbol', data.send, data.value);
                            break;
                        case "pddplink":
                        case "pddp/pddplink":
                            gui_link_open(data.link);
                            break;
                        case "flatgui/knob":
                            gui_send('Float', data.send, data.value);
                            break;
                        case "dropdown":
                            gui_dropdown_send(data);
                            break;
                    }
                }
            }
        }

        Module.Pd.receiveFloat = function (source, value) {
            if (source in subscribedData) {
                for (const data of subscribedData[source]) {
                    switch (data.type) {
                        case "bng":
                            gui_bng_update_circle(data);
                            gui_send('Bang', data.send);
                            break;
                        case "tgl":
                            data.value = value;
                            if(value != 0)
                                data.default_value = value;
                            gui_tgl_update_cross(data);
                            gui_send('Float', data.send, data.value);
                            break;
                        case "vsl":
                        case "hsl":
                            gui_slider_set(data, value);
                            gui_slider_bang(data);
                            break;
                        case "vradio":
                        case "hradio":
                            data.value = Math.min(Math.max(Math.floor(value), 0), data.number - 1);
                            gui_radio_update_button(data);
                            gui_send('Float', data.send, data.value);
                            break;
                        case "flatgui/knob":
                            data.value = Math.max(Math.min(value,data.maximum),data.minimum);
                            configure_item(data.extracircle, gui_knob_extracircle(data));
                            configure_item(data.line, gui_knob_line(data));
                            gui_send('Float', data.send, data.value);
                            break;
                        case "vu":
                            data.value = value;
                            gui_vumeter_update_bars(data);
                            gui_send('Float', data.send, data.value);
                            break;
                        case "floatatom":
                            data.value = value;
                            gui_atom_update(data);
                            break;
                        case "nbx":
                            data.value = value;
                            gui_nbx_update(data);
                            break;
                        case "dropdown":
                            data.selectedOption = Math.max(0, Math.min(data.options.length - 1, value));
                            data.render();
                            gui_dropdown_send(data);
                            break;
                    }
                }
            }
        }

        Module.Pd.receiveSymbol = function (source, symbol) {
            if (source in subscribedData) {
                for (const data of subscribedData[source]) {
                    switch (data.type) {
                        case "bng":
                            gui_bng_update_circle(data);
                            break;
                        case "symbolatom":
                            data.value = symbol;
                            gui_atom_update(data);
                            break;
                        case "pddp/pddplink":
                        case "pddplink":
                            gui_link_open(symbol);
                            break;
                    }
                }
            }
        }

        Module.Pd.receiveList = function (source, list) {
            if (source in subscribedData) {
                for (const data of subscribedData[source]) {
                    switch (data.type) {
                        case "bng":
                            gui_bng_update_circle(data);
                            break;
                        case "tgl":
                            data.value = list[0];
                            if(list[0] != 0)
                                data.default_value = list[0];
                            gui_tgl_update_cross(data);
                            gui_send('Float', data.send, data.value);
                            break;
                        case "vsl":
                        case "hsl":
                            gui_slider_set(data, list[0]);
                            gui_slider_bang(data);
                            break;
                        case "vradio":
                        case "hradio":
                            data.value = Math.min(Math.max(Math.floor(list[0]), 0), data.number - 1);
                            gui_radio_update_button(data);
                            gui_send('Float', data.send, data.value);
                            break;
                        case "vu":
                            if(list[0] !== 0 || list[1] === 0)
                                data.value = list[0];
                            data.peak = list[1];
                            gui_vumeter_update_bars(data);
                            configure_item(data.line, gui_vumeter_line(data));
                            gui_send('Float', data.send, data.value);
                            break;
                        case "nbx":
                            data.value = list[0];
                            gui_nbx_update(data);
                            break;
                        case "dropdown":
                            data.selectedOption = Math.max(0, Math.min(data.options.length - 1, list[0]));
                            data.render();
                            gui_dropdown_send(data);
                            break;

                    }
                }
            }
        }

        Module.Pd.receiveMessage = function (source, symbol, list) {
            if (source in subscribedData) {
                for (const data of subscribedData[source]) {
                    switch (data.type) {
                        case "msg":
                            switch(symbol) {
                                case "set":
                                    if(list.length == 1 && list[0] === 0)
                                        data.text = '';
                                    else
                                        data.text = list.join(' ');
                                    break;
                                case "add":
                                    data.text += list.join(' ')+';\n';
                                    break;
                                case "add2":
                                    data.text += list.join(' ');
                                    break;
                                case "addcomma":
                                    data.text += ',';
                                    break;
                                case "addsemi":
                                    data.text += ';\n';
                                    break;
                                case "adddollar":
                                    data.text += ' $'+list[0];
                                    break;
                                case "adddollsym":
                                    data.text += ' $'+list[0];
                                    break;
                            }
                            data.render(data);
                            break;
                        case "patch":
                            data.setVisibility(list[0]);
                            break;
                        case "bng":
                            switch (symbol) {
                                case "size":
                                    data.size = list[0] || 8;
                                    configure_item(data.rect, gui_bng_rect(data));
                                    configure_item(data.circle, gui_bng_circle(data));
                                    break;
                                case "flashtime":
                                    data.interrupt = list[0] || 10;
                                    data.hold = list[1] || 50;
                                    break;
                                case "init":
                                    data.init = list[0];
                                    break;
                                case "send":
                                    data.send[0] = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive[0] = list[0];
                                    gui_subscribe(data);
                                    break;
                                case "label":
                                    data.label = list[0] === "empty" ? "" : list[0];
                                    data.text.textContent = data.label;
                                    break;
                                case "label_pos":
                                    data.x_off = list[0];
                                    data.y_off = list[1] || 0;
                                    configure_item(data.text, gui_bng_text(data));
                                    break;
                                case "label_font":
                                    data.font = list[0];
                                    data.fontsize = list[1] || 0;
                                    configure_item(data.text, gui_bng_text(data));
                                    break;
                                case "color":
                                    data.bg_color = list[0];
                                    data.fg_color = list[1] || 0;
                                    data.label_color = list[2] || 0;
                                    configure_item(data.rect, gui_bng_rect(data));
                                    configure_item(data.text, gui_bng_text(data));
                                    break;
                                case "pos":
                                    data.x_pos = list[0];
                                    data.y_pos = list[1] || 0;
                                    configure_item(data.rect, gui_bng_rect(data));
                                    configure_item(data.circle, gui_bng_circle(data));
                                    configure_item(data.text, gui_bng_text(data));
                                    break;
                                case "delta":
                                    data.x_pos += list[0];
                                    data.y_pos += list[1] || 0;
                                    configure_item(data.rect, gui_bng_rect(data));
                                    configure_item(data.circle, gui_bng_circle(data));
                                    configure_item(data.text, gui_bng_text(data));
                                    break;
                                case "interactive":
                                    data.interactive = list[0];
                                    break;
                                default:
                                    gui_bng_update_circle(data);
                            }
                            break;
                        case "tgl":
                            switch (symbol) {
                                case "size":
                                    data.size = list[0] || 8;
                                    configure_item(data.rect, gui_tgl_rect(data));
                                    configure_item(data.cross1, gui_tgl_cross1(data));
                                    configure_item(data.cross2, gui_tgl_cross2(data));
                                    break;
                                case "nonzero":
                                    data.default_value = list[0];
                                    break;
                                case "init":
                                    data.init = list[0];
                                    break;
                                case "send":
                                    data.send[0] = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive[0] = list[0];
                                    gui_subscribe(data);
                                    break;
                                case "label":
                                    data.label = list[0] === "empty" ? "" : list[0];
                                    data.text.textContent = data.label;
                                    break;
                                case "label_pos":
                                    data.x_off = list[0];
                                    data.y_off = list[1] || 0;
                                    configure_item(data.text, gui_tgl_text(data));
                                    break;
                                case "label_font":
                                    data.font = list[0];
                                    data.fontsize = list[1] || 0;
                                    configure_item(data.text, gui_tgl_text(data));
                                    break;
                                case "color":
                                    data.bg_color = list[0];
                                    data.fg_color = list[1] || 0;
                                    data.label_color = list[2] || 0;
                                    configure_item(data.rect, gui_tgl_rect(data));
                                    configure_item(data.cross1, gui_tgl_cross1(data));
                                    configure_item(data.cross2, gui_tgl_cross2(data));
                                    configure_item(data.text, gui_tgl_text(data));
                                    break;
                                case "pos":
                                    data.x_pos = list[0];
                                    data.y_pos = list[1] || 0;
                                    configure_item(data.rect, gui_tgl_rect(data));
                                    configure_item(data.cross1, gui_tgl_cross1(data));
                                    configure_item(data.cross2, gui_tgl_cross2(data));
                                    configure_item(data.text, gui_tgl_text(data));
                                    break;
                                case "delta":
                                    data.x_pos += list[0];
                                    data.y_pos += list[1] || 0;
                                    configure_item(data.rect, gui_tgl_rect(data));
                                    configure_item(data.cross1, gui_tgl_cross1(data));
                                    configure_item(data.cross2, gui_tgl_cross2(data));
                                    configure_item(data.text, gui_tgl_text(data));
                                    break;
                                case "set":
                                    data.default_value = list[0];
                                    data.value = data.default_value;
                                    gui_tgl_update_cross(data);
                                    break;
                                case "interactive":
                                    data.interactive = list[0];
                                    break;
                            }
                            break;
                        case "vsl":
                        case "hsl":
                            switch (symbol) {
                                case "size":
                                    if (list.length === 1) {
                                        data.width = list[0] || 8;
                                    }
                                    else {
                                        data.width = list[0] || 8;
                                        data.height = list[1] || 2;
                                    }
                                    configure_item(data.rect, gui_slider_rect(data));
                                    configure_item(data.indicator, gui_slider_indicator(data));
                                    gui_slider_check_minmax(data);
                                    break;
                                case "range":
                                    data.bottom = list[0];
                                    data.top = list[1] || 0;
                                    gui_slider_check_minmax(data);
                                    break;
                                case "lin":
                                    data.log = 0;
                                    gui_slider_check_minmax(data);
                                    break;
                                case "log":
                                    data.log = 1;
                                    gui_slider_check_minmax(data);
                                    break;
                                case "init":
                                    data.init = list[0];
                                    break;
                                case "steady":
                                    data.steady_on_click = list[0];
                                    break;
                                case "send":
                                    data.send[0] = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive[0] = list[0];
                                    gui_subscribe(data);
                                    break;
                                case "label":
                                    data.label = list[0] === "empty" ? "" : list[0];
                                    data.text.textContent = data.label;
                                    break;
                                case "label_pos":
                                    data.x_off = list[0];
                                    data.y_off = list[1] || 0;
                                    configure_item(data.text, gui_slider_text(data));
                                    break;
                                case "label_font":
                                    data.font = list[0];
                                    data.fontsize = list[1] || 0;
                                    configure_item(data.text, gui_slider_text(data));
                                    break;
                                case "color":
                                    data.bg_color = list[0];
                                    data.fg_color = list[1] || 0;
                                    data.label_color = list[2] || 0;
                                    configure_item(data.rect, gui_slider_rect(data));
                                    configure_item(data.indicator, gui_slider_indicator(data));
                                    configure_item(data.text, gui_slider_text(data));
                                    break;
                                case "pos":
                                    data.x_pos = list[0];
                                    data.y_pos = list[1] || 0;
                                    configure_item(data.rect, gui_slider_rect(data));
                                    configure_item(data.indicator, gui_slider_indicator(data));
                                    configure_item(data.text, gui_slider_text(data));
                                    break;
                                case "delta":
                                    data.x_pos += list[0];
                                    data.y_pos += list[1] || 0;
                                    configure_item(data.rect, gui_slider_rect(data));
                                    configure_item(data.indicator, gui_slider_indicator(data));
                                    configure_item(data.text, gui_slider_text(data));
                                    break;
                                case "set":
                                    gui_slider_set(data, list[0]);
                                    break;
                                case "interactive":
                                    data.interactive = list[0];
                                    break;
                            }
                            break;
                        case "vradio":
                        case "hradio":
                            switch (symbol) {
                                case "size":
                                    data.size = list[0] || 8;
                                    configure_item(data.rect, gui_radio_rect(data));
                                    gui_radio_update_lines_buttons(data);
                                    break;
                                case "init":
                                    data.init = list[0];
                                    break;
                                case "number":
                                    const n = Math.min(Math.max(Math.floor(list[0]), 1), 128);
                                    if (n !== data.number) {
                                        data.number = n;
                                        if (data.value >= data.number) {
                                            data.value = data.number - 1;
                                        }
                                        configure_item(data.rect, gui_radio_rect(data));
                                        gui_radio_remove_lines_buttons(data);
                                        gui_radio_create_lines_buttons(data);
                                    }
                                    break;
                                case "send":
                                    data.send[0] = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive[0] = list[0];
                                    gui_subscribe(data);
                                    break;
                                case "label":
                                    data.label = list[0] === "empty" ? "" : list[0];
                                    data.text.textContent = data.label;
                                    break;
                                case "label_pos":
                                    data.x_off = list[0];
                                    data.y_off = list[1] || 0;
                                    configure_item(data.text, gui_radio_text(data));
                                    break;
                                case "label_font":
                                    data.font = list[0];
                                    data.fontsize = list[1] || 0;
                                    configure_item(data.text, gui_radio_text(data));
                                    break;
                                case "color":
                                    data.bg_color = list[0];
                                    data.fg_color = list[1] || 0;
                                    data.label_color = list[2] || 0;
                                    configure_item(data.rect, gui_radio_rect(data));
                                    gui_radio_update_lines_buttons(data);
                                    configure_item(data.text, gui_radio_text(data));
                                    break;
                                case "pos":
                                    data.x_pos = list[0];
                                    data.y_pos = list[1] || 0;
                                    configure_item(data.rect, gui_radio_rect(data));
                                    gui_radio_update_lines_buttons(data);
                                    configure_item(data.text, gui_radio_text(data));
                                    break;
                                case "delta":
                                    data.x_pos += list[0];
                                    data.y_pos += list[1] || 0;
                                    configure_item(data.rect, gui_radio_rect(data));
                                    gui_radio_update_lines_buttons(data);
                                    configure_item(data.text, gui_radio_text(data));
                                    break;
                                case "set":
                                    data.value = Math.min(Math.max(Math.floor(list[0]), 0), data.number - 1);
                                    gui_radio_update_button(data);
                                    break;
                                case "interactive":
                                    data.interactive = list[0];
                                    break;
                            }
                            break;
                        case "cnv":
                            switch (symbol) {
                                case "size":
                                    data.size = list[0] || 1;
                                    configure_item(data.selectable_rect, gui_cnv_selectable_rect(data));
                                    break;
                                case "vis_size":
                                    if (list.length === 1) {
                                        data.width = list[0] || 1;
                                        data.height = data.width;
                                    }
                                    else {
                                        data.width = list[0] || 1;
                                        data.height = list[1] || 1;
                                    }
                                    configure_item(data.visible_rect, gui_cnv_visible_rect(data));
                                    break;
                                case "send":
                                    data.send[0] = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive[0] = list[0];
                                    gui_subscribe(data);
                                    break;
                                case "label":
                                    data.label = list[0] === "empty" ? "" : list[0];
                                    data.text.textContent = data.label;
                                    break;
                                case "label_pos":
                                    data.x_off = list[0];
                                    data.y_off = list[1] || 0;
                                    configure_item(data.text, gui_cnv_text(data));
                                    break;
                                case "label_font":
                                    data.font = list[0];
                                    data.fontsize = list[1] || 0;
                                    configure_item(data.text, gui_cnv_text(data));
                                    break;
                                case "get_pos":
                                    break;
                                case "color":
                                    data.bg_color = list[0];
                                    data.label_color = list[1] || 0;
                                    configure_item(data.visible_rect, gui_cnv_visible_rect(data));
                                    configure_item(data.selectable_rect, gui_cnv_selectable_rect(data));
                                    configure_item(data.text, gui_cnv_text(data));
                                    break;
                                case "pos":
                                    data.x_pos = list[0];
                                    data.y_pos = list[1] || 0;
                                    configure_item(data.visible_rect, gui_cnv_visible_rect(data));
                                    configure_item(data.selectable_rect, gui_cnv_selectable_rect(data));
                                    configure_item(data.text, gui_cnv_text(data));
                                    break;
                                case "delta":
                                    data.x_pos += list[0];
                                    data.y_pos += list[1] || 0;
                                    configure_item(data.visible_rect, gui_cnv_visible_rect(data));
                                    configure_item(data.selectable_rect, gui_cnv_selectable_rect(data));
                                    configure_item(data.text, gui_cnv_text(data));
                                    break;
                            }
                            break;
                        case 'floatatom':
                        case 'symbolatom':
                            switch(symbol) {
                                case 'focus':
                                    if(list[0] && data.interactive)
                                        setKeyboardFocus(data, data.exclusive);
                                    else
                                        setKeyboardFocus(null, false);
                                    configure_item(data.svgText, {fill: list[0] && data.interactive ? '#ff0000' : '#000000'});
                                    break;
                                case 'commit':
                                    gui_atom_commit(data);
                                    break;
                                case 'interactive':
                                    data.interactive = list[0];
                                    if(keyboardFocus.data?.id === data.id)
                                        setKeyboardFocus(null, false);
                                    break;
                                case 'set':
                                    data.dirtyValue = '' + list[0];
                                    let send = data.send;
                                    data.send = [];
                                    gui_atom_commit(data);
                                    data.send = send;
                                    break;
                            }
                            break;
                        case 'nbx':
                            switch(symbol) {
                                case 'autoupdate':
                                    data.arrowUpdate = list[0];
                                    break;
                                case 'commit':
                                    gui_nbx_commit(data);
                                    break;
                                case 'drawstyle':
                                    data.showTriangle = list[0] % 2 == 0;
                                    data.showBorder = list[0] < 2;
                                    configure_item(data.border, {"stroke-width": data.showBorder ? '1' : '0'});
                                    configure_item(data.triangle, {'stroke-width': data.showTriangle ? '1' : '0'});
                                    break;
                                case 'exclusive':
                                    data.exclusive = list[0];
                                    break;
                                case 'focus':
                                    if(list[0] && data.interactive)
                                        setKeyboardFocus(data, data.exclusive);
                                    else
                                        setKeyboardFocus(null, false);
                                    configure_item(data.svgText, {fill: list[0] && data.interactive ? '#ff0000' : '#000000'});
                                    break;
                                case 'interactive':
                                    data.interactive = list[0];
                                    if(keyboardFocus.data?.id === data.id)
                                        setKeyboardFocus(null, false);
                                    break;
                                case 'set':
                                    data.dirtyValue = '' + list[0];
                                    let send = data.send;
                                    data.send = [];
                                    gui_atom_commit(data);
                                    data.send = send;
                                    break;
                            }
                            break;
                        case 'flatgui/knob':
                            switch(symbol) {
                                case 'size':
                                    data.size_x = list[0];
                                    data.size_y = list[1];
                                    gui_knob_render(data);
                                    break;
                                case 'range':
                                    data.minimum = list[0];
                                    data.maximum = list[1];
                                    break;
                                case 'lin':
                                    data.logarithmic = 0;
                                    break;
                                case 'log':
                                    data.logarithmic = 1;
                                    break;
                                case 'steady':
                                    data.steady_on_click = list[0];
                                    break;
                                case 'receive':
                                    data.receive[0] = list[0];
                                    break;
                                case 'send':
                                    data.send[0] = list[0];
                                    break;
                                case 'label':
                                    data.label = list[0];
                                    gui_knob_render(data);
                                    break;
                                case 'label_pos':
                                    data.x_off = list[0];
                                    data.y_off = list[1];
                                    gui_knob_render(data);
                                    break;
                                case 'label_font':
                                    data.font = list[0];
                                    data.fontsize = list[1];
                                    gui_knob_render(data);
                                    break;
                                case 'color':
                                    data.bg_color = list[0]
                                    data.fg_color = list[1];
                                    data.label_color = list[2];
                                    break;
                                case 'position':
                                    data.x_pos = list[0];
                                    data.y_pos = list[1];
                                    gui_knob_render(data);
                                    break;
                                case 'delta':
                                    data.x_pos += list[0];
                                    data.y_pos += list[1];
                                    gui_knob_render(data);
                                    break;
                                case 'interactive':
                                    data.interactive = list[0];
                                    break;
                                case 'width':
                                    data.dial_width = list[0];
                                    data.off_width = list[1];
                                    data.on_width = list[2];
                                    gui_knob_render(data);
                                    break;
                            }
                            break;
                        case "vu":
                            switch(symbol) {
                                case "size":
                                    data.width = Math.max(0,list[0]);
                                    data.height = Math.max(0,list[1]);
                                    data.height = gui_vumeter_unitheight(data) * (vu_colmap.length - 3);
                                    break;
                                case "scale":
                                    data.showScale = list[0];
                                    break;
                                case "label_pos":
                                    data.x_off = list[0];
                                    data.y_off = list[1];
                                    break;
                                case "receive":
                                    data.receive[0] = list[0];
                                    break;
                                case "label":
                                    data.label = list[0];
                                    data.text.textContent = list[0];
                                    break;
                                case "label_font":
                                    data.font = list[0];
                                    data.fontsize = list[1];
                                    break;
                                case "color":
                                    data.bg_color = list[0];
                                    data.label_color = list[1];
                                    break;
                                case "pos":
                                    data.x_pos = list[0];
                                    data.y_pos = list[1];
                                    break;
                                case "delta":
                                    data.x_pos += list[0];
                                    data.y_pos += list[1];
                                    break;
                            }
                            gui_vumeter_render(data);
                            break;
                        case "ggee/image":
                            switch(symbol) {
                                case "visible":
                                    data.visible = list[0];
                                    break;
                                case "click":
                                    data.clickBehavior = list[0];
                                    break;
                                case "alpha":
                                    data.opacity = list[0];
                                    break;
                                case "color":
                                    data.label_color = list[0];
                                    break;
                                case "pos":
                                    data.x_pos = list[0];
                                    data.y_pos = list[1];
                                    break;
                                case "delta":
                                    data.x_pos += list[0];
                                    data.y_pos += list[1];
                                    break;
                                case "label":
                                    data.label = list[0];
                                    break;
                                case "label_font":
                                    data.font = list[0];
                                    data.fontsize = list[1];
                                    break;
                                case "label_pos":
                                    data.x_off = list[0];
                                    data.y_off = list[1];
                                    break;
                                case "open":
                                    data.src = list[0];
                                    break;
                                case "gopspill_size":
                                    data.borderWidth = list[0];
                                    data.borderHeight = list[1];
                                    break;
                                case "gopspill":
                                    data.gopSpill = list[0];
                                    if(!data.gopSpill) {
                                        data.borderWidth = data.imageWidth;
                                        data.borderHeight = data.imageHeight;
                                    }
                                    break;
                                case "receive":
                                    data.receive[0] = list[0];
                                    break;
                                case "send":
                                    data.send[0] = list[0];
                                    break;
                                case "rotate":
                                    data.rotation = list[0];
                                    if(list.length > 1) {
                                        data.x_origin = list[1];
                                        data.y_origin = list[2];
                                    }
                                    break;
                                case "set":
                                    data.src = gui_image_keys[list[0]];
                                    break;
                                case "load":
                                    gui_image_keys[list[0]] = list[1];
                                    break;
                                case "constrain":
                                    data.constrain = list[0];
                                    break;
                                case "size":
                                    data.imageWidth = list[0];
                                    data.imageHeight = list[1];
                                    break;
                                case "reset":
                                    data.imageWidth = data.imageNaturalWidth;
                                    data.imageHeight = data.imageNaturalHeight;
                                    break;
                                
                            }
                            data.render(data);
                            break;
                        case "array":
                            switch(symbol) {
                                case "data":
                                    if(data.drawTimeout)
                                        clearTimeout(data.drawTimeout);
                                    let start = list[0] * 10000 - 1, initialLength = data.nums.length;
                                    let top = Math.max(data.coords.t, data.coords.b);
                                    let bot = Math.min(data.coords.t, data.coords.b);
                                    for(let i = 1; i < list.length; i++)
                                        data.nums[i + start] = list[i] > top ? top : (list[i] < bot ? bot : list[i]);
                                    if(initialLength < data.nums.length)
                                        data.resize(data.nums.length);
                                    data.drawTimeout = setTimeout(data.redraw, 16);
                                    break;
                                case "rename":
                                    for(let label of data.layer.labels)
                                    if(label.textContent == data.receive[0])
                                        label.textContent = list[0];

                                    gui_unsubscribe(data);
                                    data.receive[0] = list[0];
                                    gui_subscribe(data);
                                    data.name = list[0];
                                    data.redraw();
                                    break;
                                case "bounds":
                                    data.layer.dimensions.coords.t = list[1];
                                    data.layer.dimensions.coords.b = list[3];
                                    gui_canvas_drawTicks(data.layer);
                                    gui_canvas_drawLabels(data.layer);
                                    for(let array of data.layer.arrays)
                                        array.setCoords(data.layer.dimensions.coords);
                                    break;
                                case "xticks":
                                    data.layer.dimensions.coords.xticks.start = list[0];
                                    data.layer.dimensions.coords.xticks.interval = list[1];
                                    data.layer.dimensions.coords.xticks.big = list[2];
                                    gui_canvas_drawTicks(data.layer);
                                    break;
                                case "yticks":
                                    data.layer.dimensions.coords.yticks.start = list[0];
                                    data.layer.dimensions.coords.yticks.interval = list[1];
                                    data.layer.dimensions.coords.yticks.big = list[2];
                                    gui_canvas_drawTicks(data.layer);
                                    break;
                                case "xlabel":
                                    data.layer.dimensions.coords.xlabels.pos = list[0];
                                    data.layer.dimensions.coords.xlabels.labels = list.slice(1);
                                    gui_canvas_drawLabels(data.layer);
                                    break;
                                case "ylabel":
                                    data.layer.dimensions.coords.ylabels.pos = list[0];
                                    data.layer.dimensions.coords.ylabels.labels = list.slice(1);
                                    gui_canvas_drawLabels(data.layer);
                                    break;
                                case "resize":
                                    if(list[0] >= 0)
                                        data.resize(list[0]);
                                    break;

                            }
                            break;
                        case "dropdown":
                            switch(symbol) {
                                case "names":
                                    data.options = list;
                                    data.render();
                                    break;
                                case "set":
                                    data.selectedOption = Math.max(0, Math.min(data.options.length - 1, list[0]));
                                    data.render();
                                    break;
                            }
                            break;
                    }
                }
            }
        }

        // receive midi messages from pd and forward them to WebMidi output
        Module.Pd.receiveNoteOn = function (channel, pitch, velocity) {
            for (var i = 0; i < midiOutIds.length; i++) {
                var output = WebMidi.getOutputById(midiOutIds[i]);
                if (output) {
                    output.playNote(pitch, channel, { rawVelocity: true, velocity: velocity });
                }
            }
        }

        Module.Pd.receiveControlChange = function (channel, controller, value) {
            for (var i = 0; i < midiOutIds.length; i++) {
                var output = WebMidi.getOutputById(midiOutIds[i]);
                if (output) {
                    output.sendControlChange(controller, value, channel);
                }
            }
        }

        Module.Pd.receiveProgramChange = function (channel, value) {
            for (var i = 0; i < midiOutIds.length; i++) {
                var output = WebMidi.getOutputById(midiOutIds[i]);
                if (output) {
                    output.sendProgramChange(value, channel);
                }
            }
        }

        Module.Pd.receivePitchBend = function (channel, value) {
            for (var i = 0; i < midiOutIds.length; i++) {
                var output = WebMidi.getOutputById(midiOutIds[i]);
                if (output) {
                    // [bendin] takes 0 - 16383 while [bendout] returns -8192 - 8192
                    output.sendPitchBend(value / 8192, channel);
                }
            }
        }

        Module.Pd.receiveAftertouch = function (channel, value) {
            for (var i = 0; i < midiOutIds.length; i++) {
                var output = WebMidi.getOutputById(midiOutIds[i]);
                if (output) {
                    output.sendChannelAftertouch(value / 127, channel);
                }
            }
        }

        Module.Pd.receivePolyAftertouch = function (channel, pitch, value) {
            for (var i = 0; i < midiOutIds.length; i++) {
                var output = WebMidi.getOutputById(midiOutIds[i]);
                if (output) {
                    output.sendKeyAftertouch(pitch, channel, value / 127);
                }
            }
        }

        Module.Pd.receiveMidiByte = function (port, byte) {
        }

        // default audio settings
        var numInChannels = 0; // supported values: 0, 1, 2
        var numOutChannels = 2; // supported values: 1, 2
        var sampleRate = 44100; // might change depending on browser/system
        var ticksPerBuffer = 32; // supported values: 4, 8, 16, 32, 64, 128, 256

        // open audio devices, init pd
        if (pd.init(numInChannels, numOutChannels, sampleRate, ticksPerBuffer)) {

            // print obtained settings
            console.log("Pd-l2Ork: successfully initialized");
            console.log("Pd-l2Ork: audio input channels:", pd.getNumInChannels());
            console.log("Pd-l2Ork: audio output channels:", pd.getNumOutChannels());
            console.log("Pd-l2Ork: audio sample rate:", pd.getSampleRate());
            console.log("Pd-l2Ork: audio ticks per buffer:", pd.getTicksPerBuffer());

            // add internals/externals help/search paths
            var helpPath = "pd-l2ork-web/doc/5.reference";
            var extPath = "pd-l2ork-web/extra";
            pd.addToHelpPath(helpPath);
            pd.addToSearchPath(extPath);
            pd.addToHelpPath(extPath);
            var dir = FS.readdir(extPath);
            for (var i = 0; i < dir.length; i++) {
                var item = dir[i];
                if (item.charAt(0) != ".") {
                    var path = extPath + "/" + item;
                    pd.addToSearchPath(path); // externals can be created without path prefix
                    pd.addToHelpPath(path);
                }
            }
            init(); // call global init function
        }
        else { // failed to init pd
            alert("Pd-l2Ork: failed to initialize pd");
            console.error("Pd-l2Ork: failed to initialize pd");
            Module.mainExit();
        }
    }
    , mainLoop: function () { // called every frame (use for whatever)
    }
    , mainExit: function () { // this won't be called from emscripten
        console.error("quiting emscripten...");
        if (typeof Module.pd == "object") {
            Module.pd.clear(); // clear pd, close audio devices
            Module.pd.unsubscribeAll(); // unsubscribe all subscribed sources
            Module.pd.delete(); // quit SDL, emscripten
        }
        if (typeof WebMidi == "object") {
            WebMidi.disable(); // disable all midi devices
        }
    }
};

Module['preRun'].push(function() {

    //Cyclone shenanigans
    for(let file of hammerFiles)
        FS.symlink('/pd-l2ork-web/extra/cyclone/Hammer.wasm', `/pd-l2ork-web/extra/cyclone/${file}`);
    for(let file of sickleFiles.filter(f=>!hammerFiles.includes(f)))
        FS.symlink('/pd-l2ork-web/extra/cyclone/Sickle.wasm', `/pd-l2ork-web/extra/cyclone/${file}`);

    // Intercept file reads
    FS.open = function (path, flags, mode) {
        let isNetworkCandidate = !['wasm','so','pd','pat'].map(ext=>('' + path).endsWith(ext)).find(matches => matches == true) && (('' + path).startsWith('/pd-l2ork-web') == false);
        if (path === "") {
            throw new FS.ErrnoError(44)
        }
        flags = typeof flags == "string" ? FS_modeStringToFlags(flags) : flags;
        mode = typeof mode == "undefined" ? 438 : mode;
        if (flags & 64) {
            mode = mode & 4095 | 32768
        } else {
            mode = 0
        }
        var node;
        if (typeof path == "object") {
            node = path
        } else {
            path = PATH.normalize(path);
            try {
                var lookup = FS.lookupPath(path, {
                    follow: !(flags & 131072)
                });
                node = lookup.node
                
            } catch (e) {
                if(isNetworkCandidate) {
                    if(fileCache[path])
                        node = fileCache[path];
                    else {
                        for(let searchPath of searchPaths) {
                            const request = new XMLHttpRequest();
                            request.open("GET", window.location.origin+'/'+((new URLSearchParams(window.location.search)).get('url')||'').split('/').slice(0,-1).join('/')+'/'+searchPath.slice(0,-1)+path, false); // `false` makes the request synchronous
                            request.overrideMimeType('text/plain; charset=x-user-defined'); // Set MIME type for binary data
                            request.send();
                            if (request.status === 200) {
                                let folder = path.split('/').slice(0,-1).join('/') + '/';
                                FS.createPath('/', folder);
                                FS.createDataFile(folder, path.split('/').slice(-1)[0], request.response, true, true, true);
                                fileCache[path] = FS.lookupPath(path, { follow: !(flags & 131072) }).node;
                                node = fileCache[path];
                                break;
                            }
                        }
                    }
                }
            }
        }
        var created = false;
        if (flags & 64) {
            if (node) {
                if (flags & 128) {
                    throw new FS.ErrnoError(20)
                }
            } else {
                node = FS.mknod(path, mode, 0);
                created = true
            }
        }
        if (!node) {
            throw new FS.ErrnoError(44)
        }
        if (FS.isChrdev(node.mode)) {
            flags &= ~512
        }
        if (flags & 65536 && !FS.isDir(node.mode)) {
            throw new FS.ErrnoError(54)
        }
        if (!created) {
            var errCode = FS.mayOpen(node, flags);
            if (errCode) {
                throw new FS.ErrnoError(errCode)
            }
        }
        if (flags & 512 && !created) {
            FS.truncate(node, 0)
        }
        flags &= ~(128 | 512 | 131072);
        var stream = FS.createStream({
            node: node,
            path: FS.getPath(node),
            flags: flags,
            seekable: true,
            position: 0,
            stream_ops: node.stream_ops,
            ungotten: [],
            error: false
        });
        if (stream.stream_ops.open) {
            stream.stream_ops.open(stream)
        }
        if (Module["logReadFiles"] && !(flags & 1)) {
            if (!FS.readFiles)
                FS.readFiles = {};
            if (!(path in FS.readFiles)) {
                FS.readFiles[path] = 1
            }
        }
        return stream;
    }
});


//--------------------- pdgui.js ----------------------------

function pdsend() {
    var string = Array.prototype.join.call(arguments, " ");
    var array = string.split(" ");
    Module.pd.startMessage(array.length - 2);
    for (let i = 2; i < array.length; i++) {
        if (isNaN(array[i]) || array[i] === "") {
            Module.pd.addSymbol(array[i]);
        }
        else {
            Module.pd.addFloat(+array[i]);
        }
    }
    Module.pd.finishMessage(array[0], array[1]);
}

function gui_ping() {
    pdsend("pd ping");
}

function gui_post(string, type) {
    console.log("gui_post", string, type);
}

function gui_post_error(objectid, loglevel, error_msg) {
    console.log("gui_post_error", objectid, loglevel, error_msg);
}

function gui_print(object_id, selector, array_of_strings) {
    console.log("gui_print", object_id, selector, array_of_strings);
}

function gui_legacy_tcl_command(file, line_number, text) {
    console.log("gui_legacy_tcl_command", file, line_number, text);
}

function gui_load_default_image(dummy_cid, key) {
    console.log("gui_load_default_image", dummy_cid, key);
}

function gui_undo_menu(cid, undo_text, redo_text) {
    console.log("gui_undo_menu", cid, undo_text, redo_text);
}

function gui_startup(version, fontname_from_pd, fontweight_from_pd, apilist, midiapilist) {
    console.log("gui_startup", version, fontname_from_pd, fontweight_from_pd, apilist, midiapilist);
}

function gui_set_cwd(dummy, cwd) {
    console.log("gui_set_cwd", dummy, cwd);
}

function set_audioapi(val) {
    console.log("set_audioapi", val);
}

function gui_pd_dsp(state) {
    console.log("gui_pd_dsp", state);
}

function gui_canvas_new(cid, width, height, geometry, zoom, editmode, name, dir, dirty_flag, hide_scroll, hide_menu, cargs) {
    console.log("gui_canvas_new", cid, width, height, geometry, zoom, editmode, name, dir, dirty_flag, hide_scroll, hide_menu, cargs);
}

function gui_set_toplevel_window_list(dummy, attr_array) {
    console.log("gui_pd_dsp", dummy, attr_array);
}

function gui_window_close(cid) {
    console.log("gui_window_close", cid);
}

function gui_canvas_get_scroll(cid) {
    console.log("gui_canvas_get_scroll", cid);
}

function pd_receive_command_buffer(data) {
    var command_buffer = {
        next_command: ""
    };
    perfect_parser(data, command_buffer);
}

function perfect_parser(data, cbuf, sel_array) {
    console.log(data,cbuf,sel_array);
    var i, len, selector, args;
    len = data.length;
    for (i = 0; i < len; i++) {
        // check for end of command:
        if (data[i] === 31) { // unit separator
            // decode next_command
            try {
                // This should work for all utf-8 content
                cbuf.next_command =
                    decodeURIComponent(cbuf.next_command);
            }
            catch (err) {
                // This should work for ISO-8859-1
                cbuf.next_command = unescape(cbuf.next_command);
            }
            // Turn newlines into backslash + "n" so
            // eval will do the right thing with them
            cbuf.next_command = cbuf.next_command.replace(/\n/g, "\\n");
            cbuf.next_command = cbuf.next_command.replace(/\r/g, "\\r");
            selector = cbuf.next_command.slice(0, cbuf.next_command.indexOf(" "));
            args = cbuf.next_command.slice(selector.length + 1);
            cbuf.next_command = "";
            // Now evaluate it
            //post("Evaling: " + selector + "(" + args + ");");
            // For communicating with a secondary instance, we filter
            // incoming messages. A better approach would be to make
            // sure that the Pd engine only sends the gui_set_cwd message
            // before "gui_startup".  Then we could just check the
            // Pd engine id in "gui_startup" and branch there, instead of
            // fudging with the parser here.
            if (!sel_array || sel_array.indexOf(selector) !== -1) {
                eval(selector + "(" + args + ");");
            }
        } else {
            cbuf.next_command += "%" +
                ("0" // leading zero (for rare case of single digit)
                    + data[i].toString(16)) // to hex
                    .slice(-2); // remove extra leading zero
        }
    }
}

function gui_audio_properties(gfxstub, sys_indevs, sys_outdevs,
    pd_indevs, pd_inchans, pd_outdevs, pd_outchans, audio_attrs) {
    console.log("gui_audio_properties", gfxstub, sys_indevs, sys_outdevs,
        pd_indevs, pd_inchans, pd_outdevs, pd_outchans, audio_attrs);
}

function gui_midi_properties(gfxstub, sys_indevs, sys_outdevs,
    pd_indevs, pd_outdevs, midi_attrs) {
    console.log("gui_midi_properties", gfxstub, sys_indevs, sys_outdevs,
        pd_indevs, pd_outdevs, midi_attrs);
}

function set_midiapi(val) {
    console.log("set_midiapi", val);
}

//--------------------- gui handling ----------------------------
function create_item(type, args, canvas) {
    var item = document.createElementNS("http://www.w3.org/2000/svg", type);
    if (args !== null)
        configure_item(item, args);
    canvas.appendChild(item);
    return item;
}


function configure_item(item, attributes) {
    // draw_vis from g_template sends attributes
    // as a ["attr1",val1, "attr2", val2, etc.] array,
    // so we check for that here
    var value, i, attr;
    if (Array.isArray(attributes)) {
        // we should check to make sure length is even here...
        for (i = 0; i < attributes.length; i += 2) {
            value = attributes[i + 1];
            item.setAttributeNS(null, attributes[i],
                Array.isArray(value) ? value.join(" ") : value);
        }
    } else {
        for (attr in attributes) {
            if (attributes.hasOwnProperty(attr)) {
                if (item) {
                    item.setAttributeNS(null, attr, attributes[attr]);
                }
            }
        }
    }
}

function iemgui_fontfamily(font) {
    let family = "";
    if (font === 1) {
        family = "'Helvetica', 'DejaVu Sans', 'sans-serif'";
    }
    else if (font === 2) {
        family = "'Times New Roman', 'DejaVu Serif', 'FreeSerif', 'serif'";
    }
    else {
        family = "'DejaVu Sans Mono', 'monospace'";
    }
    return family;
}

function colfromload(col) { // decimal to hex color
    if (typeof col === "string")
        return col;
    col = col >= 0 ? vu_colors[col] : -1 - col;
    // col = ((col & 0x3f000) << 6) | ((col & 0xfc0) << 4) | ((col & 0x3f) << 2);
    return "#" + ("000000" + col.toString(16)).slice(-6);
}

function gui_subscribe(data) {
    for(let receive of data.receive) {
        if(!receive)
            continue;
        if (receive in subscribedData) {
            if(!subscribedData[receive].find(sub=>sub.id === data.id))
                subscribedData[receive].push(data);
        }
        else {
            subscribedData[receive] = [data];
            Module.pd.subscribe(receive);
        }
    }
}

function gui_unsubscribe(data) {
    if(!data.receive)
        return;
    for(let receive of data.receive) {
        if (receive in subscribedData) {
            const len = subscribedData[receive].length;
            for (let i = 0; i < len; i++) {
                if (subscribedData[receive][i].id === data.id) {
                    subscribedData[receive].splice(i, 1);
                    if (!subscribedData[receive].length) {
                        Module.pd.unsubscribe(receive);
                        delete subscribedData[receive];
                    }
                    break;
                }
            }
        }
    }
}

function gui_send(type, destinations, value) {
    for(let destination of destinations) {
        if(destination) {
            if(type === 'List') {
                if(destination) {
                    Module.pd.startMessage(value.length);
                    for(let val of value) {
                        if(typeof val === 'number')
                            Module.pd.addFloat(val);
                        else if(typeof val === 'string')
                            Module.pd.addSymbol(val);
                        else
                            return console.error('Invalid value in list, not sent!');
                    }
                    Module.pd.finishList(destination);
                }
            }
            else if(type === 'Message') {
                Module.pd.startMessage(value);
                for(let val of value.slice(1)) {
                    if(typeof val === 'number')
                        Module.pd.addFloat(val);
                    else if(typeof val === 'string')
                        Module.pd.addSymbol(val);
                    else
                        return console.error('Invalid value in message, not sent!');
                }
                Module.pd.finishMessage(destination, value[0]);
            } else
                value === undefined ? Module.pd['send'+type](destination) : Module.pd['send'+type](destination, value);
        }
    }
}

// common
function gui_rect(data) {
    return {
        x: data.x_pos,
        y: data.y_pos,
        width: data.size,
        height: data.size,
        fill: colfromload(data.bg_color),
        id: `${data.id}_rect`,
        class: "border clickable"
    }
}

function gui_text(data) {
    return {
        x: data.x_pos + data.x_off,
        y: data.y_pos + data.y_off + data.fontsize / 5,
        "font-family": iemgui_fontfamily(data.font),
        "font-weight": "normal",
        "font-size": `${data.fontsize}px`,
        fill: colfromload(data.label_color),
        transform: `translate(0, ${data.fontsize / 2 * 0.6})`, // note: modified
        id: `${data.id}_text`,
        class: "unclickable"
    }
}

function gui_mousepoint(e, canvas, precise) { // transforms the mouse position
    // If we are on firefox, we have to do shenanigans...
    // First, getScreenCTM only scales, doesnt offset on firefox, for some ungodly reason.
    // Next, firefox refuses to give us the x/y coordinates of the top-left of an svg element.
    // It just returns 0/0 for all svg elements. So, we find the border of the canvas and
    // use it instead to update the CTM. As long as it works, don't question it.
    // However, this only works reliable for arrays, as other things don't have borders as the
    // next element. So, if we don't find a border, we just pretent like this coordinate is correct.
    // This is fine, though, as arrays are the only things that need absolute mouse positions.
    // Numboxes / knobs / etc. just use relative from where it started.
    let CTM = canvas.getScreenCTM();
    if(isFirefox) {
        let border = canvas.parentNode.childNodes[[...canvas.parentNode.childNodes].indexOf(canvas)+1];
        if(border.id?.endsWith('border')) {
            CTM.e = border.getBoundingClientRect().x;
            CTM.f = border.getBoundingClientRect().y;
        }
    }
    return (new DOMPointReadOnly(e.clientX, e.clientY)).matrixTransform(CTM.inverse());
}
function gui_roundedmousepoint(e, canvas) {
    let point = gui_mousepoint(e, canvas);
    point.x = Math.round(point.x);
    point.y = Math.round(point.y);
    return point;
}

// bng
function gui_bng_rect(data) {
    return gui_rect(data);
}

function gui_bng_circle(data) {
    const r = (data.size - 2) / 2;
    const cx = data.x_pos + r + 1;
    const cy = data.y_pos + r + 1;
    return {
        cx: cx,
        cy: cy,
        r: r,
        fill: "none",
        id: `${data.id}_circle`,
        "shape-rendering": "auto",
        class: "border unclickable"
    }
}

function gui_bng_text(data) {
    return gui_text(data);
}

function gui_bng_update_circle(data) {
    if (data.flashed) {
        data.flashed = false;
        configure_item(data.circle, {
            fill: colfromload(data.fg_color),
        });
        if (data.interrupt_timer) {
            clearTimeout(data.interrupt_timer);
        }
        data.interrupt_timer = setTimeout(function () {
            data.interrupt_timer = null;
            configure_item(data.circle, {
                fill: "none",
            });
        }, data.interrupt);
        data.flashed = true;
    }
    else {
        data.flashed = true;
        configure_item(data.circle, {
            fill: colfromload(data.fg_color),
        });
    }
    if (data.hold_timer) {
        clearTimeout(data.hold_timer);
    }
    data.hold_timer = setTimeout(function () {
        data.flashed = false;
        data.hold_timer = null;
        configure_item(data.circle, {
            fill: "none",
        });
    }, data.hold);
}

function gui_bng_onmousedown(data) {
    gui_bng_update_circle(data);
    gui_send('Bang', data.send);
}

// tgl
function gui_tgl_rect(data) {
    return gui_rect(data);
}

function gui_tgl_cross1(data) {
    const w = (data.size + 29) / 30 * 0.75; // note: modified
    const x1 = data.x_pos;
    const y1 = data.y_pos;
    const x2 = x1 + data.size;
    const y2 = y1 + data.size;
    const p1 = x1 + w + 1;
    const p2 = y1 + w + 1;
    const p3 = x2 - w - 1;
    const p4 = y2 - w - 1;
    const points = [p1, p2, p3, p4].join(" ");
    return {
        points: points,
        stroke: colfromload(data.fg_color),
        "stroke-width": w,
        fill: "none",
        display: data.value ? "inline" : "none",
        id: `${data.id}_cross1`,
        class: "unclickable"
    }
}

function gui_tgl_cross2(data) {
    const w = (data.size + 29) / 30 * 0.75; // note: modified
    const x1 = data.x_pos;
    const y1 = data.y_pos;
    const x2 = x1 + data.size;
    const y2 = y1 + data.size;
    const p1 = x1 + w + 1;
    const p2 = y2 - w - 1;
    const p3 = x2 - w - 1;
    const p4 = y1 + w + 1;
    const points = [p1, p2, p3, p4].join(" ");
    return {
        points: points,
        stroke: colfromload(data.fg_color),
        "stroke-width": w,
        fill: "none",
        display: data.value ? "inline" : "none",
        id: `${data.id}_cross2`,
        class: "unclickable"
    }
}

function gui_tgl_text(data) {
    return gui_text(data);
}

function gui_tgl_update_cross(data) {
    configure_item(data.cross1, {
        display: data.value ? "inline" : "none"
    });
    configure_item(data.cross2, {
        display: data.value ? "inline" : "none"
    });
}

function gui_tgl_onmousedown(data) {
    data.value = data.value ? 0 : data.default_value;
    gui_tgl_update_cross(data);
    gui_send('Float', data.send, data.value);
}

// silder: vsl, hsl
function gui_slider_rect(data) {
    let x = data.x_pos;
    let y = data.y_pos;
    let width = data.width;
    let height = data.height;
    if (data.type === "vsl") {
        //y -= 2; // note: modified
        height += 5;
    }
    else {
        //x -= 3; // note: modified
        width += 5;
    }
    return {
        x: x,
        y: y,
        width: width,
        height: height,
        fill: colfromload(data.bg_color),
        id: `${data.id}_rect`,
        class: "border clickable"
    }
}

function gui_slider_indicator_points(data) {
    let x1 = data.x_pos;
    let y1 = data.y_pos;
    let x2 = x1 + data.width;
    let y2 = y1 + data.height;
    let r = 0;
    let p1 = 0;
    let p2 = 0;
    let p3 = 0;
    let p4 = 0;
    if (data.type === "vsl") {
        //y1 -= 2; // note: modified
        y2 += 5;
        r = y2 - 2.5 - (data.value + 50) / 100;
        p1 = x1 + 2 * 0.75; // note: modified
        p2 = r;
        p3 = x2 - 2 * 0.75; // note: modified
        p4 = r;
    }
    else {
        //x1 -= 3; // note: modified
        r = x1 + 2.5 + (data.value + 50) / 100;
        p1 = r;
        p2 = y1 + 2 * 0.75; // note: modified
        p3 = r;
        p4 = y2 - 2 * 0.75; // note: modified
    }
    return {
        x1: p1,
        y1: p2,
        x2: p3,
        y2: p4
    }
}

function gui_slider_indicator(data) {
    const p = gui_slider_indicator_points(data);
    return {
        x1: p.x1,
        y1: p.y1,
        x2: p.x2,
        y2: p.y2,
        stroke: colfromload(data.fg_color),
        "stroke-width": 3,
        fill: "none",
        id: `${data.id}_indicator`,
        class: "unclickable"
    }
}

function gui_slider_text(data) {
    return gui_text(data);
}

function gui_slider_update_indicator(data) {
    const p = gui_slider_indicator_points(data);
    configure_item(data.indicator, {
        x1: p.x1,
        y1: p.y1,
        x2: p.x2,
        y2: p.y2
    });
}

// slider events
const touches = {};

function gui_slider_check_minmax(data) {
    if (data.log) {
        if (!data.bottom && !data.top) {
            data.top = 1;
        }
        if (data.top > 0) {
            if (data.bottom <= 0) {
                data.bottom = 0.01 * data.top;
            }
        }
        else {
            if (data.bottom > 0) {
                data.top = 0.01 * data.bottom;
            }
        }
    }
    data.reverse = data.bottom > data.top;
    const w = data.type === "vsl" ? data.height : data.width;
    if (data.log) {
        data.k = Math.log(data.top / data.bottom) / (w - 1);
    }
    else {
        data.k = (data.top - data.bottom) / (w - 1);
    }
}

function gui_slider_set(data, f) {
    let g = 0;
    if (data.reverse) {
        f = Math.max(Math.min(f, data.bottom), data.top);
    }
    else {
        f = Math.max(Math.min(f, data.top), data.bottom);
    }
    if (data.log) {
        g = Math.log(f / data.bottom) / data.k;
    }
    else {
        g = (f - data.bottom) / data.k;
    }
    data.value = 100 * g + 0.49999;
    gui_slider_update_indicator(data);
}

function gui_slider_bang(data) {
    let out = 0;
    if (data.log) {
        out = data.bottom * Math.exp(data.k * data.value * 0.01);
    }
    else {
        out = data.value * 0.01 * data.k + data.bottom;
    }
    if (data.reverse) {
        out = Math.max(Math.min(out, data.bottom), data.top);
    }
    else {
        out = Math.max(Math.min(out, data.top), data.bottom);
    }
    if (out < 1.0e-10 && out > -1.0e-10) {
        out = 0;
    }
    gui_send('Float', data.send, out);
}

function gui_slider_onmousedown(data, e, id) {
    const p = gui_mousepoint(e, data.canvas);
    if (!data.steady_on_click) {
        if (data.type === "vsl") {
            data.value = Math.max(Math.min(100 * (data.height + data.y_pos - p.y), (data.height - 1) * 100), 0);
        }
        else {
            data.value = Math.max(Math.min(100 * (p.x - data.x_pos), (data.width - 1) * 100), 0);
        }
        gui_slider_update_indicator(data);
    }
    gui_slider_bang(data);
    touches[id] = {
        data: data,
        point: p,
        shift: keyDown['Shift'],
        value: data.value
    };
}

function gui_slider_onmousemove(e, id) {
    if (id in touches) {
        const p = gui_mousepoint(e, touches[id].data.canvas);
        if(keyDown['Shift'] != touches[id].shift) {
            touches[id].point = p;
            touches[id].value = touches[id].data.value
            touches[id].shift = keyDown['Shift'];
        }

        const { data, point, value } = touches[id];
        const factor = keyDown['Shift'] ? .01 : 1;
        if (data.type === "vsl") {
            data.value = Math.max(Math.min(value + factor * (point.y - p.y) * 100, (data.height - 1) * 100), 0);
        }
        else {
            data.value = Math.max(Math.min(value + factor * (p.x - point.x) * 100, (data.width - 1) * 100), 0);
        }
        gui_slider_update_indicator(data);
        gui_slider_bang(data);
    }
}

function gui_slider_onmouseup(id) {
    if (id in touches) {
        delete touches[id];
    }
}

// radio: vradio, hradio
function gui_radio_rect(data) {
    let width = data.size;
    let height = data.size;
    if (data.type === "vradio") {
        height *= data.number;
    }
    else {
        width *= data.number;
    }
    return {
        x: data.x_pos,
        y: data.y_pos,
        width: width,
        height: height,
        fill: colfromload(data.bg_color),
        id: `${data.id}_rect`,
        class: "border clickable"
    }
}

function gui_radio_line(data, p1, p2, p3, p4, button_index) {
    return {
        x1: p1,
        y1: p2,
        x2: p3,
        y2: p4,
        id: `${data.id}_line_${button_index}`,
        class: "border unclickable"
    }
}

function gui_radio_button(data, p1, p2, p3, p4, button_index, state) {
    return {
        x: p1,
        y: p2,
        width: p3 - p1,
        height: p4 - p2,
        fill: colfromload(data.fg_color),
        stroke: colfromload(data.fg_color),
        display: state ? "inline" : "none",
        id: `${data.id}_button_${button_index}`,
        class: "unclickable"
    }
}

function gui_radio_remove_lines_buttons(data) {
    for (const line of data.lines) {
        line.parentNode.removeChild(line);
    }
    for (const button of data.buttons) {
        button.parentNode.removeChild(button);
    }
}

function gui_radio_lines_buttons(data, is_creating) {
    const n = data.number;
    const d = data.size;
    const s = d / 4;
    const x1 = data.x_pos;
    const y1 = data.y_pos;
    let xi = x1;
    let yi = y1;
    const on = data.value;
    data.drawn = on;
    for (let i = 0; i < n; i++) {
        if (data.type === "vradio") {
            if (is_creating) {
                if (i) {
                    const line = create_item("line", gui_radio_line(data, x1, yi, x1 + d, yi, i), data.canvas);
                    data.lines.push(line);
                }
                const button = create_item("rect", gui_radio_button(data, x1 + s, yi + s, x1 + d - s, yi + d - s, i, on === i), data.canvas);
                data.buttons.push(button);
            }
            else {
                if (i) {
                    configure_item(data.lines[i - 1], gui_radio_line(data, x1, yi, x1 + d, yi, i));
                }
                configure_item(data.buttons[i], gui_radio_button(data, x1 + s, yi + s, x1 + d - s, yi + d - s, i, on === i));
            }
            yi += d;
        }
        else {
            if (is_creating) {
                if (i) {
                    const line = create_item("line", gui_radio_line(data, xi, y1, xi, y1 + d, i), data.canvas);
                    data.lines.push(line);
                }
                const button = create_item("rect", gui_radio_button(data, xi + s, y1 + s, xi + d - s, yi + d - s, i, on === i), data.canvas);
                data.buttons.push(button);
            }
            else {
                if (i) {
                    configure_item(data.lines[i - 1], gui_radio_line(data, xi, y1, xi, y1 + d, i));
                }
                configure_item(data.buttons[i], gui_radio_button(data, xi + s, y1 + s, xi + d - s, yi + d - s, i, on === i));
            }
            xi += d;
        }
    }
}

function gui_radio_create_lines_buttons(data) {
    data.lines = [];
    data.buttons = [];
    gui_radio_lines_buttons(data, true);
}

function gui_radio_update_lines_buttons(data) {
    gui_radio_lines_buttons(data, false);
}

function gui_radio_text(data) {
    return gui_text(data);
}

function gui_radio_update_button(data) {
    configure_item(data.buttons[data.drawn], {
        display: "none"
    });
    configure_item(data.buttons[data.value], {
        fill: colfromload(data.fg_color),
        stroke: colfromload(data.fg_color),
        display: "inline"
    });
    data.drawn = data.value;
}

function gui_radio_onmousedown(data, e) {
    const p = gui_mousepoint(e, data.canvas);
    if (data.type === "vradio") {
        data.value = Math.floor((p.y - data.y_pos) / data.size);
    }
    else {
        data.value = Math.floor((p.x - data.x_pos) / data.size);
    }
    gui_radio_update_button(data);
    gui_send('Float', data.send, data.value);
}

//knob
function polarToCartesian(centerX, centerY, radius, angleInDegrees) {
    var angleInRadians = (angleInDegrees-90) * Math.PI / 180.0;

    return {
        x: centerX + (radius * Math.cos(angleInRadians)),
        y: centerY + (radius * Math.sin(angleInRadians))
    };
}
  
function describeArc(x, y, radius, startAngle, endAngle){
    var start = polarToCartesian(x, y, radius, endAngle);
    var end = polarToCartesian(x, y, radius, startAngle);

    var largeArcFlag = endAngle - startAngle <= 180 ? "0" : "1";
    var d = [
        "M", start.x, start.y, 
        "A", radius, radius, 0, largeArcFlag, 0, end.x, end.y
    ].join(" ");

    return d;       
}

function gui_knob_vto_gui(data) {
    if(data.logarithmic)
        return (Math.log10(data.value) - Math.log10(data.minimum)) / Math.log10(data.maximum/data.minimum);
    else
        return (data.value - data.minimum) / (data.maximum - data.minimum)
}
  
function gui_knob_text(data) {
    return gui_text(data);
}

function gui_knob_clicktarget(data) {
    return {
        x: data.x_pos,
        y: data.y_pos,
        width: data.size_x,
        height: data.size_y,
        fill: "#00000000"
    }
}

function gui_knob_circle(data) {
    return {
        stroke: "black",
        fill: "none",
        "stroke-width": data.off_width,
        "shape-rendering": "auto",
        "d": describeArc(data.x_pos + data.size_x/2, data.y_pos + data.size_y/2, data.size_x/2 - 1, 193, 528)
    };
}

function gui_knob_line(data) {
    let endPos = polarToCartesian(data.size_x/2, data.size_y/2, data.size_x/2, 193 + gui_knob_vto_gui(data) * (528 - 193));
    return { // indicator
        "stroke-width": data.dial_width,
        stroke: colfromload(data.fg_color),
        x1: data.x_pos + data.size_x/2,
        x2: data.x_pos + endPos.x,
        y1: data.y_pos + data.size_y/2,
        y2: data.y_pos + endPos.y
    }
}
function gui_knob_extracircle(data) {
    return {
        "knob_w": data.size_x,
        fill: "none",
        stroke: colfromload(data.bg_color),
        "stroke-width": data.on_width,
        "shape-rendering": "auto",
        "d": describeArc(data.x_pos + data.size_x/2, data.y_pos + data.size_y/2, data.size_x/2 - 1, 193, 193 + gui_knob_vto_gui(data) * (528 - 193)),
    }
}
function gui_knob_render(data) {
    configure_item(data.circle, gui_knob_circle(data));
    configure_item(data.extracircle, gui_knob_extracircle(data));
    configure_item(data.line, gui_knob_line(data));
    configure_item(data.text, gui_knob_text(data));
    configure_item(data.clicktarget, gui_knob_clicktarget(data));
    data.text.textContent = data.label;
}


const gui_knob_touches = {};
function gui_knob_onmousedown(data, e, id) {
    if(!data.interactive)
        return;

    const p = gui_mousepoint(e, data.canvas);
    if (!data.steady_on_click) {
        //Convert absolute coordinates to coordinates inside the knob (so that we can find the angle and calculate the desired value)
        let rx = p.x - data.x_pos - data.size_x/2, 
            ry = p.y - data.y_pos - data.size_y/2;
        //Find the angle of the mouse relative to the center of the knob
        let theta = Math.asin(ry / (rx**2 + ry**2) ** .5) * 180 / Math.PI;
        //Clamp the angle to 0 - 336 and convert that to a value between minimum and maximum. 
        let factor = Math.min(Math.max(rx > 0 ? 258 + theta : 78 - theta, 0), 336) / 336;
        if(data.logarithmic)
            data.value = Math.pow(10, Math.log10(data.minimum) + factor * (Math.log10(data.maximum/data.minimum)));
        else
            data.value = data.minimum + factor * (data.maximum - data.minimum);
        
        configure_item(data.extracircle, gui_knob_extracircle(data));
        configure_item(data.line, gui_knob_line(data));
        gui_send('Float', data.send, data.value);
    }
    gui_knob_touches[id] = {
        data: data,
        point: p,
        shift: keyDown['Shift'],
        value: data.value
    };
}

function gui_knob_onmousemove(e, id) {
    if (id in gui_knob_touches) {
        let p = gui_mousepoint(e, gui_knob_touches[id].data.canvas);
        if(keyDown['Shift'] != gui_knob_touches[id].shift) {
            gui_knob_touches[id].point = p;
            gui_knob_touches[id].value = gui_knob_touches[id].data.value;
            gui_knob_touches[id].shift = keyDown['Shift'];
        }

        const { data, point, value } = gui_knob_touches[id];
        const factor = keyDown['Shift'] ? .01 : 1;
        if(data.logarithmic)
            data.value = Math.pow(10,Math.log10(value) + factor * (point.y - p.y) / data.size_y * Math.log10(data.maximum/data.minimum));
        else
            data.value = value + factor * (point.y - p.y) / data.size_y * (data.maximum - data.minimum);
        data.value = Math.max(Math.min(data.value,data.maximum),data.minimum);

        configure_item(data.extracircle, gui_knob_extracircle(data));
        configure_item(data.line, gui_knob_line(data));
        gui_send('Float', data.send, data.value);
    }
}

function gui_knob_onmouseup(id) {
    if (id in gui_knob_touches) {
        delete gui_knob_touches[id];
    }
}

//pddplink
function gui_link_open(link) {
    if(link.startsWith('http'))
        window.open(link);
    else if(link.slice(-3) == '.pd') {
        if(link.startsWith('/'))
            window.open(`${window.location.origin}/?url=${link}`);
        else { 
            window.open(`${window.location.origin}/?url=${window.location.href.split('url=')[1].split('/').slice(0,-1)}/${link}`);
        }
    }
    else
        window.open(link);
}


//VuMeter
function gui_vumeter_box(data) {
    return {
        x: data.x_pos,
        y: data.y_pos,
        width: data.width,
        height: data.height,
        stroke: '#000',
        fill: colfromload(data.bg_color),
        id: data.id + '_box',
    };
}
function gui_vumeter_col(i) {
    return `#${vu_colors[vu_colmap[vu_colmap.length - i - 2]].toString(16)}`;
}
function gui_vumeter_unitheight(data) {
    return Math.ceil((data.height - 2) / (vu_colmap.length - 3));
}
function gui_vumeter_index(value) {
    return vu_valmap[Math.round((Math.min(12,Math.max(-100,value)) + 100) * 2)];
}
function gui_vumeter_bars(data) {
    let result = [];
    let unitHeight = gui_vumeter_unitheight(data);
    let index = gui_vumeter_index(data.value);
    for(let i = 1; i < vu_colmap.length - 2; i++) {
        result.push({
            x1: data.x_pos + data.width * .25 + 1,
            x2: data.x_pos + data.width * .75,
            y1: data.y_pos + unitHeight * (i - .5), 
            y2: data.y_pos + unitHeight * (i - .5),
            stroke: gui_vumeter_col(i),
            'stroke-width': unitHeight - 1,
            id: `${data.id}_bar_${i}`,
            opacity: vu_colmap.length - i - 2 <= index ? 1 : 0
        });
    }
    return result;
}
function gui_vumeter_update_bars(data) {
    let bars = gui_vumeter_bars(data);
    for(let i=0; i<bars.length; i++)
        configure_item(data.bars[i], bars[i]);
}
function gui_vumeter_line(data) {
    let index = vu_colmap.length - 2 - gui_vumeter_index(data.peak);
    let unitHeight = gui_vumeter_unitheight(data);
    return {
        x1: data.x_pos + 1,
        y1: data.y_pos + unitHeight * (index - .5),
        x2: data.x_pos + data.width - 1,
        y2: data.y_pos + unitHeight * (index - .5),
        stroke: gui_vumeter_col(index),
        "stroke-width": unitHeight - 1,
        id: `${data.id}_line`
    }
}
function gui_vumeter_scale(data) {
    let result = [];
    let unitHeight = gui_vumeter_unitheight(data);
    for(let i = 0; i < (vu_colmap.length - 2) / 4; i++) {
        result.push({
            x: data.x_pos + data.width + 3,
            y: data.y_pos + unitHeight * i * 4 + 1,
            "font-family": iemgui_fontfamily(data.font),
            "font-weight": "normal",
            "font-size": `${data.fontsize}px`,
            fill: '#000',
            transform: `translate(0, ${data.fontsize / 2 * 0.6})`, // note: modified
            id: `${data.id}_scale_${i}`,
            class: 'unclickable',
            textContent: vu_scale_str[Math.ceil((vu_colmap.length - 2) / 4) - i - 1]
        });
    }
    return result;
}
function gui_vumeter_label(data) {
    return gui_text(data);
}
function gui_vumeter_render(data) {
    configure_item(data.box, gui_vumeter_box(data));
    configure_item(data.text, gui_vumeter_label(data));
    configure_item(data.line, gui_vumeter_line(data));
    let bars = gui_vumeter_bars(data);
    for(let i=0; i<data.bars.length; i++)
        configure_item(data.bars[i], bars[i]);
    let scales = gui_vumeter_scale(data);
    for(let i=0; i<data.scale.length; i++)
        configure_item(data.scale[i], scales[i]);
    for(let scale of data.scale)
        configure_item(scale, {display: data.showScale ? 'block' : 'none'});
}

//Arrays
const gui_array_touches = {};
function gui_canvas_drawTicks(data) {
    let coords = data.dimensions.coords;
    let xPath = '';
    for(let i = Math.floor((coords.l - coords.xticks.start) / coords.xticks.interval ); coords.xticks.start + coords.xticks.interval * i < coords.r; i++)
        if([coords.l, coords.r].includes(coords.xticks.start + coords.xticks.interval * i) == false)
            xPath += `
                    M ${coordToScreen(coords.l, coords.r, coords.w, coords.xticks.start + coords.xticks.interval * i)} ${coordToScreen(coords.t, coords.b, coords.h, coords.b)} 
                    v ${(i % coords.xticks.big) ? -2 : -4}
                    M ${coordToScreen(coords.l, coords.r, coords.w, coords.xticks.start + coords.xticks.interval * i)} ${coordToScreen(coords.t, coords.b, coords.h, coords.t)} 
                    v ${(i % coords.xticks.big) ? 2 : 4}
                `;
    if(data.xTicks)
        data.canvas.removeChild(data.xTicks);
    data.xTicks = create_item("path", {
        stroke: "black",
        fill: "none",
        "stroke-width": 1,
        d: xPath
    }, data.canvas);

    let yPath = '';
    for(let i = Math.floor((coords.t - coords.yticks.start) / coords.yticks.interval ); coords.yticks.start + coords.yticks.interval * i < coords.b; i++)
        if([coords.t, coords.b].includes(coords.yticks.start + coords.yticks.interval * i) == false)
            yPath += `
                    M ${coordToScreen(coords.l, coords.r, coords.w, coords.r)} ${coordToScreen(coords.t, coords.b, coords.h, coords.yticks.start + coords.yticks.interval * i)} 
                    h ${(i % coords.yticks.big) ? -2 : -4}
                    M ${coordToScreen(coords.l, coords.r, coords.w, coords.l)} ${coordToScreen(coords.t, coords.b, coords.h, coords.yticks.start + coords.yticks.interval * i)} 
                    h ${(i % coords.yticks.big) ? 2 : 4}
                `;
    if(data.yTicks)
        data.canvas.removeChild(data.yTicks);
    data.yTicks = create_item("path", {
        stroke: "black",
        fill: "none",
        "stroke-width": 1,
        d: yPath
    }, data.canvas);
}
function gui_canvas_drawLabels(data) {
    let coords = data.dimensions.coords;

    if(!data.xLabels)
        data.xLabels = [];
    while(data.xLabels.length)
        data.canvas.removeChild(data.xLabels.pop());
    for(let label of coords.xlabels.labels) {
        data.xLabels.push(create_item("text", {
            x: coordToScreen(coords.l, coords.r, coords.w, label) - data.fontSize * .3,
            y: coordToScreen(coords.t, coords.b, coords.h, coords.xlabels.pos) + data.fontSize * 1.5,
            "font-family": iemgui_fontfamily(0),
            "font-weight": "normal",
            "font-size": `${data.fontSize}px`,
            fill: 'black',
            id: `${data.id}_xlabel_${label}`,
            class: "unclickable"
        }, data.canvas));
        data.xLabels.at(-1).textContent = label;
    }

    if(!data.yLabels)
        data.yLabels = [];
    while(data.yLabels.length)
        data.canvas.removeChild(data.yLabels.pop());
    for(let label of coords.ylabels.labels) {
        let labelText = create_item("text", {
            y: coordToScreen(coords.t, coords.b, coords.h, label) + data.fontSize * .3,
            "font-family": iemgui_fontfamily(0),
            "font-weight": "normal",
            "font-size": `${data.fontSize}px`,
            fill: 'black',
            id: `${data.id}_ylabel_${label}`,
            class: "unclickable"
        }, data.canvas);
        labelText.textContent = label;
        configure_item(labelText, {
            x: coordToScreen(coords.l, coords.r, coords.w, coords.ylabels.pos) - labelText.getComputedTextLength(),
        });
        data.yLabels.push(labelText);
    }
}

function gui_array_onmousedown(data, e, id) {
    let p = gui_mousepoint(e, data.canvas);
    let x = Math.floor(screenToCoord(data.coords.l, data.coords.r, data.coords.w, p.x)) + 1;
    let y = screenToCoord(data.coords.t, data.coords.b, data.coords.h, p.y);
    if(x < data.nums.length - 1) {
        data.nums[x] = Math.max(Math.min(data.coords.t, data.coords.b), Math.min(Math.max(data.coords.t, data.coords.b), y));
        data.lastx = x;
        gui_send("List", [data.receive[0]], [x, data.nums[x]]);
        data.redraw();
        if(!gui_array_touches[id])
            gui_array_touches[id] = [];
        gui_array_touches[id].push(data);
    }
}
function gui_array_onmousemove(e, id) {
    if(id in gui_array_touches) {
        for(let data of gui_array_touches[id]) {
            let p = gui_mousepoint(e, data.canvas);
            p.x = Math.floor(screenToCoord(data.coords.l, data.coords.r, data.coords.w, p.x)) + 1;
            p.y = screenToCoord(data.coords.t, data.coords.b, data.coords.h, p.y);
            let start = data.lastx;
            let end = Math.min(data.nums.length - 1, Math.max(0, p.x));
            let refy = (start == data.nums.length - 1 && end >= start) ? p.y : data.nums[data.lastx];
            if(start == end)
                data.nums[start] = Math.max(Math.min(data.coords.t, data.coords.b), Math.min(Math.max(data.coords.t, data.coords.b), (refy + (p.y - refy))));
            else
                for(let i = Math.min(start, end); i <= Math.max(start,end); i++)
                    data.nums[i] = Math.max(Math.min(data.coords.t, data.coords.b), Math.min(Math.max(data.coords.t, data.coords.b), (refy + (p.y - refy) * (Math.abs(i - start) / Math.max(1,Math.abs(end - start))))));
            data.lastx = end;

            let nums = [];
            nums.push(Math.min(start, end));
            for(let i = Math.min(start,end); i <= Math.max(start,end); i++)
                nums.push(data.nums[i]);
            gui_send("List",[data.receive[0]], nums);

            data.redraw();
        }
    }
}
function gui_array_onmouseup(id) {
    if(id in gui_array_touches) {
        delete gui_array_touches[id];
    }
}


let gui_atom_touches = {};
function gui_atom_update(data) {
    gui_atom_settext(data, '' + data.value);
    gui_send(data.type === 'floatatom' ? 'Float' : 'Symbol', data.send, data.value);
}
function gui_atom_onmousedown(data, e, id) {
    if(data.interactive) {
        if(data.type === 'floatatom' && (keyDown['Meta'] || keyDown['Control'])) {
            if(data.value == 0)
                data.value = data.lastNonZero;
            else {
                data.lastNonZero = data.value;
                data.value = 0;
            }
            gui_atom_update(data);
        } else {
            setKeyboardFocus(data, data.exclusive);
            configure_item(data.svgText, {fill: '#ff0000'});
            if(data.type === 'floatatom') {
                gui_atom_touches[id] = {
                    data,
                    pos: gui_mousepoint(e, data.canvas),
                    value: data.value,
                    shift: keyDown['Shift']
                }
            }
        }
    }
}
function gui_atom_onmousemove(e, id) {
    if(id in gui_atom_touches) {
        let curPos = gui_mousepoint(e, gui_atom_touches[id].data.canvas);
        if(keyDown['Shift'] != gui_atom_touches[id].shift) {
            gui_atom_touches[id].pos = curPos;
            gui_atom_touches[id].value = gui_atom_touches[id].data.value
            gui_atom_touches[id].shift = keyDown['Shift'];
        }

        let {data, pos, value} = gui_atom_touches[id];
        if(keyDown['Shift'])
            data.dirtyValue = '' + (value - Math.round(curPos.y - pos.y) / 100);
        else
            data.dirtyValue = '' + (value - Math.round(curPos.y - pos.y));
        gui_atom_commit(data, true);
    }
}
function gui_atom_onmouseup(id) {
    if(id in gui_atom_touches)
        delete gui_atom_touches[id];
}
function gui_atom_settext(data, text) {
    if(data.dirtyValue === undefined && data.type === 'floatatom')
        data.svgText.textContent = (Math.round(+text).toString().length > +data.width) ? (data.value > 0 ? '+' : '-') : text.slice(0, +data.width);
    else
        data.svgText.textContent = text.length > +data.width ? text.slice(0, +data.width - 1) + '>' : text;
}
function gui_atom_keydown(data, e) {
    if(e.key.length == 1)
        if(e.key.match(data.regex))
            data.dirtyValue = (data.dirtyValue || '') + e.key;
    if(e.key === 'Backspace')
        data.dirtyValue = (data.dirtyValue || '').slice(0,-1);
    if(e.key === 'Enter') {
        gui_atom_commit(data);
        gui_atom_settext(data, '' + data.value);
    }

    if(data.dirtyValue !== undefined)
        gui_atom_settext(data, data.dirtyValue + (new Array(Math.max(0,3 - data.dirtyValue.length))).fill('.').join(''));
}
function gui_atom_commit(data, mousing) {
    if(!data.dirtyValue)
        data.dirtyValue = '' + data.value;

    if(data.type === 'floatatom') {
        data.value = Math.round(+(data.dirtyValue.match(data.regex)[0]) * 100000) / 100000;
        if(data.typedMinMax || mousing && !(data.min == 0 && data.max == 0))
            data.value = Math.max(data.min, Math.min(data.max, data.value));
    }
    else
        data.value = data.dirtyValue;

    delete data.dirtyValue;
    gui_atom_update(data);
}
function gui_atom_losefocus(data) {
    configure_item(data.svgText, {fill: '#000000'});
    delete data.dirtyValue;
    gui_atom_settext(data, '' + data.value);
}

//dropdown
let gui_dropdown_dropdowns = [];
function gui_dropdown_onmousedown(data, e, id) {
    data.showOptions = true;
    data.render();
}
function gui_dropdown_send(data) {
    gui_send(data.outputValue && (typeof data.options[data.selectedOption] === 'string') ? 'Symbol' : 'Float', data.send, data.outputValue ? data.options[data.selectedOption] : data.selectedOption);
}
function gui_dropdown_option_onmousedown(data, i, e, id) {
    data.selectedOption = i;
    data.showOptions = false;
    data.render();
    gui_dropdown_send(data);
}
function gui_dropdown_option_onmousemove(e, id) {
    for(let dropdown of gui_dropdown_dropdowns) {
        for(let option of dropdown.optionsGUI) {
            let bounds = option.box.getBoundingClientRect();
            if( e.pageX > bounds.left && e.pageX < bounds.right && e.pageY > bounds.top && e.pageY < bounds.bottom )
                configure_item(option.box, { fill: "#c3c3c3"});
            else
                configure_item(option.box, { fill: "#eeeeee"});
        }
    }
}


//nbx
let gui_nbx_touches = {};
function gui_nbx_update(data) {
    gui_nbx_settext(data, '' + data.value);
    gui_send('Float', data.send, data.value);
}
function gui_nbx_onmousedown(data, e, id) {
    if(data.interactive) {
        if(keyDown['Shift']) {
            data.dirtyValue = '' + data.value;
            gui_nbx_settext(data, '' + data.value);
        }
        setKeyboardFocus(data, data.exclusive);
        data.focusTimeout = setTimeout(setKeyboardFocus, 3000, null);
        configure_item(data.svgText, {fill: '#ff0000'});
        gui_nbx_touches[id] = {
            data,
            pos: gui_mousepoint(e, data.canvas),
            value: data.value,
            shift: keyDown['Shift']
        }
    }
}
function gui_nbx_onmousemove(e, id) {
    if(id in gui_nbx_touches) {
        let curPos = gui_mousepoint(e, gui_nbx_touches[id].data.canvas);
        if(keyDown['Shift'] != gui_nbx_touches[id].shift) {
            gui_nbx_touches[id].pos = curPos;
            gui_nbx_touches[id].value = gui_nbx_touches[id].data.value
            gui_nbx_touches[id].shift = keyDown['Shift'];
        }

        let {data, pos, value} = gui_nbx_touches[id];
        let factor = keyDown['Shift'] ? .01 : 1;
        if(data.logarithmic)
            data.dirtyValue = '' + (value * Math.pow(Math.exp(Math.log(data.max/data.min)/data.logHeight), factor * (pos.y - curPos.y)));
        else
            data.dirtyValue = '' + (value + factor * Math.round((pos.y - curPos.y)));

        gui_nbx_commit(data);
        gui_nbx_settext(data, '' + data.value, true);
    }
}
function gui_nbx_onmouseup(id) {
    if(id in gui_nbx_touches)
        delete gui_nbx_touches[id];
}
function gui_nbx_settext(data, text, mousing) {
    if(mousing)
        data.svgText.textContent = text.slice(Math.max(0, text.length - +data.width));
    else if(data.dirtyValue === undefined) {
        if(Math.round(+text).toString().length > +data.width)
            data.svgText.textContent = data.value > 0 ? '+' : '-';
        else
            data.svgText.textContent = text.slice(0,+data.width);
    } else
        data.svgText.textContent = text.slice(Math.max(0, text.length - +data.width + 1)) + '>';
}
function gui_nbx_keydown(data, e) {
    if(data.focusTimeout) {
        clearTimeout(data.focusTimeout);
        data.focusTimeout = setTimeout(setKeyboardFocus, 3000, null);
    }
    if(e.key.length == 1)
        if(e.key.match(data.regex))
            data.dirtyValue = (data.dirtyValue || '') + e.key;
    if(e.key === 'Backspace')
        data.dirtyValue = (data.dirtyValue || '').slice(0,-1);
    if(e.key === 'Enter')
        gui_nbx_commit(data);
    if(e.key === 'ArrowUp') {
        data.dirtyValue = '' + (+(data.dirtyValue || '0') + (keyDown['Shift'] ? .01 : 1));
        if(data.arrowUpdate) {
            gui_nbx_commit(data);
            data.dirtyValue = '' + data.value;
            return;
        }
    }
    if(e.key === 'ArrowDown') {
        data.dirtyValue = '' + (+(data.dirtyValue || '0') - (keyDown['Shift'] ? .01 : 1));
        if(data.arrowUpdate) {
            gui_nbx_commit(data);
            data.dirtyValue = '' + data.value;
            return;
        }
    }
    if(data.dirtyValue !== undefined)
        gui_nbx_settext(data, data.dirtyValue);
}
function gui_nbx_commit(data) {
    if(!data.dirtyValue)
        data.dirtyValue = '' + data.value;

    data.value = Math.round(+(data.dirtyValue.match(data.regex)[0]) * 100000) / 100000;
    if(!(data.min == 0 && data.max == 0))
        data.value = Math.max(data.min, Math.min(data.max, data.value));

    delete data.dirtyValue;
    gui_nbx_update(data);
}
function gui_nbx_losefocus(data) {
    configure_item(data.svgText, {fill: data.foreground_color});
    delete data.dirtyValue;
    gui_nbx_settext(data, '' + data.value);
}


//Image

const gui_image_touches = {};
let gui_image_keys = {};
function gui_image_onmousedown(data, e, id) {
    const p = gui_roundedmousepoint(e, data.canvas);
    gui_image_touches[id] = {
        data: data,
        p,
    };
    if(data.clickBehavior === 1)
        gui_send('Bang', data.send);
    if(data.clickBehavior === 2)
        gui_send('List', data.send, [1, data.x_pos, data.y_pos, p.x - data.x_pos, p.y - data.y_pos]);
}
function gui_image_onmousemove(e, id) {
    if (id in gui_image_touches) {
        const { data } = gui_image_touches[id];
        const p = gui_roundedmousepoint(e, data.canvas);
        gui_image_touches[id].p = p;
        if(data.clickBehavior === 2)
            gui_send('List', data.send, [1, data.x_pos, data.y_pos, p.x - data.x_pos, p.y - data.y_pos]);
    }
}
function gui_image_onmouseup(id) {
    if (id in gui_image_touches) {
        const { data, p } = gui_image_touches[id];
        if(data.clickBehavior === 2)
            gui_send('List', data.send, [0, data.x_pos, data.y_pos, p.x - data.x_pos, p.y - data.y_pos]);
        delete gui_image_touches[id];
    }
}

// window

const gui_window_touches = {};
function gui_window_onmousedown(data, e, id) {
    const p = gui_roundedmousepoint(e, document.getElementById('canvas'));
    gui_window_touches[id] = {
        data: data,
        startMouse: p,
        startWindow: {
            x: data.windowX,
            y: data.windowY
        }
    };
}
function gui_window_onmousemove(e, id) {
    if (id in gui_window_touches) {
        const { data, startMouse, startWindow } = gui_window_touches[id];
        const p = gui_roundedmousepoint(e, document.getElementById('canvas'));
        data.windowX = startWindow.x + p.x - startMouse.x;
        data.windowY = startWindow.y + p.y - startMouse.y;
        data.updateWindow();
    }
}
function gui_window_onmouseup(id) {
    if (id in gui_window_touches)
        delete gui_window_touches[id];
}


// keyboard events
let keyboardFocus = { data: null, exclusive: false, current: false};
let inputListeners = [];
let keyDown = {}
function onKeyDown(e) {
    if(keyboardFocus.data?.onKeyDown)
        keyboardFocus.data.onKeyDown(keyboardFocus.data, e);
    if(keyboardFocus.exclusive == false) {
        keyDown[e.key] = true;
        for(let listener of inputListeners) {
            if(listener.onKeyDown)
                listener.onKeyDown(e);
            if(listener.onKeyUp && e.repeat)
                listener.onKeyUp(e);
        }
    }
}
function onKeyUp(e) {
    if(keyboardFocus.data?.onKeyUp)
        keyboardFocus.data.onKeyUp(keyboardFocus.data, e);
    keyDown[e.key] = false;
    if(keyboardFocus.exclusive == false) {
        for(let listener of inputListeners)
            if(listener.onKeyUp)
                listener.onKeyUp(e);
    }
}
function setKeyboardFocus(data, exclusive) {
    for(let key in keyDown) {
        if(exclusive)
            if(keyDown[key])
                onKeyUp({key});
        if(keyboardFocus?.data?.onKeyUp)
            keyboardFocus.data.onKeyUp(keyboardFocus.data, {key});
    }
    if(keyboardFocus?.data?.onLoseFocus)
        keyboardFocus.data.onLoseFocus(keyboardFocus.data);
    keyboardFocus.data = data;
    keyboardFocus.exclusive = exclusive;
    keyboardFocus.current = true;
}
function onMouseDown(e) {
    if(keyboardFocus.current == false)
        setKeyboardFocus(null, false);
    keyboardFocus.current = false;

    for(let listener of inputListeners)
        if(listener.onMouseDown)
            listener.onMouseDown(e);
}
function onMouseUp(e) {
    for(let listener of inputListeners)
        if(listener.onMouseUp)
            listener.onMouseUp(e);
}
function onMouseMove(e) {
    for(let listener of inputListeners)
        if(listener.onMouseMove)
            listener.onMouseMove(e);
}

// drag events
if (isMobile) {
    window.addEventListener("touchmove", function (e) {
        e = e || window.event;
        for (const touch of e.changedTouches) {
            gui_slider_onmousemove(touch, touch.identifier);
            gui_knob_onmousemove(touch, touch.identifier);
            gui_array_onmousemove(touch, touch.identifier);
            gui_atom_onmousemove(touch, touch.identifier);
            gui_nbx_onmousemove(touch, touch.identifier);
            gui_image_onmousemove(touch, touch.identifier);
            gui_window_onmousemove(touch, touch.identifier);
        }
    });
    window.addEventListener("touchend", function (e) {
        e = e || window.event;
        for (const touch of e.changedTouches) {
            gui_slider_onmouseup(touch.identifier);
            gui_knob_onmouseup(touch.identifier);
            gui_array_onmouseup(touch.identifier);
            gui_atom_onmouseup(touch.identifier);
            gui_nbx_onmouseup(touch.identifier);
            gui_image_onmouseup(touch.identifier);
            gui_window_onmouseup(touch.identifier);
        }
    });
    window.addEventListener("touchcancel", function (e) {
        e = e || window.event;
        for (const touch of e.changedTouches) {
            gui_slider_onmouseup(touch.identifier);
            gui_knob_onmouseup(touch.identifier);
            gui_array_onmouseup(touch.identifier);
            gui_atom_onmouseup(touch.identifier);
            gui_nbx_onmouseup(touch.identifier);
            gui_image_onmouseup(touch.identifier);
            gui_window_onmouseup(touch.identifier);
        }
    });
}
else {
    window.addEventListener("mousemove", function (e) {
        e = e || window.event;
        gui_slider_onmousemove(e, 0);
        gui_knob_onmousemove(e, 0);
        gui_array_onmousemove(e, 0);
        gui_atom_onmousemove(e,0);
        gui_nbx_onmousemove(e,0);
        gui_image_onmousemove(e,0);
        gui_window_onmousemove(e,0);
        gui_dropdown_option_onmousemove(e, 0);
    });
    window.addEventListener("mouseup", function (e) {
        gui_slider_onmouseup(0);
        gui_knob_onmouseup(0);
        gui_array_onmouseup(0);
        gui_atom_onmouseup(0);
        gui_nbx_onmouseup(0);
        gui_image_onmouseup(0);
        gui_window_onmouseup(0);
    });
    window.addEventListener("mouseleave", function (e) {
        gui_slider_onmouseup(0);
        gui_knob_onmouseup(0);
        gui_array_onmouseup(0);
        gui_atom_onmouseup(0);
        gui_nbx_onmouseup(0);
        gui_image_onmouseup(0);
        gui_window_onmouseup(0);
    });
}

// cnv
function gui_cnv_visible_rect(data) {
    return {
        x: data.x_pos,
        y: data.y_pos,
        width: data.width,
        height: data.height,
        fill: colfromload(data.bg_color),
        stroke: colfromload(data.bg_color),
        id: `${data.id}_visible_rect`,
        class: "unclickable"
    }
}

function gui_cnv_selectable_rect(data) {
    return {
        x: data.x_pos,
        y: data.y_pos,
        width: data.size,
        height: data.size,
        fill: "none",
        stroke: colfromload(data.bg_color),
        id: `${data.id}_selectable_rect`,
        class: "unclickable"
    }
}

function gui_cnv_text(data) {
    return gui_text(data);
}

// text
function gobj_font_y_kludge(fontsize) {
    switch (fontsize) {
        case 8: return -0.5;
        case 10: return -1;
        case 12: return -1;
        case 16: return -1.5;
        case 24: return -3;
        case 36: return -6;
        default: return 0;
    }
}

let font_engine_sanity = false;

function set_font_engine_sanity() {
    const canvas = document.createElement("canvas"),
        ctx = canvas.getContext("2d"),
        test_text = "struct theremin float x float y";
    canvas.id = "font_sanity_checker_canvas";
    document.body.appendChild(canvas);
    ctx.font = "11.65px DejaVu Sans Mono";
    if (Math.floor(ctx.measureText(test_text).width) <= 217) {
        font_engine_sanity = true;
    } else {
        font_engine_sanity = false;
    }
    canvas.parentNode.removeChild(canvas);
}
set_font_engine_sanity();

function font_stack_is_maintained_by_troglodytes() {
    return !font_engine_sanity;
}

function font_map() {
    return {
        // pd_size: gui_size
        8: 8.33,
        12: 11.65,
        16: 16.65,
        24: 23.3,
        36: 36.6
    };
}

function suboptimal_font_map() {
    return {
        // pd_size: gui_size
        8: 8.45,
        12: 11.4,
        16: 16.45,
        24: 23.3,
        36: 36
    }
}

function font_height_map() {
    return {
        // fontsize: fontheight 
        8: 11,
        10: 13,
        12: 16,
        16: 19,
        24: 29,
        36: 44
    };
}

function gobj_fontsize_kludge(fontsize, return_type) {
    // These were tested on an X60 running Trisquel (based
    // on Ubuntu 14.04)
    var ret, prop,
        fmap = font_stack_is_maintained_by_troglodytes() ?
            suboptimal_font_map() : font_map();
    if (return_type === "gui") {
        ret = fmap[fontsize];
        return ret ? ret : fontsize;
    } else {
        for (prop in fmap) {
            if (fmap.hasOwnProperty(prop)) {
                if (fmap[prop] == fontsize) {
                    return +prop;
                }
            }
        }
        return fontsize;
    }
}

function pd_fontsize_to_gui_fontsize(fontsize) {
    return gobj_fontsize_kludge(fontsize, "gui");
}

function gui_text_text(data, line_index, fontSize) {
    const left_margin = 2;
    const fmap = font_height_map();
    const font_height = fmap[fontSize] * (line_index + 1);
    return {
        transform: `translate(${left_margin - 0.5})`,
        x: data.x_pos,
        y: data.y_pos + font_height + gobj_font_y_kludge(fontSize),
        "shape-rendering": "crispEdges",
        "font-size": pd_fontsize_to_gui_fontsize(fontSize) + "px",
        "font-weight": "normal",
        id: `${data.id}_text_${line_index}`,
        class: "unclickable"
    }
}

//--------------------- patch handling ----------------------------
async function openPatch(content, filename) {
    console.log(`Loading Patch: ${filename}`);
    document.title=filename;

    document.getElementById('loadingstage').innerHTML=`Fetching Dependancies`;
    await new Promise(Resolve => setTimeout(Resolve, 10));
    let abstractions = {};
    let fetchAbstractions = async(content, path) => {
        let missingAbstractions = [... new Set(content.split(';\n').filter( line => line.startsWith('#X obj') && !known_objects.includes(line.split(' ')[4]) ).map( line => line.split(' ')[4]))];
        let abstractionData = await Promise.all(missingAbstractions.map(async(obj) => (await getPatchData(`${((new URLSearchParams(window.location.search)).get('url')||'').split('/').slice(0,-1).join('/')}/${path}${obj}.pd`))));
        let promises = [];
        for(let abstraction in missingAbstractions) {
            if(abstractionData[abstraction].content.startsWith('#') && !abstractions[missingAbstractions[abstraction]]) {
                abstractions[missingAbstractions[abstraction]] = abstractionData[abstraction].content;
                promises.push(fetchAbstractions(abstractionData[abstraction].content, missingAbstractions[abstraction].split('/').slice(0,-1).join('/')+'/'));
            }
        }
        searchPaths = [...new Set([...searchPaths, path])];
        await Promise.all(promises);
    }
    await fetchAbstractions(content,'/');

    document.getElementById('loadingstage').innerHTML=`Parsing Patch`;
    await new Promise(Resolve => setTimeout(Resolve, 10));

    let start = Date.now();

    document.removeEventListener('keydown', onKeyDown);
    document.removeEventListener('keyup', onKeyUp);
    document.removeEventListener('mousemove', onMouseMove);
    document.removeEventListener('mousedown', onMouseDown);
    document.removeEventListener('mouseup', onMouseUp);
    document.addEventListener('keydown', onKeyDown);
    document.addEventListener('keyup', onKeyUp);
    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mousedown', onMouseDown);
    document.addEventListener('mouseup', onMouseUp);
    
    let rootCanvas = document.getElementById('canvas');
    while (rootCanvas.lastChild) // clear svg
        rootCanvas.removeChild(rootCanvas.lastChild);
        
    Module.pd.unsubscribeAll();
    subscribedData = {};
    
    let maxNumInChannels = 0;
    let nextHTMLID = 0;       //This is used to assign unique IDs to gui objects so that their subscriptions can be managed
    let nextPatchID = 1003;   //This is used to assign patch IDs to subpatches (which will be collapsed by the web parser)
    let nextLayerID = 0;      //This is used to assign unique IDs to layers to keep track of their objects
    let nextArgs = [];        //This is used to pass arguments from abstractions which are collapsed by the web parser
    let layers = [ { } ];   //Holds all the layers currently being processed. Used as a stack.

    let lines = content.split(';\n');
    for(let i = 0; i < lines.length; i++)
        while(lines[i].endsWith('\\'))
            lines.splice(i,2,`${lines[i].slice(0,-1)};\n${lines[i+1]}`);
    for (let i = 0; i < lines.length; i++) {
        let layer = layers.at(-1); //Shortcut for the current layer being processed so we aren't always writing layers.at(-1)

        //Some lexical lines are split between two physical lines in the file, so we must remove all newlines
        //Then we split by " " to seperate the line into arguments
        //We also replace escaped $ with real $ since $ has no meaning on the web version
        let argParts = lines[i].replace(/[\r\n]+/g, " ").trim().split(',');
        for(let i=0;i<argParts.length;i++)
            while(argParts[i].endsWith("\\"))
                argParts.splice(i,2,`${argParts[i].slice(0,-1)},${argParts[i+1]}`);
        let args = argParts[0].split(" ");
        let widthOverride = argParts[1]?.split(' ')?.[2];
        //Sometimes, we need a space in an argument, and this is signified by "\ ".
        //Therefore, we must combines the arguments which end with a \ and re-add the whitespace
        for(let i=0;i<args.length;i++)
            while(args[i].endsWith("\\"))
                args.splice(i,2,`${args[i].slice(0,-1)} ${args[i+1]}`);
        
        //We must process $0 for our frontend objects so that we give them the right sends/receives/ids
        //This is espeically important since we flatten all the canvases for compatibility with the emscriptem pd-l2ork
        //Symbolatoms use #0 instead of $0, so we have two cases
        if(args.slice(0,2).join(' ') == '#X symbolatom')
            args = args.map(arg => arg.replace(new RegExp(`(?<!\\\\)\\#0`,`g`),layer.patchID));
        else
            args = args.map(arg => arg.replace(new RegExp(`(?<!\\\\)\\\\\\$0`,`g`),layer.patchID));

        //If an object is not a message, we also process the $1+, filling in with the current canvas' arguments
        //This is especially important since we flatten all the canvases for compatibility with the emscriptem pd-l2ork
        if(args.slice(0,2).join(' ') != '#X msg')
            for(let i = 0; i < layer.args?.length; i++)
                for(let arg = 0; arg < args.length; arg++)
                    args[arg] = args[arg].replace(new RegExp(`(?<!\\\\)\\\\\\$${i+1}`,`g`),layer.args[i]);

        //If we are looking at something that can be connected with a wire, increment the wire counter
        if(object_types.find(type=>lines[i].startsWith(type)))
            layer.nextGUIID++;
        lines[i] = args.map(arg => arg.replace(/ /g,'\\ ')).join(' ').replace(/,/g,'\\,')+(argParts.length > 1 ? ','+argParts.slice(1).map(arg=>arg.replace(/,/g,'\\,')).join(',') : '');
        //Now we switch based on the type of line (first two arguments)
        switch (args.slice(0,2).join(' ')) {
            case "#N canvas":
                if(layers.length > 25) {
                    console.error('Max layer depth exceeded, ignoring further layers');
                    let depth = 0;
                    for(;lines[i].startsWith('#X restore') == false || depth > 0; i++) {
                        if(lines[i].startsWith('#N canvas'))
                            depth++;
                        if(lines[i].startsWith('#X restore'))
                            depth--;
                    }
                    break;
                }
                layers.push({
                    arrays: [],
                    messages: [],
                    guiObjects: [],
                    labels: [],
                    nextGUIID: -1,
                    canvas: layers.length > 1 ? document.createElementNS('http://www.w3.org/2000/svg', 'svg') : rootCanvas,
                    fontSize: (Number.isNaN(+args[6]) ? null : +args[6]) || layer.fontSize,
                    dimensions: {
                        windowX: +args[2],
                        windowY: +args[3],
                        width: +args[4],
                        height: +args[5]
                    },
                    config: {
                        showLabel: false,
                        showHandle: false,
                    },
                    patchID: Number.isNaN(+args[6]) ? layer.patchID : nextPatchID++,
                    args: nextArgs ? nextArgs : layer.args,
                    argsInherited: nextArgs ? true : false,
                    id: nextLayerID++,
                    HTMLID: nextHTMLID++
                });
                nextArgs = null;
                layer = layers.at(-1);
                configure_item(layer.canvas, {
                    viewBox: `0 0 ${layer.dimensions.width} ${layer.dimensions.height}`,
                    "shape-rendering": "crispEdges",
                    id: layers.length == 2 ? 'canvas' : `${layer.HTMLID}_svg`
                });
                break;
            case "#X coords":
                if(args.length > 8 && layers.length > 2) {
                    layer.showLabel = +args[8] == 1 && layer.patchID == layers.at(-2).patchID;
                    layer.showContents = +args[8] > 0;
                    layer.dimensions.coords = {
                        l: +args[2],
                        t: +args[3],
                        r: +args[4],
                        b: +args[5],
                        w: +args[6],
                        h: +args[7],
                        xticks: {},
                        yticks: {},
                        xlabels: { labels: [] },
                        ylabels: { labels: [] },
                    };
                    layer.dimensions.contentX = +args[9] || 0;
                    layer.dimensions.contentY = +args[10] || 0;
                }
                break;
            case "#X gopspill":
                if(+args[2] == 1) {
                    layer.canvas.style.overflow = 'visible';
                    layer.spill = true;
                }
                break;
            case "#X restore":
                if(args.length > 3) {
                    layer.dimensions.inlineX = +args[2];
                    layer.dimensions.inlineY = +args[3];
                    layer.name = [...args,...(layer.argsInherited ? [] : layer.args)].slice(4).join(' ');
                    if(layer.showContents) {
                        for(let message of layer.messages) {
                            for(let text of message.svgText)
                                configure_item(text, {display: "none"});
                            configure_item(message.border, {display: "none"});
                        }
                        configure_item(layer.canvas, {
                            viewBox: `${layer.dimensions.contentX} ${layer.dimensions.contentY} ${layer.dimensions.coords.w} ${layer.dimensions.coords.h}`,
                            width: layer.dimensions.coords.w,
                            height: layer.dimensions.coords.h,
                            x: layer.dimensions.inlineX,
                            y: layer.dimensions.inlineY,
                        });
                        if(!layer.spill)
                            layers.at(-2).canvas.appendChild(layer.canvas);
                        layer.border = create_item('rect', {
                            x: layer.dimensions.inlineX,
                            y: layer.dimensions.inlineY,
                            width: layer.dimensions.coords.w,
                            height: layer.dimensions.coords.h,
                            stroke: 'black',
                            fill: 'none',
                            'stroke-width': 1,
                            id: `${layer.HTMLID}_border`
                        }, layers.at(-2).canvas);
                        if(layer.spill)
                            layers.at(-2).canvas.appendChild(layer.canvas);


                        if(layer.arrays.length) {
                            // It is important to make a copy since these functions will be called
                            // after layer has gone out of scope and been replaced
                            let {dimensions, canvas, arrays} = layer;

                            configure_item(layer.border, { fill: '#00000000' });
                            layer.canvas.style.overflow = 'visible';
                            if (isMobile) {
                                layer.border.addEventListener("touchstart", function (e) {
                                    for (const touch of (e || window.event).changedTouches) {
                                        let p = gui_mousepoint(touch, canvas);
                                        let x = Math.floor(screenToCoord(dimensions.coords.l, dimensions.coords.r, dimensions.coords.w, p.x));
                                        let y = screenToCoord(dimensions.coords.t, dimensions.coords.b, dimensions.coords.h, p.y);
                                        let array, min = Infinity;
                                        for(let a of arrays)
                                            if(a.nums.length > x && Math.abs(a.nums[x] - y) < min)
                                                min = Math.abs(a.nums[x] - y), array = a;
                                        array.onmousedown(array, touch, touch.identifier);
                                    }
                                });
                            }
                            else {
                                layer.border.addEventListener("mousedown", function (e) {
                                    let p = gui_mousepoint(e, canvas);
                                    let x = Math.floor(screenToCoord(dimensions.coords.l, dimensions.coords.r, dimensions.coords.w, p.x));
                                    let y = screenToCoord(dimensions.coords.t, dimensions.coords.b, dimensions.coords.h, p.y);
                                    let array, min = Infinity;
                                    for(let a of arrays)
                                        if(a.nums.length > x && Math.abs(a.nums[x] - y) < min)
                                            min = Math.abs(a.nums[x] - y), array = a;
                                    array.onmousedown(array, e || window.event, 0);
                                });
                            }
                        }
                        for(let array of layer.arrays)
                            array.setCoords(layer.dimensions.coords);

                        if(layer.showLabel) {
                            if(layer.arrays.length > 1) {
                                for(let i = 0; i < layer.arrays.length; i++) {
                                    create_item("rect", {
                                        x: layer.dimensions.contentX + 4,
                                        y: layer.dimensions.contentY + 3.9 + 13 * i,
                                        width: 10,
                                        height: 10,
                                        fill: layer.arrays[i].outlineColor,
                                        stroke: '#000',
                                        id: `title_legend_${nextHTMLID}_${i}`
                                    }, layer.canvas)
                                    let text = create_item("text", gui_text_text({
                                        x_pos: layer.dimensions.contentX + 17,
                                        y_pos: layer.dimensions.contentY - 1.5,
                                        id: `title_${nextHTMLID}_${i}`,
                                    }, i, layer.fontSize), layer.canvas);
                                    text.textContent = layer.arrays[i].name;
                                    layer.labels.push(text);
                                    nextHTMLID++;
                                }
                            } else {
                                let text = create_item("text", gui_text_text({
                                    x_pos: layer.dimensions.contentX,
                                    y_pos: layer.dimensions.contentY,
                                    id: `title_${nextHTMLID++}`
                                }, 0, layer.fontSize), layer.canvas);
                                if(layer.arrays.length)
                                    text.textContent = layer.arrays[0].name;
                                else
                                    text.textContent = args.slice(4).join(' ');
                                layer.labels.push(text);
                            }
                        }
                    } else if(layers.at(-2).showContents !== true) {
                        let parentLayer = layers.at(-2);
                        let data = {};
                        data.type = 'patch';
                        data.windowX = layer.dimensions.windowX - layers.at(-2).dimensions.windowX;
                        data.windowY = layer.dimensions.windowY - layers.at(-2).dimensions.windowY;
                        data.receive = [args[4] == 'pd' ? `pd-${args.slice(5).join(' ')}` : `pd-${args.slice(4).join(' ')}.pd`];
                        data.canvas = layer.canvas;
                        data.layer = layer;
                        data.id = `patch_${nextHTMLID++}`;

                        data.handleText = create_item('text', {
                            'font-size':  pd_fontsize_to_gui_fontsize(parentLayer.fontSize) + 'px',
                            transform: `translate(2.5,0)`,
                            fill: 'white',
                            x: layer.dimensions.inlineX,
                            y: layer.dimensions.inlineY + font_height_map()[parentLayer.fontSize] + gobj_font_y_kludge(parentLayer.fontSize),
                            id: `${data.id}_text`,
                            class: 'unclickable'
                        }, rootCanvas);
                        data.handleText.textContent = args.slice(4).join(' ');

                        data.handleRect = create_item('rect', {
                            id: data.id,
                            fill: '#666766',
                            x: layer.dimensions.inlineX,
                            y: layer.dimensions.inlineY,
                            width: data.handleText.getComputedTextLength() + 5,
                            height: font_height_map()[parentLayer.fontSize] + 4
                        }, parentLayer.canvas);

                        rootCanvas.removeChild(data.handleText);
                        parentLayer.canvas.appendChild(data.handleText);

                        data.windowGroup = create_item('g', {
                            id: `${data.id}_windowGroup`
                        }, rootCanvas);

                        data.windowRect = create_item('rect', {
                            id: `${data.id}_windowRect`,
                            fill: '#4F4F4F',
                            width: layer.dimensions.width + 2,
                            height: layer.dimensions.height + 16,
                        }, data.windowGroup);

                        data.windowBackground = create_item('rect', {
                            id: `${data.id}_windowBackground`,
                            fill: '#9E9E9E',
                            width: layer.dimensions.width,
                            height: layer.dimensions.height,
                        }, data.windowGroup);
                        

                        data.windowText = create_item("text", {
                            'font-size': pd_fontsize_to_gui_fontsize(10) + 'px',
                            transform: `translate(2.5,0)`,
                            fill: 'white',
                            id: `${data.id}_windowText`,
                            class: 'unclickable'
                        }, data.windowGroup);
                        data.windowText.textContent = args.slice(5).join(' ');

                        data.closeBtn = create_item('path', {
                            stroke: 'red',
                            "stroke-width": '1',
                            id: `${data.id}_btn`
                        }, data.windowGroup)
                        data.closeRect = create_item('rect', {
                            fill: 'black',
                            opacity: 0,
                            width: 15,
                            height: 15,
                        }, data.windowGroup);


                        data.updateWindow = () => {
                            configure_item(data.windowBackground, {
                                x: data.windowX + 1,
                                y: data.windowY
                            });
                            configure_item(data.windowRect, {
                                x: data.windowX,
                                y: data.windowY - 15
                            });
                            configure_item(data.windowText, {
                                x: data.windowX,
                                y: data.windowY - 16 + font_height_map()[10] + gobj_font_y_kludge(10),
                            });
                            configure_item(data.closeRect, {
                                x: data.windowX + data.layer.dimensions.width - 15,
                                y: data.windowY - 15
                            })
                            configure_item(data.closeBtn, {
                                d: `M ${data.windowX + data.layer.dimensions.width - 13} ${data.windowY - 13} l 11 11 m 0 -11 l -11 11`,
                            })
                            configure_item(data.canvas, {
                                x:  data.windowX + 1,
                                y: data.windowY + 1,
                                width: data.layer.dimensions.width,
                                height: data.layer.dimensions.height
                            });
                            data.canvas.style.overflow = "hidden";
                        }
                        data.setVisibility = visibility => {
                            rootCanvas.removeChild(data.windowGroup);
                            rootCanvas.appendChild(data.windowGroup);
                            data.windowGroup.style.display = visibility ? 'block' : 'none';
                        }

                        data.updateWindow();
                        data.setVisibility(false);
                        data.windowGroup.appendChild(layer.canvas);
                        
                        if (isMobile) {
                            data.handleRect.addEventListener("touchstart", function (e) {
                                data.setVisibility(true);
                            });
                            data.closeRect.addEventListener("mousedown", function (e) {
                                data.setVisibility(false);
                            });
                            data.windowRect.addEventListener("touchstart", function (e) {
                                e = e || window.event;
                                for (const touch of e.changedTouches) {
                                    gui_window_onmousedown(data, touch, touch.identifier);
                                }
                            });
                        }
                        else {
                            data.handleRect.addEventListener("mousedown", function (e) {
                                data.setVisibility(true);
                            });
                            data.closeRect.addEventListener("mousedown", function (e) {
                                data.setVisibility(false);
                            });
                            data.windowRect.addEventListener('mousedown', function (e) {
                                gui_window_onmousedown(data,e,0);
                            });
                        }

                        gui_subscribe(data);
                    }
                    layers.pop();
                }
                break;
            case "#X obj":
                if (args.length > 4) {
                    switch (args[4]) {
                        case "adc~":
                            if (!maxNumInChannels) {
                                maxNumInChannels = 1;
                            }
                            for (let i = 5; i < args.length; i++) {
                                if (!isNaN(args[i])) {
                                    const numInChannels = +args[i];
                                    if (numInChannels > maxNumInChannels) {
                                        maxNumInChannels = numInChannels > 2 ? 2 : numInChannels;
                                    }
                                }
                            }
                            break;
                        case "bng":
                            if (args.length >= 20) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.size = +args[5];
                                data.hold = +args[6];
                                data.interrupt = +args[7];
                                data.init = +args[8];
                                data.send = args[9] === "empty" ? [null] : [args[9]];
                                data.receive = args[10] === "empty" ? [null] : [args[10]];
                                data.label = args[11] === "empty" ? "" : args[11];
                                data.x_off = +args[12];
                                data.y_off = +args[13];
                                data.font = +args[14];
                                data.fontsize = +args[15];
                                data.bg_color = isNaN(args[16]) ? args[16] : +args[16];
                                data.fg_color = isNaN(args[17]) ? args[17] : +args[17];
                                data.label_color = isNaN(args[18]) ? args[18] : +args[18];
                                data.interactive = +args[19];
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;

                                // create svg
                                data.rect = create_item("rect", gui_bng_rect(data), data.canvas);
                                data.circle = create_item("circle", gui_bng_circle(data), data.canvas);
                                data.text = create_item("text", gui_bng_text(data), data.canvas);
                                data.text.textContent = data.label;

                                // handle event
                                data.flashed = false;
                                data.interrupt_timer = null;
                                data.hold_timer = null;
                                if (isMobile) {
                                    data.rect.addEventListener("touchstart", function () {
                                        if(data.interactive)
                                            gui_bng_onmousedown(data);
                                    });
                                }
                                else {
                                    data.rect.addEventListener("mousedown", function () {
                                        if(data.interactive)
                                            gui_bng_onmousedown(data);
                                    });
                                }
                                // subscribe receiver
                                gui_subscribe(data);
                                layer.guiObjects[layer.nextGUIID] = data;
                            }
                            break;
                        case "tgl":
                            if (args.length >= 20) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.size = +args[5];
                                data.init = +args[6];
                                data.send = args[7] === "empty" ? [null] : [args[7]];
                                data.receive = args[8] === "empty" ? [null] : [args[8]];
                                data.label = args[9] === "empty" ? "" : args[9];
                                data.x_off = +args[10];
                                data.y_off = +args[11];
                                data.font = +args[12];
                                data.fontsize = +args[13];
                                data.bg_color = isNaN(args[14]) ? args[14] : +args[14];
                                data.fg_color = isNaN(args[15]) ? args[15] : +args[15];
                                data.label_color = isNaN(args[16]) ? args[16] : +args[16];
                                data.init_value = +args[17];
                                data.default_value = +args[18];
                                data.interactive = +args[19];
                                data.value = data.init && data.init_value ? data.default_value : 0;
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;

                                // create svg
                                data.rect = create_item("rect", gui_tgl_rect(data), data.canvas);
                                data.cross1 = create_item("polyline", gui_tgl_cross1(data), data.canvas);
                                data.cross2 = create_item("polyline", gui_tgl_cross2(data), data.canvas);
                                data.text = create_item("text", gui_tgl_text(data), data.canvas);
                                data.text.textContent = data.label;

                                // handle event
                                if (isMobile) {
                                    data.rect.addEventListener("touchstart", function () {
                                        if(data.interactive)
                                            gui_tgl_onmousedown(data);
                                    });
                                }
                                else {
                                    data.rect.addEventListener("mousedown", function () {
                                        if(data.interactive)
                                            gui_tgl_onmousedown(data);
                                    });
                                }
                                // subscribe receiver
                                gui_subscribe(data);
                                layer.guiObjects[layer.nextGUIID] = data;
                            }
                            break;
                        case "vsl":
                        case "hsl":
                            if (args.length >= 25) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.width = +args[5];
                                data.height = +args[6];
                                data.bottom = +args[7];
                                data.top = +args[8];
                                data.log = +args[9];
                                data.init = +args[10];
                                data.send = args[11] === "empty" ? [null] : [args[11]];
                                data.receive = args[12] === "empty" ? [null] : [args[12]];
                                data.label = args[13] === "empty" ? "" : args[13];
                                data.x_off = +args[14];
                                data.y_off = +args[15];
                                data.font = +args[16];
                                data.fontsize = +args[17];
                                data.bg_color = isNaN(args[18]) ? args[18] : +args[18];
                                data.fg_color = isNaN(args[19]) ? args[19] : +args[19];
                                data.label_color = isNaN(args[20]) ? args[20] : +args[20];
                                data.default_value = +args[21];
                                data.steady_on_click = +args[22];
                                data.interactive = +args[24];
                                data.value = data.init ? data.default_value : 0;
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;

                                // create svg
                                data.rect = create_item("rect", gui_slider_rect(data), data.canvas);
                                data.indicator = create_item("line", gui_slider_indicator(data), data.canvas);
                                data.text = create_item("text", gui_slider_text(data), data.canvas);
                                data.text.textContent = data.label;

                                // handle event
                                gui_slider_check_minmax(data);
                                if (isMobile) {
                                    data.rect.addEventListener("touchstart", function (e) {
                                        e = e || window.event;
                                        if(data.interactive)
                                            for (const touch of e.changedTouches)
                                                gui_slider_onmousedown(data, touch, touch.identifier);
                                    });
                                }
                                else {
                                    data.rect.addEventListener("mousedown", function (e) {
                                        e = e || window.event;
                                        if(data.interactive)
                                            gui_slider_onmousedown(data, e, 0);
                                    });
                                }
                                // subscribe receiver
                                gui_subscribe(data);
                                layer.guiObjects[layer.nextGUIID] = data;
                            }
                            break;
                        case "vradio":
                        case "hradio":
                            if (args.length >= 21) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.size = +args[5];
                                data.new_old = +args[6];
                                data.init = +args[7];
                                data.number = +args[8] || 1;
                                data.send = args[9] === "empty" ? [null] : [args[9]];
                                data.receive = args[10] === "empty" ? [null] : [args[10]];
                                data.label = args[11] === "empty" ? "" : args[11];
                                data.x_off = +args[12];
                                data.y_off = +args[13];
                                data.font = +args[14];
                                data.fontsize = +args[15];
                                data.bg_color = isNaN(args[16]) ? args[16] : +args[16];
                                data.fg_color = isNaN(args[17]) ? args[17] : +args[17];
                                data.label_color = isNaN(args[18]) ? args[18] : +args[18];
                                data.default_value = +args[19];
                                data.interactive = +args[20];
                                data.value = data.init ? data.default_value : 0;
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;

                                // create svg
                                data.rect = create_item("rect", gui_radio_rect(data), data.canvas);
                                gui_radio_create_lines_buttons(data);
                                data.text = create_item("text", gui_radio_text(data), data.canvas);
                                data.text.textContent = data.label;

                                // handle event
                                if (isMobile) {
                                    data.rect.addEventListener("touchstart", function (e) {
                                        e = e || window.event;
                                        if(data.interactive)
                                            for (const touch of e.changedTouches)
                                                gui_radio_onmousedown(data, touch);
                                    });
                                }
                                else {
                                    data.rect.addEventListener("mousedown", function (e) {
                                        e = e || window.event;
                                        if(data.interactive)
                                            gui_radio_onmousedown(data, e);
                                    });
                                }
                                // subscribe receiver
                                gui_subscribe(data);
                                layer.guiObjects[layer.nextGUIID] = data;
                            }
                            break;
                        case "flatgui/knob":
                            if (args.length >= 26) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.size_x = +args[5];
                                data.size_y = +args[6];
                                data.minimum = +args[7];
                                data.maximum = +args[8];

                                data.logarithmic = +args[9];
                                data.init = +args[10];
                
                                data.send = args[11] === "empty" ? [null] : [args[11]];
                                data.receive = args[12] === "empty" ? [null] : [args[12]];
                                data.label = args[13] === "empty" ? "" : args[13];
                                data.x_off = +args[14];
                                data.y_off = +args[15];
                                data.font = +args[16];
                                data.fontsize = +args[17];
                                data.bg_color = isNaN(args[18]) ? args[18] : +args[18];
                                data.fg_color = isNaN(args[19]) ? args[19] : +args[19];
                                data.label_color = isNaN(args[20]) ? args[20] : +args[20];
                                data.default_value = +args[21];
                                data.steady_on_click = +args[22];
                                data.interactive = +args[23];
                                data.dial_width = +args[24];
                                data.off_width = +args[25];
                                data.on_width = +args[26];
                                data.value = data.init ? data.default_value : data.minimum;
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;

                                data.circle = create_item("path", gui_knob_circle(data), data.canvas);
                                data.extracircle = create_item("path", gui_knob_extracircle(data), data.canvas);
                                data.line = create_item("line", gui_knob_line(data), data.canvas);
                                data.text = create_item("text", gui_knob_text(data), data.canvas);
                                data.clicktarget = create_item("rect", gui_knob_clicktarget(data), data.canvas);
                                data.text.textContent = data.label;

                                // handle event
                                if (isMobile) {
                                    data.clicktarget.addEventListener("touchstart", function (e) {
                                        e = e || window.event;
                                        for (const touch of e.changedTouches) {
                                            gui_knob_onmousedown(data, touch, touch.identifier);
                                        }
                                    });
                                }
                                else {
                                    data.clicktarget.addEventListener("mousedown", function (e) {
                                        e = e || window.event;
                                        gui_knob_onmousedown(data, e, 0);
                                    });
                                }

                                // subscribe receiver
                                gui_subscribe(data);
                                layer.guiObjects[layer.nextGUIID] = data;

                            }
                            break;
                        case "vu":
                            if (args.length >= 17) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.width = +args[5];
                                data.height = +args[6];
                                data.send = [];
                                data.receive = args[7] === 'empty' ? [null] : [args[7]];
                                data.label = args[8] === 'empty' ? '' : args[8];
                                data.x_off = +args[9];
                                data.y_off = +args[10];
                                data.font = +args[11];
                                data.fontsize = +args[12];
                                data.bg_color= isNaN(args[13]) ? args[13] : +args[13];
                                data.label_color = isNaN(args[14]) ? args[14] : +args[14];
                                data.showScale = +args[15];
                                data.unknown = +args[16];
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;
                                data.value = 0;
                                data.peak = -101

                                //create svg
                                data.box = create_item('rect', gui_vumeter_box(data), data.canvas);
                                data.text = create_item('text', gui_vumeter_label(data), data.canvas);
                                data.text.textContent = data.label;
                                data.line = create_item('line', gui_vumeter_line(data), data.canvas);
                                data.bars = [];
                                for(let bar of gui_vumeter_bars(data)) {
                                    data.bars.push(create_item('line', bar, data.canvas));
                                }
                                data.scale = [];
                                for(let line of gui_vumeter_scale(data)) {
                                    data.scale.push(create_item('text', line, data.canvas));
                                    data.scale[data.scale.length - 1].textContent = line.textContent;
                                }
                                if(!data.showScale)
                                    for(let scale of data.scale)
                                        configure_item(scale, {display: 'none'});

                                gui_subscribe(data);
                                layer.guiObjects[layer.nextGUIID] = data;

                            }
                            break;
                        case "pddplink":
                        case "pddp/pddplink":
                            if (args.length >= 6) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.link = args.slice(5).join(' ');
                                data.visible = true;
                                data.canvas = layer.canvas;
                                data.receive = [null];
                                data.id = `${data.type}_${nextHTMLID++}`;

                                if(data.link.includes(' -box')) {
                                    data.visible = false;
                                    data.link = data.link.split(' -box')[0];
                                }
                                if(data.link.includes(' -text ')) {
                                    data.text = data.link.split(' -text ')[1];
                                    data.link = data.link.split(' -text ')[0];
                                }
                                else
                                    data.text = data.link;

                                data.svgText = create_item("text", {
                                    x: data.x_pos,
                                    y: data.y_pos + font_height_map()[layer.fontSize] + gobj_font_y_kludge(layer.fontSize),
                                    "shape-rendering": "crispEdges",
                                    "font-size": pd_fontsize_to_gui_fontsize(layer.fontSize) + "px",
                                    "font-weight": "normal",
                                    display: data.visible ? 'block' : 'none',
                                    fill: '#0000ff',
                                    id: `${data.id}_text`,
                                }, data.canvas);
                                data.svgText.textContent = data.text;

                                if (isMobile) {
                                    data.svgText.addEventListener("touchstart", function (e) {
                                        gui_link_open(data.link);
                                    });
                                }
                                else {
                                    data.svgText.addEventListener("mousedown", function (e) {
                                        gui_link_open(data.link);
                                    });
                                }

                                gui_subscribe(data);
                                layer.guiObjects[layer.nextGUIID] = data;
                            }
                            break;
                        case "nbx":
                            if (args.length >= 27) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.width = +args[5];
                                data.height = +args[6];
                                data.min = +args[7];
                                data.max = +args[8];
                                data.logarithmic = +args[9];
                                data.init = +args[10];
                                data.send = args[11] === "empty" ? [null] : [args[11]];
                                data.receive = args[12] === "empty" ? [null] : [args[12]];
                                data.label = args[13] === "empty" ? "" : args[13];
                                data.x_off = +args[14];
                                data.y_off = +args[15];
                                data.font = +args[16];
                                data.fontsize = +args[17];
                                data.bg_color= isNaN(args[18]) ? args[18] : +args[18];
                                data.foreground_color= isNaN(args[19]) ? args[19] : +args[19];
                                data.label_color= isNaN(args[20]) ? args[20] : +args[20];
                                data.value = data.init ? +args[21] : 0;
                                data.logHeight = +args[22];
                                data.showTriangle = +args[23] % 2 == 0;
                                data.showBorder = +args[23] < 2;
                                data.exclusive = +args[24];
                                data.interactive = +args[25];
                                data.arrowUpdate = +args[26];
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;
            
                                data.regex = /^-?[\d]*\.?[\d]*e?[\d]*/;
                                data.onKeyDown = gui_nbx_keydown;
                                data.onLoseFocus = gui_nbx_losefocus;
            
                                data.svgText = create_item("text", {
                                    x: data.x_pos,
                                    y: data.y_pos + data.height - data.height * .15,
                                    "shape-rendering": "crispEdges",
                                    "font-size": data.height + "px",
                                    "font-weight": "normal",
                                    fill: data.foreground_color,
                                    id: `${data.id}_text`,
                                    class: "unclickable",
                                }, rootCanvas);
                                data.svgText.textContent = 'A';
                                let width = data.svgText.getComputedTextLength();
                                configure_item(data.svgText, {
                                    x: data.x_pos + width,
                                    'font-size': data.height * .9 + 'px'
                                })


                                data.svgText.textContent = new Array(+data.width + 1).fill('A').join('');
                                width = data.svgText.getComputedTextLength() + 5;
                                data.border = create_item('path', {
                                    id: `${data.id}_border`,
                                    stroke: '#000000',
                                    "stroke-width": data.showBorder ? '1' : '0',
                                    fill: data.bg_color,
                                    d: `M ${data.x_pos} ${data.y_pos} h ${width - 4} l 4 4 v ${data.height-4} H ${data.x_pos} V ${data.y_pos}`
                                }, data.canvas);
                                data.triangle = create_item('path', {
                                    id: `${data.id}_triangle`,
                                    stroke: '#000000',
                                    fill: '#00000000',
                                    'stroke-width': data.showTriangle ? '1' : '0',
                                    d: `M ${data.x_pos} ${data.y_pos} l ${data.height/2} ${data.height/2} l ${-data.height/2} ${data.height/2}`
                                }, data.canvas);
            
                                gui_nbx_settext(data, '' + data.value);
                                rootCanvas.removeChild(data.svgText);
                                layer.canvas.appendChild(data.svgText);

                                
                                data.labelText = create_item("text", gui_text(data), data.canvas);
                                data.labelText.textContent = data.label;
            
                                
            
                                if (isMobile) {
                                    data.border.addEventListener("touchstart", function (e) {
                                        e = e || window.event;
                                        for (const touch of e.changedTouches) {
                                            gui_nbx_onmousedown(data, touch, touch.identifier);
                                        }
                                    });
                                }
                                else {
                                    data.border.addEventListener("mousedown", function (e) {
                                        gui_nbx_onmousedown(data, e, 0);
                                    });
                                }
            
                                gui_subscribe(data);
                                layer.guiObjects[layer.nextGUIID] = data;
                            }
                            break;
                        case "cnv":
                            if (args.length >= 18) {
                                const data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.size = +args[5];
                                data.width = +args[6];
                                data.height = +args[7];
                                data.send = args[8] === "empty" ? [null] : [args[8]];
                                data.receive = args[9] === "empty" ? [null] : [args[9]];
                                data.label = args[10] === "empty" ? "" : args[10];
                                data.x_off = +args[11];
                                data.y_off = +args[12];
                                data.font = +args[13];
                                data.fontsize = +args[14];
                                data.bg_color = isNaN(args[15]) ? args[15] : +args[15];
                                data.label_color = isNaN(args[16]) ? args[16] : +args[16];
                                data.unknown = +args[17];
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;

                                // create svg
                                data.visible_rect = create_item("rect", gui_cnv_visible_rect(data), data.canvas);
                                data.selectable_rect = create_item("rect", gui_cnv_selectable_rect(data), data.canvas);
                                data.text = create_item("text", gui_cnv_text(data), data.canvas);
                                data.text.textContent = data.label;

                                // subscribe receiver
                                gui_subscribe(data);
                            }
                            break;
                            
                        case "ggee/image":
                            if(args.length >= 26) {
                                let data = {};
                                data.x_pos = +args[2];
                                data.y_pos = +args[3];
                                data.type = args[4];
                                data.src = args[5];
                                data.gopSpill = +args[6];
                                data.clickBehavior = +args[7];
                                data.borderWidth = +args[8];
                                data.borderHeight = +args[9];
                                data.send = args[10] === "empty" ? [null] : [args[10]];
                                data.receive = args[11] === "empty" ? [null] : [args[11]];
                                data.label = args[12] === "empty" ? "" : args[12];
                                data.imageWidth = +args[13];
                                data.imageCurrentWidth = +args[13];
                                data.imageHeight = +args[14];
                                data.imageCurrentHeight = +args[14];
                                data.constrain = +args[15]; // 0 - None, 1 - Image, 2 - Current
                                data.font = +args[16];
                                data.fontsize = +args[17];
                                data.label_color = args[18];
                                data.x_off = +args[19];
                                data.y_off = +args[20];
                                data.x_origin = +args[21];
                                data.y_origin = +args[22];
                                data.rotation = +args[23];
                                data.visible = +args[24];
                                data.opacity = +args[25];
                                data.id = `${data.type}_${nextHTMLID++}`;
                                data.canvas = layer.canvas;

                                data.image = create_item("image", {}, layer.canvas);
                                data.border = create_item("rect", {}, layer.canvas);
                                data.text = create_item("text", gui_text(data), layer.canvas);

                                data.render = async(data) => {
                                    await new Promise((Resolve)=>{
                                        let i = new Image();
                                        i.src = data.src;
                                        i.onload = e => {
                                            data.imageNaturalWidth = i.naturalWidth;
                                            data.imageNaturalHeight = i.naturalHeight;
                                            Resolve(true);
                                        }
                                    });
                                    if(data.constrain == 1)
                                        data.imageHeight = data.imageWidth * (data.imageNaturalHeight / data.imageNaturalWidth);
                                    if(data.constrain == 2)
                                        data.imageHeight = data.imageWidth * (data.imageCurrentHeight / data.imageCurrentWidth);
                                    if(data.gopSpill) {
                                        data.borderWidth = Math.min(data.imageWidth, data.borderWidth);
                                        data.borderHeight = Math.min(data.imageHeight, data.borderHeight);
                                    } else {
                                        data.borderWidth = data.imageWidth;
                                        data.borderHeight = data.imageHeight;
                                    }
                                    configure_item(data.image, {
                                        x: data.x_pos - (data.imageWidth - data.borderWidth) / 2,
                                        y: data.y_pos - (data.imageHeight - data.borderHeight) / 2,
                                        transform: `rotate(${data.rotation}, ${data.x_pos + data.x_origin}, ${data.y_pos + data.y_origin})`,
                                        width: data.imageWidth,
                                        height: data.imageHeight,
                                        href: data.src,
                                        opacity: data.visible ? data.opacity : '0',
                                        id: `${data.id}_image`,
                                        preserveAspectRatio: "none",
                                        class: 'unclickable'
                                    });
                                    configure_item(data.border, {
                                        x: data.x_pos,
                                        y: data.y_pos,
                                        width: data.borderWidth,
                                        height: data.borderHeight,
                                        stroke: 'red',
                                        fill: 'red',
                                        opacity: '0',
                                        id: `${data.id}_border`,
                                        class: (data.clickBehavior === 3 || data.clickBehavior === 0) ? "unclickable" : ''
                                    });
                                    configure_item(data.text, gui_text(data));
                                    data.text.textContent = data.label;
                                }
                                data.render(data);

                                data.contains = p => p.x > data.x_pos && p.y > data.y_pos && p.x < data.x_pos + data.borderWidth && p.y < data.y_pos + data.borderHeight;

                                inputListeners.push({
                                    onMouseDown: e => {
                                        let p = gui_roundedmousepoint(e, data.canvas);
                                        if(data.clickBehavior === 3 && data.contains(p))
                                            gui_send('List', data.send, [1, data.x_pos, data.y_pos, p.x - data.x_pos, p.y - data.y_pos]);
                                    },
                                    onMouseUp: e => {
                                        let p = gui_roundedmousepoint(e, data.canvas);
                                        if(data.clickBehavior === 3 && data.contains(p))
                                            gui_send('List', data.send, [-1, data.x_pos, data.y_pos, p.x - data.x_pos, p.y - data.y_pos]);
                                    },
                                    onMouseMove: e => {
                                        let p = gui_roundedmousepoint(e, data.canvas);
                                        if(data.clickBehavior === 3 && data.contains(p))
                                            gui_send('List', data.send, [0, data.x_pos, data.y_pos, p.x - data.x_pos, p.y - data.y_pos]);
                                    },
                                });

                                if (isMobile) {
                                    data.border.addEventListener("touchstart", function (e) {
                                        e = e || window.event;
                                        for (const touch of e.changedTouches) {
                                            gui_image_onmousedown(data, touch, touch.identifier);
                                        }
                                    });
                                }
                                else {
                                    data.border.addEventListener("mousedown", function (e) {
                                        gui_image_onmousedown(data, e, 0);
                                    });
                                }

                                layer.guiObjects[layer.nextGUIID] = data;
                            }
                            break;
                        case "key": {
                            let data = {};
                            data.repeat = args[5] === '1';
                            data.send = [null];
                            layer.guiObjects[layer.nextGUIID] = data;
                            inputListeners.push({
                                onKeyDown: (e) => {
                                    if(e.repeat === false || data.repeat === true)
                                        gui_send('Float',data.send, e.key.length == 1 ? e.key.charCodeAt(0) : e.keyCode);
                                }
                            })
                            break;
                        }
                        case "keyup": {
                            let data = {};
                            data.repeat = args[5] === '1';
                            data.send = [null];
                            layer.guiObjects[layer.nextGUIID] = data;
                            inputListeners.push({
                                onKeyUp: (e) => {
                                    if(e.repeat === false || data.repeat === true)
                                        gui_send('Float',data.send, e.key.length == 1 ? e.key.charCodeAt(0) : e.keyCode);
                                }
                            })
                            break;
                        }
                        case "keyname": {
                            let data = {};
                            data.repeat = args[5] === '1';
                            data.send = [null];
                            data.auxSend = [[null]];
                            layer.guiObjects[layer.nextGUIID] = data;
                            inputListeners.push({
                                onKeyDown: e => {
                                    if(e.repeat === false || data.repeat === true) {
                                        gui_send('Float', data.send, 1);
                                        if(e.key.match(/^F\d$/) && keyDown['Shift'])
                                            gui_send('Symbol', data.auxSend[0], "Shift"+e.key);
                                        else
                                            gui_send('Symbol', data.auxSend[0], e.key);
                                    }
                                },
                                onKeyUp: e => {
                                    if(e.repeat === false || data.repeat === true) {
                                        gui_send('Float', data.send, 0);
                                        if(e.key.match(/^F\d$/) && keyDown['Shift'])
                                            gui_send('Symbol', data.auxSend[0], "Shift"+e.key);
                                        else
                                            gui_send('Symbol', data.auxSend[0], e.key);
                                    }
                                }
                            })
                            break;
                        }
                        case "preset_node":
                            //Putting this here just to be able to ignore it when creating connections to silence the warnings
                            //This does NOT implement preset_nodes
                            layer.guiObjects[layer.nextGUIID] = {
                                type: 'preset_node'
                            }
                            break;
                        case "legacy_mousemotion": {
                            let data = {};
                            data.send = [null];
                            data.auxSend = [[null]];
                            data.canvas = layer.canvas;
                            layer.guiObjects[layer.nextGUIID] = data;
                            inputListeners.push({
                                onMouseMove: e => {
                                    let p = gui_roundedmousepoint(e, rootCanvas);
                                    gui_send('Float', data.send, p.x);
                                    gui_send('Float', data.auxSend[0], p.y);
                                }
                            })
                            break;
                        }
                        case "legacy_mouseclick": {
                            let data = {};
                            data.send = [null];
                            data.auxSend = [[null],[null],[null]];
                            data.canvas = layer.canvas;
                            layer.guiObjects[layer.nextGUIID] = data;
                            inputListeners.push({
                                onMouseDown: e => {
                                    let p = gui_roundedmousepoint(e, rootCanvas);
                                    gui_send('Float', data.send, 1);
                                    gui_send('Float', data.auxSend[0], e.which);
                                    gui_send('Float', data.auxSend[1], p.x);
                                    gui_send('Float', data.auxSend[2], p.y);
                                },
                                onMouseUp: e => {
                                    let p = gui_roundedmousepoint(e, rootCanvas);
                                    gui_send('Float', data.send, 0);
                                    gui_send('Float', data.auxSend[0], e.which);
                                    gui_send('Float', data.auxSend[1], p.x);
                                    gui_send('Float', data.auxSend[2], p.y);
                                }
                            })
                            break;
                        }
                        default: //If we don't have an explicit handling for the object, it's possible that it is an external patch load 
                            if(args[4] in abstractions) {
                                //We must add an #X restore at the end to undo the #N canvas at the beginning
                                nextArgs = args.length > 5 ? args.slice(5) : null;
                                lines.splice(i,1,...abstractions[args[4]].split(';\n').slice(0,-1),`#X restore ${args[2]} ${args[3]} ${args[4]}`);
                                //Since we removed the line that we just processed, our subpatch starts at line i, so we have to process line i again.
                                i--;
                                layer.nextGUIID--;
                            }
                            break;
                    }
                }
                break;
            case "#X array":
                if(args.length > 7) {
                    let data = {}
                    data.type = args[1];
                    data.name = args[2];
                    data.size = +args[3];
                    data.receive = [data.name];
                    data.send = [null];
                    data.valtype = args[4];
                    //args[5] is jumpOnClick, which is unused in the web version
                    data.displayMode = Math.floor((+args[5] % 16) / 2);
                    //displayMode:
                    //0 - polygon
                    //1 - points
                    //2 - bezier curve
                    //3 - bar graph
                    data.fillColor = args[6];
                    data.outlineColor = args[7];
                    data.id = `array_${nextHTMLID++}`;
                    data.canvas = layer.canvas;
                    data.layer = layer;
                    if(lines[i+1].startsWith('#A '))
                        data.nums = lines[i+1].replace(/\n/g,' ').split(' ').slice(1).map(num=>+num);
                    else
                        data.nums = (new Array(data.size)).fill(0);
                    data.path = create_item('path', {
                        id: data.id,
                        stroke: data.outlineColor,
                        "stroke-width": "1",
                        "shape-rendering": "crispEdges",
                        fill: 'none'
                    }, data.canvas);
                    if(layer.arrays.length) {
                        data.canvas.removeChild(data.path);
                        data.canvas.insertBefore(data.path, layer.arrays[0].path);
                    }
                    data.setCoords = coords => {
                        data.coords = coords;
                        data.redraw();
                    }
                    data.resize = size => {
                        if(size > data.nums.length)
                            data.nums = [...data.nums, ...(new Array(size - data.nums.length)).fill(0)];
                        else
                            data.nums = data.nums.slice(0,size);
                        data.redraw();

                        let smallestArr = data.layer.arrays.reduce((p, c) => p < c.nums.length ? c.nums.length : p, 0);
                        data.layer.dimensions.coords.l = 0;
                        data.layer.dimensions.coords.r = smallestArr - 1;
                        for(let array of data.layer.arrays)
                            array.setCoords(data.layer.dimensions.coords);
                        gui_canvas_drawTicks(data.layer);
                        gui_canvas_drawLabels(data.layer);
                    }
                    data.redraw = () => {
                        let path = data.displayMode % 2 ? '' : 'M ';
                        let c = data.coords;
                        let lastX = -1;
                        for(let i = 0; i < data.nums.length; i++) {
                            let curX = coordToScreen(c.l,c.r,c.w,i);
                            if(curX != lastX) {
                                lastX = curX;
                                if(data.displayMode == 0 || data.displayMode == 2)
                                    path += `${curX} ${coordToScreen(c.t,c.b,c.h,data.nums[i])} `;
                                if(data.displayMode == 1 && i+1 <= c.r)
                                    path += `M ${curX} ${coordToScreen(c.t,c.b,c.h,data.nums[i])} H ${coordToScreen(c.l,c.r,c.w,i + 1) - 1} V ${coordToScreen(c.t,c.b,c.h,data.nums[i])+1} H ${curX} Z `;
                                if(data.displayMode == 3 && i+1 <= c.r)
                                    path += `M ${curX} ${coordToScreen(c.t,c.b,c.h,data.nums[i])} H ${coordToScreen(c.l,c.r,c.w,i + 1)} V ${c.h} H ${curX} Z `;
                            }
                        }
                        if(data.displayMode == 3)
                            configure_item(data.path, {fill: data.fillColor});
                        configure_item(data.path,{d: path});
                    }
                    data.onmousedown = gui_array_onmousedown;
                    data.onmouseup = gui_array_onmouseup;
                    data.onmousemove = gui_array_onmousemove;

                    layer.arrays.push(data);
                    layer.guiObjects[layer.nextGUIID] = data;
                    gui_subscribe(data);
                }
                break;
            case "#X text":
                if (args.length > 4) {
                    const data = {};
                    data.type = args[1];
                    data.x_pos = +args[2];
                    data.y_pos = +args[3];
                    data.comment = [];
                    data.canvas = layer;
                    const lines = args.slice(4).join(" ").replace(/ \\,/g, ",").replace(/\\; /g, ";\n").replace(/ ;/g, ";").replace(//g,'\n').split("\n");
                    for (const line of lines) {
                        const lines = line.match(new RegExp(`.{1,${widthOverride || 60}}(\\s|$)`,'g')) || [];
                        for (const line of lines) {
                            data.comment.push(line.trim());
                        }
                    }
                    data.id = `${data.type}_${nextHTMLID++}`;

                    // create svg
                    data.texts = [];
                    for (let i = 0; i < data.comment.length; i++) {
                        const text = create_item("text", gui_text_text(data, i, layer.fontSize), layer.canvas);
                        text.textContent = data.comment[i];
                        data.texts.push(text);
                    }
                }
                break;
            case "#X msg":
                if(args.length > 3) {
                    const data = {};
                    data.type = args[1];
                    data.x_pos = +args[2];
                    data.y_pos = +args[3];
                    data.text = args.slice(4).join(' ').replace(/\\\; /g,';\n').replace(/ ,/g,',').replace(/\\\$/g,'$');
                    data.id = `${data.type}_${nextHTMLID++}`;
                    data.receive = [null];
                    data.shortCircuit = false;
                    data.send = [null];
                    data.clickSend = `msg_${layer.id}_${layer.nextGUIID}`
                    data.canvas = layer.canvas;
                    layer.messages.push(data);

                    let nextObjId = layer.nextGUIID, nextSlot = i, depth = 0;
                    for(;lines[nextSlot].startsWith('#X connect') == false || depth > 0; nextSlot++) {
                        if(object_types.find(type=>lines[nextSlot].startsWith(type)) && depth == 0)
                            nextObjId++;
                        if(lines[nextSlot].startsWith('#N canvas'))
                            depth++;
                        if(lines[nextSlot].startsWith('#X restore'))
                            depth--;
                    }
                    lines.splice(nextSlot, 0, `#X obj 0 0 r ${data.clickSend}`,`#X connect ${nextObjId} 0 ${layer.nextGUIID} 0 __IGNORE__`);

                    //create svg
                    data.sizeText = create_item("text", {
                        "font-size": pd_fontsize_to_gui_fontsize(layer.fontSize) + "px",
                        id: `${data.id}_size`,
                        class: "unclickable",
                    }, rootCanvas);
                    data.svgText = [];

                    
                    data.border = create_item('path', {
                        id: data.id,
                        stroke:'#d9d9d9',
                        "stroke-width": "0",
                        fill: '#d9d9d9',
                    }, data.canvas);

                    data.render = (data) => {
                        let textLines = data.text.split('\n');
                        data.sizeText.textContent = new Array(+widthOverride || Math.max(2,textLines.reduce((p,c)=>c.length>p?c.length:p,0))).fill('A').join('');
                        let width = data.sizeText.getComputedTextLength() + 5, height = font_height_map()[layer.fontSize] * textLines.length + 4;
                        configure_item(data.border, {d: `M ${data.x_pos} ${data.y_pos} h ${width+4} l -4 4 v ${height-8} l 4 4 H ${data.x_pos} V ${data.y_pos}`}); 
                        for(let i = 0; i < textLines.length; i++) {
                            if(!data.svgText[i]) {
                                data.svgText.push(create_item("text", {
                                    transform: `translate(2.5,${font_height_map()[layer.fontSize] * i})`,
                                    x: data.x_pos,
                                    y: data.y_pos + font_height_map()[layer.fontSize] + gobj_font_y_kludge(layer.fontSize),
                                    "shape-rendering": "crispEdges",
                                    "font-size": pd_fontsize_to_gui_fontsize(layer.fontSize) + "px",
                                    "font-weight": "normal",
                                    id: `${data.id}_text_${i}`,
                                    class: "unclickable",
                                }, layer.canvas));
                            }
                            data.svgText[i].textContent = textLines[i];
                        }
                        for(let i = textLines.length; i < data.svgText.length; i++)
                            data.svgText[i].textContent = '';
                    }

                    data.render(data);
                    
                    if (isMobile) {
                        data.border.addEventListener("touchstart", function (e) {
                            gui_send('Bang', [data.clickSend]);
                            configure_item(data.border,{"stroke-width":"4"});
                            setTimeout(()=>configure_item(data.border,{"stroke-width":"0"}), 100);
                        });
                    }
                    else {
                        data.border.addEventListener("mousedown", function (e) {
                            gui_send('Bang', [data.clickSend]);
                            configure_item(data.border,{"stroke-width":"4"});
                            setTimeout(()=>configure_item(data.border,{"stroke-width":"0"}), 100);
                        });
                    }

                    layer.guiObjects[layer.nextGUIID] = data;
                    gui_subscribe(data);
                }
                break;
            case "#X floatatom":
            case "#X symbolatom":
                if (args.length > 13) {
                    const data = {};
                    data.type = args[1];
                    data.x_pos = +args[2];
                    data.y_pos = +args[3];
                    data.width = Math.max(2,+args[4]);
                    data.min = +args[5];
                    data.max = +args[6];
                    data.labelSide = +args[7];
                    data.label = args[8] === "-" ? "" : args[8];
                    data.receive = args[9] === "-" ? [null] : [args[9]];
                    data.send = args[10] === "-" ? [null] : [args[10]];
                    data.exclusive = +args[11];
                    data.typedMinMax = +args[12];
                    data.interactive = +args[13];
                    data.id = `${data.type}_${nextHTMLID++}`;
                    data.canvas = layer.canvas;

                    data.value = data.type === 'floatatom' ? 0 : 'symbol';
                    data.lastNonZero = 1;
                    data.regex = data.type === 'floatatom' ? /^-?[\d]*\.?[\d]*e?[\d]*$/ : /^.*$/;
                    data.onKeyDown = gui_atom_keydown;
                    data.onLoseFocus = gui_atom_losefocus;

                    data.svgText = create_item("text", {
                        transform: `translate(1.5)`,
                        x: data.x_pos,
                        y: data.y_pos + font_height_map()[layer.fontSize] + gobj_font_y_kludge(layer.fontSize),
                        "shape-rendering": "crispEdges",
                        "font-size": pd_fontsize_to_gui_fontsize(layer.fontSize) + "px",
                        "font-weight": "normal",
                        id: `${data.id}_text`,
                        class: "unclickable",
                    }, rootCanvas);
                    data.svgText.textContent = new Array(+data.width).fill('A').join('');
                    
                    let width = data.svgText.getComputedTextLength() + 2.5, height = font_height_map()[layer.fontSize] + 4;
                    data.border = create_item('path', {
                        id: data.id,
                        stroke:'#d9d9d9',
                        "stroke-width": "0",
                        fill: '#d9d9d9',
                        d: `M ${data.x_pos} ${data.y_pos} h ${width-4} l 4 4 v ${height-4} H ${data.x_pos} V ${data.y_pos}`
                    }, data.canvas);

                    gui_atom_settext(data, '' + data.value);
                    rootCanvas.removeChild(data.svgText);
                    layer.canvas.appendChild(data.svgText);

                    // LabelSide
                    // 0 - Left
                    // 1 - Right
                    // 2 - Top
                    // 3 - Bottom
                    data.labelText = create_item("text", {
                        x: data.x_pos,
                        y: data.y_pos + (data.labelSide < 2 ? font_height_map()[layer.fontSize] : 0) - 3,
                        'shape-rendering': 'crispEdges',
                        'font-size': pd_fontsize_to_gui_fontsize(layer.fontSize) + 'px',
                        id: `${data.id}_label`,
                        class: 'unclickable',
                    }, data.canvas);
                    data.labelText.textContent = data.label;
                    if(data.labelSide == 0)
                        configure_item(data.labelText, {transform: `translate(-${data.labelText.getComputedTextLength()})`});
                    if(data.labelSide == 1)
                        configure_item(data.labelText, {transform: `translate(${width+1})`});

                    if (isMobile) {
                        data.border.addEventListener("touchstart", function (e) {
                            e = e || window.event;
                            for (const touch of e.changedTouches) {
                                gui_atom_onmousedown(data, touch, touch.identifier);
                            }
                        });
                    }
                    else {
                        data.border.addEventListener("mousedown", function (e) {
                            gui_atom_onmousedown(data, e, 0);
                        });
                    }

                    gui_subscribe(data);
                    layer.guiObjects[layer.nextGUIID] = data;
                }
                break;
            case "#X dropdown":
                if(args.length >= 11) {
                    const data = {};
                    data.x_pos = +args[2];
                    data.y_pos = +args[3];
                    data.width = +args[4];
                    data.outputValue = +args[5];
                    //args[6], not sure what its for?
                    data.labelSide = +args[7];
                    //0 - Left
                    //1 - Right
                    //2 - Top
                    //3 - Bottom
                    data.label = args[8] === '-' ? '' : args[8];
                    data.receive = args[9] === '-' ? [null] : [args[9]];
                    data.send = args[10] === '-' ? [null] : [args[10]];
                    data.options = [
                        "symbol",
                        "float",
                        "bang",
                        "list"
                    ];
                    data.selectedOption = 0;
                    data.canvas = layer.canvas;
                    data.type = 'dropdown';
                    data.id = `${data.type}_${nextHTMLID}`;

                    data.svgText = create_item("text", {
                        transform: `translate(1.5)`,
                        x: data.x_pos,
                        y: data.y_pos + font_height_map()[layer.fontSize] + gobj_font_y_kludge(layer.fontSize),
                        "shape-rendering": "crispEdges",
                        "font-size": pd_fontsize_to_gui_fontsize(layer.fontSize) + "px",
                        "font-weight": "normal",
                        id: `${data.id}_text`,
                        class: "unclickable",
                    }, rootCanvas);

                    data.box = create_item('path', {
                        id: `${data.id}_box`,
                        stroke: '#d9d9d9',
                        "stroke-width": "0",
                        fill: '#d9d9d9'
                    }, data.canvas);
                    data.triangle = create_item('path', {
                        id: `${data.id}_triangle`,
                        stroke: data.outputValue ? 'none' : 'black',
                        "stroke-width": "1",
                        fill: data.outputValue ? 'black' : 'none'
                    }, data.canvas);
                    rootCanvas.removeChild(data.svgText);
                    layer.canvas.appendChild(data.svgText);

                    data.labelText = create_item("text", {
                        x: data.x_pos,
                        y: data.y_pos + (data.labelSide < 2 ? font_height_map()[layer.fontSize] : 0) - 3,
                        'shape-rendering': 'crispEdges',
                        'font-size': pd_fontsize_to_gui_fontsize(layer.fontSize) + 'px',
                        id: `${data.id}_label`,
                        class: 'unclickable',
                    }, data.canvas);
                    data.labelText.textContent = data.label;

                    data.optionsRect = create_item('rect', {
                        x: data.x_pos,
                        y: data.y_pos + font_height_map()[layer.fontSize]*1.5 - 2.5,
                        fill: "#C3C3C3",
                        id: `${data.id}_options_rect`,
                    }, data.canvas);
                    data.optionsGUI = [];

                    data.render = () => {
                        let optionOffset = font_height_map()[layer.fontSize]*1.5 - 2.5;
                        let optionHeight = font_height_map()[layer.fontSize]*2 - 3.5;

                        data.svgText.textContent = new Array(+data.width == 0 ? data.options.reduce((p, c) => c.length > p ? c.length : p, 0) : data.width).fill('A').join('');
                        let width = data.svgText.getComputedTextLength() + 15, height = font_height_map()[layer.fontSize] + 4;
                        configure_item(data.box, { d: `M ${data.x_pos} ${data.y_pos} h ${width-4} l 4 4 v ${height-8} l -4 4 H ${data.x_pos} V ${data.y_pos}` } );
                        configure_item(data.triangle, { d: `M ${data.x_pos + width - 9} ${data.y_pos + 7} l 6 0 l -3 5 l -3 -5` });
                        if(data.labelSide == 0)
                            configure_item(data.labelText, {transform: `translate(-${data.labelText.getComputedTextLength()})`});
                        if(data.labelSide == 1)
                            configure_item(data.labelText, {transform: `translate(${width+1})`});
                        data.svgText.textContent = data.options[data.selectedOption];

                        configure_item(data.optionsRect, {
                            width,
                            height: optionHeight * data.options.length + 1
                        });
                        data.canvas.removeChild(data.optionsRect);
                        data.canvas.appendChild(data.optionsRect);
                        data.optionsRect.style.display = data.showOptions ? 'block' : 'none';
                        while(data.optionsGUI.length) {
                            let {box, text} = data.optionsGUI.pop();
                            layer.canvas.removeChild(box);
                            layer.canvas.removeChild(text);
                        }
                        if(data.showOptions) {
                            for(let i=0; i<data.options.length; i++) {
                                let box = create_item('rect', {
                                    x: data.x_pos + 1,
                                    y: data.y_pos + optionOffset + optionHeight * i,
                                    width: width - 2,
                                    height: optionHeight,
                                    fill: '#eeeeee'
                                }, data.canvas);
                                let text = create_item('text', {
                                    x: data.x_pos + 6,
                                    y: data.y_pos + font_height_map()[layer.fontSize] * 1.5 - 3.5 + optionOffset + optionHeight * i,
                                    'shape-rendering': 'crispEdges',
                                    'font-size': pd_fontsize_to_gui_fontsize(layer.fontSize) + 'px',
                                    id: `${data.id}_label`,
                                    class: 'unclickable',
                                }, data.canvas);
                                text.textContent = data.options[i];

                                let option = i; // necessary to create local variable because i will get overwritten
                                if (isMobile) {
                                    box.addEventListener("touchstart", function (e) {
                                        e = e || window.event;
                                        for (const touch of e.changedTouches)
                                            gui_dropdown_option_onmousedown(data, i, touch, touch.identifier);
                                    });
                                }
                                else {
                                    box.addEventListener("mousedown", function (e) {
                                        gui_dropdown_option_onmousedown(data, i, e, 0);
                                    });
                                }
                                data.optionsGUI.push({box, text});
                            }
                        }
                    }

                    data.render();

                    if (isMobile) {
                        data.box.addEventListener("touchstart", function (e) {
                            e = e || window.event;
                            for (const touch of e.changedTouches)
                                gui_dropdown_onmousedown(data, touch, touch.identifier);
                        });
                    }
                    else {
                        data.box.addEventListener("mousedown", function (e) {
                            gui_dropdown_onmousedown(data, e, 0);
                        });
                    }

                    gui_subscribe(data);
                    layer.guiObjects[layer.nextGUIID] = data;
                    gui_dropdown_dropdowns.push(data);
                }
                break;
            case "#X connect":
                if (args.length > 5) {
                    if(args[6] === '__IGNORE__' || layer.guiObjects[args[4]]?.type == 'preset_node' || layer.guiObjects[args[2]]?.type == 'preset_node')
                        break;
                    //We generate a name based off of the arguments of the connect (which will be unique)
                    let connectionName = `__WIRE_${layer.id}_${args[2]}_${args[3]}_${args[4]}_${args[5]}`;
                    if(layer.guiObjects[args[4]]?.shortCircuit === false)
                        connectionName = layer.guiObjects[args[4]].clickSend;

                    if(layer.guiObjects[args[2]] && !layer.guiObjects[args[4]]) {
                        //If the sender is a gui object, and the receiver is not, we must add a receive object so that the
                        //sender can send wirelessly. Then we connect the receive object to the receiver, and the sender wirelessly to the receive object
                        if(args[3] == '0') {
                            if(layer.guiObjects[args[2]]?.shortCircuit !== false)
                                lines.splice(i++,1,`#X obj 0 0 r ${connectionName}`,`#X connect ${++layer.nextGUIID} 0 ${args[4]} ${args[5]} __IGNORE__`)
                            layer.guiObjects[args[2]].send.push(connectionName);
                        } else if(layer.guiObjects[args[2]].auxSend) {
                            if(layer.guiObjects[args[2]]?.shortCircuit !== false)
                                lines.splice(i++,1,`#X obj 0 0 r ${connectionName}`,`#X connect ${++layer.nextGUIID} 0 ${args[4]} ${args[5]} __IGNORE__`)
                            layer.guiObjects[args[2]].auxSend[+args[3] - 1].push(connectionName);
                        } else
                            console.warn('Ignoring unsupported wired connection (Code A)'); //This should never happen
                    }
                    if((!layer.guiObjects[args[2]] || layer.guiObjects[args[2]].shortCircuit === false) && layer.guiObjects[args[4]] && args[5] === '0') {
                        //If the receiver is a gui object, and the sender is not, we must add a send object so that the receiver
                        //can receive wirelessly. Then we connect the send object to the sender, and the receiver wirelessly to the send object
                        lines.splice(i++,1,`#X obj 0 0 s ${connectionName}`,`#X connect ${args[2]} ${args[3]} ${++layer.nextGUIID} 0 __IGNORE__`);
                        layer.guiObjects[args[4]].receive.push(connectionName);
                        gui_subscribe(layer.guiObjects[args[4]]);
                    }
                    if(layer.guiObjects[args[2]] && layer.guiObjects[args[4]] && args[5] === '0' && layer.guiObjects[args[2]].shortCircuit !== false) {
                        //If both the sender and receiver are gui objects, we can directly set their sends and receives
                        //Theoretically, they should only have 1 input/output, so the input/output id should always be 0
                        if(args[3] === '0') {
                            layer.guiObjects[args[2]].send.push(connectionName);
                            if(layer.guiObjects[args[4]].shortCircuit !== false) {
                                layer.guiObjects[args[4]].receive.push(connectionName);
                                gui_subscribe(layer.guiObjects[args[4]]);
                            }
                        } else if(layer.guiObjects[args[2]].auxSend) {
                            layer.guiObjects[args[2]].auxSend[+args[3] - 1].push(connectionName);
                            if(layer.guiObjects[args[4]].shortCircuit !== false) {
                                layer.guiObjects[args[4]].receive.push(connectionName);
                                gui_subscribe(layer.guiObjects[args[4]]);
                            }
                        }
                        else
                            console.warn('Ignoring unsupported wired connection (Code B)')
                    }
                    if(args[5] !== '0' && layer.guiObjects[args[4]]) {
                        if(args[5] === '1') {
                            lines.splice(i,1,
                                `#X obj 0 0 pack`,
                                `#X obj 0 0 t b a`,
                                `#X obj 0 0 s ${connectionName}`,
                                `#X connect ${layer.nextGUIID + 2} 0 ${layer.nextGUIID + 1} 0`,
                                `#X connect ${layer.nextGUIID + 2} 1 ${layer.nextGUIID + 1} 1`,
                                `#X connect ${layer.nextGUIID + 1} 0 ${layer.nextGUIID + 3} 0`,
                                `#X connect ${args[2]} ${args[3]} ${layer.nextGUIID + 2} 0`
                            );
                            layer.nextGUIID++;
                            layer.guiObjects[args[4]].receive.push(connectionName);
                            gui_subscribe(layer.guiObjects[args[4]]);
                        } else
                            console.warn('Ignoring unsupported wired connection (Code C');
                    }
                }
        }
    }

    document.getElementById('loadingstage').innerHTML=`Starting Pd-L2Ork Engine`;
    await new Promise(resolve => setTimeout(resolve, 10));

    if (nextLayerID == 0)
        return alert("The main canvas not found in the pd file.");

    if (maxNumInChannels) {
        if (Module.pd.init(maxNumInChannels, Module.pd.getNumOutChannels(), Module.pd.getSampleRate(), Module.pd.getTicksPerBuffer())) {
            // print obtained settings
            console.log("Pd-l2Ork: successfully reinitialized");
            console.log("Pd-l2Ork: audio input channels: " + Module.pd.getNumInChannels());
            console.log("Pd-l2Ork: audio output channels: " + Module.pd.getNumOutChannels());
            console.log("Pd-l2Ork: audio sample rate: " + Module.pd.getSampleRate());
            console.log("Pd-l2Ork: audio ticks per buffer: " + Module.pd.getTicksPerBuffer());
        }
        else {
            // failed to reinit pd
            alert("Pd-l2Ork: failed to reinitialize pd");
            console.error("Pd-l2Ork: failed to reinitialize pd");
            Module.mainExit();
            return;
        }
    }

    //Next we load our modified lines into the backend of pd-l2ork
    FS.createDataFile("/", filename, new TextEncoder().encode(lines.join(';\n')), true, true, true);
    console.log('Parse time: '+(Date.now() - start)+'ms');
    try {
        Module.pd.openPatch(filename, "/");
        pdsend("pd dsp 1");
    } catch (e) {
        alert("Failed to start PD engine!");
        console.error(e.stack);
        return;
    }
}

function uploadPatch(file) {
    if (file.name.split(".").pop() !== "pd" || file.name === "pd") {
        alert("Please upload a pd file.");
        return;
    }
    if (currentFile) {
        pdsend("pd dsp 0");
        Module.pd.closePatch(currentFile);
        FS.unlink("/" + currentFile);
    }
    const reader = new FileReader();
    reader.onload = function () {
        const uint8Array = new Uint8Array(reader.result);
        const content = new TextDecoder("utf-8").decode(uint8Array);
        openPatch(content, file.name);
    };
    reader.readAsArrayBuffer(file);
}


let cachedData = {};
async function getPatchData(url) {
    if(!cachedData[url])
        cachedData[url] = await (await fetch(`/api/patch/?url=${encodeURIComponent(url)}`,{method: 'GET'})).json();
    return cachedData[url];
}

// called after Module.mainInit() is called
async function init() {
    const urlParams = new URLSearchParams(window.location.search);
    let patchURL = urlParams.get("url");
    let content = "";
    let filename = "";
    if (!patchURL) {
        patchURL = "default.pd";
    }
    if (patchURL) {
        filename = patchURL.split("/").pop();
        const patchData = await getPatchData(patchURL);
        if (patchData.error) {
            alert(`Failed to access the file ${patchURL}`);
            loading.style.display = "none";
            return;
        }
        else {
            content = patchData.content;
        }
    }
    if (!content) {
        // we should not reach this because above we now
        // always make patchURL have a default value
        alert("Bug: init function");
        loading.style.display = "none";
        return;
    }
    await openPatch(content, filename);
    loading.style.display = "none";
}

// drag & drop file uploading
function showFilter() {
    filter.style.display = "flex";
}

function hideFilter() {
    filter.style.display = "none";
}

document.addEventListener("dragenter", function (e) {
    e.preventDefault();
    showFilter();
});

filter.addEventListener("dragleave", function (e) {
    e.preventDefault();
    hideFilter();
});

document.addEventListener("dragover", function (e) {
    e.preventDefault();
});

filter.addEventListener("drop", function (e) {
    e.preventDefault();
    hideFilter();
    const file = e.dataTransfer.files[0];
    uploadPatch(file);
});

// disable context menu
window.oncontextmenu = function (e) {
    e = e || window.event;
    e.preventDefault();
    e.stopPropagation();
    return false;
};