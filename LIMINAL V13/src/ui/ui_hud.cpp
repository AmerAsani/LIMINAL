// HUD (Schnellleiste, Werte, Hinweise, Karte, Debug), Inventar und Todesbildschirm.
#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <string>

#include "app/settings.hpp"
#include "gameplay/content.hpp"
#include "gameplay/session.hpp"
#include "input/input.hpp"
#include "render/renderer.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"
#include "world/materials.hpp"
#include "world/room.hpp"

namespace lim::ui {

using namespace theme;
using input::Action;

static const char* kHint =
    "W A S D  gehen   ·   Leertaste  springen   ·   E  aufnehmen   ·   T  Taschenlampe   ·   Tab  Inventar   ·   M  Karte   ·   ESC  Menü & Hilfe";

// V11: Kapitaelchen mit Sperrsatz ("H E A L T H"), fuer kleine Beschriftungen
static std::string spacedCaps(const std::string& t) {
    std::string out;
    for (char32_t c : gfx::utf8To32(upperUtf8(t))) {
        if (!out.empty()) out += ' ';
        out += gfx::utf32To8(std::u32string(1, c));
    }
    return out;
}

void Ui::slotBox(Host& h, const game::Inventory& inv, int idx, float x, float y, float size, bool selected, bool cursor) {
    float s = S();
    const auto& slot = inv.slots[(size_t)idx];
    // V11: dunkles Glas mit feiner Kante; aktiver Platz mit Akzentkante, Inventar-Cursor hell
    const vec4 edge = selected ? kAccent : (cursor ? kSelText : kSlotBorder);
    r_->rect(x, y, size, size, selected ? kSlotSel : (slot ? kSlotFull : kSlot), 0.0f, (selected || cursor) ? 2.0f * s : 1.0f * s,
             edge);
    if (slot) {
        const game::ItemDef* d = h.content().item(slot->kind);
        auto* icon = h.renderer().itemIcon(d ? d->icon : slot->kind);
        float pad = size * 0.16f;
        r_->image(icon, x + pad, y + pad, size - 2 * pad, size - 2 * pad);
        if (slot->count > 1)
            r_->text(x + size - 5 * s, y + size - 19 * s, 14 * s, std::format("{}", slot->count), kValue, gfx::FontFace::Bold,
                     gfx::Align::Right);
    }
}

// V11: ruhiges HUD. Unten links Health und Sanity als schmale Balken (voll und unveraendert treten sie zurueck,
// niedrig bleiben sie deutlich), daneben die Lampe nur, wenn sie an oder schwach ist. Unten mittig die
// Schnellleiste - nur mit Inhalt, nach einer Aenderung kurz deutlich, sonst gedimmt; darueber die Ausdauer als
// duenne Linie, nur wenn sie nicht voll ist.
void Ui::drawHotbar(Host& h, game::GameSession& ses) {
    float s = S();
    const auto& inv = ses.inventory();
    const auto& st = ses.stats();
    const float base = r_->alpha();  // Blick nach unten auf den eigenen Koerper dimmt alles
    // --- Schnellleiste
    size_t sig = (size_t)inv.selected;
    bool any = false;
    for (int i = 0; i < game::Inventory::kHotbar; ++i) {
        const auto& sl = inv.slots[(size_t)i];
        any = any || (bool)sl;
        sig = sig * 31u + (sl ? std::hash<std::string>{}(sl->kind) + (size_t)sl->count : 0u);
    }
    if (sig != hotSig_) hotSig_ = sig, hotShow_ = now_ + 3.0;
    if (inv.selected != hotSel_) {
        if (hotSel_ >= 0) nameShow_ = now_ + 2.0;
        hotSel_ = inv.selected;
    }
    const float hotA = any ? 0.42f + 0.58f * (float)std::clamp((hotShow_ - now_) / 0.8, 0.0, 1.0) : 0.0f;
    const float size = 50 * s, gap = 5 * s;
    const float total = game::Inventory::kHotbar * size + (game::Inventory::kHotbar - 1) * gap;
    const float x0 = (W_ - total) * 0.5f, y0 = H_ - size - 30 * s;
    if (hotA > 0.01f) {
        r_->setAlpha(base * hotA);
        for (int i = 0; i < game::Inventory::kHotbar; ++i) {
            float x = x0 + i * (size + gap);
            slotBox(h, inv, i, x, y0, size, i == inv.selected, false);
            r_->text(x + 5 * s, y0 + 3 * s, 12 * s, std::to_string(i + 1), i == inv.selected ? kAccent : kMuted);
        }
        r_->setAlpha(base);
    }
    // Name des gewaehlten Gegenstands nach dem Wechsel kurz ueber der Leiste
    if (const auto& sl = inv.slots[(size_t)inv.selected]; sl && now_ < nameShow_) {
        const game::ItemDef* d = h.content().item(sl->kind);
        float a = (float)std::clamp((nameShow_ - now_) / 0.5, 0.0, 1.0) * base;
        std::string t = d ? d->name : sl->kind;
        float tw = r_->textWidth(t, 15 * s);
        r_->rect(W_ * 0.5f - tw * 0.5f - 12 * s, y0 - 42 * s, tw + 24 * s, 28 * s, rgba8(0, 0, 0, (int)(110 * a)), 4 * s);
        r_->text(W_ * 0.5f, y0 - 38 * s, 15 * s, t, vec4{kValue.x, kValue.y, kValue.z, a}, gfx::FontFace::Regular, gfx::Align::Center);
    }
    // --- Ausdauer
    {
        float stam = (float)std::clamp(st.stamina / 100.0, 0.0, 1.0);
        if (stam < 0.995f || st.exhausted) staminaShow_ = now_ + 1.2;
        float a = (float)std::clamp((staminaShow_ - now_) / 0.5, 0.0, 1.0) * base;
        if (a > 0.01f) {
            float sw = 160 * s, sh = 3 * s, sx = W_ * 0.5f - sw * 0.5f, sy = y0 - 12 * s;
            r_->setAlpha(a);
            r_->rect(sx - 5 * s, sy - 4 * s, sw + 10 * s, sh + 8 * s, rgba8(0, 0, 0, 90), 4 * s);
            r_->rect(sx, sy, sw, sh, kEmpty, sh * 0.5f);
            bool blink = st.exhausted && std::fmod(now_ * 2.5, 1.0) < 0.5;
            vec4 col = st.exhausted ? (blink ? kLow : rgba8(170, 70, 70)) : kValue;
            r_->rect(sx, sy, sw * stam, sh, col, sh * 0.5f);
            // erschoepft: Marke, ab der wieder gesprintet werden kann
            if (st.exhausted) r_->rect(sx + sw * 0.3f - 1 * s, sy - 3 * s, 2 * s, sh + 6 * s, rgba8(255, 255, 255, 140));
            r_->setAlpha(base);
        }
    }
    // --- Werte unten links
    if (std::abs(st.health - vitH_) >= 2.0 || std::abs(st.sanity - vitS_) >= 2.0) {
        vitH_ = st.health, vitS_ = st.sanity;
        vitalsShow_ = now_ + 4.0;
    }
    const bool low = st.health < 50.0 || st.sanity < 50.0;
    const float va = low ? 1.0f : 0.5f + 0.5f * (float)std::clamp((vitalsShow_ - now_) / 1.0, 0.0, 1.0);
    const float vx = 44 * s, bw = 190 * s, bh = 4 * s;
    const float rowS = H_ - 44 * s, rowH = rowS - 40 * s;  // y der Balken (Sanity unten, Health darueber)
    auto backdrop = [&](float x, float y, float w, float hh) {  // weicher dunkler Grund fuer helle Waende
        for (int k = 0; k < 3; ++k) {
            float g = (float)k * 9 * s;
            r_->rect(x - g, y - g, w + 2 * g, hh + 2 * g, rgba8(0, 0, 0, 34), 12 * s + g);
        }
    };
    r_->setAlpha(base * va);
    backdrop(vx - 18 * s, rowH - 28 * s, bw + 36 * s, 80 * s);
    auto bar = [&](float y, const char* label, double v, vec4 col) {
        r_->text(vx, y - 20 * s, 13 * s, spacedCaps(label), kMuted, gfx::FontFace::Bold);
        r_->text(vx + bw, y - 20 * s, 13 * s, std::format("{}", (int)std::lround(v)), col, gfx::FontFace::Bold, gfx::Align::Right);
        r_->rect(vx, y, bw, bh, kEmpty, bh * 0.5f);
        r_->rect(vx, y, (float)(bw * std::clamp(v / 100.0, 0.0, 1.0)), bh, col, bh * 0.5f);
    };
    bar(rowH, "Leben", st.health, st.health < 35 ? kLow : kHealth);
    bool pulse = st.sanity < 20 && std::fmod(now_ * 3.0, 2.0) < 1.0;
    bar(rowS, "Verstand", st.sanity, (pulse || st.sanity <= 0) ? kLow : kSanity);
    r_->setAlpha(base);
    // --- Lampe rechts daneben (an, gerade geschaltet oder schwach)
    {
        const game::Flashlight& fl = ses.flashlight();
        float charge = (float)std::clamp(fl.charge / 100.0, 0.0, 1.0);
        if (fl.on || fl.charge < 25.0) flashShow_ = now_ + 2.5;
        float a = (float)std::clamp((flashShow_ - now_) / 0.6, 0.0, 1.0) * base;
        if (a > 0.01f) {
            r_->setAlpha(a);
            const float lx = vx + bw + 44 * s, bw2 = 30 * s, bh2 = 12 * s, by = rowS - 4 * s;
            backdrop(lx - 16 * s, rowS - 28 * s, 108 * s, 40 * s);
            bool empty = fl.charge <= 0.0, blink = fl.low() && std::fmod(now_ * 2.0, 1.0) < 0.5;
            vec4 col = fl.charge < 15.0 ? (blink ? kLow : rgba8(190, 80, 60)) : fl.charge < 30.0 ? rgba8(226, 168, 80) : kValue;
            r_->text(lx, rowS - 20 * s, 13 * s, spacedCaps("Lampe"), kMuted, gfx::FontFace::Bold);
            r_->rect(lx, by, bw2, bh2, rgba8(0, 0, 0, 0), 2 * s, 1.2f * s, fl.on ? col : kSlotBorder);
            r_->rect(lx + bw2 + 1 * s, by + bh2 * 0.3f, 2.5f * s, bh2 * 0.4f, fl.on ? col : kSlotBorder, 1 * s);
            if (!empty) r_->rect(lx + 2.5f * s, by + 2.5f * s, (bw2 - 5 * s) * charge, bh2 - 5 * s, col, 1 * s);
            r_->text(lx + bw2 + 12 * s, by - 2 * s, 13 * s, empty ? "leer" : std::format("{}", (int)std::ceil(fl.charge)),
                     fl.on ? kValue : kMuted, gfx::FontFace::Bold);
            r_->setAlpha(base);
        }
    }
    if (st.speedLeft > 0) {
        int sl = (int)st.speedLeft;
        r_->text(W_ * 0.5f, y0 - 66 * s, 15 * s, std::format("⚡ +{} %   {}:{:02}", (int)std::lround(st.speedBonus * 100), sl / 60, sl % 60),
                 kAccent, gfx::FontFace::Bold, gfx::Align::Center);
    }
}

void Ui::drawHud(Host& h, double dt) {
    game::GameSession* ses = h.session();
    if (!ses) return;
    float s = S();
    const auto& st = ses->stats();
    // V6: Blick nach unten auf den eigenen Koerper - Schnellleiste und Werte treten dezent zurueck
    float look = 1.0f;
    if (!h.settings().thirdPerson && h.settings().showBody)
        look = 1.0f - 0.7f * smoothstep(0.8f, 1.2f, -(float)ses->player().pitch);
    // V13.3 Teil 4 Level 5 (KREATUR_Ants.md): die Tierchen am Koerper - schwarze, flauschige Kugeln kriechen vom unteren Rand
    // ins Bild (je mehr dran sind, desto hoeher und dichter); jede ein Kreis mit weichem Rand, Licht schluckend
    if (const float cr = ses->hotel().antsCrawl(0); cr > 0.0f && !st.dead) {
        static double crawlT = 0.0;
        crawlT += dt;
        const float t = (float)crawlT;
        auto hh = [](unsigned i) {
            i ^= i >> 16, i *= 0x7feb352du, i ^= i >> 15, i *= 0x846ca68bu, i ^= i >> 16;
            return (float)(i & 0xFFFFFF) / 16777216.0f;
        };
        const int n = 14 + (int)(cr * 80.0f);  // Runde 2: kaum zu sehen - mehr, groesser, dunkler
        for (int i = 0; i < n; ++i) {
            const float a = hh((unsigned)i * 3u + 1u), b = hh((unsigned)i * 3u + 2u), c = hh((unsigned)i * 3u + 3u);
            const float side = a < 0.5f ? a * 0.3f : 0.7f + (a - 0.5f) * 0.6f;  // eher an den Raendern
            const float rise = std::fmod(b + t * (0.015f + 0.03f * c), 1.0f) * (0.22f + 0.4f * cr);
            const float x = W_ * (side + 0.012f * std::sin(t * (2.0f + 3.0f * c) + (float)i));
            const float y = H_ * (1.02f - rise);
            const float sz = (22.0f + 48.0f * c) * s;
            r_->rect(x - sz * 0.5f, y - sz * 0.5f, sz, sz, vec4{0.006f, 0.006f, 0.008f, 0.7f}, sz * 0.5f);
            r_->rect(x - sz * 0.38f, y - sz * 0.38f, sz * 0.76f, sz * 0.76f, vec4{0.0f, 0.0f, 0.0f, 0.9f}, sz * 0.38f);
        }
    }
    r_->setAlpha(look);
    drawHotbar(h, *ses);
    r_->setAlpha(1.0f);
    drawCoopHud(h);  // V13.2/V13.3 Koop

    double cd = st.deathCountdown();
    std::string prompt = ses->targetPrompt();
    auto withAlpha = [](vec4 c, float a) { return vec4{c.x, c.y, c.z, c.w * a}; };
    // V11: Meldungen als ruhige Zeilen auf dunklem Glas, kleine Ecken
    auto pill = [&](float y, const std::string& t, vec4 fg, vec4 bg, float size, float a = 1.0f) {
        if (a <= 0.01f) return;
        float tw = r_->textWidth(t, size * s);
        const float bx = W_ * 0.5f - tw * 0.5f - 16 * s, bh = size * s * 1.45f + 12 * s;
        r_->rect(bx, y - 6 * s, tw + 32 * s, bh, withAlpha(bg, a));
        r_->rect(bx, y - 6 * s, 3 * s, bh, withAlpha(kAccent, a * 0.9f));  // V12: Signalkante wie an den Tafeln
        r_->text(W_ * 0.5f, y, size * s, t, withAlpha(fg, a), gfx::FontFace::Regular, gfx::Align::Center);
    };
    // Interaktion (V11): knapp unter dem Fadenkreuz, die Taste als kleine Kappe (Tastatur oder Gamepad), weich
    if (!prompt.empty()) lastKey_ = in_->bindingLabel(Action::Interact), lastPrompt_ = prompt;
    promptAlpha_ += ((prompt.empty() ? 0.0f : 1.0f) - promptAlpha_) * std::min(1.0f, (float)dt * 14.0f);
    const float py = H_ * 0.5f + 48 * s;
    if (cd >= 0 && !st.dead) {
        pill(py, std::format("Du verlierst den Verstand …  {} s", (int)std::ceil(cd)), kDanger, kDangerBg, 20);
    } else if (promptAlpha_ > 0.01f && !lastPrompt_.empty()) {
        const float a = promptAlpha_, fs = 17 * s, ks = 14 * s;
        const float kw = std::max(fs * 1.35f, r_->textWidth(lastKey_, ks, gfx::FontFace::Bold) + 12 * s), kh = fs * 1.3f + 2 * s;
        const float tw = r_->textWidth(lastPrompt_, fs), total = kw + 10 * s + tw;
        const float x = W_ * 0.5f - total * 0.5f;
        r_->rect(x - 10 * s, py - 7 * s, total + 20 * s, kh + 12 * s, withAlpha(kPanelSoft, a), 4 * s);
        r_->rect(x, py - 1 * s, kw, kh, rgba8(0, 0, 0, 0), 3 * s, 1.2f * s, withAlpha(kValue, a * 0.75f));
        r_->text(x + kw * 0.5f, py + (kh - ks * 1.2f) * 0.5f - 1 * s, ks, lastKey_, withAlpha(kValue, a), gfx::FontFace::Bold,
                 gfx::Align::Center);
        r_->text(x + kw + 10 * s, py + 1 * s, fs, lastPrompt_, withAlpha(kText, a));
    }
    // Meldungen ueber der Schnellleiste: neueste unten, aeltere wandern nach oben; weich ein- und ausblenden
    float ty = H_ - 200 * s;
    if (now_ < helpUntil_) {
        float a = (float)std::min(1.0, (helpUntil_ - now_) / 0.6);
        pill(ty, kHint, kMuted, kPanelSoft, 15, a);
        ty -= 40 * s;
    }
    int shown = now_ < helpUntil_ ? 1 : 0;
    for (auto it = toasts_.rbegin(); it != toasts_.rend() && shown < 3; ++it) {
        if (it->until + 0.4 < now_) continue;
        ++shown;
        float in = (float)std::min(1.0, (now_ - it->start) / 0.15);
        float out = (float)std::clamp((it->until + 0.4 - now_) / 0.4, 0.0, 1.0);
        float a = std::min(in, out);
        pill(ty + (1.0f - in) * 8 * s, it->text, kText, kPanelSoft, 17, a);
        ty -= 42 * s * a;
    }
    // Zone (V11): klein und gesperrt ueber den Werten unten links
    if (now_ < zoneUntil_) {
        float a = (float)std::min(1.0, (zoneUntil_ - now_) / 0.8) * std::min(1.0f, (float)(3.5 - (zoneUntil_ - now_)) / 0.5f);
        a = std::clamp(a, 0.0f, 1.0f);
        std::string z = spacedCaps(zone_);  // V6: auch Umlaute ("BÜROFLURE" statt "BüROFLURE")
        float tw = r_->textWidth(z, 14 * s, gfx::FontFace::Bold);
        r_->rect(44 * s - 12 * s, H_ - 158 * s, tw + 24 * s, 30 * s, rgba8(0, 0, 0, (int)(80 * a)), 4 * s);
        r_->text(44 * s, H_ - 152 * s, 14 * s, z, withAlpha(kValue, a * 0.9f), gfx::FontFace::Bold);
    }
    // V8: entdeckte Ebene - leise, mittig, gesperrt gesetzt; blendet langsam ein und aus
    {
        const double t = now_ - regionStart_;
        const double kIn = 2.2, kHold = 3.6, kOut = 2.8;
        if (t >= 0.0 && t < kIn + kHold + kOut && !regionTitle_.empty()) {
            float a = (float)(t < kIn ? t / kIn : (t < kIn + kHold ? 1.0 : 1.0 - (t - kIn - kHold) / kOut));
            a = a * a * (3.0f - 2.0f * a);
            std::string head = regionTitle_, sub;
            if (auto dot = regionTitle_.find(" · "); dot != std::string::npos)
                head = regionTitle_.substr(0, dot), sub = regionTitle_.substr(dot + std::string(" · ").size());
            // V12: Ankunftstafel - oben die Nummer gesperrt in Signalgelb, darunter der Titel gross, eine Kante,
            // die Quelle klein (die Ebenen stammen aus dem Wiki)
            float cy = H_ * 0.24f;
            const float tw = std::max(r_->textWidth(sub.empty() ? head : sub, 46 * s, gfx::FontFace::Bold), 280 * s);
            r_->rect(W_ * 0.5f - tw * 0.5f - 40 * s, cy - 22 * s, tw + 80 * s, (regionSource_.empty() ? 128 : 160) * s,
                     withAlpha(kPanelSoft, a * 0.8f));
            r_->rect(W_ * 0.5f - tw * 0.5f - 40 * s, cy - 22 * s, 4 * s, (regionSource_.empty() ? 128 : 160) * s, withAlpha(kAccent, a));
            r_->text(W_ * 0.5f, cy, 16 * s, spacedCaps(head), withAlpha(kAccent, a), gfx::FontFace::Bold, gfx::Align::Center);
            if (!sub.empty())
                r_->text(W_ * 0.5f, cy + 30 * s, 46 * s, sub, withAlpha(kTitle, a), gfx::FontFace::Bold, gfx::Align::Center);
            r_->rect(W_ * 0.5f - 40 * s * a, cy + 96 * s, 80 * s * a, 2.0f * s, withAlpha(kAccent, a * 0.9f));
            if (!regionSource_.empty())
                r_->text(W_ * 0.5f, cy + 110 * s, 15 * s, regionSource_, withAlpha(kText, a * 0.8f), gfx::FontFace::Regular, gfx::Align::Center);
        }
    }
    drawIntro();  // V9
    if (now_ < savedUntil_) {
        // V6: unten rechts (oben rechts sitzt die Minimap), weich ausgeblendet; V11: ohne Kasten
        float a = (float)std::min(1.0, (savedUntil_ - now_) / 0.5);
        float tw = r_->textWidth("◆  Gespeichert", 14 * s);
        r_->rect(W_ - 44 * s - tw - 12 * s, H_ - 58 * s, tw + 24 * s, 28 * s, rgba8(0, 0, 0, (int)(90 * a)), 4 * s);
        r_->text(W_ - 44 * s, H_ - 53 * s, 14 * s, "◆  Gespeichert", withAlpha(kMuted, a), gfx::FontFace::Regular, gfx::Align::Right);
    }
    // Fadenkreuz: dezenter Punkt, waechst leicht, wenn etwas aufgenommen werden kann
    {
        float cr = (2.5f + 1.0f * promptAlpha_) * s;
        r_->rect(W_ * 0.5f - cr, H_ * 0.5f - cr, cr * 2, cr * 2, rgba8(255, 255, 255, 70 + (int)(100 * promptAlpha_)), cr);
    }

    if (showDebug_) drawDebug(h);
    if (showMap_) drawBigMap(h, *ses, dt);
    else {
        bigAnim_ = 0.0f;
        drawMinimap(h, *ses, dt);
    }
}

void Ui::drawDebug(Host& h) {
    float s = S();
    std::string text = h.debugInfo();
    std::vector<std::string> lines;
    size_t p = 0;
    while (p <= text.size()) {
        size_t e = text.find('\n', p);
        lines.push_back(text.substr(p, e == std::string::npos ? std::string::npos : e - p));
        if (e == std::string::npos) break;
        p = e + 1;
    }
    float lh = 22 * s, w = 0;
    for (auto& l : lines) w = std::max(w, r_->textWidth(l, 16 * s, gfx::FontFace::Mono));
    r_->rect(12 * s, 12 * s, w + 28 * s, lh * lines.size() + 20 * s, rgba8(8, 8, 8, 200), 6 * s);
    for (size_t i = 0; i < lines.size(); ++i) r_->text(26 * s, 22 * s + i * lh, 16 * s, lines[i], kText, gfx::FontFace::Mono);
}

// --- Inventar -------------------------------------------------------------------------------------
void Ui::closeInventory(Host& h) {
    if (auto* ses = h.session()) {
        auto& inv = ses->inventory();
        if (hand_) {
            if (handFrom_ >= 0 && !inv.slots[(size_t)handFrom_]) inv.slots[(size_t)handFrom_] = hand_;
            else
                for (int i = 0; i < hand_->count; ++i) inv.add(hand_->kind);
            hand_.reset();
            handFrom_ = -1;
        }
    }
    setMode(Mode::Hud);
}

void Ui::drawInventory(Host& h) {
    game::GameSession* ses = h.session();
    if (!ses) {
        setMode(Mode::Hud);
        return;
    }
    float s = S();
    auto& inv = ses->inventory();
    const auto& st = ses->stats();
    auto slotIndex = [](int row, int col) { return row == 3 ? col : game::Inventory::kHotbar + row * 9 + col; };
    // Eingabe
    if (in_->actionPressed(Action::Inventory) || in_->actionPressed(Action::UiBack)) {
        h.playSound("ui_back");
        closeInventory(h);
        return;
    }
    if (in_->actionPressed(Action::UiLeft)) invCol_ = (invCol_ + 8) % 9, h.playSound("ui_move");
    if (in_->actionPressed(Action::UiRight)) invCol_ = (invCol_ + 1) % 9, h.playSound("ui_move");
    if (in_->actionPressed(Action::UiUp)) invRow_ = (invRow_ + 3) % 4, h.playSound("ui_move");
    if (in_->actionPressed(Action::UiDown)) invRow_ = (invRow_ + 1) % 4, h.playSound("ui_move");

    float size = 78 * s, gap = 8 * s;
    float gw = 9 * size + 8 * gap;
    float pw = gw + 80 * s, ph = 4 * size + 3 * gap + 330 * s;
    float px = (W_ - pw) * 0.5f, py = (H_ - ph) * 0.5f;
    panel(px, py, pw, ph);
    r_->rect(px + 40 * s, py, 44 * s, 2 * s, kAccent);
    r_->text(px + 40 * s, py + 30 * s, kFsTitle * s, "I N V E N T A R", kTitle, gfx::FontFace::Bold);
    r_->text(px + pw - 40 * s, py + 38 * s, kFsCaps * s,
             std::format("LEBEN  {}       VERSTAND  {}", (int)std::lround(st.health), (int)std::lround(st.sanity)), kMuted,
             gfx::FontFace::Bold, gfx::Align::Right);
    r_->rect(px + 40 * s, py + 76 * s, pw - 80 * s, 1.0f * s, kRule);
    float gx = px + 40 * s, gy = py + 100 * s;
    bool confirm = in_->actionPressed(Action::UiConfirm);
    for (int row = 0; row < 4; ++row) {
        float ry = gy + row * (size + gap) + (row == 3 ? 46 * s : 0);
        if (row == 3) r_->text(gx, ry - 32 * s, kFsCaps * s, "SCHNELLLEISTE", kMuted, gfx::FontFace::Bold);
        for (int col = 0; col < 9; ++col) {
            float cx = gx + col * (size + gap);
            if (in_->cursorMoved && hovered(cx, ry, size, size)) invRow_ = row, invCol_ = col;
            if (clicked(cx, ry, size, size)) {
                invRow_ = row, invCol_ = col;
                confirm = true;
            }
            slotBox(h, inv, slotIndex(row, col), cx, ry, size, row == 3 && col == inv.selected, row == invRow_ && col == invCol_);
        }
    }
    int idx = slotIndex(invRow_, invCol_);
    auto& cur = inv.slots[(size_t)idx];
    if (confirm) {
        h.playSound("ui_select");
        if (!hand_) {
            if (cur) {
                hand_ = cur;
                handFrom_ = idx;
                cur.reset();
            }
        } else if (!cur) {
            cur = hand_;
            hand_.reset();
            handFrom_ = -1;
        } else if (cur->kind == hand_->kind) {
            int move = std::min(inv.maxStack(cur->kind) - cur->count, hand_->count);
            cur->count += move;
            hand_->count -= move;
            if (hand_->count <= 0) hand_.reset(), handFrom_ = -1;
        } else {
            std::swap(*cur, *hand_);
            handFrom_ = idx;
        }
    }
    if (!hand_ && in_->actionPressed(Action::UseItem)) ses->useSlot(idx);
    // V6: Q legt ab (aus der Hand oder vom Platz unter dem Cursor), Strg+Q den ganzen Stapel
    if (in_->actionPressed(Action::DropItem)) {
        bool all = in_->down(VK_CONTROL);
        if (hand_) {
            int n = all ? hand_->count : 1;
            ses->dropStack(hand_->kind, n);
            hand_->count -= n;
            if (hand_->count <= 0) hand_.reset(), handFrom_ = -1;
            h.playSound("ui_move");
        } else if (inv.slots[(size_t)idx]) {
            ses->dropSlot(idx, all);
            h.playSound("ui_move");
        }
    }
    for (int k = 0; k < 9; ++k)
        if (!hand_ && in_->actionPressed((Action)((int)Action::Hotbar1 + k))) {
            inv.swap(idx, k);
            h.playSound("ui_move");
        }
    // Beschreibung
    float dy = gy + 4 * (size + gap) + 46 * s + 20 * s;
    if (hand_) {
        const game::ItemDef* d = h.content().item(hand_->kind);
        r_->text(gx, dy, 19 * s, std::format("In der Hand: {} ×{}  –  Enter/Klick: ablegen / tauschen", d ? d->name : hand_->kind, hand_->count), kAccent);
        auto* icon = h.renderer().itemIcon(d ? d->icon : hand_->kind);
        r_->image(icon, in_->cursor.x + 8 * s, in_->cursor.y + 8 * s, size * 0.7f, size * 0.7f);
    } else if (cur) {
        const game::ItemDef* d = h.content().item(cur->kind);
        r_->text(gx, dy, 20 * s, std::format("{} ×{}", d ? d->name : cur->kind, cur->count), kSelText, gfx::FontFace::Bold);
        if (d) r_->text(gx, dy + 32 * s, 17 * s, d->description, kText);
    } else {
        r_->text(gx, dy, 17 * s, "(leerer Platz)", kMuted);
    }
    r_->rect(px + 40 * s, py + ph - 80 * s, pw - 80 * s, 1.0f * s, kRule);
    r_->text(px + 40 * s, py + ph - 64 * s, kFsSmall * s, "←↑↓→ / Maus bewegen    Enter / Klick nehmen und ablegen    F benutzen", kMuted);
    r_->text(px + 40 * s, py + ph - 40 * s, kFsSmall * s,
             "1 – 9 in die Schnellleiste legen    Q ablegen (Strg+Q ganzer Stapel)    Tab schließen", kMuted);
}

// --- Tod ---------------------------------------------------------------------------------------------
void Ui::drawDead(Host& h) {
    game::GameSession* ses = h.session();
    float s = S();
    float w = std::min(W_ - 60 * s, 980 * s), hh = 470 * s;
    float x = (W_ - w) * 0.5f, y = (H_ - hh) * 0.5f;
    // V11: kein Kasten - ein dunkles Band ueber die ganze Breite, oben und unten weich auslaufend (Streifen
    // ohne Ueberlappung, damit keine Stufen entstehen)
    {
        const int n = 32;
        const float fade = 110 * s;
        r_->rect(0.0f, std::floor(y), W_, std::floor(y + hh) - std::floor(y), rgba8(6, 2, 2, 200));
        for (int k = 0; k < n; ++k) {
            const float y0 = std::floor(fade * (float)k / n), y1 = std::floor(fade * (float)(k + 1) / n);
            const vec4 c = rgba8(6, 2, 2, (int)(200 * std::pow(1.0f - ((float)k + 0.5f) / n, 1.6f)));
            r_->rect(0.0f, std::floor(y) - y1, W_, y1 - y0, c);
            r_->rect(0.0f, std::floor(y + hh) + y0, W_, y1 - y0, c);
        }
    }
    // V9: Todesursache - der Verstand oder eine Entity (V10)
    const bool caught = ses && ses->deathCause() == "stalker";
    const std::string cause = ses ? ses->deathCause() : std::string();
    r_->text(x + w * 0.5f, y + 38 * s, 30 * s,
             caught ? "E S   H A T   D I C H   E R W I S C H T"
             : cause == "wasser" ? "D U   B I S T   E R T R U N K E N"
             : cause == "tiefe"  ? "E S   H A T   D I C H   G E H O L T"
             : cause == "pilz"   ? "D E R   P I L Z   H A T   D I C H"           // V13.3 (KREATUR_Crawler.md)
             : cause == "garten" ? "D U   B I S T   E I N E   P F L A N Z E"   // V13.3 (Garden)
             : cause == "photoshop" ? "A U S G E S C H N I T T E N"           // V13.3 Teil 4 (KREATUR_Photoshop.md)
             : cause == "automat" ? "D E R   A U T O M A T   H A T   D I C H"  // V13.3 Teil 4 (KREATUR_CannibalCuisine.md)
                                 : "D U   H A S T   D E N   V E R S T A N D   V E R L O R E N",
             rgba8(226, 104, 92), gfx::FontFace::Bold, gfx::Align::Center);
    r_->rect(x + w * 0.5f - 60 * s, y + 94 * s, 120 * s, 1.5f * s, rgba8(200, 84, 72, 170));
    if (ses) {
        // V7: Laufbuch
        auto lines = runLogLines(h, *ses);
        r_->text(x + w * 0.5f, y + 116 * s, 20 * s, lines[0], rgba8(230, 200, 190), gfx::FontFace::Regular, gfx::Align::Center);
        r_->text(x + w * 0.5f, y + 146 * s, 18 * s, lines[1], rgba8(190, 150, 140), gfx::FontFace::Regular, gfx::Align::Center);
        if (lines.size() > 2 && !lines[2].empty())
            r_->text(x + w * 0.5f, y + 174 * s, 17 * s, lines[2], rgba8(170, 130, 120), gfx::FontFace::Regular, gfx::Align::Center);
    }
    // V11: Wiedereinstieg am letzten sicheren Ort; darunter, was geholfen haette (F5). Die ersten 0,9 s
    // zaehlen keine Eingaben (wer beim Tod noch rannte, haelt die Tasten oft noch)
    if (deadShown_ < 0.0) deadShown_ = now_;
    if (now_ - deadShown_ < 0.9) in_->clearState();
    if (h.canRespawn() && ses) {
        std::vector<Item> items = {
            {std::format("Weiter  ·  zurück zu: {}", ses->checkpointLabel().empty() ? "Anfang" : ses->checkpointLabel()), nullptr,
             nullptr, [&h] { h.respawn(); }, ""},
            {"Zum Hauptmenü", nullptr, nullptr, [&h] { h.toTitle(); }, ""},
            {"Beenden", nullptr, nullptr, [&h] { h.quit(); }, ""},
        };
        runList(h, items, lists_[10], x + 60 * s, y + 210 * s, w - 120 * s, 50 * s, 3);
        const int role = ses->killRole();
        const std::string& cause = ses->deathCause();
        const char* hint = cause == "wasser" ? "Was geholfen hätte: rechtzeitig auftauchen; nicht zu lange im kalten Wasser bleiben."
                           : cause == "tiefe" ? "Was geholfen hätte: nicht so lange über der Tiefe bleiben. Licht nach unten hält es kurz fern."
                           : cause == "pilz" ? "Was geholfen hätte: nichts Offenes trinken, nicht durch weiß befallene Pfützen waten; früh Mandelwasser."
                           : cause == "garten" ? "Was geholfen hätte: den Garden nicht zu lange durchstreifen. Draußen erholt man sich langsam."
                           : cause == "photoshop" ? "Was geholfen hätte: fünf Fuß Abstand zum Klotz. In der Box nicht gehen – ihm etwas hinwerfen (Q)."
                           : cause == "automat" ? "Was geholfen hätte: den Automaten nicht angreifen. Klappern die Klappen: zurück, weiter als zwei Schritte."
                           : role == 0   ? "Was geholfen hätte: nicht rennen. Hinsehen, stehen bleiben, Lampe aus, langsam zurück ins Licht."
                           : role == 1 ? "Was geholfen hätte: ihm nie den Rücken zukehren, nicht rennen. Ansehen, wenn es knurrt, dann langsam weg."
                           : role == 2 ? "Was geholfen hätte: kein Licht auf das Nest. Lampe aus, ruhig an der Wand vorbei."
                           : role == 5 ? "Was geholfen hätte: leise gehen (C), 2,5 m Abstand, nicht rennen. Etwas werfen (Q) lockt es weg."
                           : role == 18 ? "Was geholfen hätte: dem blauen Vogel nicht in die Augen sehen. Wegsehen, umkehren – und nie näher als zwei Schritte."  // V13.3 Teil 4: Jerry
                                       : "Was geholfen hätte: essen, Mandelwasser, im Licht bleiben.";
        r_->text(x + w * 0.5f, y + hh - 54 * s, 17 * s, hint, rgba8(200, 150, 140), gfx::FontFace::Regular, gfx::Align::Center);
        r_->text(x + w * 0.5f, y + hh - 30 * s, 15 * s, "Inventar und Funde bleiben. Volles Leben, der Stromausfall ist vorbei.",
                 rgba8(150, 110, 100), gfx::FontFace::Regular, gfx::Align::Center);
        return;
    }
    std::vector<Item> items = {
        {std::string("Letzten Spielstand laden") + (h.hasSave() ? "" : "  (keiner vorhanden)"), nullptr, nullptr,
         [&h] { h.loadLastSave(); }, ""},
        {"Zum Hauptmenü", nullptr, nullptr, [&h] { h.toTitle(); }, ""},
        {"Beenden", nullptr, nullptr, [&h] { h.quit(); }, ""},
    };
    runList(h, items, lists_[10], x + 60 * s, y + 210 * s, w - 120 * s, 50 * s, 3);
    if (h.hasSave())
        r_->text(x + w * 0.5f, y + hh - 50 * s, 17 * s, "Beim Laden startest du mit vollem Leben und mindestens 30 % Verstand.",
                 rgba8(160, 120, 110), gfx::FontFace::Regular, gfx::Align::Center);
}

}  // namespace lim::ui
