#include "eg2/render/screen.hh"
#include "eg2/network/curl.hh"
#include "eg2/render/frames.hh"
#include "eg2/ax/stub.hh"
#include "inkay/render/gui.hh"
#include "inkay/updater/app.hh"

int main(void) {
    Render::Init();
    Network::Init();
    AX::Init();

    Network::GiveCertCA("fs:/vol/content/shoumeisho/CA.pem");
    Network::SetUserAgent("Inkay-Updater/1.0 (Wii U; +https://github.com/sennecaelen/Inkay-Updater)");
    Network::SetHeader("Accept: application/vnd.github+json");
    Network::SetHeader("X-GitHub-Api-Version: 2022-11-28");

    Inkay::GUI::Init();
    Updater::App::Start();

    while (Render::IsRunning()) {
        Updater::App::Update();
        Inkay::GUI::Update();

        Render::BeginFrame();
            Render::DRC(Inkay::GUI::Render);
            Render::TV(Inkay::GUI::Render);
        Render::EndFrame();
    }

    Inkay::GUI::Shutdown();
    AX::Shutdown();
    Network::Shutdown();
    Render::Shutdown();
    return 0;
}
