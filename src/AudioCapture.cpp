#include "AudioCapture.hpp"
#include "AudioBridge.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/fmod/fmod.hpp>

#include <android/log.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace recordaudio {
static SharedAudioRing* s_ring = nullptr;
static int s_fd = -1;
static FMOD::DSP* s_dsp = nullptr;
static FMOD::ChannelGroup* s_master = nullptr;
static std::atomic_bool s_installed{false};
static std::mutex s_mutex;

static bool openSharedRing() {
    if (s_ring) return true;

    const char* path = SHARED_PATH;
    size_t slash = std::string(path).find_last_of('/');
    if (slash != std::string::npos) {
        std::string directory(path, slash);
        size_t pos = 1;
        while ((pos = directory.find('/', pos)) != std::string::npos) {
            std::string part = directory.substr(0, pos++);
            if (!part.empty()) mkdir(part.c_str(), 0775);
        }
        mkdir(directory.c_str(), 0775);
    }

    s_fd = open(path, O_CREAT | O_RDWR, 0664);
    if (s_fd < 0) return false;

    if (ftruncate(s_fd, sizeof(SharedAudioRing)) != 0) {
        close(s_fd);
        s_fd = -1;
        return false;
    }

    void* memory = mmap(nullptr, sizeof(SharedAudioRing),
                        PROT_READ | PROT_WRITE, MAP_SHARED, s_fd, 0);
    if (memory == MAP_FAILED) {
        close(s_fd);
        s_fd = -1;
        return false;
    }

    s_ring = static_cast<SharedAudioRing*>(memory);
    if (s_ring->magic != MAGIC) {
        std::memset(memory, 0, sizeof(SharedAudioRing));
        s_ring->magic = MAGIC;
        s_ring->sampleRate = SAMPLE_RATE;
        s_ring->channels = CHANNELS;
        s_ring->capacityFrames = RING_FRAMES;
        new (&s_ring->active) std::atomic<uint32_t>(0);
        new (&s_ring->writeFrame) std::atomic<uint64_t>(0);
        new (&s_ring->readFrame) std::atomic<uint64_t>(0);
    }
    return true;
}

static FMOD_RESULT F_CALL readCallback(
    FMOD_DSP_STATE*, float* input, float* output,
    unsigned int length, int inputChannels, int* outputChannels
) {
    if (!input || !output || inputChannels < 1) {
        *outputChannels = inputChannels;
        return FMOD_OK;
    }

    const size_t sampleCount = static_cast<size_t>(length) * inputChannels;
    std::memcpy(output, input, sampleCount * sizeof(float));
    *outputChannels = inputChannels;

    auto* ring = s_ring;
    if (!ring || ring->magic != MAGIC || !ring->active.load(std::memory_order_relaxed))
        return FMOD_OK;

    uint64_t write = ring->writeFrame.load(std::memory_order_relaxed);
    uint64_t read = ring->readFrame.load(std::memory_order_acquire);
    uint64_t available = RING_FRAMES - std::min<uint64_t>(RING_FRAMES, write - read);
    uint32_t frames = static_cast<uint32_t>(std::min<uint64_t>(length, available));
    if (!frames) return FMOD_OK;

    for (uint32_t i = 0; i < frames; ++i) {
        float left = input[i * inputChannels];
        float right = inputChannels > 1 ? input[i * inputChannels + 1] : left;
        size_t slot = static_cast<size_t>((write + i) % RING_FRAMES) * 2;
        ring->samples[slot] = left;
        ring->samples[slot + 1] = right;
    }

    ring->writeFrame.store(write + frames, std::memory_order_release);
    return FMOD_OK;
}

bool install() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_installed.load()) return true;
    if (!openSharedRing()) {
        geode::log::error("RecordAudio: cannot create shared PCM ring");
        return false;
    }

    auto* engine = FMODAudioEngine::sharedEngine();
    if (!engine || !engine->m_system) {
        geode::log::error("RecordAudio: FMOD system is unavailable");
        return false;
    }

    FMOD_DSP_DESCRIPTION description{};
    description.pluginsdkversion = FMOD_PLUGIN_SDK_VERSION;
    std::strncpy(description.name, "RecordAudio PCM Tap", sizeof(description.name) - 1);
    description.version = 1;
    description.numinputbuffers = 1;
    description.numoutputbuffers = 1;
    description.read = &readCallback;

    FMOD_RESULT result = engine->m_system->createDSP(&description, &s_dsp);
    if (result != FMOD_OK || !s_dsp) {
        geode::log::error("RecordAudio: createDSP failed ({})", static_cast<int>(result));
        return false;
    }

    result = engine->m_system->getMasterChannelGroup(&s_master);
    if (result != FMOD_OK || !s_master) {
        s_dsp->release();
        s_dsp = nullptr;
        geode::log::error("RecordAudio: master channel group unavailable");
        return false;
    }

    result = s_master->addDSP(FMOD_CHANNELCONTROL_DSP_TAIL, s_dsp);
    if (result != FMOD_OK) {
        s_dsp->release();
        s_dsp = nullptr;
        s_master = nullptr;
        geode::log::error("RecordAudio: addDSP failed ({})", static_cast<int>(result));
        return false;
    }

    s_installed.store(true);
    geode::log::info("RecordAudio: FMOD capture tap installed");
    return true;
}

void uninstall() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_ring) s_ring->active.store(0, std::memory_order_release);
    if (s_master && s_dsp) s_master->removeDSP(s_dsp);
    if (s_dsp) s_dsp->release();
    s_dsp = nullptr;
    s_master = nullptr;
    s_installed.store(false);

    if (s_ring) munmap(s_ring, sizeof(SharedAudioRing));
    if (s_fd >= 0) close(s_fd);
    s_ring = nullptr;
    s_fd = -1;
}

bool isAvailable() {
    return s_installed.load() && s_ring && s_ring->magic == MAGIC;
}
}
