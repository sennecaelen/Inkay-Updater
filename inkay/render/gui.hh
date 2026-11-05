#pragma once

#include "eg2/libs/typedefs.h"

namespace Inkay {
    namespace GUI {
        // 0xRRGGBBAA
        enum Colors : u32 {
            Shade      = 0x0000005CU, // header, footer
            Card       = 0x00000052U,
            CardActive = 0xFFFFFF30U,
            Fg         = 0xF5F5F5FFU,
            Dim        = 0xCDBEE1FFU, // light lilac, from the icon gradient
            Pretendo   = 0x9B62F0FFU,
            Protarium  = 0xF0525CFFU,
            NoNetwork  = 0x8A8494FFU,
            Good       = 0x5FD69BFFU,
            Warn       = 0xFFC452FFU
        };

        void Init(void);
        void Render(void);
        void Update(void);
        void Shutdown(void);
    }
}
