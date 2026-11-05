#pragma once

#include <cstdint>

namespace Music {
    // Loads the MP3 and starts looping it on the TV and GamePad once the first
    // seconds are decoded (the rest is decoded a little every frame in
    // Update). loopStart is in samples: at the end the song jumps back there
    // instead of to the beginning.
    void Start(const char* path, uint32_t loopStart, float volume);

    void Update(void); // call every frame

    void ToggleMute(void);
    bool IsMuted(void);
    bool IsAvailable(void); // decoded and playing (or muted)

    void Shutdown(void); // before AX::Shutdown()
}
