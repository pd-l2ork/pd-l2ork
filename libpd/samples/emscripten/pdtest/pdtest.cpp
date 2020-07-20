#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <dirent.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include "z_libpd.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#endif

using namespace emscripten;
static SDL_AudioDeviceID deviceIn = 0;
static SDL_AudioDeviceID deviceOut = 0;
static int numInChannels = 0;
static int numOutChannels = 2;
static int sampleRate = 44100;
static int ticksPerBuffer = 16;
static int blockSize = libpd_blocksize();
static int bufferSize = ticksPerBuffer * blockSize;
static float *inBuffer = nullptr;
static bool isBufferCleared = true;

void audioIn(void *userdata, Uint8 *stream, int len) {
    try {
        if (inBuffer != nullptr) {
            float *input = reinterpret_cast<float *>(stream);
            memcpy(inBuffer, input, bufferSize * numInChannels * sizeof(float));
        }
    }
    catch (...) {
        SDL_Log("pd: could not copy input buffer");
    }
}

void audioOut(void *userdata, Uint8 *stream, int len) {
    if (inBuffer != nullptr) {
        float *output = reinterpret_cast<float *>(stream);
        if (canvas_dspstate == 0 && !isBufferCleared) {
            sys_lock();
            memset(inBuffer, 0, numInChannels * bufferSize * sizeof(float));
            memset(output, 0, numOutChannels * bufferSize * sizeof(float));
            sys_unlock();
            isBufferCleared = true;
        }
        if (canvas_dspstate == 1 && isBufferCleared) {
            isBufferCleared = false;
        }
        if (libpd_process_float(ticksPerBuffer, inBuffer, output)) {
            SDL_Log("pd: could not process output buffer");
        }
    }
}

int sendBang(const std::string &recv) {
    return libpd_bang(recv.c_str());
}

int sendFloat(const std::string &recv, float f) {
    return libpd_float(recv.c_str(), f);
}

int sendSymbol(const std::string &recv, const std::string &s) {
    return libpd_symbol(recv.c_str(), s.c_str());
}

int startMessage(int maxlen) {
    return libpd_start_message(maxlen);
}

void addFloat(float f) {
    libpd_add_float(f);
}

void addSymbol(const std::string &s) {
    libpd_add_symbol(s.c_str());
}

int finishList(const std::string &recv) {
    return libpd_finish_list(recv.c_str());
}

int finishMessage(const std::string &recv, const std::string &msg) {
    return libpd_finish_message(recv.c_str(), msg.c_str());
}

void addToSearchPath(const std::string &path) {
    libpd_add_to_search_path(path.c_str());
}

void openPatch(const std::string &name, const std::string &dir) {
    libpd_openfile(name.c_str(), dir.c_str());
}

void closePatch(const std::string &name) {
    const std::string &recv = std::string("pd-") + name;
    libpd_start_message(1);
    libpd_add_float(1);
    libpd_finish_message(recv.c_str(), "menuclose");
}

EMSCRIPTEN_BINDINGS(my_module) {
    function("sendBang", &sendBang);
    function("sendFloat", &sendFloat);
    function("sendSymbol", &sendSymbol);
    function("startMessage", &startMessage);
    function("addFloat", &addFloat);
    function("addSymbol", &addSymbol);
    function("finishList", &finishList);
    function("finishMessage", &finishMessage);
    function("addToSearchPath", &addToSearchPath);
    function("openPatch", &openPatch);
    function("closePatch", &closePatch);
}

void mainLoop(void) {
}

SDL_AudioDeviceID openAudioDevice(bool isAudioIn, SDL_AudioSpec &desired, SDL_AudioSpec &obtained) {
    desired.freq = sampleRate;
    desired.format = AUDIO_F32;
    desired.channels = isAudioIn ? numInChannels : numOutChannels;
    desired.samples = bufferSize;
    desired.callback = isAudioIn ? audioIn : audioOut;
    SDL_AudioDeviceID deviceID = SDL_OpenAudioDevice(nullptr, isAudioIn, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    const char *audioTypeStr = isAudioIn ? "audio in" : "audio out";
    if (!deviceID) {
        SDL_Log("%s: failed to open device: %s", audioTypeStr, SDL_GetError());
    }
    else {
        if (desired.freq != obtained.freq) {
            SDL_Log("%s: desired sample rate was %d, but obtained %d", audioTypeStr, desired.freq, obtained.freq);
        }
        if (desired.format != obtained.format) {
            SDL_Log("%s: desired format was %d, but obtained %d", audioTypeStr, desired.format, obtained.format);
        }
        if (desired.channels != obtained.channels) {
            SDL_Log("%s: desired number of channels was %d, but obtained %d", audioTypeStr, desired.channels, obtained.channels);
        }
        if (desired.samples != obtained.samples) {
            SDL_Log("%s: desired buffer size was %d, but obtained %d", audioTypeStr, desired.samples, obtained.samples);
        }
    }
    return deviceID;
}

void pauseAudioDevice(bool pauseOn) {
    if (deviceIn) {
        SDL_PauseAudioDevice(deviceIn, pauseOn);
    }
    if (deviceOut) {
        SDL_PauseAudioDevice(deviceOut, pauseOn);
    }
}

void closeAudioDevice() {
    if (deviceIn) {
        SDL_CloseAudioDevice(deviceIn);
    }
    if (deviceOut) {
        SDL_CloseAudioDevice(deviceOut);
    }
}

int main(int argc, char **argv) {
    
    // initialize audio
    SDL_Init(SDL_INIT_AUDIO);
    SDL_AudioSpec desired, obtained;
    if (numInChannels) {
        deviceIn = openAudioDevice(true, desired, obtained);
        numInChannels = obtained.channels;
        sampleRate = obtained.freq;
        bufferSize = obtained.samples;
        ticksPerBuffer = bufferSize / blockSize;
    }
    if (numOutChannels) {
        deviceOut = openAudioDevice(false, desired, obtained);
        numOutChannels = obtained.channels;
        sampleRate = obtained.freq;
        bufferSize = obtained.samples;
        ticksPerBuffer = bufferSize / blockSize;
    }
    inBuffer = new float[numInChannels * bufferSize];
    
    // initialize libpd
    libpd_init();
    libpd_start_gui(const_cast<char *>("/"));
    libpd_init_audio(numInChannels, numOutChannels, sampleRate);
    
    // add internals help path
    const std::string &internals_help_path = std::string(DST_DOC_DIR) + std::string("/5.reference");
    libpd_add_to_help_path(internals_help_path.c_str());
    
    // add externals search/help path
    libpd_add_to_search_path(DST_EXT_DIR);
    libpd_add_to_help_path(DST_EXT_DIR);
    DIR *ext_dir = opendir(DST_EXT_DIR);
    if (ext_dir) {
        struct dirent *dir;
        const std::string &externals_prefix = std::string(DST_EXT_DIR) + std::string("/");
        while ((dir = readdir(ext_dir)) != nullptr) {
            if (dir->d_name[0] != '.') {
                const std::string &externals_path = externals_prefix + std::string(dir->d_name);
                libpd_add_to_search_path(externals_path.c_str());
                libpd_add_to_help_path(externals_path.c_str());
            }
        }
        closedir(ext_dir);
    }

    // start audio processing
    pauseAudioDevice(false);
    emscripten_set_main_loop(mainLoop, 0, true);
    
    // stop audio processing & close audio device
    pauseAudioDevice(true);
    closeAudioDevice();
    if (inBuffer != nullptr) {
        delete[] inBuffer;
        inBuffer = nullptr;
    }
    return 0;
}
