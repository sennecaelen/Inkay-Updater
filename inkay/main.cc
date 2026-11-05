#include "eg2/render/screen.hh"
#include "eg2/network/curl.hh"
#include "eg2/render/frames.hh"
#include "eg2/ax/stub.hh"
#include "inkay/render/gui.hh"
#include "inkay/updater/app.hh"
#include "inkay/audio/music.hh"

int main(void) {
    Render::Init();
    Network::Init();
    AX::Init();

    Network::GiveCertCA("fs:/vol/content/shoumeisho/CA.pem");
    Network::SetUserAgent("Inkay-Updater/1.0 (Wii U; +https://github.com/sennecaelen/Inkay-Updater)");
    Network::SetHeader("Accept: application/vnd.github+json");
    Network::SetHeader("X-GitHub-Api-Version: 2022-11-28");

    Inkay::GUI::Init();

    // Background work starts once the first frames are on screen, so the app
    // never sits on the splash screen while it checks or decodes.
    u32 frame = 0;

    while (Render::IsRunning()) {
        if (frame < 10 && ++frame == 3) {
            Updater::App::Start();
            // Background music; loops back to 17.92 s (sample 573440 at 32 kHz)
            Music::Start("fs:/vol/content/music/background.mp3", 573440, 0.5f);
        }

        Updater::App::Update();
        Music::Update();
        Inkay::GUI::Update();

        Render::BeginFrame();
            Render::DRC(Inkay::GUI::Render);
            Render::TV(Inkay::GUI::Render);
        Render::EndFrame();
    }

    Inkay::GUI::Shutdown();
    Music::Shutdown();
    AX::Shutdown();
    Network::Shutdown();
    Render::Shutdown();
    return 0;
}
