const isMobile = (typeof window.orientation !== "undefined") || (navigator.userAgent.indexOf('IEMobile') !== -1);
const canvas = document.getElementById("canvas");
const loading = document.getElementById("loading");
const filter = document.getElementById("filter");
let currentFile = "";
let canvasWidth = 450;
let canvasHeight = 300;
let fontSize = 12;
let subscribedData = {};

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
        Module.Pd.receivePrint = function (s) {
            console.log(s);
        }

        // receive from pd's subscribed sources
        Module.Pd.receiveBang = function (source) {
            if (source in subscribedData) {
                for (const data of subscribedData[source]) {
                    switch (data.type) {
                        case "bng":
                            gui_bng_update_circle(data);
                            break;
                        case "tgl":
                            data.value = data.value ? 0 : data.default_value;
                            gui_tgl_update_cross(data);
                            break;
                        case "vsl":
                        case "hsl":
                            gui_slider_bang(data);
                            break;
                        case "vradio":
                        case "hradio":
                            Module.pd.sendFloat(data.send, data.value);
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
                            break;
                        case "tgl":
                            data.value = value;
                            gui_tgl_update_cross(data);
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
                            Module.pd.sendFloat(data.send, data.value);
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
                            gui_tgl_update_cross(data);
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
                            Module.pd.sendFloat(data.send, data.value);
                            break;
                    }
                }
            }
        }

        Module.Pd.receiveMessage = function (source, symbol, list) {
            if (source in subscribedData) {
                for (const data of subscribedData[source]) {
                    switch (data.type) {
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
                                    data.send = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive = list[0];
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
                                    data.send = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive = list[0];
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
                                    data.send = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive = list[0];
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
                                    data.send = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive = list[0];
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
                                    data.send = list[0];
                                    break;
                                case "receive":
                                    gui_unsubscribe(data);
                                    data.receive = list[0];
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
            Module.pd.addFloat(parseFloat(array[i]));
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
function create_item(type, args) {
    var item = document.createElementNS("http://www.w3.org/2000/svg", type);
    if (args !== null) {
        configure_item(item, args);
    }
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
    if (typeof col === "string") {
        return col;
    }
    col = -1 - col;
    col = ((col & 0x3f000) << 6) | ((col & 0xfc0) << 4) | ((col & 0x3f) << 2);
    return "#" + ("000000" + col.toString(16)).slice(-6);
}

function gui_subscribe(data) {
    if (data.receive in subscribedData) {
        subscribedData[data.receive].push(data);
    }
    else {
        subscribedData[data.receive] = [data];
    }
    Module.pd.subscribe(data.receive);
}

function gui_unsubscribe(data) {
    if (data.receive in subscribedData) {
        const len = subscribedData[data.receive].length;
        for (let i = 0; i < len; i++) {
            if (subscribedData[data.receive][i].id === data.id) {
                Module.pd.unsubscribe(data.receive);
                subscribedData[data.receive].splice(i, 1);
                if (!subscribedData[data.receive].length) {
                    delete subscribedData[data.receive];
                }
                break;
            }
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
        y: data.y_pos + data.y_off,
        "font-family": iemgui_fontfamily(data.font),
        "font-weight": "normal",
        "font-size": `${data.fontsize}px`,
        fill: colfromload(data.label_color),
        transform: `translate(0, ${data.fontsize / 2 * 0.6})`, // note: modified
        id: `${data.id}_text`,
        class: "unclickable"
    }
}

function gui_mousepoint(e) { // transforms the mouse position
    let point = canvas.createSVGPoint();
    point.x = e.clientX;
    point.y = e.clientY;
    point = point.matrixTransform(canvas.getScreenCTM().inverse());
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
    Module.pd.sendBang(data.send);
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
    Module.pd.sendFloat(data.send, data.value);
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
        y2 += 3;
        r = y2 - 3 - (data.value + 50) / 100;
        p1 = x1 + 2 * 0.75; // note: modified
        p2 = r;
        p3 = x2 - 2 * 0.75; // note: modified
        p4 = r;
    }
    else {
        //x1 -= 3; // note: modified
        r = x1 + 3 + (data.value + 50) / 100;
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
    Module.pd.sendFloat(data.send, out);
}

function gui_slider_onmousedown(data, e, id) {
    const p = gui_mousepoint(e);
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
        value: data.value
    };
}

function gui_slider_onmousemove(e, id) {
    if (id in touches) {
        const { data, point, value } = touches[id];
        const p = gui_mousepoint(e);
        if (data.type === "vsl") {
            data.value = Math.max(Math.min(value + (point.y - p.y) * 100, (data.height - 1) * 100), 0);
        }
        else {
            data.value = Math.max(Math.min(value + (p.x - point.x) * 100, (data.width - 1) * 100), 0);
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
                    const line = create_item("line", gui_radio_line(data, x1, yi, x1 + d, yi, i));
                    data.lines.push(line);
                }
                const button = create_item("rect", gui_radio_button(data, x1 + s, yi + s, x1 + d - s, yi + d - s, i, on === i));
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
                    const line = create_item("line", gui_radio_line(data, xi, y1, xi, y1 + d, i));
                    data.lines.push(line);
                }
                const button = create_item("rect", gui_radio_button(data, xi + s, y1 + s, xi + d - s, yi + d - s, i, on === i));
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
    const p = gui_mousepoint(e);
    if (data.type === "vradio") {
        data.value = Math.floor((p.y - data.y_pos) / data.size);
    }
    else {
        data.value = Math.floor((p.x - data.x_pos) / data.size);
    }
    gui_radio_update_button(data);
    Module.pd.sendFloat(data.send, data.value);
}

// drag events
if (isMobile) {
    window.addEventListener("touchmove", function (e) {
        e = e || window.event;
        for (const touch of e.changedTouches) {
            gui_slider_onmousemove(touch, touch.identifier);
        }
    });
    window.addEventListener("touchend", function (e) {
        e = e || window.event;
        for (const touch of e.changedTouches) {
            gui_slider_onmouseup(touch.identifier);
        }
    });
    window.addEventListener("touchcancel", function (e) {
        e = e || window.event;
        for (const touch of e.changedTouches) {
            gui_slider_onmouseup(touch.identifier);
        }
    });
}
else {
    window.addEventListener("mousemove", function (e) {
        e = e || window.event;
        gui_slider_onmousemove(e, 0);
    });
    window.addEventListener("mouseup", function (e) {
        gui_slider_onmouseup(0);
    });
    window.addEventListener("mouseleave", function (e) {
        gui_slider_onmouseup(0);
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

function gui_text_text(data, line_index) {
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
function openPatch(content, filename) {
    console.log(`patch: ${filename}`);
    let maxNumInChannels = 0;
    let canvasLevel = 0; // 0: no canvas, 1: main canvas, 2~: subcanvases
    let id = 0; // gui id
    while (canvas.lastChild) { // clear svg
        canvas.removeChild(canvas.lastChild);
    }
    Module.pd.unsubscribeAll();
    for (const source in subscribedData) {
        delete subscribedData[source];
    }
    const lines = content.split(";\n");
    for (let line of lines) {
        line = line.replace(/[\r\n]+/g, " ").trim(); // remove newlines & carriage returns
        const args = line.split(" ");
        const type = args.slice(0, 2).join(" ");
        switch (type) {
            case "#N canvas":
                canvasLevel++;
                if (canvasLevel === 1 && args.length === 7) { // should be called only once
                    canvasWidth = parseInt(args[4]);
                    canvasHeight = parseInt(args[5]);
                    fontSize = parseInt(args[6]);
                    canvas.setAttributeNS(null, "viewBox", `0 0 ${canvasWidth} ${canvasHeight}`);
                    console.log(`canvas size: ${canvasWidth}x${canvasHeight}`);
                }
                break;
            case "#X restore":
                canvasLevel--;
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
                                    const numInChannels = parseInt(args[i]);
                                    if (numInChannels > maxNumInChannels) {
                                        maxNumInChannels = numInChannels > 2 ? 2 : numInChannels;
                                    }
                                }
                            }
                            break;
                        case "bng":
                            if (canvasLevel === 1 && args.length >= 19 && args[9] !== "empty" && args[10] !== "empty") {
                                const data = {};
                                data.x_pos = parseInt(args[2]);
                                data.y_pos = parseInt(args[3]);
                                data.type = args[4];
                                data.size = parseInt(args[5]);
                                data.hold = parseInt(args[6]);
                                data.interrupt = parseInt(args[7]);
                                data.init = parseInt(args[8]);
                                data.send = args[9];
                                data.receive = args[10];
                                data.label = args[11] === "empty" ? "" : args[11];
                                data.x_off = parseInt(args[12]);
                                data.y_off = parseInt(args[13]);
                                data.font = parseInt(args[14]);
                                data.fontsize = parseInt(args[15]);
                                data.bg_color = isNaN(args[16]) ? args[16] : parseInt(args[16]);
                                data.fg_color = isNaN(args[17]) ? args[17] : parseInt(args[17]);
                                data.label_color = isNaN(args[18]) ? args[18] : parseInt(args[18]);
                                data.id = `${data.type}_${id++}`;

                                // create svg
                                data.rect = create_item("rect", gui_bng_rect(data));
                                data.circle = create_item("circle", gui_bng_circle(data));
                                data.text = create_item("text", gui_bng_text(data));
                                data.text.textContent = data.label;

                                // handle event
                                data.flashed = false;
                                data.interrupt_timer = null;
                                data.hold_timer = null;
                                if (isMobile) {
                                    data.rect.addEventListener("touchstart", function () {
                                        gui_bng_onmousedown(data);
                                    });
                                }
                                else {
                                    data.rect.addEventListener("mousedown", function () {
                                        gui_bng_onmousedown(data);
                                    });
                                }
                                // subscribe receiver
                                gui_subscribe(data);
                            }
                            break;
                        case "tgl":
                            if (canvasLevel === 1 && args.length >= 19 && args[7] !== "empty" && args[8] !== "empty") {
                                const data = {};
                                data.x_pos = parseInt(args[2]);
                                data.y_pos = parseInt(args[3]);
                                data.type = args[4];
                                data.size = parseInt(args[5]);
                                data.init = parseInt(args[6]);
                                data.send = args[7];
                                data.receive = args[8];
                                data.label = args[9] === "empty" ? "" : args[9];
                                data.x_off = parseInt(args[10]);
                                data.y_off = parseInt(args[11]);
                                data.font = parseInt(args[12]);
                                data.fontsize = parseInt(args[13]);
                                data.bg_color = isNaN(args[14]) ? args[14] : parseInt(args[14]);
                                data.fg_color = isNaN(args[15]) ? args[15] : parseInt(args[15]);
                                data.label_color = isNaN(args[16]) ? args[16] : parseInt(args[16]);
                                data.init_value = parseFloat(args[17]);
                                data.default_value = parseFloat(args[18]);
                                data.value = data.init && data.init_value ? data.default_value : 0;
                                data.id = `${data.type}_${id++}`;

                                // create svg
                                data.rect = create_item("rect", gui_tgl_rect(data));
                                data.cross1 = create_item("polyline", gui_tgl_cross1(data));
                                data.cross2 = create_item("polyline", gui_tgl_cross2(data));
                                data.text = create_item("text", gui_tgl_text(data));
                                data.text.textContent = data.label;

                                // handle event
                                if (isMobile) {
                                    data.rect.addEventListener("touchstart", function () {
                                        gui_tgl_onmousedown(data);
                                    });
                                }
                                else {
                                    data.rect.addEventListener("mousedown", function () {
                                        gui_tgl_onmousedown(data);
                                    });
                                }
                                // subscribe receiver
                                gui_subscribe(data);
                            }
                            break;
                        case "vsl":
                        case "hsl":
                            if (canvasLevel === 1 && args.length >= 23 && args[11] !== "empty" && args[12] !== "empty") {
                                const data = {};
                                data.x_pos = parseInt(args[2]);
                                data.y_pos = parseInt(args[3]);
                                data.type = args[4];
                                data.width = parseInt(args[5]);
                                data.height = parseInt(args[6]);
                                data.bottom = parseInt(args[7]);
                                data.top = parseInt(args[8]);
                                data.log = parseInt(args[9]);
                                data.init = parseInt(args[10]);
                                data.send = args[11];
                                data.receive = args[12];
                                data.label = args[13] === "empty" ? "" : args[13];
                                data.x_off = parseInt(args[14]);
                                data.y_off = parseInt(args[15]);
                                data.font = parseInt(args[16]);
                                data.fontsize = parseInt(args[17]);
                                data.bg_color = isNaN(args[18]) ? args[18] : parseInt(args[18]);
                                data.fg_color = isNaN(args[19]) ? args[19] : parseInt(args[19]);
                                data.label_color = isNaN(args[20]) ? args[20] : parseInt(args[20]);
                                data.default_value = parseFloat(args[21]);
                                data.steady_on_click = parseFloat(args[22]);
                                data.value = data.init ? data.default_value : 0;
                                data.id = `${data.type}_${id++}`;

                                // create svg
                                data.rect = create_item("rect", gui_slider_rect(data));
                                data.indicator = create_item("line", gui_slider_indicator(data));
                                data.text = create_item("text", gui_slider_text(data));
                                data.text.textContent = data.label;

                                // handle event
                                gui_slider_check_minmax(data);
                                if (isMobile) {
                                    data.rect.addEventListener("touchstart", function (e) {
                                        e = e || window.event;
                                        for (const touch of e.changedTouches) {
                                            gui_slider_onmousedown(data, touch, touch.identifier);
                                        }
                                    });
                                }
                                else {
                                    data.rect.addEventListener("mousedown", function (e) {
                                        e = e || window.event;
                                        gui_slider_onmousedown(data, e, 0);
                                    });
                                }
                                // subscribe receiver
                                gui_subscribe(data);
                            }
                            break;
                        case "vradio":
                        case "hradio":
                            if (canvasLevel === 1 && args.length >= 20 && args[9] !== "empty" && args[10] !== "empty") {
                                const data = {};
                                data.x_pos = parseInt(args[2]);
                                data.y_pos = parseInt(args[3]);
                                data.type = args[4];
                                data.size = parseInt(args[5]);
                                data.new_old = parseInt(args[6]);
                                data.init = parseInt(args[7]);
                                data.number = parseInt(args[8]) || 1;
                                data.send = args[9];
                                data.receive = args[10];
                                data.label = args[11] === "empty" ? "" : args[11];
                                data.x_off = parseInt(args[12]);
                                data.y_off = parseInt(args[13]);
                                data.font = parseInt(args[14]);
                                data.fontsize = parseInt(args[15]);
                                data.bg_color = isNaN(args[16]) ? args[16] : parseInt(args[16]);
                                data.fg_color = isNaN(args[17]) ? args[17] : parseInt(args[17]);
                                data.label_color = isNaN(args[18]) ? args[18] : parseInt(args[18]);
                                data.default_value = parseFloat(args[19]);
                                data.value = data.init ? data.default_value : 0;
                                data.id = `${data.type}_${id++}`;

                                // create svg
                                data.rect = create_item("rect", gui_radio_rect(data));
                                gui_radio_create_lines_buttons(data);
                                data.text = create_item("text", gui_radio_text(data));
                                data.text.textContent = data.label;

                                // handle event
                                if (isMobile) {
                                    data.rect.addEventListener("touchstart", function (e) {
                                        e = e || window.event;
                                        for (const touch of e.changedTouches) {
                                            gui_radio_onmousedown(data, touch);
                                        }
                                    });
                                }
                                else {
                                    data.rect.addEventListener("mousedown", function (e) {
                                        e = e || window.event;
                                        gui_radio_onmousedown(data, e);
                                    });
                                }
                                // subscribe receiver
                                gui_subscribe(data);
                            }
                            break;
                        case "cnv":
                            if (canvasLevel === 1 && args.length >= 18 && args[8] !== "empty" && args[9] !== "empty") {
                                const data = {};
                                data.x_pos = parseInt(args[2]);
                                data.y_pos = parseInt(args[3]);
                                data.type = args[4];
                                data.size = parseInt(args[5]);
                                data.width = parseInt(args[6]);
                                data.height = parseInt(args[7]);
                                data.send = args[8];
                                data.receive = args[9];
                                data.label = args[10] === "empty" ? "" : args[10];
                                data.x_off = parseInt(args[11]);
                                data.y_off = parseInt(args[12]);
                                data.font = parseInt(args[13]);
                                data.fontsize = parseInt(args[14]);
                                data.bg_color = isNaN(args[15]) ? args[15] : parseInt(args[15]);
                                data.label_color = isNaN(args[16]) ? args[16] : parseInt(args[16]);
                                data.unknown = parseFloat(args[17]);
                                data.id = `${data.type}_${id++}`;

                                // create svg
                                data.visible_rect = create_item("rect", gui_cnv_visible_rect(data));
                                data.selectable_rect = create_item("rect", gui_cnv_selectable_rect(data));
                                data.text = create_item("text", gui_cnv_text(data));
                                data.text.textContent = data.label;

                                // subscribe receiver
                                gui_subscribe(data);
                            }
                            break;
                    }
                }
                break;
            case "#X text":
                if (args.length > 4) {
                    const data = {};
                    data.type = args[1];
                    data.x_pos = parseInt(args[2]);
                    data.y_pos = parseInt(args[3]);
                    data.comment = [];
                    const lines = args.slice(4).join(" ").replace(/ \\,/g, ",").replace(/\\; /g, ";\n").replace(/ ;/g, ";").split("\n");
                    for (const line of lines) {
                        const lines = line.match(/.{1,60}(\s|$)/g);
                        for (const line of lines) {
                            data.comment.push(line.trim());
                        }
                    }
                    data.id = `${data.type}_${id++}`;

                    // create svg
                    data.texts = [];
                    for (let i = 0; i < data.comment.length; i++) {
                        const text = create_item("text", gui_text_text(data, i));
                        text.textContent = data.comment[i];
                        data.texts.push(text);
                    }
                }
                break;
        }
    }
    if (!canvasLevel) {
        alert("The main canvas not found in the pd file.");
        return;
    }
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
    const uint8Array = new TextEncoder().encode(content);
    FS.createDataFile("/", filename, uint8Array, true, true, true);
    currentFile = filename;
    Module.pd.openPatch(currentFile, "/");
    pdsend("pd dsp 1");
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

async function getPatchData(url) {
    const options = { method: "GET" };
    const res = await fetch(`/api/patch/?url=${url}`, options);
    const json = await res.json();
    return json;
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
    loading.style.display = "none";
    openPatch(content, filename);
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