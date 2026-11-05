#pragma once

#include <cstdint>

namespace Music {
    // Decodes the MP3 in the background and starts looping it on the TV and
    // GamePad when ready. loopStart is in samples: at the end the song jumps
    // back there instead of to the beginning.
    void Start(const char* path, uint32_t loopStart, float volume);

    void Update(void); // call every frame (AX calls stay on the main thread)

    void ToggleMute(void);
    bool IsMuted(void);
    bool IsAvailable(void); // decoded and playing (or muted)

    void Shutdown(void); // before AX::Shutdown()
}
