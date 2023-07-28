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
#include "pd.hpp"
#include <emscripten.h>
#include <emscripten/bind.h>

using namespace emscripten;

EMSCRIPTEN_BINDINGS(Pd_class) {
  class_<Pd>("Pd")
    .constructor<>()
    .function("init", &Pd::init)
    .function("clear", &Pd::clear)
    .function("addToSearchPath", &Pd::addToSearchPath)
    .function("clearSearchPath", &Pd::clearSearchPath)
    .function("addToHelpPath", &Pd::addToHelpPath)
    .function("clearHelpPath", &Pd::clearHelpPath)
    .function("openPatch", &Pd::openPatch)
    .function("closePatch", &Pd::closePatch)
    .function("computeAudio", &Pd::computeAudio)
    .function("sendBang", &Pd::sendBang)
    .function("sendFloat", &Pd::sendFloat)
    .function("sendSymbol", &Pd::sendSymbol)
    .function("startMessage", &Pd::startMessage)
    .function("addFloat", &Pd::addFloat)
    .function("addSymbol", &Pd::addSymbol)
    .function("finishList", &Pd::finishList)
    .function("finishMessage", &Pd::finishMessage)
    .function("sendNoteOn", &Pd::sendNoteOn)
    .function("sendControlChange", &Pd::sendControlChange)
    .function("sendProgramChange", &Pd::sendProgramChange)
    .function("sendPitchBend", &Pd::sendPitchBend)
    .function("sendAftertouch", &Pd::sendAftertouch)
    .function("sendPolyAftertouch", &Pd::sendPolyAftertouch)
    .function("subscribe", &Pd::subscribe)
    .function("unsubscribe", &Pd::unsubscribe)
    .function("unsubscribeAll", &Pd::unsubscribeAll)
    .function("isInited", &Pd::isInited)
    .function("getNoGui", &Pd::getNoGui)
    .function("setNoGui", &Pd::setNoGui)
    .function("getInDeviceID", &Pd::getInDeviceID)
    .function("getOutDeviceID", &Pd::getOutDeviceID)
    .function("getTicksPerBuffer", &Pd::getTicksPerBuffer)
    .function("getBlockSize", &Pd::getBlockSize)
    .function("getBufferSize", &Pd::getBufferSize)
    .function("getSampleRate", &Pd::getSampleRate)
    .function("getAudioFormat", &Pd::getAudioFormat)
    .function("getNumInChannels", &Pd::getNumInChannels)
    .function("getNumOutChannels", &Pd::getNumOutChannels)
    ;
}

void mainLoop() {
    __mainLoop();
}

int main() {
    __mainInit();
    emscripten_set_main_loop(mainLoop, 0, true);
    __mainExit(); // this won't be executed
}
