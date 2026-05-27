#include "MarioAudio.hpp"

#include "libsm64.h"

#include <xaudio2.h>
#include <cstdio>
#include <atomic>
#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>

#pragma comment(lib, "xaudio2.lib")

namespace HaloCE::Mod::Mario::MarioAudio {

// SM64 audio output format
static constexpr UINT32 SAMPLE_RATE      = 32000;
static constexpr UINT32 CHANNELS         = 2;
static constexpr UINT32 SAMPLES_PER_TICK = 544; // ~32000 / 58.8 fps
static constexpr UINT32 BYTES_PER_SAMPLE = sizeof(int16_t);

// Ring of PCM buffers. XAudio2 reads asynchronously, so each submitted buffer
// must remain valid until the OnBufferEnd callback fires.
static constexpr int NUM_RING_BUFFERS = 4;
static int16_t s_ringBuf[NUM_RING_BUFFERS][SAMPLES_PER_TICK * CHANNELS * 2];

// Tracks how many ring slots are currently free (not in-flight with XAudio2).
static std::atomic<int> s_freeSlots{ NUM_RING_BUFFERS };
static int              s_writeIdx = 0;

// Running total of samples submitted, used alongside SamplesPlayed to compute
// the true queue depth for sm64_audio_tick.
static uint64_t s_samplesSubmitted = 0;

static IXAudio2*               s_xaudio = nullptr;
static IXAudio2MasteringVoice* s_master = nullptr;
static IXAudio2SourceVoice*    s_source = nullptr;
static bool                    s_initialized = false;

// Serializes sm64_mario_tick (game thread) and sm64_audio_tick (audio thread).
// Both access SM64 global state; they must not run concurrently.
static std::mutex s_sm64Mutex;

// Wakes the audio thread when a new game tick has been processed.
static std::mutex              s_tickMutex;
static std::condition_variable s_tickCv;
static bool                    s_tickPending = false;
static bool                    s_shutdown    = false;
static std::thread             s_thread;

// ---- voice callback --------------------------------------------------------

class AudioCallback : public IXAudio2VoiceCallback {
public:
    void STDMETHODCALLTYPE OnBufferEnd(void*) noexcept override {
        s_freeSlots.fetch_add(1, std::memory_order_release);
    }
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) noexcept override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd()         noexcept override {}
    void STDMETHODCALLTYPE OnStreamEnd()                      noexcept override {}
    void STDMETHODCALLTYPE OnBufferStart(void*)               noexcept override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*)                   noexcept override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT)       noexcept override {}
};

static AudioCallback s_callback;

// ---- audio thread ----------------------------------------------------------

static void doAudioTick() {
    XAUDIO2_VOICE_STATE state = {};
    s_source->GetState(&state);
    uint32_t queuedSamples = static_cast<uint32_t>(
        s_samplesSubmitted - state.SamplesPlayed);

    // Acquire a free ring slot. If all slots are in-flight, skip this tick.
    int free = s_freeSlots.fetch_sub(1, std::memory_order_acquire);
    if (free <= 0) {
        s_freeSlots.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    int16_t* buf = s_ringBuf[s_writeIdx];
    s_writeIdx   = (s_writeIdx + 1) % NUM_RING_BUFFERS;

    uint32_t samplesWritten;
    {
        std::lock_guard<std::mutex> sm64Lock(s_sm64Mutex);
        samplesWritten = sm64_audio_tick(queuedSamples, SAMPLES_PER_TICK, buf);
    }

    if (samplesWritten == 0) {
        s_freeSlots.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    XAUDIO2_BUFFER xbuf = {};
    xbuf.AudioBytes     = samplesWritten * CHANNELS * BYTES_PER_SAMPLE * 2;
    xbuf.pAudioData     = reinterpret_cast<const BYTE*>(buf);
    s_source->SubmitSourceBuffer(&xbuf);
    s_samplesSubmitted += samplesWritten;
}

static void audioThreadFunc() {
    // Wake on game tick signal, or after one audio tick interval — whichever
    // comes first. The timeout keeps XAudio2 fed during slow motion when the
    // Halo tick rate drops below real-time audio rate.
    constexpr auto kTickInterval = std::chrono::microseconds(
        1000000ULL * SAMPLES_PER_TICK / SAMPLE_RATE); // ~17ms

    while (true) {
        {
            std::unique_lock<std::mutex> lock(s_tickMutex);
            s_tickCv.wait_for(lock, kTickInterval, [] { return s_tickPending || s_shutdown; });
            if (s_shutdown) break;
            s_tickPending = false;
        }
        doAudioTick();
    }
}

// ---- public API ------------------------------------------------------------

std::mutex& sm64Mutex() {
    return s_sm64Mutex;
}

void setGameSpeed(float speed) {
    if (!s_initialized) return;
    // Clamp to the range declared in CreateSourceVoice [1/4, 4].
    float ratio = speed < 0.25f ? 0.25f : (speed > 4.0f ? 4.0f : speed);
    s_source->SetFrequencyRatio(ratio);
}

void init(const uint8_t* rom) {
    sm64_audio_init(rom);

    HRESULT hr = XAudio2Create(&s_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        printf("[MarioAudio] XAudio2Create failed: 0x%08X\n", (unsigned)hr);
        return;
    }

    hr = s_xaudio->CreateMasteringVoice(&s_master);
    if (FAILED(hr)) {
        printf("[MarioAudio] CreateMasteringVoice failed: 0x%08X\n", (unsigned)hr);
        s_xaudio->Release();
        s_xaudio = nullptr;
        return;
    }

    WAVEFORMATEX wfx    = {};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = static_cast<WORD>(CHANNELS);
    wfx.nSamplesPerSec  = SAMPLE_RATE;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    // MaxFrequencyRatio of 4.0 allows playback down to 0.25x (slow motion min).
    hr = s_xaudio->CreateSourceVoice(&s_source, &wfx,
        0, 4.0f, &s_callback);
    if (FAILED(hr)) {
        printf("[MarioAudio] CreateSourceVoice failed: 0x%08X\n", (unsigned)hr);
        s_master->DestroyVoice();
        s_master = nullptr;
        s_xaudio->Release();
        s_xaudio = nullptr;
        return;
    }

    s_source->Start();
    s_initialized = true;
    s_thread = std::thread(audioThreadFunc);
    printf("[MarioAudio] Initialized\n");
}

void update() {
    if (!s_initialized) return;
    {
        std::lock_guard<std::mutex> lock(s_tickMutex);
        s_tickPending = true;
    }
    s_tickCv.notify_one();
}

void free() {
    s_initialized = false;

    // Signal the audio thread to exit and wait for it to finish its current tick.
    if (s_thread.joinable()) {
        {
            std::lock_guard<std::mutex> lock(s_tickMutex);
            s_shutdown = true;
        }
        s_tickCv.notify_one();
        s_thread.join();
        s_shutdown    = false;
        s_tickPending = false;
    }

    if (s_source) {
        s_source->Stop();
        s_source->FlushSourceBuffers();
        s_source->DestroyVoice();
        s_source = nullptr;
    }
    if (s_master) {
        s_master->DestroyVoice();
        s_master = nullptr;
    }
    if (s_xaudio) {
        s_xaudio->Release();
        s_xaudio = nullptr;
    }
}

} // namespace HaloCE::Mod::Mario::MarioAudio
