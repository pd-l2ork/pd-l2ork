#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
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
static int numInChannels = 0;
static int numOutChannels = 2;
static int sampleRate = 44100;
static int ticksPerBuffer = 32;
static int blockSize = libpd_blocksize();
static int bufferSize = ticksPerBuffer * blockSize;
static float *inBuffer = nullptr;

void audioIn(void *userdata, Uint8 *stream, int len) {
    try {
        if (inBuffer != nullptr) {
            float *input = reinterpret_cast<float *>(stream);
            memcpy(inBuffer, input, bufferSize * numInChannels * sizeof(float));
        }
    }
    catch (...) {
        SDL_Log("Pd: could not copy input buffer");
    }
}

void audioOut(void *userdata, Uint8 *stream, int len) {
    if (inBuffer != nullptr) {
        float *output = reinterpret_cast<float *>(stream);
        if (libpd_process_float(ticksPerBuffer, inBuffer, output)) {
            SDL_Log("Pd: could not process output buffer");
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

EMSCRIPTEN_BINDINGS(my_module) {
    function("sendBang", &sendBang);
    function("sendFloat", &sendFloat);
    function("sendSymbol", &sendSymbol);
    function("startMessage", &startMessage);
    function("addFloat", &addFloat);
    function("addSymbol", &addSymbol);
    function("finishList", &finishList);
    function("finishMessage", &finishMessage);
}

void pdprint(const char *s) {
  printf("%s", s);
}

void pdnoteon(int ch, int pitch, int vel) {
  printf("noteon: %d %d %d\n", ch, pitch, vel);
}

void mainLoop(void) {
    libpd_poll_gui();
}

SDL_AudioDeviceID openAudioDevice(bool isAudioIn, SDL_AudioSpec &desired, SDL_AudioSpec &obtained) {
    desired.freq = sampleRate;
    desired.format = AUDIO_F32;
    desired.channels = isAudioIn ? numInChannels : numOutChannels;
    desired.samples = bufferSize;
    desired.callback = isAudioIn ? audioIn : audioOut;
    SDL_AudioDeviceID deviceID = SDL_OpenAudioDevice(nullptr, isAudioIn, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    const char *audioTypeStr = isAudioIn ? "Audio In" : "Audio Out";
    if (!deviceID) {
        SDL_Log("%s: Failed to open device: %s", audioTypeStr, SDL_GetError());
    }
    else {
        if (desired.freq != obtained.freq) {
            SDL_Log("%s: Desired sample rate was %d, but obtained %d", audioTypeStr, desired.freq, obtained.freq);
        }
        if (desired.format != obtained.format) {
            SDL_Log("%s: Desired format was %d, but obtained %d", audioTypeStr, desired.format, obtained.format);
        }
        if (desired.channels != obtained.channels) {
            SDL_Log("%s: Desired number of channels was %d, but obtained %d", audioTypeStr, desired.channels, obtained.channels);
        }
        if (desired.samples != obtained.samples) {
            SDL_Log("%s: Desired buffer size was %d, but obtained %d", audioTypeStr, desired.samples, obtained.samples);
        }
    }
    return deviceID;
}

int main(int argc, char **argv) {
    // initialize audio
    SDL_Init(SDL_INIT_AUDIO);
    SDL_AudioDeviceID deviceIn = 0, deviceOut = 0;
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
    libpd_set_printhook(pdprint);
    libpd_init();
    libpd_start_gui(const_cast<char *>("./"));
    libpd_init_audio(numInChannels, numOutChannels, sampleRate);

    // compute audio    [; pd dsp 1(
    libpd_start_message(1); // one entry in list
    libpd_add_float(1.0f);
    libpd_finish_message("pd", "dsp");

    // open patch       [; pd open file folder(
    libpd_add_to_search_path("./");
    libpd_openfile("test.pd", ".");

    // start audio processing
    if (deviceIn) {
        SDL_PauseAudioDevice(deviceIn, 0);
    }
    if (deviceOut) {
        SDL_PauseAudioDevice(deviceOut, 0);
    }
    emscripten_set_main_loop(mainLoop, 0, 1);
    
    // stop audio processing & close audio device
    if (deviceIn) {
        SDL_PauseAudioDevice(deviceIn, 1);
        SDL_CloseAudioDevice(deviceIn);
    }
    if (deviceOut) {
        SDL_PauseAudioDevice(deviceOut, 1);
        SDL_CloseAudioDevice(deviceOut);
    }
    if(inBuffer != nullptr) {
        delete[] inBuffer;
        inBuffer = nullptr;
    }
    return 0;
}
