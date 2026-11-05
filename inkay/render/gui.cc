#include "eg2/libs/typedefs.h"
#include "eg2/tex/font.hh"
#include "eg2/tex/color.hh"
#include "eg2/tex/textures.hh"
#include "eg2/render/screen.hh"
#include "eg2/controller/drc.hh"
#include "inkay/render/gui.hh"
#include "inkay/updater/app.hh"
#include "inkay/audio/music.hh"
#include "inkay/version.hh"
#include <memory>
#include <string>

// Layout is in 854x480 units with (0, 0) in the middle of the screen.
// Only ASCII text: the font renders one byte per character.

namespace Inkay {
    namespace GUI {
        using namespace Updater;

        static std::unique_ptr<Tex::LatteFont> Font;
        static std::unique_ptr<Tex::LatteColor> Fill;
        static std::unique_ptr<Tex::LatteTex> Background;
        static std::unique_ptr<Tex::LatteTex> Logo;
        static bool DidCenterFont = false;

        static constexpr int VISIBLE_ROWS = 5;
        static constexpr f32 ROW_Y0 = -128.0f;
        static constexpr f32 ROW_STEP = 66.0f;

        void Init(void) {
            Font = std::make_unique<Tex::LatteFont>(FONT_CAFESTD, 24.0f);
            Fill = std::make_unique<Tex::LatteColor>((u32)Colors::Card);
            // Same purple to red gradient as the app icon
            Background = std::make_unique<Tex::LatteTex>("fs:/vol/content/ui/background.png");
            Logo = std::make_unique<Tex::LatteTex>("fs:/vol/content/ui/logo.png");
        }

        static void DrawBox(u32 color, f32 w, f32 h, f32 x, f32 y) {
            if (!Fill) return;
            Fill->SetColor(color);
            Fill->DrawSolidCenteredScaled(RENDERRATIO_ASPECT_854x480, w, h, x, y);
        }

        // Centered text, sized by its height (the width is only a maximum)
        static void DrawText(const std::string& text, u32 color, f32 x, f32 y, f32 h, f32 maxW = 780.0f) {
            if (!Font || text.empty()) return;
            Font->SetColor(color);
            Font->RenderTextCenteredScaled(x, y, RENDERRATIO_ASPECT_854x480, maxW, h, text.c_str());
        }

        static u32 NetColor(Net net) {
            if (net == Net::Pretendo) return Colors::Pretendo;
            if (net == Net::Protarium) return Colors::Protarium;
            return Colors::NoNetwork;
        }

        static u32 StatusColor(Status status) {
            switch (status) {
                case Status::UpToDate: return Colors::Good;
                case Status::UpdateAvailable:
                case Status::Switch: return Colors::Warn;
                default: return Colors::Dim;
            }
        }

        // What pressing A will do for this environment
        static std::string ActionText(const Environment& env) {
            std::string latest = App::LatestTag(env.target);
            std::string name = Info(env.target).name;
            switch (App::GetStatus(env)) {
                case Status::NotInstalled: return "A: Install " + name + " Inkay " + latest;
                case Status::UpdateAvailable: return "A: Update to " + latest;
                case Status::UpToDate: return "A: Reinstall " + latest;
                case Status::UnknownVersion: return "A: Install " + name + " Inkay " + latest;
                case Status::Switch: return "A: Switch to " + name + " Inkay " + latest;
                case Status::NoRelease: return "Couldn't check " + name + " - press + to try again";
            }
            return "";
        }

        static void RenderList(void) {
            const auto& envs = App::GetEnvironments();
            const int selected = App::GetSelected();
            const int count = (int)envs.size();

            int first = 0;
            if (selected >= VISIBLE_ROWS) first = selected - VISIBLE_ROWS + 1;

            for (int i = first; i < count && i < first + VISIBLE_ROWS; i++) {
                const Environment& env = envs[i];
                const bool active = i == selected;
                const f32 y = ROW_Y0 + (i - first) * ROW_STEP;
                const Status status = App::GetStatus(env);

                DrawBox(active ? Colors::CardActive : Colors::Card, 780.0f, 58.0f, 0.0f, y);
                DrawBox(NetColor(env.installed), 6.0f, 58.0f, -387.0f, y);
                if (active) DrawBox(NetColor(env.target), 6.0f, 58.0f, 387.0f, y);

                DrawText(env.name, Colors::Fg, 0.0f, y - 12.0f, 24.0f);

                std::string installed = env.installed == Net::None
                    ? "No Inkay"
                    : std::string(Info(env.installed).name) + " " + (env.installedVersion.empty() ? "(version unknown)" : env.installedVersion);
                std::string line = installed + "   |   " + Environments::StatusText(status);
                if (active) line = installed + "   |   install: < " + Info(env.target).name + " >";
                DrawText(line, active ? Colors::Fg : StatusColor(status), 0.0f, y + 14.0f, 18.0f);
            }

            if (count > VISIBLE_ROWS) {
                DrawText(std::to_string(selected + 1) + " / " + std::to_string(count), Colors::Dim, 0.0f, ROW_Y0 + VISIBLE_ROWS * ROW_STEP - 26.0f, 16.0f);
            }

            if (count > 0) {
                const Environment& env = envs[selected];
                const Status status = App::GetStatus(env);
                DrawText(ActionText(env), status == Status::NoRelease ? Colors::Warn : Colors::Fg, 0.0f, 172.0f, 22.0f);

                if (!env.otherInkayFiles.empty()) {
                    DrawText("Another Inkay file is in this environment; installing switches it off", Colors::Warn, 0.0f, 150.0f, 16.0f);
                }
            }
        }

        void Render(void) {
            if (Background) Background->RenderTexCenteredScaled(RENDERRATIO_ASPECT_854x480, 854.0f, 480.0f, 0.0f, 0.0f);
            DrawBox(Colors::Shade, 854.0f, 55.0f, 0.0f, -214.0f);
            DrawBox(Colors::Shade, 854.0f, 55.0f, 0.0f, 214.0f);

            if (Logo) Logo->RenderTexCenteredScaled(RENDERRATIO_ASPECT_854x480, 38.0f, 38.0f, -112.0f, -214.0f);
            DrawText("Inkay Updater", Colors::Fg, 12.0f, -216.0f, 30.0f, 240.0f);
            DrawText(UPDATER_VERSION, Colors::Dim, 370.0f, -214.0f, 16.0f, 120.0f);

            switch (App::GetScreen()) {
                case App::Screen::Checking:
                    DrawText(App::GetBusyText(), Colors::Fg, 0.0f, 0.0f, 30.0f);
                    break;

                case App::Screen::List:
                    RenderList();
                    DrawText(std::string("Up/Down: environment    Left/Right: network    +: check again    Y: music ") + (Music::IsMuted() ? "on" : "off"), Colors::Dim, 0.0f, 214.0f, 18.0f);
                    break;

                case App::Screen::Downloading:
                    DrawText(App::GetBusyText(), Colors::Fg, 0.0f, -10.0f, 60.0f);
                    DrawText("Don't turn off the console", Colors::Dim, 0.0f, 214.0f, 20.0f);
                    break;

                case App::Screen::Done: {
                    const InstallResult& result = App::GetResult();
                    std::string text = result.message + "\n\nRestart the console to use it.";
                    if (!result.disabled.empty()) text += "\n" + std::to_string(result.disabled.size()) + " other Inkay file(s) were switched off.";
                    DrawText(text, Colors::Good, 0.0f, -10.0f, result.disabled.empty() ? 100.0f : 130.0f);
                    DrawText("B: restart now    A: back to the list", Colors::Fg, 0.0f, 214.0f, 20.0f);
                    break;
                }

                case App::Screen::Error:
                    DrawText(App::GetError(), Colors::Warn, 0.0f, -10.0f, 70.0f);
                    DrawText("A: back", Colors::Fg, 0.0f, 214.0f, 20.0f);
                    break;
            }
        }

        void Update(void) {
            DRC::Update();

            if (Font && !DidCenterFont) {
                Font->SwitchRenderType(FONT_ALIGN_CENTER);
                DidCenterFont = true;
            }

            if (DRC::ButtonTriggered(DRC::Input::Y)) Music::ToggleMute();

            switch (App::GetScreen()) {
                case App::Screen::List:
                    if (DRC::ButtonTriggered(DRC::Input::Down) || DRC::ButtonTriggered(DRC::Input::LS_Down)) App::MoveSelection(1);
                    if (DRC::ButtonTriggered(DRC::Input::Up) || DRC::ButtonTriggered(DRC::Input::LS_Up)) App::MoveSelection(-1);
                    if (DRC::ButtonTriggered(DRC::Input::Left) || DRC::ButtonTriggered(DRC::Input::Right) ||
                        DRC::ButtonTriggered(DRC::Input::LS_Left) || DRC::ButtonTriggered(DRC::Input::LS_Right)) App::ToggleTarget();
                    if (DRC::ButtonTriggered(DRC::Input::A)) App::InstallSelected();
                    if (DRC::ButtonTriggered(DRC::Input::Plus)) App::Start();
                    break;

                case App::Screen::Done:
                    if (DRC::ButtonTriggered(DRC::Input::B)) Render::FullyRebootConsole();
                    if (DRC::ButtonTriggered(DRC::Input::A)) App::BackToList();
                    break;

                case App::Screen::Error:
                    if (DRC::ButtonTriggered(DRC::Input::A)) App::BackToList();
                    break;

                default:
                    break;
            }
        }

        void Shutdown(void) {
            Font.reset();
            Fill.reset();
            Background.reset();
            Logo.reset();
        }
    }
}
