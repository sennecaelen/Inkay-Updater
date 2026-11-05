#include "inkay/audio/music.hh"
#include "eg2/sys/report.hh"
#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <sndcore2/voice.h>
#include <coreinit/cache.h>
#include <coreinit/time.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <vector>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_NO_STDIO
#include "eg2/libs/minimp3_ex.h"

// Everything runs on the main thread: the song is decoded a few
// milliseconds per frame, so the screen and the HOME button never wait for
// it. (A separate decoding thread froze the app on the console.)

namespace Music {
    enum State { Idle, Decoding, Done, Failed };

    static State gState = Idle;
    static std::vector<uint8_t> gFile;
    static mp3dec_ex_t gDecoder;

    static int16_t* gChannel[2] = {nullptr, nullptr}; // one mono buffer per voice
    static int gChannels = 0;
    static uint32_t gFrames = 0;   // total samples per channel
    static uint32_t gDecoded = 0;  // samples per channel decoded so far
    static int gRate = 0;
    static uint32_t gLoopStart = 0;
    static uint16_t gVolume = 0x8000;
    static bool gMuted = false;
    static bool gPlaying = false;
    static AXVoice* gVoices[2] = {nullptr, nullptr};

    static constexpr uint32_t BUDGET_US = 3000;     // decoding time per frame
    static constexpr uint32_t BOOST_US = 10000;     // when playback gets close
    static constexpr uint32_t START_SECONDS = 4;    // decoded before playing

    static void FreeAll(void) {
        for (auto& ch : gChannel) { free(ch); ch = nullptr; }
        gFile.clear();
        gFile.shrink_to_fit();
    }

    void Start(const char* path, uint32_t loopStart, float volume) {
        if (gState != Idle) return;
        gLoopStart = loopStart;
        gVolume = (uint16_t)(volume <= 0 ? 0 : volume >= 1 ? 0x8000 : volume * 0x8000);
        gState = Failed;

        FILE* f = fopen(path, "rb");
        if (!f) {
            SYS::Report::Log("Music: can't open %s\n", path);
            return;
        }
        uint8_t buffer[16 * 1024];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) gFile.insert(gFile.end(), buffer, buffer + n);
        fclose(f);

        if (gFile.empty() || mp3dec_ex_open_buf(&gDecoder, gFile.data(), gFile.size(), MP3D_SEEK_TO_SAMPLE) != 0 ||
            (gDecoder.info.channels != 1 && gDecoder.info.channels != 2) || gDecoder.samples == 0) {
            SYS::Report::Log("Music: not a usable MP3\n");
            FreeAll();
            return;
        }

        gChannels = gDecoder.info.channels;
        gRate = gDecoder.info.hz;
        gFrames = (uint32_t)(gDecoder.samples / gChannels);
        if (gLoopStart >= gFrames) gLoopStart = 0;

        for (int c = 0; c < gChannels; c++) {
            gChannel[c] = (int16_t*)memalign(0x40, gFrames * sizeof(int16_t));
            if (!gChannel[c]) {
                mp3dec_ex_close(&gDecoder);
                FreeAll();
                return;
            }
        }

        gDecoded = 0;
        gState = Decoding;
        SYS::Report::Log("Music: %u frames, %d ch, %d Hz\n", gFrames, gChannels, gRate);
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

        gPlaying = gVoices[0] != nullptr;
    }

    // Samples per channel the song has played so far (0 before it starts)
    static uint32_t PlayPosition(void) {
        if (!gPlaying || !gVoices[0]) return 0;
        uint32_t pos = AXGetVoiceCurrentOffsetEx(gVoices[0], gChannel[0]);
        return pos < gFrames ? pos : gFrames;
    }

    static void DecodeSome(void) {
        // Normally a small slice per frame; more when playback gets close to
        // the end of what's decoded (decoding is many times faster than real time)
        uint32_t margin = (uint32_t)gRate * 2;
        uint32_t budget = (gPlaying && PlayPosition() + margin > gDecoded) ? BOOST_US : BUDGET_US;

        static mp3d_sample_t chunk[MINIMP3_MAX_SAMPLES_PER_FRAME * 4];
        const uint32_t first = gDecoded;
        const OSTime start = OSGetTime();

        while (gDecoded < gFrames && OSTicksToMicroseconds(OSGetTime() - start) < budget) {
            size_t got = mp3dec_ex_read(&gDecoder, chunk, sizeof(chunk) / sizeof(chunk[0]));
            uint32_t frames = (uint32_t)(got / gChannels);
            if (frames == 0) {
                // End of the data (or an error): fill the rest with silence
                for (int c = 0; c < gChannels; c++) memset(gChannel[c] + gDecoded, 0, (gFrames - gDecoded) * sizeof(int16_t));
                gDecoded = gFrames;
                break;
            }
            if (frames > gFrames - gDecoded) frames = gFrames - gDecoded;

            // Samples are native endian, which on the Wii U is the big endian AX wants
            for (int c = 0; c < gChannels; c++) {
                int16_t* dst = gChannel[c] + gDecoded;
                for (uint32_t i = 0; i < frames; i++) dst[i] = chunk[i * gChannels + c];
            }
            gDecoded += frames;
        }

        // Make the new samples visible to the audio hardware
        for (int c = 0; c < gChannels; c++) DCFlushRange(gChannel[c] + first, (gDecoded - first) * sizeof(int16_t));

        if (gDecoded >= gFrames) {
            mp3dec_ex_close(&gDecoder);
            gFile.clear();
            gFile.shrink_to_fit();
            gState = Done;
            SYS::Report::Log("Music: decoded\n");
        }
    }

    void Update(void) {
        if (gState != Decoding && !(gState == Done && !gPlaying)) return;

        if (gState == Decoding) DecodeSome();

        uint32_t needed = (uint32_t)gRate * START_SECONDS;
        if (!gPlaying && (gDecoded >= needed || gState == Done)) Play();
    }

    void ToggleMute(void) {
        gMuted = !gMuted;
        ApplyVolume();
    }

    bool IsMuted(void) { return gMuted; }
    bool IsAvailable(void) { return gPlaying; }

    void Shutdown(void) {
        for (AXVoice*& v : gVoices) {
            if (!v) continue;
            AXVoiceBegin(v);
            AXSetVoiceState(v, AX_VOICE_STATE_STOPPED);
            AXVoiceEnd(v);
            AXFreeVoice(v);
            v = nullptr;
        }
        gPlaying = false;
        if (gState == Decoding) mp3dec_ex_close(&gDecoder);
        FreeAll();
        gState = Idle;
    }
}
