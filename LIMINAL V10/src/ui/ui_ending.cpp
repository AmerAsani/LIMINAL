// V9: Einleitung (leise Zeilen zu Beginn eines Laufs) und Abspann ("Draussen").
//
// Der Abspann folgt auf die letzte Fahrt: Die Kabine wird weiss, und dahinter ist kein Flur mehr.
// Auf hellem Grund erscheinen ein kurzer Text und das Laufbuch dieses Laufs als Abspann - Zeile fuer
// Zeile. Danach kann man zurueck ins Hauptmenue oder weiter erkunden (die Tuer geht wieder auf ...).
#include <algorithm>
#include <cmath>
#include <format>

#include "gameplay/content.hpp"
#include "gameplay/session.hpp"
#include "input/input.hpp"
#include "render/renderer.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace lim::ui {

using namespace theme;
using input::Action;

namespace {
float ease(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

std::string spacedUpper(const std::string& s) {
    std::string out;
    for (char32_t c : gfx::utf8To32(upperUtf8(s))) {
        if (!out.empty()) out += ' ';
        out += gfx::utf32To8(std::u32string(1, c));
    }
    return out;
}

std::string clock(double seconds) {
    int t = (int)seconds;
    return t >= 3600 ? std::format("{}:{:02}:{:02} h", t / 3600, t / 60 % 60, t % 60) : std::format("{}:{:02} min", t / 60, t % 60);
}
}  // namespace

// Einleitung: Zeilen nacheinander in der oberen Bildhaelfte, leise ein- und ausgeblendet
void Ui::drawIntro() {
    if (introLines_.empty()) return;
    if (introStart_ < 0.0) introStart_ = now_;
    const double kDelay = 1.6, kEach = 4.6, kFade = 0.9;
    const double t = now_ - introStart_ - kDelay;
    if (t < 0.0) return;
    const size_t i = (size_t)(t / kEach);
    if (i >= introLines_.size()) {
        introLines_.clear();
        return;
    }
    const double lt = t - (double)i * kEach;
    float a = (float)std::min({1.0, lt / kFade, (kEach - lt) / kFade});
    a = ease(a);
    const float s = S();
    const std::string& line = introLines_[i];
    const float tw = r_->textWidth(line, 24 * s);
    r_->rect(W_ * 0.5f - tw * 0.5f - 26 * s, H_ * 0.36f - 12 * s, tw + 52 * s, 24 * s * 1.5f + 24 * s,
             {0.03f, 0.03f, 0.025f, 0.45f * a}, 14 * s);
    r_->text(W_ * 0.5f, H_ * 0.36f, 24 * s, line, {0.93f, 0.89f, 0.78f, a}, gfx::FontFace::Regular, gfx::Align::Center);
}

void Ui::drawEnding(Host& h) {
    game::GameSession* ses = h.session();
    const float s = S();
    if (endingStart_ < 0.0) endingStart_ = now_;
    const float t = (float)(now_ - endingStart_);
    // Grund: reines Weiss, das langsam warm wird
    const float warm = ease(t / 6.0f);
    r_->rect(0, 0, W_, H_, {1.0f - 0.04f * warm, 1.0f - 0.055f * warm, 0.97f - 0.08f * warm, 1.0f});
    const vec4 ink = rgba8(58, 54, 46), soft = rgba8(120, 112, 96);
    auto fade = [&](vec4 c, float from, float len = 1.6f) { return vec4{c.x, c.y, c.z, c.w * ease((t - from) / len)}; };

    float y = H_ * 0.11f;
    r_->text(W_ * 0.5f, y, 46 * s, "D R A U S S E N", fade(ink, 1.0f, 2.2f), gfx::FontFace::Bold, gfx::Align::Center);
    y += 86 * s;
    static const char* kText =
        "Die Tür ging auf, und dahinter war kein Flur. Nur Licht – so hell, dass es keine Wände mehr gab, an denen "
        "etwas hätte summen können. Zum ersten Mal seit Langem hast du gar nichts gehört.";
    const float tw = std::min(W_ - 120 * s, 980 * s);
    for (const auto& line : r_->wrap(kText, 22 * s, tw)) {
        r_->text(W_ * 0.5f, y, 22 * s, line, fade(soft, 3.0f), gfx::FontFace::Regular, gfx::Align::Center);
        y += 32 * s;
    }
    y += 34 * s;

    // Laufbuch als Abspann
    std::vector<std::pair<std::string, bool>> credits;  // Zeile, hervorgehoben
    if (ses) {
        const auto& rl = ses->runLog();
        const auto& c = h.content();
        credits.push_back({"L A U F B U C H", true});
        credits.push_back({std::format("{}   ·   {}", ses->name, c.difficulty(ses->difficulty).label), false});
        credits.push_back({std::format("Entkommen nach {}", clock(rl.escaped ? rl.escapeTime : ses->stats().playTime)), true});
        credits.push_back({std::format("{:.0f} m zurückgelegt   ·   {} {} betreten", ses->player().distance, rl.rooms,
                                       rl.rooms == 1 ? "Raum" : "Räume"),
                           false});
        const int lv = rl.distinctLevels();
        credits.push_back({std::format("{} {}   ·   {} {}", lv, lv == 1 ? "Ebene" : "Ebenen", rl.exits,
                                       rl.exits == 1 ? "Übergang" : "Übergänge"),
                           false});
        credits.push_back({std::format("{} von {} Notizen gelesen   ·   {} {}", ses->journal().size(), c.notes().size(), rl.visions,
                                       rl.visions == 1 ? "Halluzination" : "Halluzinationen"),
                           false});
        credits.push_back({rl.encounters == 0 ? std::string("Gesehen hast du keinen von ihnen.")
                                              : std::format("{} {}   ·   {}× hat er dich erwischt", rl.encounters,
                                                            rl.encounters == 1 ? "Begegnung" : "Begegnungen", rl.caught),
                           false});
        credits.push_back({std::format("Seed {}", ses->seed), false});
    }
    for (size_t i = 0; i < credits.size(); ++i) {
        const float from = 5.0f + 0.45f * (float)i;
        const bool hi = credits[i].second;
        r_->text(W_ * 0.5f, y, (hi ? 21.0f : 19.0f) * s, credits[i].first, fade(hi ? ink : soft, from, 1.2f),
                 hi ? gfx::FontFace::Bold : gfx::FontFace::Regular, gfx::Align::Center);
        y += (i == 0 ? 42.0f : 31.0f) * s;
    }

    // Auswahl (erst, wenn der Abspann steht)
    const float ready = 5.0f + 0.45f * (float)credits.size() + 0.8f;
    if (t < ready) {
        if (in_->actionPressed(Action::UiConfirm) || in_->actionPressed(Action::UiBack)) endingStart_ -= 100.0;  // ueberspringen
        return;
    }
    const float a = ease((t - ready) / 0.8f);
    const float w = std::min(W_ - 80 * s, 640 * s), hh = 150 * s;
    const float x = (W_ - w) * 0.5f, py = std::max(y + 20 * s, H_ - hh - 56 * s);
    r_->setAlpha(a);
    r_->rect(x, py, w, hh, kPanel, 12 * s, 1.5f * s, kBorder);
    std::vector<Item> items = {
        {"Weiter erkunden", nullptr, nullptr,
         [this, &h] {
             if (auto* se = h.session()) se->finishEscape();
             setMode(Mode::Hud);
             fadeFromWhite(2.0);
         },
         ""},
        {"Zum Hauptmenü", nullptr, nullptr, [&h] { h.toTitle(); }, ""},
    };
    runList(h, items, lists_[9], x + 24 * s, py + 22 * s, w - 48 * s, 52 * s, 2);
    r_->setAlpha(1.0f);
}

}  // namespace lim::ui
