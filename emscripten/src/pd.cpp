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

#include "pd.hpp"
#include "js.hpp"
#include "z_libpd.h"
#include "m_pd.h"
#include "s_stuff.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <emscripten.h>
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <cstdio>
#include <cstring>

Pd::Pd() {
    if (SDL_Init(SDL_INIT_AUDIO)) {
        std::cerr << "Pd: unable to initialize SDL: " << SDL_GetError() << std::endl;
        SDL_Quit();
        emscripten_force_exit(1);
    }
    inDev = 0;
    outDev = 0;
    format = AUDIO_F32;
    bInited = false;
    inBuffer = nullptr;
    bBufferCleared = true;
    clear();
}

Pd::~Pd() {
    clear();
    unsubscribeAll();
    SDL_Quit();
    emscripten_force_exit(1);
}

bool Pd::init(int numInChannels, int numOutChannels,
                 int sampleRate, int ticksPerBuffer) {
    bool isDspOn = static_cast<bool>(canvas_dspstate);
    
    // clear before reinit
    clear();
    
    // open audio devices
    inDev = openAudioDevice(true, numInChannels, sampleRate, ticksPerBuffer);
    outDev = openAudioDevice(false, numOutChannels, sampleRate, ticksPerBuffer);
    if (!outDev) {
        std::cerr << "Pd: could not init pd" << std::endl;
        clear();
        return false;
    }
    // set receivers
    if (sys_nogui) {
        libpd_set_printhook(receivePrint);
    }
    libpd_set_banghook(receiveBang);
    libpd_set_floathook(receiveFloat);
    libpd_set_symbolhook(receiveSymbol);
    libpd_set_listhook(receiveList);
    libpd_set_messagehook(receiveMessage);
    libpd_set_noteonhook(receiveNoteOn);
    libpd_set_controlchangehook(receiveControlChange);
    libpd_set_programchangehook(receiveProgramChange);
    libpd_set_pitchbendhook(receivePitchBend);
    libpd_set_aftertouchhook(receiveAftertouch);
    libpd_set_polyaftertouchhook(receivePolyAftertouch);
    libpd_set_midibytehook(receiveMidiByte);
    
    // init libpd, then start gui (should only be called once)
    if (!libpd_init() && !sys_nogui && libpd_start_gui(const_cast<char *>("/"))) {
        std::cerr << "Pd: could not start gui" << std::endl;
        clear();
        return false;
    }
    // init audio
    if (libpd_init_audio(inChannels, outChannels, srate)) {
        std::cerr << "Pd: could not init audio" << std::endl;
        clear();
        return false;
    }
    inBuffer = new float[inChannels * bsize];
    if (isDspOn) {
        computeAudio(true);
    }
    bInited = true;
    return bInited;
}

void Pd::clear() {
    if (bInited) {
        computeAudio(false);
    }
    if (inBuffer) {
        delete[] inBuffer;
        inBuffer = nullptr;
    }
    libpd_set_printhook(nullptr);
    libpd_set_banghook(nullptr);
    libpd_set_floathook(nullptr);
    libpd_set_symbolhook(nullptr);
    libpd_set_listhook(nullptr);
    libpd_set_messagehook(nullptr);
    libpd_set_noteonhook(nullptr);
    libpd_set_controlchangehook(nullptr);
    libpd_set_programchangehook(nullptr);
    libpd_set_pitchbendhook(nullptr);
    libpd_set_aftertouchhook(nullptr);
    libpd_set_polyaftertouchhook(nullptr);
    libpd_set_midibytehook(nullptr);
    ticks = 0;
    bsize = 0;
    srate = 0;
    inChannels = 0;
    outChannels = 0;
    bInited = false;
    
    // close audio devices
    closeAudioDevice(inDev);
    closeAudioDevice(outDev);
}

void Pd::addToSearchPath(const std::string& path) {
    libpd_add_to_search_path(path.c_str());
}

void Pd::clearSearchPath() {
    libpd_clear_search_path();
}

void Pd::addToHelpPath(const std::string& path) {
    libpd_add_to_help_path(path.c_str());
}

void Pd::clearHelpPath() {
    libpd_clear_help_path();
}

void Pd::openPatch(const std::string &name, const std::string &dir) {
    libpd_openfile(name.c_str(), dir.c_str());
}

void Pd::closePatch(const std::string &name) {
    const std::string &recv = std::string("pd-") + name;
    libpd_start_message(1);
    libpd_add_float(1);
    libpd_finish_message(recv.c_str(), "menuclose");
}

void Pd::computeAudio(bool state) {
    libpd_start_message(1);
    libpd_add_float(static_cast<float>(state));
    libpd_finish_message("pd", "dsp");
}

void Pd::sendBang(const std::string& dest) {
    libpd_bang(dest.c_str());
}
void Pd::sendFloat(const std::string& dest, float value) {
    libpd_float(dest.c_str(), value);
}

void Pd::sendSymbol(const std::string& dest, const std::string& symbol) {
    libpd_symbol(dest.c_str(), symbol.c_str());
}

void Pd::startMessage(int maxlen) {
    libpd_start_message(maxlen);
}

void Pd::addFloat(float num) {
    libpd_add_float(num);
}

void Pd::addSymbol(const std::string& symbol) {
    libpd_add_symbol(symbol.c_str());
}

void Pd::finishList(const std::string& dest) {
    libpd_finish_list(dest.c_str());
}

void Pd::finishMessage(const std::string& dest, const std::string& msg) {
    libpd_finish_message(dest.c_str(), msg.c_str());
}

void Pd::sendNoteOn(int channel, int pitch, int velocity) {
    libpd_noteon(channel - 1, pitch, velocity);
}

void Pd::sendControlChange(int channel, int control, int value) {
    libpd_controlchange(channel - 1, control, value);
}

void Pd::sendProgramChange(int channel, int program) {
    libpd_programchange(channel - 1, program - 1);
}

void Pd::sendPitchBend(int channel, int value) {
    libpd_pitchbend(channel - 1, value);
}

void Pd::sendAftertouch(int channel, int value) {
    libpd_aftertouch(channel - 1, value);
}

void Pd::sendPolyAftertouch(int channel, int pitch, int value) {
    libpd_polyaftertouch(channel - 1, pitch, value);
}

void Pd::subscribe(const std::string &source) {
    if (sources.find(source) != sources.end()) {
        std::cout << "Pd: ignoring already subscribed source '" << source << "'" << std::endl;
        return;
    }
    void *pointer = libpd_bind(source.c_str());
    if (pointer) {
        sources.insert(std::pair<std::string, void *>(source, pointer));
    }
}

void Pd::unsubscribe(const std::string &source) {
    std::map<std::string, void *>::iterator iter;
    iter = sources.find(source);
    if (iter == sources.end()) {
        std::cout << "Pd: ignoring already unsubscribed source '" << source << "'" << std::endl;
        return;
    }
    libpd_unbind(iter->second);
    sources.erase(iter);
}

void Pd::unsubscribeAll() {
    std::map<std::string, void *>::iterator iter;
    for (iter = sources.begin(); iter != sources.end(); ++iter) {
        libpd_unbind(iter->second);
    }
    sources.clear();
}

bool Pd::isInited() const {
    return bInited;
}

bool Pd::getNoGui() const {
    return sys_nogui;
}

void Pd::setNoGui(bool noGui) {
    sys_nogui = noGui;
}

SDL_AudioDeviceID Pd::getInDeviceID() const {
    return inDev;
}

SDL_AudioDeviceID Pd::getOutDeviceID() const {
    return outDev;
}

int Pd::getTicksPerBuffer() const {
    return ticks;
}

int Pd::getBlockSize() const {
    return libpd_blocksize();
}

int Pd::getBufferSize() const {
    return bsize;
}

int Pd::getSampleRate() const {
    return srate;
}

SDL_AudioFormat Pd::getAudioFormat() const {
    return format;
}

int Pd::getNumInChannels() const {
    return inChannels;
}

int Pd::getNumOutChannels() const {
    return outChannels;
}

SDL_AudioDeviceID Pd::openAudioDevice(bool isInput, int numChannels,
                                        int sampleRate, int ticksPerBuffer) {
    if (isInput && !numChannels) {
        return 0;
    }
    SDL_AudioSpec desired, obtained;
    desired.channels = static_cast<Uint8>(numChannels);
    desired.freq = sampleRate;
    desired.format = format;
    desired.samples = static_cast<Uint16>(ticksPerBuffer * getBlockSize());
    desired.callback = isInput ? forwardAudioIn : forwardAudioOut;
    desired.userdata = this;
    SDL_AudioDeviceID deviceID = SDL_OpenAudioDevice(nullptr, isInput, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    const std::string &type = isInput ? "audio input device: " : "audio output device: ";
    if (!deviceID) {
        std::cerr << "Pd: failed to open " << type << SDL_GetError() << std::endl;
        return deviceID;
    }
    if (desired.channels != obtained.channels) {
        std::cout << "Pd: " << type << "desired number of channels was " <<
        static_cast<int>(desired.channels) << ", but obtained " <<
        static_cast<int>(obtained.channels) << std::endl;
    }
    if (desired.freq != obtained.freq) {
        std::cout << "Pd: " << type << "desired sample rate was " <<
        static_cast<int>(desired.freq) << ", but obtained " <<
        static_cast<int>(obtained.freq) << std::endl;
    }
    if (desired.format != obtained.format) {
        std::cout << "Pd: " << type << "desired format was " <<
        static_cast<int>(desired.format) << ", but obtained " <<
        static_cast<int>(obtained.format) << std::endl;
    }
    if (desired.samples != obtained.samples) {
        std::cout << "Pd: " << type << "desired buffer size was " <<
        static_cast<int>(desired.samples) << ", but obtained " <<
        static_cast<int>(obtained.samples) << std::endl;
    }
    if (isInput) {
        inChannels = static_cast<int>(obtained.channels);
    }
    else {
        outChannels = static_cast<int>(obtained.channels);
    }
    srate = obtained.freq;
    format = obtained.format;
    bsize = static_cast<int>(obtained.samples);
    ticks = bsize / getBlockSize();
    SDL_PauseAudioDevice(deviceID, false);
    return deviceID;
}

void Pd::closeAudioDevice(SDL_AudioDeviceID deviceID) {
    if (deviceID) {
        SDL_PauseAudioDevice(deviceID, true);
        SDL_CloseAudioDevice(deviceID);
        deviceID = 0;
    }
}

void Pd::forwardAudioIn(void *userdata, Uint8 *stream, int len) {
    static_cast<Pd*>(userdata)->audioIn(stream, len);
}

void Pd::forwardAudioOut(void *userdata, Uint8 *stream, int len) {
    static_cast<Pd*>(userdata)->audioOut(stream, len);
}

void Pd::audioIn(Uint8 *stream, int len) {
    try {
        if (inBuffer) {
            float *input = reinterpret_cast<float *>(stream);
            memcpy(inBuffer, input, inChannels * bsize * sizeof(float));
        }
    }
    catch (...) {
        std::cerr << "Pd: could not copy input buffer" << std::endl;
    }
}

void Pd::audioOut(Uint8 *stream, int len) {
    if (inBuffer) {
        float *output = reinterpret_cast<float *>(stream);
        if (!canvas_dspstate && !bBufferCleared) {
            sys_lock();
            memset(inBuffer, 0, inChannels * bsize * sizeof(float));
            memset(output, 0, outChannels * bsize * sizeof(float));
            sys_unlock();
            bBufferCleared = true;
        }
        if (canvas_dspstate && bBufferCleared) {
            bBufferCleared = false;
        }
        if (libpd_process_float(ticks, inBuffer, output)) {
            std::cerr << "Pd: could not process output buffer" << std::endl;
        }
    }
}

void Pd::receivePrint(const char *s) {
    __Pd_receivePrint(s);
}

void Pd::receiveBang(const char *source) {
    __Pd_receiveBang(source);
}

void Pd::receiveFloat(const char *source, float value) {
    __Pd_receiveFloat(source, value);
}

void Pd::receiveSymbol(const char *source, const char *symbol) {
    __Pd_receiveSymbol(source, symbol);
}

void Pd::receiveList(const char *source, int argc, t_atom *argv) {
    
    // create a comma-separated string
    std::ostringstream ss;
    for (int i = 0; i < argc; ++i) {
        if (argv[i].a_type == A_FLOAT) {
            ss << argv[i].a_w.w_float;
        }
        else if (argv[i].a_type == A_SYMBOL) {
            ss << argv[i].a_w.w_symbol->s_name;
        }
        if (i != argc - 1) {
            ss << ',';
        }
    }
    __Pd_receiveList(source, ss.str().c_str());
}

void Pd::receiveMessage(const char *source, const char *symbol,
                        int argc, t_atom *argv) {
    
    // create a comma-separated string
    std::ostringstream ss;
    for (int i = 0; i < argc; ++i) {
        if (argv[i].a_type == A_FLOAT) {
            ss << argv[i].a_w.w_float;
        }
        else if (argv[i].a_type == A_SYMBOL) {
            ss << argv[i].a_w.w_symbol->s_name;
        }
        if (i != argc - 1) {
            ss << ',';
        }
    }
    __Pd_receiveMessage(source, symbol, ss.str().c_str());
}

void Pd::receiveNoteOn(int channel, int pitch, int velocity) {
    __Pd_receiveNoteOn(channel + 1, pitch, velocity);
}

void Pd::receiveControlChange(int channel, int controller, int value) {
    __Pd_receiveControlChange(channel + 1, controller, value);
}

void Pd::receiveProgramChange(int channel, int value) {
    __Pd_receiveProgramChange(channel + 1, value + 1);
}

void Pd::receivePitchBend(int channel, int value) {
    __Pd_receivePitchBend(channel + 1, value);
}

void Pd::receiveAftertouch(int channel, int value) {
    __Pd_receiveAftertouch(channel + 1, value);
}

void Pd::receivePolyAftertouch(int channel, int pitch, int value) {
    __Pd_receivePolyAftertouch(channel + 1, pitch, value);
}

void Pd::receiveMidiByte(int port, int byte) {
    __Pd_receiveMidiByte(port + 1, byte);
}
