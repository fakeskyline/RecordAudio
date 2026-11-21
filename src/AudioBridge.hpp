#pragma once

#include <atomic>
#include <cstdint>

namespace recordaudio {
constexpr char SHARED_PATH[] =
    "/storage/emulated/0/Android/media/com.geode.launcher/game/renders/RecordAudio.shared";
constexpr uint32_t MAGIC = 0x52415544;
constexpr uint32_t SAMPLE_RATE = 48000;
constexpr uint32_t CHANNELS = 2;
constexpr uint32_t RING_FRAMES = SAMPLE_RATE * 8;

struct SharedAudioRing {
    uint32_t magic;
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t capacityFrames;
    std::atomic<uint32_t> active;
    std::atomic<uint64_t> writeFrame;
    std::atomic<uint64_t> readFrame;
    alignas(64) float samples[RING_FRAMES * CHANNELS];
};
}
