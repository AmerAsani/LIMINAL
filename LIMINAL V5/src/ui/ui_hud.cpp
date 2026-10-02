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

static const char* kHint = "W A S D  gehen   ·   Maus  umsehen   ·   E  aufnehmen   ·   Tab  Inventar   ·   ESC  Menü & Hilfe";

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
    drawHotbar(h, *ses);
    float my = H_ - 64 * s - 22 * s - 34 * s - 110 * s;

    // Hinweise ueber der Leiste
    double cd = st.deathCountdown();
    std::string prompt = ses->targetPrompt();
    auto pill = [&](float y, const std::string& t, vec4 fg, vec4 bg, float size) {
        float tw = r_->textWidth(t, size * s);
        r_->rect(W_ * 0.5f - tw * 0.5f - 18 * s, y - 6 * s, tw + 36 * s, size * s * 1.5f + 12 * s, bg, 10 * s);
        r_->text(W_ * 0.5f, y, size * s, t, fg, gfx::FontFace::Regular, gfx::Align::Center);
    };
    if (cd >= 0 && !st.dead) pill(my, std::format("Du verlierst den Verstand …  {} s", (int)std::ceil(cd)), kDanger, kDangerBg, 22);
    else if (!prompt.empty()) pill(my, prompt, kValue, kBarBg, 22);
    if (now_ < helpUntil_) pill(my - 64 * s, kHint, kTitle, kPanelSoft, 19);
    else if (now_ < toastUntil_) pill(my - 64 * s, toast_, kTitle, kPanelSoft, 20);
    if (now_ < zoneUntil_) {
        float a = (float)std::min(1.0, (zoneUntil_ - now_) / 0.8) * std::min(1.0f, (float)(3.5 - (zoneUntil_ - now_)) / 0.5f);
        std::string z;
        for (char c : zone_) z += (char)std::toupper((unsigned char)c);
        vec4 col = kMuted;
        col.w = std::clamp(a, 0.0f, 1.0f);
        r_->text(40 * s, H_ * 0.62f, 26 * s, z, col, gfx::FontFace::Bold);
    }
    if (now_ < savedUntil_) {
        r_->rect(W_ - 210 * s, 24 * s, 186 * s, 40 * s, kBarBg, 8 * s);
        r_->text(W_ - 117 * s, 32 * s, 18 * s, "◆  Gespeichert", kMuted, gfx::FontFace::Regular, gfx::Align::Center);
    }
    // Fadenkreuz: dezenter Punkt
    r_->rect(W_ * 0.5f - 2.5f * s, H_ * 0.5f - 2.5f * s, 5 * s, 5 * s, rgba8(255, 255, 255, prompt.empty() ? 70 : 170), 2.5f * s);

    if (showDebug_) drawDebug(h);
    if (showMap_) drawMap(h, *ses);
    (void)dt;
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

// Draufsicht der geladenen Umgebung (Norden oben). Wird 4x pro Sekunde neu erzeugt.
void Ui::drawMap(Host& h, game::GameSession& ses) {
    float s = S();
    const int MW = 97, MH = 65;
    const auto& p = ses.player();
    i64 px = ifloor(p.x), py = ifloor(p.y);
    auto& ren = h.renderer();
    mapTimer_ -= 1.0 / 60.0;
    ID3D11ShaderResourceView* srv = ren.uiImage("map");
    if (!srv || mapTimer_ <= 0) {
        mapTimer_ = 0.25;
        std::vector<u8> img((size_t)MW * MH * 4);
        auto& world = ses.world();
        std::unordered_set<i64> itemTiles;
        for (const auto* it : world.itemsNear(p.x, p.y, 60.0, ses.picked))
            itemTiles.insert(((i64)ifloor(it->x) << 32) ^ (i64)(u32)ifloor(it->y));
        i64 fx = px + (i64)std::lround(std::cos(p.angle) * 2.0), fy = py + (i64)std::lround(std::sin(p.angle) * 2.0);
        for (int y = 0; y < MH; ++y)
            for (int x = 0; x < MW; ++x) {
                i64 tx = px - MW / 2 + x, ty = py - MH / 2 + y;
                u8* c = &img[((size_t)y * MW + x) * 4];
                c[3] = 255;
                auto set = [&](int r, int g, int b) { c[0] = (u8)r, c[1] = (u8)g, c[2] = (u8)b; };
                if (tx == px && ty == py) set(255, 70, 60);
                else if (itemTiles.count((tx << 32) ^ (i64)(u32)ty)) set(255, 228, 90);
                else if (tx == fx && ty == fy) set(255, 170, 90);
                else {
                    const world::Tile* t = world.tileIfLoaded(tx, ty);
                    if (!t) set(0, 0, 0);
                    else if (t->solid()) set(22, 22, 26);
                    else if (t->door) set(205, 150, 70);
                    else if (t->flags & world::TF_MACHINE) set(255, 60, 200);
                    else {
                        const auto& m = world.bank()[t->fmat];
                        float k = 0.35f + 0.45f * std::min(1.0f, t->light);
                        if (t->room && t->floor > t->room->floor + 0.3) k *= 0.6f;
                        set((int)(m.rgb[0] * 255 * k), (int)(m.rgb[1] * 255 * k), (int)(m.rgb[2] * 255 * k));
                    }
                }
            }
        srv = ren.uiImage("map", img.data(), MW, MH);
    }
    float cell = 5 * s;
    float mw = MW * cell, mh = MH * cell;
    float x = W_ - mw - 30 * s, y = 30 * s;
    r_->rect(x - 10 * s, y - 40 * s, mw + 20 * s, mh + 50 * s, kPanel, 8 * s, 1.2f * s, kBorder);
    r_->text(x, y - 32 * s, 18 * s, std::format("KARTE  {}, {}", px, py), kTitle, gfx::FontFace::Bold);
    r_->text(x + mw, y - 32 * s, 16 * s, "■ Automat   ■ Item", rgba8(255, 140, 220), gfx::FontFace::Regular, gfx::Align::Right);
    r_->image(srv, x, y, mw, mh);
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
    r_->text(px + 40 * s, py + ph - 40 * s, 17 * s, "1 – 9 in die Schnellleiste legen    Tab schließen", kMuted);
}

// --- Tod ---------------------------------------------------------------------------------------------
void Ui::drawDead(Host& h) {
    game::GameSession* ses = h.session();
    float s = S();
    float w = std::min(W_ - 60 * s, 880 * s), hh = 420 * s;
    float x = (W_ - w) * 0.5f, y = (H_ - hh) * 0.5f;
    r_->rect(x, y, w, hh, rgba8(18, 6, 6, 235), 10 * s, 1.5f * s, rgba8(120, 30, 26));
    r_->text(x + w * 0.5f, y + 36 * s, 34 * s, "D U   H A S T   D E N   V E R S T A N D   V E R L O R E N", rgba8(255, 96, 84),
             gfx::FontFace::Bold, gfx::Align::Center);
    r_->rect(x + 36 * s, y + 96 * s, w - 72 * s, 1.5f * s, rgba8(160, 60, 50));
    if (ses) {
        int pt = (int)ses->stats().playTime;
        r_->text(x + w * 0.5f, y + 124 * s, 21 * s,
                 std::format("Spielzeit {}:{:02} min   ·   {:.0f} m zurückgelegt", pt / 60, pt % 60, ses->player().distance),
                 rgba8(230, 200, 190), gfx::FontFace::Regular, gfx::Align::Center);
    }
    std::vector<Item> items = {
        {std::string("Letzten Spielstand laden") + (h.hasSave() ? "" : "  (keiner vorhanden)"), nullptr, nullptr,
         [&h] { h.loadLastSave(); }, ""},
        {"Zum Hauptmenü", nullptr, nullptr, [&h] { h.toTitle(); }, ""},
        {"Beenden", nullptr, nullptr, [&h] { h.quit(); }, ""},
    };
    runList(h, items, lists_[10], x + 60 * s, y + 180 * s, w - 120 * s, 50 * s, 3);
    if (h.hasSave())
        r_->text(x + w * 0.5f, y + hh - 50 * s, 17 * s, "Beim Laden startest du mit voller Health und mindestens 30 % Sanity.",
                 rgba8(160, 120, 110), gfx::FontFace::Regular, gfx::Align::Center);
}

}  // namespace lim::ui
