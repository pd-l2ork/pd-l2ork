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

#pragma once

#include "m_pd.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <string>
#include <map>

class Pd {
    public :
        Pd();
        virtual ~Pd();
        bool init(int numInChannels, int numOutChannels,
                  int sampleRate, int ticksPerBuffer);
        void clear();
        void addToSearchPath(const std::string& path);
        void clearSearchPath();
        void addToHelpPath(const std::string& path);
        void clearHelpPath();
        void openPatch(const std::string &name, const std::string &dir);
        void closePatch(const std::string &name);
        void computeAudio(bool state);
        void sendBang(const std::string& dest);
        void sendFloat(const std::string& dest, float value);
        void sendSymbol(const std::string& dest, const std::string& symbol);
        void startMessage(int maxlen);
        void addFloat(float num);
        void addSymbol(const std::string& symbol);
        void finishList(const std::string& dest);
        void finishMessage(const std::string& dest, const std::string& msg);
        void sendNoteOn(int channel, int pitch, int velocity);
        void sendControlChange(int channel, int controller, int value);
        void sendProgramChange(int channel, int value);
        void sendPitchBend(int channel, int value);
        void sendAftertouch(int channel, int value);
        void sendPolyAftertouch(int channel, int pitch, int value);
        void subscribe(const std::string &source);
        void unsubscribe(const std::string &source);
        void unsubscribeAll();
        bool isInited() const;
        bool getNoGui() const;
        void setNoGui(bool noGui);
        SDL_AudioDeviceID getInDeviceID() const;
        SDL_AudioDeviceID getOutDeviceID() const;
        int getTicksPerBuffer() const;
        int getBlockSize() const;
        int getBufferSize() const;
        int getSampleRate() const;
        SDL_AudioFormat getAudioFormat() const;
        int getNumInChannels() const;
        int getNumOutChannels() const;
    
    private:
        SDL_AudioDeviceID openAudioDevice(bool isInput, int numChannels,
                                            int sampleRate, int ticksPerBuffer);
        void closeAudioDevice(SDL_AudioDeviceID deviceID);
        static void forwardAudioIn(void *userdata, Uint8 *stream, int len);
        static void forwardAudioOut(void *userdata, Uint8 *stream, int len);
        void audioIn(Uint8 *stream, int len);
        void audioOut(Uint8 *stream, int len);
        static void receivePrint(const char *s);
        static void receiveBang(const char *source);
        static void receiveFloat(const char *source, float value);
        static void receiveSymbol(const char *source, const char *symbol);
        static void receiveList(const char *source, int argc, t_atom *argv);
        static void receiveMessage(const char *source, const char *symbol,
                                   int argc, t_atom *argv);
        static void receiveNoteOn(int channel, int pitch, int velocity);
        static void receiveControlChange(int channel, int controller, int value);
        static void receiveProgramChange(int channel, int value);
        static void receivePitchBend(int channel, int value);
        static void receiveAftertouch(int channel, int value);
        static void receivePolyAftertouch(int channel, int pitch, int value);
        static void receiveMidiByte(int port, int byte);
        bool bInited;
        SDL_AudioDeviceID inDev;
        SDL_AudioDeviceID outDev;
        int ticks;
        int bsize;
        int srate;
        SDL_AudioFormat format;
        int inChannels;
        int outChannels;
        float *inBuffer;
        bool bBufferCleared;
        std::map<std::string, void *> sources;
};
