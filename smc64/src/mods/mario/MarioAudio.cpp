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

#define ENABLE_MARIO_AUDIO 1

namespace HaloCE::Mod::Mario::MarioAudio {

// SM64 audio output format
static constexpr UINT32 SAMPLE_RATE      = 32000;
static constexpr UINT32 CHANNELS         = 2;
static constexpr UINT32 SAMPLES_PER_TICK = 544; // ~32000 / 58.8 fps
static constexpr UINT32 BYTES_PER_SAMPLE = sizeof(int16_t);

// Ring of PCM buffers. XAudio2 reads asynchronously, so each submitted buffer
// must remain valid until the OnBufferEnd callback fires.
static constexpr int NUM_RING_BUFFERS = 4;

// ---- state -----------------------------------------------------------------

// All mutable audio state lives here. A fresh instance is heap-allocated by
// init() and deleted by free(), so every reinit starts with a clean slate —
// no stale counters, no dangling thread handles.
struct AudioState {
    // Ring buffers — owned here so they remain valid for XAudio2's async DMA.
    int16_t ringBuf[NUM_RING_BUFFERS][SAMPLES_PER_TICK * CHANNELS * 2] = {};
    std::atomic<int> freeSlots{ NUM_RING_BUFFERS };
    int      writeIdx         = 0;
    uint64_t samplesSubmitted = 0;

    // XAudio2
    IXAudio2*               xaudio = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    IXAudio2SourceVoice*    source = nullptr;

    // Protects source pointer across setGameSpeed (game thread) and free() teardown.
    std::mutex              voiceMutex;

    // Audio thread wakeup.
    std::mutex              tickMutex;
    std::condition_variable tickCv;
    bool tickPending = false;
    bool shutdown    = false;
    std::thread thread;

    // Voice callback — needs a back-pointer to reach freeSlots.
    struct Callback : IXAudio2VoiceCallback {
        AudioState* owner = nullptr;
        void STDMETHODCALLTYPE OnBufferEnd(void*) noexcept override {
            owner->freeSlots.fetch_add(1, std::memory_order_release);
        }
        void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) noexcept override {}
        void STDMETHODCALLTYPE OnVoiceProcessingPassEnd()         noexcept override {}
        void STDMETHODCALLTYPE OnStreamEnd()                      noexcept override {}
        void STDMETHODCALLTYPE OnBufferStart(void*)               noexcept override {}
        void STDMETHODCALLTYPE OnLoopEnd(void*)                   noexcept override {}
        void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT)       noexcept override {}
    } callback;

    AudioState() { callback.owner = this; }
};

static AudioState* s_state = nullptr;

// Serializes sm64_mario_tick (game thread) and sm64_audio_tick (audio thread).
// Lives outside AudioState so it's always valid, even when audio is disabled.
static std::mutex s_sm64Mutex;

// ---- audio thread ----------------------------------------------------------

static void doAudioTick(AudioState& st) {
    XAUDIO2_VOICE_STATE state = {};
    st.source->GetState(&state);
    uint32_t queuedSamples = static_cast<uint32_t>(
        st.samplesSubmitted - state.SamplesPlayed);

    // Acquire a free ring slot. If all slots are in-flight, skip this tick.
    int free = st.freeSlots.fetch_sub(1, std::memory_order_acquire);
    if (free <= 0) {
        st.freeSlots.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    int16_t* buf = st.ringBuf[st.writeIdx];
    st.writeIdx  = (st.writeIdx + 1) % NUM_RING_BUFFERS;

    uint32_t samplesWritten;
    {
        std::lock_guard<std::mutex> sm64Lock(s_sm64Mutex);
        samplesWritten = sm64_audio_tick(queuedSamples, SAMPLES_PER_TICK, buf);
    }

    if (samplesWritten == 0) {
        st.freeSlots.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    XAUDIO2_BUFFER xbuf = {};
    xbuf.AudioBytes  = samplesWritten * CHANNELS * BYTES_PER_SAMPLE * 2;
    xbuf.pAudioData  = reinterpret_cast<const BYTE*>(buf);
    st.source->SubmitSourceBuffer(&xbuf);
    st.samplesSubmitted += samplesWritten;
}

static void audioThreadFunc(AudioState* st) {
    // Wake on game tick signal, or after one audio tick interval — whichever
    // comes first. The timeout keeps XAudio2 fed during slow motion when the
    // Halo tick rate drops below real-time audio rate.
    constexpr auto kTickInterval = std::chrono::microseconds(
        1000000ULL * SAMPLES_PER_TICK / SAMPLE_RATE); // ~17ms

    while (true) {
        {
            std::unique_lock<std::mutex> lock(st->tickMutex);
            st->tickCv.wait_for(lock, kTickInterval,
                [st] { return st->tickPending || st->shutdown; });
            if (st->shutdown) break;
            st->tickPending = false;
        }
        doAudioTick(*st);
    }
}

// ---- public API ------------------------------------------------------------

std::mutex& sm64Mutex() {
    return s_sm64Mutex;
}

void setGameSpeed(float speed) {
    if (!s_state) return;
    const float maxSpeed = 4.0f; // Must match the MaxFrequencyRatio set in init().
    const float minSpeed = 0.25f; // Must match the MinFrequencyRatio set in init().
    float ratio = speed < minSpeed ? minSpeed : (speed > maxSpeed ? maxSpeed : speed);
    std::lock_guard<std::mutex> lock(s_state->voiceMutex);
    if (s_state && s_state->source)
        s_state->source->SetFrequencyRatio(ratio);
}

void init(const uint8_t* rom) {
    #ifdef ENABLE_MARIO_AUDIO
    s_state = new AudioState();
    AudioState& st = *s_state;

    sm64_audio_init(rom);

    HRESULT hr = XAudio2Create(&st.xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        printf("[MarioAudio] XAudio2Create failed: 0x%08X\n", (unsigned)hr);
        delete s_state; s_state = nullptr;
        return;
    }

    hr = st.xaudio->CreateMasteringVoice(&st.master);
    if (FAILED(hr)) {
        printf("[MarioAudio] CreateMasteringVoice failed: 0x%08X\n", (unsigned)hr);
        st.xaudio->Release();
        delete s_state; s_state = nullptr;
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
    hr = st.xaudio->CreateSourceVoice(&st.source, &wfx,
        0, 4.0f, &st.callback);
    if (FAILED(hr)) {
        printf("[MarioAudio] CreateSourceVoice failed: 0x%08X\n", (unsigned)hr);
        st.master->DestroyVoice();
        st.xaudio->Release();
        delete s_state; s_state = nullptr;
        return;
    }

    st.source->Start();
    st.thread = std::thread(audioThreadFunc, s_state);
    printf("[MarioAudio] Initialized\n");
    #endif
}

void update() {
    #ifdef ENABLE_MARIO_AUDIO
    if (!s_state) return;
    {
        std::lock_guard<std::mutex> lock(s_state->tickMutex);
        s_state->tickPending = true;
    }
    s_state->tickCv.notify_one();
    #endif
}

void free() {
    #ifdef ENABLE_MARIO_AUDIO
    if (!s_state) return;
    AudioState& st = *s_state;

    // Signal the audio thread to exit and wait for it to finish its current tick.
    if (st.thread.joinable()) {
        {
            std::lock_guard<std::mutex> lock(st.tickMutex);
            st.shutdown = true;
        }
        st.tickCv.notify_one();
        st.thread.join();
    }

    // Null out source under voiceMutex before destroying, so setGameSpeed
    // (called from the game thread) can never call into a freed voice.
    IXAudio2SourceVoice* srcToDestroy = nullptr;
    {
        std::lock_guard<std::mutex> lock(st.voiceMutex);
        srcToDestroy = st.source;
        st.source = nullptr;
    }
    if (srcToDestroy) {
        srcToDestroy->Stop();
        srcToDestroy->FlushSourceBuffers();
        srcToDestroy->DestroyVoice();
    }
    if (st.master) {
        st.master->DestroyVoice();
    }
    if (st.xaudio) {
        st.xaudio->Release();
    }

    delete s_state;
    s_state = nullptr;
    #endif
}

} // namespace HaloCE::Mod::Mario::MarioAudio
