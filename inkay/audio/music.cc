#include "inkay/audio/music.hh"
#include "eg2/sys/report.hh"
#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <sndcore2/voice.h>
#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <string>
#include <thread>
#include <vector>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_NO_STDIO
#include "eg2/libs/minimp3_ex.h"

namespace Music {
    enum State : int { Idle, Decoding, Ready, Playing, Failed };

    static std::atomic<int> gState{Idle};
    static std::atomic<bool> gAbort{false};
    static std::thread gThread;

    static int16_t* gChannel[2] = {nullptr, nullptr}; // one mono buffer per voice
    static int gChannels = 0;
    static uint32_t gFrames = 0;
    static int gRate = 0;
    static uint32_t gLoopStart = 0;
    static uint16_t gVolume = 0x8000;
    static bool gMuted = false;
    static AXVoice* gVoices[2] = {nullptr, nullptr};

    // Called after every MP3 frame. Gives the CPU back regularly so the
    // screen keeps running while the song is decoded.
    static int Progress(void*, size_t, uint64_t, mp3dec_frame_info_t*) {
        static uint32_t frames = 0;
        if (++frames % 8 == 0) OSSleepTicks(OSMicrosecondsToTicks(500));
        return gAbort.load() ? 1 : 0; // non-zero stops decoding
    }

    static void Decode(std::string path) {
        // Lower priority than the main thread (higher number = lower priority)
        OSSetThreadPriority(OSGetCurrentThread(), 28);

        std::vector<uint8_t> file;
        FILE* f = fopen(path.c_str(), "rb");
        if (f) {
            uint8_t buffer[16 * 1024];
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) file.insert(file.end(), buffer, buffer + n);
            fclose(f);
        }
        if (file.empty()) {
            SYS::Report::Log("Music: can't read %s\n", path.c_str());
            gState = Failed;
            return;
        }

        static mp3dec_t dec;
        mp3dec_file_info_t info;
        memset(&info, 0, sizeof(info));
        if (mp3dec_load_buf(&dec, file.data(), file.size(), &info, Progress, nullptr) != 0 || !info.buffer ||
            info.samples == 0 || (info.channels != 1 && info.channels != 2) || gAbort) {
            free(info.buffer);
            SYS::Report::Log("Music: decoding failed\n");
            gState = Failed;
            return;
        }

        gChannels = info.channels;
        gRate = info.hz;
        gFrames = (uint32_t)(info.samples / info.channels);

        // Samples are native endian, which on the Wii U is the big endian AX wants
        bool ok = true;
        for (int c = 0; c < gChannels; c++) {
            gChannel[c] = (int16_t*)memalign(0x40, gFrames * sizeof(int16_t));
            if (!gChannel[c]) { ok = false; break; }
            for (uint32_t i = 0; i < gFrames; i++) gChannel[c][i] = info.buffer[i * gChannels + c];
            DCFlushRange(gChannel[c], gFrames * sizeof(int16_t));
        }
        free(info.buffer);

        if (!ok) {
            for (auto& ch : gChannel) { free(ch); ch = nullptr; }
            gState = Failed;
            return;
        }

        if (gLoopStart >= gFrames) gLoopStart = 0;
        SYS::Report::Log("Music: %u frames, %d ch, %d Hz\n", gFrames, gChannels, gRate);
        gState = Ready;
    }

    void Start(const char* path, uint32_t loopStart, float volume) {
        if (gState != Idle) return;
        gLoopStart = loopStart;
        gVolume = (uint16_t)(volume <= 0 ? 0 : volume >= 1 ? 0x8000 : volume * 0x8000);
        gState = Decoding;
        gThread = std::thread(Decode, std::string(path));
    }

    static void ApplyVolume(void) {
        for (AXVoice* v : gVoices) {
            if (!v) continue;
            AXVoiceVeData ve = {(uint16_t)(gMuted ? 0 : gVolume), 0};
            AXVoiceBegin(v);
            AXSetVoiceVe(v, &ve);
            AXVoiceEnd(v);
        }
    }

    static void Play(void) {
        if (!AXIsInit()) return;

        for (int c = 0; c < gChannels; c++) {
            AXVoice* v = AXAcquireVoice(31, nullptr, nullptr);
            if (!v) break;
            gVoices[c] = v;

            AXVoiceBegin(v);
            AXSetVoiceType(v, AX_VOICE_TYPE_UNKNOWN);

            AXVoiceVeData ve = {(uint16_t)(gMuted ? 0 : gVolume), 0};
            AXSetVoiceVe(v, &ve);

            // Left voice to the left speaker, right to the right; mono to both
            AXVoiceDeviceMixData tv[6], drc[4];
            memset(tv, 0, sizeof(tv));
            memset(drc, 0, sizeof(drc));
            for (int out = 0; out < 2; out++) {
                if (gChannels == 1 || out == c) {
                    tv[out].bus[0].volume = 0x8000;
                    drc[out].bus[0].volume = 0x8000;
                }
            }
            AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_TV, 0, tv);
            AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_DRC, 0, drc);

            AXSetVoiceSrcType(v, AX_VOICE_SRC_TYPE_LINEAR);
            AXSetVoiceSrcRatio(v, (float)gRate / (float)AXGetInputSamplesPerSec());

            AXVoiceOffsets offsets;
            offsets.dataType = AX_VOICE_FORMAT_LPCM16;
            offsets.loopingEnabled = AX_VOICE_LOOP_ENABLED;
            offsets.loopOffset = gLoopStart;
            offsets.endOffset = gFrames - 1;
            offsets.currentOffset = 0;
            offsets.data = gChannel[c];
            AXSetVoiceOffsets(v, &offsets);
            AXVoiceEnd(v);
        }

        // Start both channels together so they stay in sync
        for (AXVoice* v : gVoices) if (v) AXVoiceBegin(v);
        for (AXVoice* v : gVoices) if (v) AXSetVoiceState(v, AX_VOICE_STATE_PLAYING);
        for (AXVoice* v : gVoices) if (v) AXVoiceEnd(v);

        gState = Playing;
    }

    void Update(void) {
        if (gState == Ready) Play();
    }

    void ToggleMute(void) {
        gMuted = !gMuted;
        ApplyVolume();
    }

    bool IsMuted(void) { return gMuted; }
    bool IsAvailable(void) { return gState == Playing; }

    void Shutdown(void) {
        gAbort = true;
        if (gThread.joinable()) gThread.join();

        for (AXVoice*& v : gVoices) {
            if (!v) continue;
            AXVoiceBegin(v);
            AXSetVoiceState(v, AX_VOICE_STATE_STOPPED);
            AXVoiceEnd(v);
            AXFreeVoice(v);
            v = nullptr;
        }
        for (auto& ch : gChannel) { free(ch); ch = nullptr; }
        gState = Idle;
    }
}
