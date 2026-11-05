#pragma once

#include "eg2/libs/typedefs.h"

namespace Inkay {
    namespace GUI {
        // 0xRRGGBBAA
        enum Colors : u32 {
            Background = 0x15161CFF,
            Bar        = 0x23252EFF,
            Row        = 0x2C2F3AFF,
            RowActive  = 0x3D4252FF,
            Fg         = 0xF2F2F2FF,
            Dim        = 0xA0A4B0FF,
            Pretendo   = 0x8C50DCFF,
            Protarium  = 0xDE3E3EFF,
            NoNetwork  = 0x5A5D66FF,
            Good       = 0x4CC38AFF,
            Warn       = 0xF0B43CFF
        };

        void Init(void);
        void Render(void);
        void Update(void);
        void Shutdown(void);
    }
}
