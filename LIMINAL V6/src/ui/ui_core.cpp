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
    if (m == Mode::Dead && mode_ != Mode::Dead) lists_[10] = {};
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

void Ui::panel(float x, float y, float w, float hh) {
    r_->rect(x, y, w, hh, kPanel, 10 * S(), 1.2f * S(), kBorder);
}

float Ui::header(float x, float y, float w, const std::string& title, const std::string& sub) {
    float s = S();
    r_->text(x + 36 * s, y + 26 * s, 34 * s, title, kTitle, gfx::FontFace::Bold);
    if (!sub.empty()) r_->text(x + w - 36 * s, y + 38 * s, 20 * s, sub, kMuted, gfx::FontFace::Regular, gfx::Align::Right);
    r_->rect(x + 36 * s, y + 78 * s, w - 72 * s, 1.5f * s, kRule);
    return y + 96 * s;
}

void Ui::footer(float x, float y, float w, const std::string& hint) {
    float s = S();
    r_->rect(x + 36 * s, y, w - 72 * s, 1.5f * s, kRule);
    r_->text(x + 36 * s, y + 12 * s, 17 * s, hint, kMuted);
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
        if (sel) r_->rect(x, ry + 2 * s, w, rowH - 4 * s, kSelBg, 6 * s);
        vec4 tc = sel ? kSelText : (it.muted ? kMuted : kText);
        float ty = ry + (rowH - 24 * s) * 0.5f - 2 * s;
        r_->text(x + 22 * s, ty, 24 * s, (sel ? "▸  " : "    ") + it.label, tc, sel ? gfx::FontFace::Bold : gfx::FontFace::Regular);
        if (it.value) {
            std::string v = it.value();
            if (it.adjust && sel) v = "◂  " + v + "  ▸";
            r_->text(x + w - 22 * s, ty, 24 * s, v, sel ? kSelText : kValue, gfx::FontFace::Regular, gfx::Align::Right);
        }
    }
    if (maxScroll > 0) {
        float barH = rowH * visible;
        float th = barH * visible / n;
        float ty = y + (barH - th) * (st.scroll / maxScroll);
        r_->rect(x + w + 6 * s, ty, 4 * s, th, kMuted, 2 * s);
    }
}

void Ui::drawLoading(Host& h) {
    float s = S();
    r_->text(W_ * 0.5f, H_ * 0.42f, 64 * s, "L I M I N A L", kTitle, gfx::FontFace::Bold, gfx::Align::Center);
    r_->text(W_ * 0.5f, H_ * 0.42f + 90 * s, 22 * s, loadingText_, kMuted, gfx::FontFace::Regular, gfx::Align::Center);
    float bw = 420 * s, bx = (W_ - bw) * 0.5f, by = H_ * 0.42f + 140 * s;
    r_->rect(bx, by, bw, 6 * s, kBarBg, 3 * s);
    float t = (float)std::fmod(now_ * 0.6, 1.0);
    if (loadingProgress_ > 0.0f) r_->rect(bx, by, bw * std::clamp(loadingProgress_, 0.02f, 1.0f), 6 * s, kTitle, 3 * s);
    else r_->rect(bx + bw * t * 0.75f, by, bw * 0.25f, 6 * s, kTitle, 3 * s);
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
    switch (mode_) {
        case Mode::Loading: drawLoading(h); break;
        case Mode::Title: drawTitle(h); break;
        case Mode::Hud: drawHud(h, dt); break;
        case Mode::Pause: drawPause(h); break;
        case Mode::Inventory:
            drawInventory(h);
            break;
        case Mode::Dead: drawDead(h); break;
    }
    // V6: Ueberblendung aus Schwarz ueber allem
    if (fadeStart_ < 0.0) fadeStart_ = now_;
    if (fadeLen_ > 0.0 && now_ - fadeStart_ < fadeLen_) {
        float t = (float)((now_ - fadeStart_) / fadeLen_);
        float a = 1.0f - t * t * (3.0f - 2.0f * t);
        r_->rect(0, 0, W_, H_, {0.0f, 0.0f, 0.0f, a});
    }
    // abgelaufene Meldungen entfernen
    std::erase_if(toasts_, [&](const Toast& t) { return t.until < now_ - 1.0; });
}

}  // namespace lim::ui
