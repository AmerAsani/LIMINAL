// V7: Notizen lesen (Papier vor dem abgedunkelten Spiel) und Notizbuch im Pausenmenue.
#include <algorithm>
#include <format>

#include "gameplay/content.hpp"
#include "input/input.hpp"
#include "gameplay/session.hpp"
#include "render/renderer.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace lim::ui {

using namespace theme;
using input::Action;

namespace {
constexpr vec4 kPaper = rgba8(222, 213, 184, 250);
constexpr vec4 kPaperEdge = rgba8(160, 148, 112, 255);
constexpr vec4 kInk = rgba8(38, 44, 70);
constexpr vec4 kInkSoft = rgba8(90, 86, 72);

// Text mit Zeilenumbruechen ("\n") und automatischem Umbruch; liefert die Endhoehe
float paragraphs(gfx::UiRenderer& r, float x, float y, float size, float width, float lineH, const std::string& text,
                 vec4 color, bool draw) {
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        std::string part = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part.empty()) y += lineH * 0.6f;
        for (const auto& line : r.wrap(part, size, width)) {
            if (draw) r.text(x, y, size, line, color);
            y += lineH;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return y;
}
}  // namespace

// V7: Laufbuch in zwei Zeilen (Pausenmenue, Todesbildschirm)
std::vector<std::string> Ui::runLogLines(Host& h, game::GameSession& ses) {
    const auto& rl = ses.runLog();
    int pt = (int)ses.stats().playTime;
    const auto& lv = ses.world().level();
    std::string time = pt >= 3600 ? std::format("{}:{:02}:{:02} h", pt / 3600, pt / 60 % 60, pt % 60)
                                  : std::format("{}:{:02} min", pt / 60, pt % 60);
    int ebenen = rl.distinctLevels();
    return {std::format("Spielzeit {}   ·   {:.0f} m zurückgelegt   ·   {} {} betreten", time, ses.player().distance, rl.rooms,
                        rl.rooms == 1 ? "Raum" : "Räume"),
            std::format("{} {} ({} · {})   ·   {} {}   ·   {} von {} Notizen   ·   {} {}", ebenen, ebenen == 1 ? "Ebene" : "Ebenen",
                        lv.name, lv.title, rl.exits, rl.exits == 1 ? "Notausgang" : "Notausgänge", ses.journal().size(),
                        h.content().notes().size(), rl.visions, rl.visions == 1 ? "Halluzination" : "Halluzinationen")};
}

void Ui::drawNote(Host& h) {
    const auto& notes = h.content().notes();
    float s = S();
    r_->rect(0, 0, W_, H_, {0.0f, 0.0f, 0.0f, 0.55f});
    if (noteIndex_ < 0 || noteIndex_ >= (int)notes.size()) {
        setMode(Mode::Hud);
        return;
    }
    const game::NoteDef& n = notes[(size_t)noteIndex_];
    const float w = std::min(W_ - 60 * s, 720 * s), pad = 54 * s, size = 25 * s, lineH = 36 * s;
    float textH = paragraphs(*r_, 0, 0, size, w - 2 * pad, lineH, n.text, kInk, false);
    float hh = std::min(H_ - 60 * s, textH + 190 * s);
    float x = (W_ - w) * 0.5f, y = (H_ - hh) * 0.5f;
    // Papier, leicht schief "gehalten": Schatten, Rand, Faltlinie
    r_->rect(x + 10 * s, y + 14 * s, w, hh, {0.0f, 0.0f, 0.0f, 0.35f}, 4 * s);
    r_->rect(x, y, w, hh, kPaper, 4 * s, 1.5f * s, kPaperEdge);
    r_->rect(x + 20 * s, y + hh * 0.5f, w - 40 * s, 1.2f * s, rgba8(180, 168, 132, 160));
    r_->text(x + pad, y + 34 * s, 30 * s, n.title, kInk, gfx::FontFace::Bold);
    r_->rect(x + pad, y + 78 * s, w - 2 * pad, 1.5f * s, rgba8(150, 140, 110, 200));
    paragraphs(*r_, x + pad, y + 100 * s, size, w - 2 * pad, lineH, n.text, kInk, true);
    if (auto* ses = h.session())
        r_->text(x + w - pad, y + hh - 46 * s, 17 * s,
                 std::format("Notiz {} von {} im Notizbuch", ses->journal().size(), notes.size()), kInkSoft,
                 gfx::FontFace::Regular, gfx::Align::Right);
    r_->text(x + pad, y + hh - 46 * s, 17 * s, "E / ESC / Enter: weiter", kInkSoft);
    if (in_->actionPressed(Action::UiBack) || in_->actionPressed(Action::UiConfirm) || in_->actionPressed(Action::Interact) ||
        in_->pressed(input::MOUSE_LEFT)) {
        h.playSound("ui_back");
        setMode(Mode::Hud);
    }
}

void Ui::journalPage(Host& h, float x, float y, float w, float hgt) {
    float s = S();
    auto* ses = h.session();
    const auto& notes = h.content().notes();
    if (!ses || ses->journal().empty()) {
        r_->text(x + 40 * s, y + 10 * s, 22 * s, "Noch keine Notizen gefunden.", kText);
        paragraphs(*r_, x + 40 * s, y + 50 * s, 19 * s, w - 80 * s, 28 * s,
                   "Andere Wanderer haben Zettel hinterlassen. Sie liegen selten irgendwo auf dem Boden – mit E aufheben "
                   "und lesen. Manche enthalten Hinweise.",
                   kMuted, true);
    } else {
        const auto& j = ses->journal();
        std::vector<Item> items;
        for (size_t i = 0; i < j.size(); ++i) {
            int idx = j[i];
            items.push_back({std::format("{:>2}.  {}", i + 1, notes[(size_t)idx].title), nullptr, nullptr, nullptr, ""});
        }
        const int rows = std::max(3, std::min((int)items.size(), (int)((hgt * 0.42f) / (40 * s))));
        ListState& st = lists_[8];
        runList(h, items, st, x + 24 * s, y, w - 48 * s, 40 * s, rows);
        journalSel_ = std::clamp(st.sel, 0, (int)j.size() - 1);
        float ty = y + rows * 40 * s + 16 * s;
        r_->rect(x + 40 * s, ty, w - 80 * s, std::max(0.0f, y + hgt - ty - 8 * s), rgba8(222, 213, 184, 30), 6 * s);
        paragraphs(*r_, x + 60 * s, ty + 16 * s, 20 * s, w - 120 * s, 29 * s, notes[(size_t)j[(size_t)journalSel_]].text,
                   rgba8(225, 214, 180), true);
    }
    if (in_->actionPressed(Action::UiBack)) {
        h.playSound("ui_back");
        page_ = pageStack_.empty() ? "main" : pageStack_.back();
        if (!pageStack_.empty()) pageStack_.pop_back();
    }
}

}  // namespace lim::ui
