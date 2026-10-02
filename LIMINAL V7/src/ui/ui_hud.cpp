// HUD (Schnellleiste, Werte, Hinweise, Karte, Debug), Inventar und Todesbildschirm.
#include <algorithm>
#include <cmath>
#include <format>

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

void Ui::slotBox(Host& h, const game::Inventory& inv, int idx, float x, float y, float size, bool selected, bool cursor) {
    float s = S();
    const auto& slot = inv.slots[(size_t)idx];
    r_->rect(x, y, size, size, selected ? kSlotSel : kSlot, 6 * s, (selected || cursor) ? 2.2f * s : 1.2f * s,
             (selected || cursor) ? kSelBg : kSlotBorder);
    if (slot) {
        const game::ItemDef* d = h.content().item(slot->kind);
        auto* icon = h.renderer().itemIcon(d ? d->icon : slot->kind);
        float pad = size * 0.14f;
        r_->image(icon, x + pad, y + pad, size - 2 * pad, size - 2 * pad);
        if (slot->count > 1)
            r_->text(x + size - 6 * s, y + size - 24 * s, 17 * s, std::format("×{}", slot->count), kValue,
                     gfx::FontFace::Bold, gfx::Align::Right);
    }
}

void Ui::drawHotbar(Host& h, game::GameSession& ses) {
    float s = S();
    const auto& inv = ses.inventory();
    const auto& st = ses.stats();
    float size = 64 * s, gap = 6 * s;
    float total = game::Inventory::kHotbar * size + (game::Inventory::kHotbar - 1) * gap;
    float x0 = (W_ - total) * 0.5f, y0 = H_ - size - 22 * s;
    for (int i = 0; i < game::Inventory::kHotbar; ++i) {
        float x = x0 + i * (size + gap);
        slotBox(h, inv, i, x, y0, size, i == inv.selected, false);
        r_->text(x + 6 * s, y0 + 3 * s, 14 * s, std::to_string(i + 1), i == inv.selected ? kSelBg : kMuted);
    }
    // Health (links) und Sanity (rechts) ueber der Leiste
    float by = y0 - 34 * s, bw = 250 * s, bh = 12 * s;
    auto bar = [&](float x, const char* label, double v, vec4 col, bool right) {
        float lw = r_->textWidth(label, 17 * s, gfx::FontFace::Bold);
        float bx = right ? x - bw : x;
        r_->rect(bx - 10 * s, by - 8 * s, bw + 20 * s, bh + 16 * s, kBarBg, 8 * s);
        r_->rect(bx, by, bw, bh, kEmpty, bh * 0.5f);
        r_->rect(bx, by, (float)(bw * std::clamp(v / 100.0, 0.0, 1.0)), bh, col, bh * 0.5f);
        float ty = by - 30 * s;
        r_->text(right ? bx + bw : bx, ty, 17 * s, std::format("{}  {} %", label, (int)std::lround(v)), col, gfx::FontFace::Bold,
                 right ? gfx::Align::Right : gfx::Align::Left);
        (void)lw;
    };
    vec4 hc = st.health < 35 ? kLow : kHealth;
    bar(x0, "♥ HEALTH", st.health, hc, false);
    bool pulse = st.sanity < 20 && std::fmod(now_ * 3.0, 2.0) < 1.0;
    bar(x0 + total, "SANITY", st.sanity, (pulse || st.sanity <= 0) ? kLow : kSanity, true);
    // V6: Ausdauer - schmale Leiste zwischen Health und Sanity; ist sie voll, blendet sie aus
    {
        float stam = (float)std::clamp(st.stamina / 100.0, 0.0, 1.0);
        if (stam < 0.995f || st.exhausted) staminaShow_ = now_ + 1.2;
        float a = (float)std::clamp((staminaShow_ - now_) / 0.5, 0.0, 1.0) * r_->alpha();
        if (a > 0.01f) {
            auto fade = [&](vec4 c) { return vec4{c.x, c.y, c.z, c.w * a}; };
            float sw = 80 * s, sh = 8 * s, sx = W_ * 0.5f - sw * 0.5f, sy = by + (bh - sh) * 0.5f;
            float keep = r_->alpha();
            r_->setAlpha(1.0f);
            r_->rect(sx - 6 * s, sy - 6 * s, sw + 12 * s, sh + 12 * s, fade(kBarBg), 7 * s);
            r_->rect(sx, sy, sw, sh, fade(kEmpty), sh * 0.5f);
            bool blink = st.exhausted && std::fmod(now_ * 2.5, 1.0) < 0.5;
            vec4 col = st.exhausted ? (blink ? kLow : rgba8(170, 70, 70)) : rgba8(232, 218, 170);
            r_->rect(sx, sy, sw * stam, sh, fade(col), sh * 0.5f);
            // erschoepft: Marke, ab der wieder gesprintet werden kann
            if (st.exhausted) r_->rect(sx + sw * 0.3f - 1 * s, sy - 3 * s, 2 * s, sh + 6 * s, fade(rgba8(255, 255, 255, 140)));
            r_->setAlpha(keep);
        }
    }
    // V7: Batterie der Taschenlampe rechts neben der Schnellleiste (an, gerade geschaltet oder schwach)
    {
        const game::Flashlight& fl = ses.flashlight();
        float charge = (float)std::clamp(fl.charge / 100.0, 0.0, 1.0);
        if (fl.on || fl.charge < 25.0) flashShow_ = now_ + 2.5;
        float a = (float)std::clamp((flashShow_ - now_) / 0.6, 0.0, 1.0);
        if (a > 0.01f) {
            auto fade = [&](vec4 c) { return vec4{c.x, c.y, c.z, c.w * a}; };
            float bw2 = 44 * s, bh2 = 20 * s, bx = x0 + total + 22 * s, byy = y0 + size * 0.5f - bh2 * 0.5f - 9 * s;
            bool empty = fl.charge <= 0.0, blink = fl.low() && std::fmod(now_ * 2.0, 1.0) < 0.5;
            vec4 col = fl.charge < 15.0 ? (blink ? kLow : rgba8(190, 80, 60)) : fl.charge < 30.0 ? rgba8(232, 170, 70) : rgba8(232, 218, 170);
            r_->rect(bx - 8 * s, byy - 8 * s, bw2 + 22 * s, bh2 + 40 * s, fade(kBarBg), 8 * s);
            r_->rect(bx, byy, bw2, bh2, fade(kEmpty), 4 * s, 1.5f * s, fade(fl.on ? col : kSlotBorder));
            r_->rect(bx + bw2, byy + bh2 * 0.3f, 4 * s, bh2 * 0.4f, fade(fl.on ? col : kSlotBorder), 1.5f * s);
            if (!empty) r_->rect(bx + 3 * s, byy + 3 * s, (bw2 - 6 * s) * charge, bh2 - 6 * s, fade(col), 2 * s);
            r_->text(bx + bw2 * 0.5f + 2 * s, byy + bh2 + 4 * s, 14 * s, empty ? "leer" : std::format("{} %", (int)std::ceil(fl.charge)),
                     fade(fl.on ? kValue : kMuted), gfx::FontFace::Bold, gfx::Align::Center);
        }
    }
    if (st.speedLeft > 0) {
        int sl = (int)st.speedLeft;
        r_->text(W_ * 0.5f, by - 30 * s, 17 * s, std::format("⚡ +{} %   {}:{:02}", (int)std::lround(st.speedBonus * 100), sl / 60, sl % 60),
                 kTitle, gfx::FontFace::Bold, gfx::Align::Center);
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
    r_->setAlpha(look);
    drawHotbar(h, *ses);
    r_->setAlpha(1.0f);
    float my = H_ - 64 * s - 22 * s - 34 * s - 110 * s;

    // Hinweise ueber der Leiste
    double cd = st.deathCountdown();
    std::string prompt = ses->targetPrompt();
    auto withAlpha = [](vec4 c, float a) { return vec4{c.x, c.y, c.z, c.w * a}; };
    auto pill = [&](float y, const std::string& t, vec4 fg, vec4 bg, float size, float a = 1.0f) {
        if (a <= 0.01f) return;
        float tw = r_->textWidth(t, size * s);
        r_->rect(W_ * 0.5f - tw * 0.5f - 18 * s, y - 6 * s, tw + 36 * s, size * s * 1.5f + 12 * s, withAlpha(bg, a), 10 * s);
        r_->text(W_ * 0.5f, y, size * s, t, withAlpha(fg, a), gfx::FontFace::Regular, gfx::Align::Center);
    };
    // V6: Aufnehmen-Hinweis mit der tatsaechlichen Belegung (Tastatur oder Gamepad), weich ein-/ausgeblendet
    if (!prompt.empty()) lastPrompt_ = "[" + in_->bindingLabel(Action::Interact) + "]  " + prompt;
    promptAlpha_ += ((prompt.empty() ? 0.0f : 1.0f) - promptAlpha_) * std::min(1.0f, (float)dt * 14.0f);
    if (cd >= 0 && !st.dead) pill(my, std::format("Du verlierst den Verstand …  {} s", (int)std::ceil(cd)), kDanger, kDangerBg, 22);
    else pill(my, lastPrompt_, kValue, kBarBg, 22, promptAlpha_);
    // Meldungen: neueste unten, aeltere wandern nach oben; weich ein- und ausblenden
    float ty = my - 64 * s;
    if (now_ < helpUntil_) {
        float a = (float)std::min(1.0, (helpUntil_ - now_) / 0.6);
        pill(ty, kHint, kTitle, kPanelSoft, 19, a);
        ty -= 54 * s;
    }
    int shown = now_ < helpUntil_ ? 1 : 0;
    for (auto it = toasts_.rbegin(); it != toasts_.rend() && shown < 3; ++it) {
        if (it->until + 0.4 < now_) continue;
        ++shown;
        float in = (float)std::min(1.0, (now_ - it->start) / 0.15);
        float out = (float)std::clamp((it->until + 0.4 - now_) / 0.4, 0.0, 1.0);
        float a = std::min(in, out);
        pill(ty + (1.0f - in) * 10 * s, it->text, kTitle, kPanelSoft, 20, a);
        ty -= 54 * s * a;
    }
    if (now_ < zoneUntil_) {
        float a = (float)std::min(1.0, (zoneUntil_ - now_) / 0.8) * std::min(1.0f, (float)(3.5 - (zoneUntil_ - now_)) / 0.5f);
        std::string z = upperUtf8(zone_);  // V6: auch Umlaute ("BÜROFLURE" statt "BüROFLURE")
        vec4 col = kMuted;
        col.w = std::clamp(a, 0.0f, 1.0f);
        r_->text(40 * s, H_ * 0.62f, 26 * s, z, col, gfx::FontFace::Bold);
    }
    if (now_ < savedUntil_) {
        // V6: unten rechts (oben rechts sitzt die Minimap), weich ausgeblendet
        float a = (float)std::min(1.0, (savedUntil_ - now_) / 0.5);
        r_->rect(W_ - 210 * s, H_ - 64 * s, 186 * s, 40 * s, withAlpha(kBarBg, a), 8 * s);
        r_->text(W_ - 117 * s, H_ - 56 * s, 18 * s, "◆  Gespeichert", withAlpha(kMuted, a), gfx::FontFace::Regular,
                 gfx::Align::Center);
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
    r_->text(px + 40 * s, py + 26 * s, 30 * s, "I N V E N T A R", kTitle, gfx::FontFace::Bold);
    r_->text(px + pw - 40 * s, py + 34 * s, 20 * s,
             std::format("♥ {} %     SANITY {} %", (int)std::lround(st.health), (int)std::lround(st.sanity)), kText,
             gfx::FontFace::Regular, gfx::Align::Right);
    r_->rect(px + 40 * s, py + 76 * s, pw - 80 * s, 1.5f * s, kRule);
    float gx = px + 40 * s, gy = py + 100 * s;
    bool confirm = in_->actionPressed(Action::UiConfirm);
    for (int row = 0; row < 4; ++row) {
        float ry = gy + row * (size + gap) + (row == 3 ? 46 * s : 0);
        if (row == 3) r_->text(gx, ry - 36 * s, 18 * s, "Schnellleiste", kMuted);
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
        r_->text(gx, dy, 21 * s, std::format("In der Hand: {} ×{}  –  Enter/Klick: ablegen / tauschen", d ? d->name : hand_->kind, hand_->count), kTitle);
        auto* icon = h.renderer().itemIcon(d ? d->icon : hand_->kind);
        r_->image(icon, in_->cursor.x + 8 * s, in_->cursor.y + 8 * s, size * 0.7f, size * 0.7f);
    } else if (cur) {
        const game::ItemDef* d = h.content().item(cur->kind);
        r_->text(gx, dy, 22 * s, std::format("{} ×{}", d ? d->name : cur->kind, cur->count), kTitle, gfx::FontFace::Bold);
        if (d) r_->text(gx, dy + 32 * s, 19 * s, d->description, kValue);
    } else {
        r_->text(gx, dy, 19 * s, "(leerer Platz)", kMuted);
    }
    r_->rect(px + 40 * s, py + ph - 80 * s, pw - 80 * s, 1.5f * s, kRule);
    r_->text(px + 40 * s, py + ph - 66 * s, 17 * s, "←↑↓→ / Maus bewegen    Enter / Klick nehmen und ablegen    F benutzen", kMuted);
    r_->text(px + 40 * s, py + ph - 40 * s, 17 * s, "1 – 9 in die Schnellleiste legen    Q ablegen (Strg+Q ganzer Stapel)    Tab schließen",
             kMuted);
}

// --- Tod ---------------------------------------------------------------------------------------------
void Ui::drawDead(Host& h) {
    game::GameSession* ses = h.session();
    float s = S();
    float w = std::min(W_ - 60 * s, 980 * s), hh = 470 * s;
    float x = (W_ - w) * 0.5f, y = (H_ - hh) * 0.5f;
    r_->rect(x, y, w, hh, rgba8(18, 6, 6, 235), 10 * s, 1.5f * s, rgba8(120, 30, 26));
    r_->text(x + w * 0.5f, y + 36 * s, 34 * s, "D U   H A S T   D E N   V E R S T A N D   V E R L O R E N", rgba8(255, 96, 84),
             gfx::FontFace::Bold, gfx::Align::Center);
    r_->rect(x + 36 * s, y + 96 * s, w - 72 * s, 1.5f * s, rgba8(160, 60, 50));
    if (ses) {
        // V7: Laufbuch
        auto lines = runLogLines(h, *ses);
        r_->text(x + w * 0.5f, y + 116 * s, 20 * s, lines[0], rgba8(230, 200, 190), gfx::FontFace::Regular, gfx::Align::Center);
        r_->text(x + w * 0.5f, y + 146 * s, 18 * s, lines[1], rgba8(190, 150, 140), gfx::FontFace::Regular, gfx::Align::Center);
    }
    std::vector<Item> items = {
        {std::string("Letzten Spielstand laden") + (h.hasSave() ? "" : "  (keiner vorhanden)"), nullptr, nullptr,
         [&h] { h.loadLastSave(); }, ""},
        {"Zum Hauptmenü", nullptr, nullptr, [&h] { h.toTitle(); }, ""},
        {"Beenden", nullptr, nullptr, [&h] { h.quit(); }, ""},
    };
    runList(h, items, lists_[10], x + 60 * s, y + 210 * s, w - 120 * s, 50 * s, 3);
    if (h.hasSave())
        r_->text(x + w * 0.5f, y + hh - 50 * s, 17 * s, "Beim Laden startest du mit voller Health und mindestens 30 % Sanity.",
                 rgba8(160, 120, 110), gfx::FontFace::Regular, gfx::Align::Center);
}

}  // namespace lim::ui
