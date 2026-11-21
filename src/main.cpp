#include <Geode/Geode.hpp>
#include "AudioCapture.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    if (!recordaudio::install()) {
        log::error("RecordAudio failed to attach to FMOD.");
    }
}
