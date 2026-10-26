// Grundlagen der Oberflaeche: Ablauf je Bild, Listen-Widget, Ladebildschirm.
#include <algorithm>
#include <cmath>
#include <format>

#include "input/input.hpp"
#include "render/renderer.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace lim::ui {

using namespace theme;
using input::Action;

std::string upperUtf8(const std::string& s) {
    std::u32string u = gfx::utf8To32(s);
    for (char32_t& c : u) {
        if (c >= U'a' && c <= U'z') c -= 32;
        else if ((c >= 0xE0 && c <= 0xFE && c != 0xF7)) c -= 32;  // Latin-1: ä ö ü é ... -> Ä Ö Ü É ...
    }
    return gfx::utf32To8(u);
}

void Ui::setMode(Mode m) {
    // Von aussen (Spieltaste) geoeffnet: derselbe Tastendruck darf im Menue nichts mehr ausloesen
    if (!inFrame_ && m != mode_) swallowInput_ = true;
    if (m == Mode::Hud || m == Mode::Title) helpFromGame_ = false;
    // Menues beginnen immer auf dem ersten Eintrag ("Weiter erkunden", "Neues Spiel" ...)
    if (m == Mode::Pause && mode_ != Mode::Pause) {
        page_ = "main";
        pageStack_.clear();
        lists_[0] = {};
    }
    if (m == Mode::Title && mode_ != Mode::Title) {
        page_ = "main";
        pageStack_.clear();
        titleMsg_.clear();
        lists_[0] = {};
    }
    if (m == Mode::Dead && mode_ != Mode::Dead) lists_[10] = {}, deadShown_ = -1.0;
    if (m == Mode::Ending && mode_ != Mode::Ending) lists_[9] = {}, endingStart_ = -1.0;  // V9
    // V7: nach dem Ebenenwechsel nicht schwarz bleiben (V9: der Abspann zeichnet sein Weiss selbst)
    if (m == Mode::Hud || m == Mode::Title || m == Mode::Ending) fadeOutLen_ = 0.0;
    if (m == Mode::Title) introLines_.clear();
    if (m == Mode::Inventory) {
        invRow_ = 3;
        invCol_ = 0;
    }
    mode_ = m;
}

void Ui::toast(const std::string& text, double duration) {
    // gleiche Meldung erneut: nur verlaengern statt doppelt anzeigen
    for (auto& t : toasts_)
        if (t.text == text && t.until > now_) {
            t.until = std::max(t.until, now_ + duration);
            return;
        }
    toasts_.push_back({text, now_, now_ + duration});
    // hoechstens drei sichtbar: die aelteste raeumt zuegig (aber weich) den Platz
    int active = 0;
    for (auto& t : toasts_) active += t.until > now_;
    for (auto& t : toasts_)
        if (active > 3 && t.until > now_) {
            t.until = std::min(t.until, now_ + 0.25);
            --active;
        }
}

void Ui::regionTitle(const std::string& text) {
    regionTitle_ = text;
    regionSource_.clear();
    regionStart_ = now_;
}

void Ui::levelCard(const std::string& id, const std::string& name, const std::string& title) {
    // Wiki-Seite aus der Level-ID: level5_3 -> level-5-3
    std::string slug = id;
    if (slug.rfind("level", 0) == 0) {
        slug = "level-" + slug.substr(5);
        for (char& c : slug)
            if (c == '_') c = '-';
    }
    regionTitle_ = title.empty() ? name : name + " · " + title;
    regionSource_ = "nach backrooms-wiki.wikidot.com/" + slug + "  ·  CC BY-SA 3.0";
    regionStart_ = now_;
}

void Ui::zoneTitle(const std::string& text) {
    zone_ = text;
    zoneUntil_ = now_ + 3.5;
}

void Ui::openHelp() {
    if (!inFrame_) swallowInput_ = true;
    helpFromGame_ = mode_ == Mode::Hud;
    if (mode_ == Mode::Hud) setMode(Mode::Pause);
    pageStack_.push_back(page_);
    page_ = "help";
    helpScroll_ = 0;
}

int Ui::listIndex(const std::string& page) const {
    static const char* names[] = {"main", "mouse", "graphics", "audio", "help", "new", "load", "controls"};
    for (int i = 0; i < 8; ++i)
        if (page == names[i]) return i;
    return 11;
}

bool Ui::hovered(float x, float y, float w, float hh) const {
    vec2 c = in_->cursor;
    return c.x >= x && c.x < x + w && c.y >= y && c.y < y + hh;
}

bool Ui::clicked(float x, float y, float w, float hh) const {
    return in_->pressed(input::MOUSE_LEFT) && hovered(x, y, w, hh);
}

// V12: Tafel wie ein Stationsschild - gerade Kanten, links eine Signalkante ueber die volle Hoehe
void Ui::panel(float x, float y, float w, float hh) {
    float s = S();
    r_->rect(x, y, w, hh, kPanel, 0.0f, 1.0f * s, kBorder);
    r_->rect(x, y, 4 * s, hh, kAccent);
}

float Ui::header(float x, float y, float w, const std::string& title, const std::string& sub) {
    float s = S();
    r_->text(x + 40 * s, y + 28 * s, kFsTitle * s, title, kTitle, gfx::FontFace::Bold);
    if (!sub.empty())
        r_->text(x + w - 36 * s, y + 38 * s, kFsCaps * s, upperUtf8(sub), kAccent, gfx::FontFace::Bold, gfx::Align::Right);
    r_->rect(x + 40 * s, y + 78 * s, w - 76 * s, 1.0f * s, kRule);
    return y + 94 * s;
}

void Ui::footer(float x, float y, float w, const std::string& hint) {
    float s = S();
    r_->rect(x + 40 * s, y, w - 76 * s, 1.0f * s, kRule);
    r_->text(x + 40 * s, y + 16 * s, kFsSmall * s, hint, kMuted);
}

// V12: Streckenplan wie auf einer Bahnsteigtafel. Hauptlinie 0-1-2-3-5-903-36, Abzweige ueber/unter der Linie:
// 5.3 (oben) und 188 (unten) an Level 5, 37 (oben) an 903. Die aktuelle Ebene ist ein gefuellter Punkt mit Ring.
float Ui::drawRoute(float x, float y, float w, const std::string& current) {
    float s = S();
    struct Stop {
        const char* id;
        const char* label;
        float t;    // Position entlang der Linie (0..1)
        int branch; // 0 Hauptlinie, -1 oben, +1 unten
    };
    static const Stop stops[] = {{"level0", "0", 0.0f, 0},     {"level1", "1", 0.15f, 0},     {"level2", "2", 0.3f, 0},
                                 {"level3", "3", 0.45f, 0},    {"level5", "5", 0.6f, 0},      {"level5_3", "5.3", 0.6f, -1},
                                 {"level188", "188", 0.6f, 1}, {"level903", "903", 0.8f, 0},  {"level37", "37", 0.8f, -1},
                                 {"level36", "36", 1.0f, 0}};
    const float lx0 = x + 14 * s, lx1 = x + w - 14 * s, ly = y + 34 * s, dy = 26 * s, r = 6 * s;
    r_->text(x, y, kFsCaps * s, "S T R E C K E", kMuted, gfx::FontFace::Bold);
    r_->rect(lx0, ly - 1 * s, lx1 - lx0, 2 * s, kAccent);
    for (const Stop& st : stops) {
        const float cx = lx0 + (lx1 - lx0) * st.t;
        const float cy = ly + dy * (float)st.branch;
        if (st.branch != 0) r_->rect(cx - 1 * s, std::min(ly, cy), 2 * s, std::abs(cy - ly), kAccent);
        const bool cur = current == st.id;
        if (cur) r_->rect(cx - r - 4 * s, cy - r - 4 * s, 2 * (r + 4 * s), 2 * (r + 4 * s), rgba8(0, 0, 0, 0), r + 4 * s, 1.5f * s, kAccent);
        r_->rect(cx - r, cy - r, 2 * r, 2 * r, cur ? kAccent : kPanel, r, 2 * s, kAccent);
        // Beschriftung: Hauptlinie unter dem Punkt, Abzweige seitlich
        const vec4 tc = cur ? kSelText : kText;
        if (st.branch == 0) r_->text(cx, ly + 14 * s, 13 * s, st.label, tc, gfx::FontFace::Bold, gfx::Align::Center);
        else r_->text(cx + 12 * s, cy - 9 * s, 13 * s, st.label, tc, gfx::FontFace::Bold);
    }
    return 34 * s + dy + 30 * s;
}

// V12: Seitenleiste fuer Titel und Pause: tiefes Blauschwarz, links bis 34 % der Breite (min. 520 px bei 1080 p),
// rechts weicher Uebergang in den Raum dahinter; oben die Wortmarke
float Ui::sidebar(const std::string& tagline) {
    float s = S();
    const float sw = std::clamp(W_ * 0.34f, 520 * s, 760 * s);
    r_->rect(0, 0, sw, H_, kSidebar);
    const int strips = 16;
    const float fade = 120 * s;
    for (int k = 0; k < strips; ++k) {
        const float t = ((float)k + 0.5f) / strips;
        r_->rect(sw + fade * (float)k / strips, 0, fade / strips + 1, H_, rgba8(7, 9, 11, (int)(244 * (1.0f - t) * (1.0f - t))));
    }
    r_->rect(sw - 1 * s, 0, 1 * s, H_, kRule);
    const float lx = 64 * s, ly = 72 * s;
    r_->text(lx, ly, 64 * s, "L I M I N A L", kTitle, gfx::FontFace::Bold);
    r_->rect(lx + 2 * s, ly + 92 * s, 56 * s, 4 * s, kAccent);
    r_->text(lx + 2 * s, ly + 110 * s, kFsCaps * s, tagline, kMuted, gfx::FontFace::Bold);
    return sw;
}

// Liste mit Auswahl: Tastatur/Gamepad (hoch/runter, links/rechts, bestaetigen), Maus, Mausrad.
void Ui::runList(Host& h, std::vector<Item>& items, ListState& st, float x, float y, float w, float rowH, int visible) {
    if (items.empty()) return;
    float s = S();
    int n = (int)items.size();
    st.sel = std::clamp(st.sel, 0, n - 1);
    if (in_->actionPressed(Action::UiUp)) {
        st.sel = (st.sel - 1 + n) % n;
        h.playSound("ui_move");
    }
    if (in_->actionPressed(Action::UiDown)) {
        st.sel = (st.sel + 1) % n;
        h.playSound("ui_move");
    }
    int maxScroll = std::max(0, n - visible);
    if (in_->wheel() && hovered(x, y, w, rowH * visible)) st.scroll = std::clamp(st.scroll - in_->wheel(), 0.0f, (float)maxScroll);
    if (st.sel < (int)st.scroll) st.scroll = (float)st.sel;
    if (st.sel >= (int)st.scroll + visible) st.scroll = (float)(st.sel - visible + 1);
    Item& cur = items[(size_t)st.sel];
    if (in_->actionPressed(Action::UiLeft) && cur.adjust) {
        cur.adjust(-1);
        h.playSound("ui_move");
    }
    if (in_->actionPressed(Action::UiRight) && cur.adjust) {
        cur.adjust(+1);
        h.playSound("ui_move");
    }
    bool confirm = in_->actionPressed(Action::UiConfirm);
    int first = (int)st.scroll;
    for (int i = first; i < std::min(n, first + visible); ++i) {
        float ry = y + (float)(i - first) * rowH;
        if (in_->cursorMoved && hovered(x, ry, w, rowH)) st.sel = i;
        if (clicked(x, ry, w, rowH)) {
            st.sel = i;
            confirm = true;
            // Klick auf die Pfeile links/rechts verstellt den Wert
            Item& it = items[(size_t)i];
            if (it.adjust && it.value) {
                float cx = in_->cursor.x;
                if (cx > x + w - 300 * s && cx < x + w - 150 * s) {
                    it.adjust(-1);
                    confirm = false;
                    h.playSound("ui_move");
                } else if (cx >= x + w - 150 * s) {
                    it.adjust(+1);
                    confirm = false;
                    h.playSound("ui_move");
                }
            }
        }
    }
    if (confirm) {
        Item& it = items[(size_t)st.sel];
        if (it.activate) {
            h.playSound("ui_select");
            it.activate();
        } else if (it.adjust) {
            h.playSound("ui_move");
            it.adjust(+1);
        }
    }
    // Zeichnen
    for (int i = first; i < std::min(n, first + visible); ++i) {
        const Item& it = items[(size_t)i];
        float ry = y + (float)(i - first) * rowH;
        bool sel = i == st.sel;
        // V11: zurueckhaltende Auswahl - leicht aufgehellte Zeile, schmale Akzentmarke links, hellerer Text
        // V12: Auswahl wie ein beleuchtetes Feld der Tafel - Hauch Signalgelb, volle Signalkante links, Pfeil
        if (sel) {
            r_->rect(x, ry + 2 * s, w, rowH - 4 * s, kSelRow);
            r_->rect(x, ry + 2 * s, 4 * s, rowH - 4 * s, kAccent);
        }
        vec4 tc = sel ? kSelText : (it.muted ? kMuted : kText);
        float ty = ry + (rowH - kFsRow * s) * 0.5f - 2 * s;
        if (sel) r_->text(x + 18 * s, ty, kFsRow * s, "›", kAccent, gfx::FontFace::Bold);
        r_->text(x + (sel ? 40 : 26) * s, ty, kFsRow * s, it.label, tc, sel ? gfx::FontFace::Bold : gfx::FontFace::Regular);
        if (it.value) {
            std::string v = it.value();
            if (it.adjust && sel) v = "◂  " + v + "  ▸";
            r_->text(x + w - 22 * s, ty, kFsRow * s, v, sel ? kAccent : kValue, gfx::FontFace::Regular, gfx::Align::Right);
        }
    }
    if (maxScroll > 0) {
        float barH = rowH * visible;
        float th = barH * visible / n;
        float ty = y + (barH - th) * (st.scroll / maxScroll);
        r_->rect(x + w + 6 * s, y, 2 * s, barH, kRule, 1 * s);
        r_->rect(x + w + 6 * s, ty, 2 * s, th, kMuted, 1 * s);
    }
}

void Ui::drawLoading(Host& h) {
    float s = S();
    // V12: wie die Tafel bei der Einfahrt - Wortmarke, Text, haarfeiner Fortschritt in Signalgelb
    const float cx = W_ * 0.5f, cy = H_ * 0.40f;
    r_->text(cx, cy, 56 * s, "L I M I N A L", kTitle, gfx::FontFace::Bold, gfx::Align::Center);
    r_->rect(cx - 28 * s, cy + 86 * s, 56 * s, 4 * s, kAccent);
    r_->text(cx, cy + 108 * s, kFsBody * s, loadingText_, kMuted, gfx::FontFace::Regular, gfx::Align::Center);
    float bw = 420 * s, bx = (W_ - bw) * 0.5f, by = cy + 150 * s;
    r_->rect(bx, by, bw, 2 * s, kEmpty);
    float t = (float)std::fmod(now_ * 0.6, 1.0);
    if (loadingProgress_ > 0.0f) r_->rect(bx, by, bw * std::clamp(loadingProgress_, 0.02f, 1.0f), 2 * s, kAccent);
    else r_->rect(bx + bw * t * 0.75f, by, bw * 0.25f, 2 * s, kAccent);
}

void Ui::frame(Host& h, double dt) {
    r_ = &h.renderer().ui();
    in_ = &h.input();
    now_ = h.time();
    W_ = (float)r_->width();
    H_ = (float)r_->height();
    scale_ = std::clamp(H_ / 1080.0f, 0.6f, 3.0f);
    if (swallowInput_) {
        in_->consumePressed();
        swallowInput_ = false;
    }
    inFrame_ = true;
    struct Leave {
        bool& f;
        ~Leave() { f = false; }
    } leave{inFrame_};
    // V7: Abblenden in Schwarz (bleibt schwarz, bis die neue Ebene einblendet). V11: der Todesschirm
    // liegt ueber dem Schwarz (nach dem Schreck), alles andere darunter
    auto fadeOut = [&] {
        if (fadeOutLen_ <= 0.0) return;
        if (fadeOutStart_ < 0.0) fadeOutStart_ = now_;
        float t = std::clamp((float)((now_ - fadeOutStart_) / fadeOutLen_), 0.0f, 1.0f);
        const float c = fadeOutWhite_ ? 1.0f : 0.0f;  // V9: die letzte Fahrt endet in Weiss
        r_->rect(0, 0, W_, H_, {c, c, c * 0.97f, t * t * (3.0f - 2.0f * t)});
    };
    if (mode_ == Mode::Dead) fadeOut();
    switch (mode_) {
        case Mode::Loading: drawLoading(h); break;
        case Mode::Title: drawTitle(h); break;
        case Mode::Hud: drawHud(h, dt); break;
        case Mode::Pause: drawPause(h); break;
        case Mode::Inventory:
            drawInventory(h);
            break;
        case Mode::Dead: drawDead(h); break;
        case Mode::Note: drawNote(h); break;
        case Mode::Ending: drawEnding(h); break;  // V9
    }
    // V6: Ueberblendung aus Schwarz ueber allem
    if (fadeStart_ < 0.0) fadeStart_ = now_;
    if (fadeLen_ > 0.0 && now_ - fadeStart_ < fadeLen_) {
        float t = (float)((now_ - fadeStart_) / fadeLen_);
        float a = 1.0f - t * t * (3.0f - 2.0f * t);
        const float c = fadeInWhite_ ? 1.0f : 0.0f;  // V9: nach dem Abspann aus Weiss
        r_->rect(0, 0, W_, H_, {c, c, c * 0.97f, a});
    }
    if (mode_ != Mode::Dead) fadeOut();
    // abgelaufene Meldungen entfernen
    std::erase_if(toasts_, [&](const Toast& t) { return t.until < now_ - 1.0; });
}

}  // namespace lim::ui
