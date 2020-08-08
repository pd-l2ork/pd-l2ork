#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <dirent.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include "z_libpd.h"
#include <emscripten.h>
#include <emscripten/bind.h>

using namespace emscripten;
static SDL_AudioDeviceID device_in = 0;
static SDL_AudioDeviceID device_out = 0;
static int num_in_channels = 1;
static int num_out_channels = 2;
static int samplerate = 44100;
static int ticks_per_buffer = 16;
static int blocksize = libpd_blocksize();
static int buffersize = ticks_per_buffer * blocksize;
static float *in_buffer = nullptr;
static bool is_buffer_cleared = true;

void audio_in(void *userdata, Uint8 *stream, int len) {
    try {
        if (in_buffer != nullptr) {
            float *input = reinterpret_cast<float *>(stream);
            memcpy(in_buffer, input, buffersize * num_in_channels * sizeof(float));
        }
    }
    catch (...) {
        SDL_Log("pd: could not copy input buffer");
    }
}

void audio_out(void *userdata, Uint8 *stream, int len) {
    if (in_buffer != nullptr) {
        float *output = reinterpret_cast<float *>(stream);
        if (canvas_dspstate == 0 && !is_buffer_cleared) {
            sys_lock();
            memset(in_buffer, 0, num_in_channels * buffersize * sizeof(float));
            memset(output, 0, num_out_channels * buffersize * sizeof(float));
            sys_unlock();
            is_buffer_cleared = true;
        }
        if (canvas_dspstate == 1 && is_buffer_cleared) {
            is_buffer_cleared = false;
        }
        if (libpd_process_float(ticks_per_buffer, in_buffer, output)) {
            SDL_Log("pd: could not process output buffer");
        }
    }
}

int send_bang(const std::string &recv) {
    return libpd_bang(recv.c_str());
}

int send_float(const std::string &recv, float f) {
    return libpd_float(recv.c_str(), f);
}

int send_symbol(const std::string &recv, const std::string &s) {
    return libpd_symbol(recv.c_str(), s.c_str());
}

int start_message(int maxlen) {
    return libpd_start_message(maxlen);
}

void add_float(float f) {
    libpd_add_float(f);
}

void add_symbol(const std::string &s) {
    libpd_add_symbol(s.c_str());
}

int finish_list(const std::string &recv) {
    return libpd_finish_list(recv.c_str());
}

int finish_message(const std::string &recv, const std::string &msg) {
    return libpd_finish_message(recv.c_str(), msg.c_str());
}

void add_to_search_path(const std::string &path) {
    libpd_add_to_search_path(path.c_str());
}

void open_patch(const std::string &name, const std::string &dir) {
    libpd_openfile(name.c_str(), dir.c_str());
}

void close_patch(const std::string &name) {
    const std::string &recv = std::string("pd-") + name;
    libpd_start_message(1);
    libpd_add_float(1);
    libpd_finish_message(recv.c_str(), "menuclose");
}

EMSCRIPTEN_BINDINGS(my_module) {
    function("send_bang", &send_bang);
    function("send_float", &send_float);
    function("send_symbol", &send_symbol);
    function("start_message", &start_message);
    function("add_float", &add_float);
    function("add_symbol", &add_symbol);
    function("finish_list", &finish_list);
    function("finish_message", &finish_message);
    function("add_to_search_path", &add_to_search_path);
    function("open_patch", &open_patch);
    function("close_patch", &close_patch);
}

void main_loop(void) {
}

SDL_AudioDeviceID open_audio_device(bool is_audio_in, SDL_AudioSpec &desired, SDL_AudioSpec &obtained) {
    desired.freq = samplerate;
    desired.format = AUDIO_F32;
    desired.channels = is_audio_in ? num_in_channels : num_out_channels;
    desired.samples = buffersize;
    desired.callback = is_audio_in ? audio_in : audio_out;
    SDL_AudioDeviceID device_id = SDL_OpenAudioDevice(nullptr, is_audio_in, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    const char *type_str = is_audio_in ? "audio in" : "audio out";
    if (!device_id) {
        SDL_Log("%s: failed to open device: %s", type_str, SDL_GetError());
    }
    else {
        if (desired.freq != obtained.freq) {
            SDL_Log("%s: desired sample rate was %d, but obtained %d", type_str, desired.freq, obtained.freq);
        }
        if (desired.format != obtained.format) {
            SDL_Log("%s: desired format was %d, but obtained %d", type_str, desired.format, obtained.format);
        }
        if (desired.channels != obtained.channels) {
            SDL_Log("%s: desired number of channels was %d, but obtained %d", type_str, desired.channels, obtained.channels);
        }
        if (desired.samples != obtained.samples) {
            SDL_Log("%s: desired buffer size was %d, but obtained %d", type_str, desired.samples, obtained.samples);
        }
    }
    return device_id;
}

void pause_audio_device(bool pauseOn) {
    if (device_in) {
        SDL_PauseAudioDevice(device_in, pauseOn);
    }
    if (device_out) {
        SDL_PauseAudioDevice(device_out, pauseOn);
    }
}

void close_audio_device() {
    if (device_in) {
        SDL_CloseAudioDevice(device_in);
    }
    if (device_out) {
        SDL_CloseAudioDevice(device_out);
    }
}

int main(int argc, char **argv) {
    
    /* initialize audio */
    SDL_Init(SDL_INIT_AUDIO);
    atexit(SDL_Quit);
    SDL_AudioSpec desired, obtained;
    if (num_in_channels) {
        device_in = open_audio_device(true, desired, obtained);
        num_in_channels = obtained.channels;
        samplerate = obtained.freq;
        buffersize = obtained.samples;
        ticks_per_buffer = buffersize / blocksize;
    }
    if (num_out_channels) {
        device_out = open_audio_device(false, desired, obtained);
        num_out_channels = obtained.channels;
        samplerate = obtained.freq;
        buffersize = obtained.samples;
        ticks_per_buffer = buffersize / blocksize;
    }
    in_buffer = new float[num_in_channels * buffersize];
    
    /* initialize libpd */
    libpd_init();
    libpd_start_gui(const_cast<char *>("/"));
    libpd_init_audio(num_in_channels, num_out_channels, samplerate);
    
    /* add internals help path */
    const char *internals_path = "purr-data/doc/5.reference";
    libpd_add_to_help_path(internals_path);
    
    /* add externals search/help path */
    const char *externals_path = "purr-data/extra";
    libpd_add_to_search_path(externals_path);
    libpd_add_to_help_path(externals_path);
    DIR *ext_dir = opendir(externals_path);
    if (ext_dir) {
        struct dirent *dir;
        while ((dir = readdir(ext_dir)) != nullptr) {
            if (dir->d_name[0] != '.') {
                const std::string &path = std::string(externals_path) + "/" + std::string(dir->d_name);
                libpd_add_to_search_path(path.c_str());
                libpd_add_to_help_path(path.c_str());
            }
        }
        closedir(ext_dir);
    }

    /* start audio processing */
    pause_audio_device(false);
    emscripten_set_main_loop(main_loop, 0, true);
    
    /* stop audio processing & close audio device */
    pause_audio_device(true);
    close_audio_device();
    if (in_buffer != nullptr) {
        delete[] in_buffer;
        in_buffer = nullptr;
    }
    return 0;
}
