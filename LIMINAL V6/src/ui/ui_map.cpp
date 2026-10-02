// V6: Karten - runde Minimap (oben rechts) und grosse Karte (Taste M).
//
// Beide zeigen ausschliesslich, was der Spieler erkundet hat (game::ExploreMap):
// Unbekanntes bleibt verborgen, statt nur abgedunkelt zu werden. Das Kartenbild wird
// als Grundriss erzeugt (Boden in gedaempften Farben der Zone, Waende als helle
// Kontur, Tueren, Treppen, Einbauten, Automaten) und nur neu gezeichnet, wenn sich
// etwas aendert. Die Minimap dreht sich mit dem Blick (einstellbar) und scrollt
// stufenlos; Pfeil und Sichtkegel zeigen Position und Blickrichtung.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>

#include "app/settings.hpp"
#include "gameplay/session.hpp"
#include "render/renderer.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"
#include "world/room.hpp"

namespace lim::ui {

using namespace theme;

namespace {

constexpr float kMiniMeters = 19.0f;  // sichtbarer Radius der Minimap (m)
constexpr float kBigMeters = 58.0f;   // sichtbarer Radius der grossen Karte (m)

struct Rgb8 {
    int r, g, b;
};

Rgb8 floorColor(const game::MapCell& c) {
    // Bodenfarbe der Zone, stark gedaempft und zu einem warmen Grau gezogen (Grundriss-Look)
    float mr = ((c.color >> 11) & 31) / 31.0f, mg = ((c.color >> 5) & 63) / 63.0f, mb = (c.color & 31) / 31.0f;
    float gray = 0.30f * mr + 0.59f * mg + 0.11f * mb;
    float k = 0.42f;  // Restsaettigung
    mr = gray + (mr - gray) * k, mg = gray + (mg - gray) * k, mb = gray + (mb - gray) * k;
    float light = c.light / 255.0f * 1.6f;
    float b = 0.42f + 0.38f * std::min(1.0f, light) + 0.12f * std::max(0.0f, light - 1.0f);
    // warme Tonung wie die Oberflaeche
    return {(int)std::clamp((mr * 0.8f + 0.2f * 0.62f) * b * 255.0f, 0.0f, 255.0f),
            (int)std::clamp((mg * 0.8f + 0.2f * 0.56f) * b * 255.0f, 0.0f, 255.0f),
            (int)std::clamp((mb * 0.8f + 0.2f * 0.44f) * b * 255.0f, 0.0f, 255.0f)};
}

}  // namespace

// Grundriss um (cx, cy): (2 * half + 1)^2 Kacheln, texelsPerTile Bildpunkte je Kachel
void Ui::buildMapImage(game::GameSession& s, i64 cx, i64 cy, int half, int tpp) {
    using game::MapKind;
    const auto& ex = s.explore();
    const int n = 2 * half + 1;       // Kacheln je Seite
    const int g = n + 2;              // mit Rand (Nachbarn fuer Konturen)
    const i64 x0 = cx - half - 1, y0 = cy - half - 1;
    // Zellen einmal je Chunk holen statt je Kachel suchen
    std::vector<game::MapCell> cells((size_t)g * g);
    for (i64 ky = world::chunkOf(x0, y0).y; ky <= world::chunkOf(x0, y0 + g - 1).y; ++ky)
        for (i64 kx = world::chunkOf(x0, y0).x; kx <= world::chunkOf(x0 + g - 1, y0).x; ++kx) {
            const game::MapCell* cc = ex.chunkCells({kx, ky});
            if (!cc) continue;
            for (int ly = 0; ly < world::CS; ++ly)
                for (int lx = 0; lx < world::CS; ++lx) {
                    i64 wx = kx * world::CS + lx, wy = ky * world::CS + ly;
                    if (wx < x0 || wy < y0 || wx >= x0 + g || wy >= y0 + g) continue;
                    cells[(size_t)((wy - y0) * g + (wx - x0))] = cc[ly * world::CS + lx];
                }
        }
    auto cell = [&](int gx, int gy) -> const game::MapCell& { return cells[(size_t)(gy * g + gx)]; };
    auto open = [&](int gx, int gy) {
        const auto& c = cell(gx, gy);
        return c.known() && c.kind != MapKind::Wall;
    };
    const int W = n * tpp;
    mapPixels_.assign((size_t)W * W * 4, 0);
    const Rgb8 ink{236, 218, 170}, wallFill{30, 27, 22}, door{214, 156, 72}, machine{255, 96, 196}, pending{122, 114, 94};
    for (int ty = 0; ty < n; ++ty)
        for (int tx = 0; tx < n; ++tx) {
            const int gx = tx + 1, gy = ty + 1;
            const game::MapCell& c = cell(gx, gy);
            if (!c.known()) continue;
            bool wall = c.kind == MapKind::Wall;
            // Kontur: Seiten der Wand, die an bekannten offenen Boden grenzen
            bool eL = wall && open(gx - 1, gy), eR = wall && open(gx + 1, gy);
            bool eT = wall && open(gx, gy - 1), eB = wall && open(gx, gy + 1);
            Rgb8 base = wallFill;
            int alpha = 230;
            switch (c.kind) {
                case MapKind::Wall: break;
                case MapKind::Door: base = door; break;
                case MapKind::Machine: base = machine; break;
                case MapKind::Pending: base = pending; break;
                default: {
                    base = floorColor(c);
                    if (c.kind == MapKind::Low) base = {base.r * 62 / 100, base.g * 62 / 100, base.b * 62 / 100};
                    if (c.kind == MapKind::High) base = {base.r * 40 / 100, base.g * 40 / 100, base.b * 40 / 100};
                    break;
                }
            }
            for (int py = 0; py < tpp; ++py)
                for (int px = 0; px < tpp; ++px) {
                    Rgb8 col = base;
                    if (wall) {
                        bool edge = (eL && px == 0) || (eR && px == tpp - 1) || (eT && py == 0) || (eB && py == tpp - 1);
                        // Aussenecken schliessen
                        if (!edge && px == 0 && py == 0 && open(gx - 1, gy - 1) && !open(gx - 1, gy) && !open(gx, gy - 1))
                            edge = true;
                        if (!edge && px == tpp - 1 && py == 0 && open(gx + 1, gy - 1) && !open(gx + 1, gy) && !open(gx, gy - 1))
                            edge = true;
                        if (!edge && px == 0 && py == tpp - 1 && open(gx - 1, gy + 1) && !open(gx - 1, gy) && !open(gx, gy + 1))
                            edge = true;
                        if (!edge && px == tpp - 1 && py == tpp - 1 && open(gx + 1, gy + 1) && !open(gx + 1, gy) &&
                            !open(gx, gy + 1))
                            edge = true;
                        if (edge) col = ink;
                    } else if (c.kind == MapKind::Stair && ((ty * tpp + py) / std::max(1, tpp / 2)) % 2 == 0) {
                        col = {col.r * 78 / 100, col.g * 78 / 100, col.b * 78 / 100};  // Stufen als Streifen
                    } else if ((c.kind == MapKind::Low || c.kind == MapKind::High) &&
                               (px == 0 || py == 0 || px == tpp - 1 || py == tpp - 1)) {
                        col = {std::min(255, col.r + 26), std::min(255, col.g + 24), std::min(255, col.b + 18)};
                    }
                    u8* p = &mapPixels_[((size_t)(ty * tpp + py) * W + (size_t)(tx * tpp + px)) * 4];
                    p[0] = (u8)col.r, p[1] = (u8)col.g, p[2] = (u8)col.b, p[3] = (u8)alpha;
                }
        }
}

// Items, Spielerpfeil, Sichtkegel und Norden ueber dem Kartenbild
void Ui::drawMapOverlay(Host& h, game::GameSession& ses, float cx, float cy, float r, float meters, float angle, bool big) {
    float s = S();
    const auto& p = ses.player();
    const float scale = r / meters;
    float ca = std::cos(angle), sa = std::sin(angle);
    auto toScreen = [&](double wx, double wy, float& sx, float& sy) {
        float qx = (float)(wx - p.x), qy = (float)(wy - p.y);
        sx = cx + (qx * ca + qy * sa) * scale;
        sy = cy + (-qx * sa + qy * ca) * scale;
    };
    // Items (auf Theken und vom Spieler abgelegte) nur auf erkundeten Kacheln
    ses.registry().each<game::Pickup, game::Transform>([&](ecs::Entity, game::Pickup& pk, game::Transform& tr) {
        if (!ses.explore().known(ifloor(tr.pos.x), ifloor(tr.pos.y))) return;
        float sx, sy;
        toScreen(tr.pos.x, tr.pos.y, sx, sy);
        float dx = sx - cx, dy = sy - cy;
        if (dx * dx + dy * dy > (r - 6 * s) * (r - 6 * s)) return;
        const game::ItemDef* d = ses.content().item(pk.item);
        bool rare = d && d->rare;
        float pulse = rare ? 0.5f + 0.5f * (float)std::sin(now_ * 4.0) : 0.0f;
        float rr = (big ? 4.2f : 3.4f) * s * (1.0f + 0.35f * pulse);
        if (rare) r_->rect(sx - rr * 2.2f, sy - rr * 2.2f, rr * 4.4f, rr * 4.4f, rgba8(255, 214, 90, (int)(70 * pulse)), rr * 2.2f);
        r_->rect(sx - rr, sy - rr, rr * 2, rr * 2, rare ? rgba8(255, 226, 110) : rgba8(240, 222, 150), rr, 1.0f * s,
                 rgba8(40, 30, 10, 200));
    });
    // Sichtkegel und Pfeil des Spielers (Bildschirmwinkel: 0 = oben, im Uhrzeigersinn)
    float look = (float)p.angle + kPi * 0.5f - angle;
    r_->cone(cx, cy, (big ? 70.0f : 46.0f) * s, look, 0.62f, rgba8(255, 230, 170, 70));
    r_->arrow(cx, cy, (big ? 11.0f : 9.0f) * s, look, rgba8(255, 236, 190), rgba8(24, 18, 10, 230));
    // Norden am Rand
    float nx = -std::sin(angle), ny = -std::cos(angle);
    float nr = r + (big ? 0.0f : 1.0f) * s;
    float px = cx + nx * nr, py = cy + ny * nr;
    float br = (big ? 13.0f : 10.0f) * s;
    r_->rect(px - br, py - br, br * 2, br * 2, rgba8(20, 18, 14, 235), br, 1.2f * s, kBorder);
    r_->text(px, py - br * 0.78f, br * 1.35f, "N", kTitle, gfx::FontFace::Bold, gfx::Align::Center);
    (void)h;
}

void Ui::drawMinimap(Host& h, game::GameSession& ses, double dt) {
    Settings& st = h.settings();
    if (!st.minimap) return;
    float s = S();
    const auto& p = ses.player();
    const float r = 100.0f * s;
    const float cx = W_ - r - 26.0f * s, cy = r + 26.0f * s;
    const int half = (int)std::ceil(kMiniMeters) + 2, tpp = 4;
    const i64 tx = ifloor(p.x), ty = ifloor(p.y);
    auto& ren = h.renderer();
    mini_.timer -= dt;
    ID3D11ShaderResourceView* srv = ren.uiImage("minimap");
    if (!srv || mini_.rev != ses.explore().revision() || mini_.cx != tx || mini_.cy != ty || mini_.timer <= 0.0) {
        buildMapImage(ses, tx, ty, half, tpp);
        srv = ren.uiImage("minimap", mapPixels_.data(), (2 * half + 1) * tpp, (2 * half + 1) * tpp);
        mini_ = {ses.explore().revision(), tx, ty, 1.0, half, tpp};
    }
    // Rahmen: dunkles Glas, darauf der Grundriss, dann ein feiner Ring
    float angle = st.minimapRotate ? (float)p.angle + kPi * 0.5f : 0.0f;
    r_->rect(cx - r - 5 * s, cy - r - 5 * s, (r + 5 * s) * 2, (r + 5 * s) * 2, rgba8(12, 11, 9, 150), r + 5 * s);
    r_->rect(cx - r, cy - r, r * 2, r * 2, rgba8(8, 8, 7, 120), r);
    const float n = (float)(2 * half + 1);
    vec2 uvc{(float)(p.x - (double)(tx - half)) / n, (float)(p.y - (double)(ty - half)) / n};
    r_->imageDisc(srv, cx, cy, r, uvc, kMiniMeters / n, angle);
    r_->rect(cx - r - 3 * s, cy - r - 3 * s, (r + 3 * s) * 2, (r + 3 * s) * 2, rgba8(0, 0, 0, 0), r + 3 * s, 1.6f * s,
             rgba8(150, 138, 104, 210));
    drawMapOverlay(h, ses, cx, cy, r, kMiniMeters, angle, false);
    // Zone unter der Karte (dezent)
    if (const world::Room* room = ses.currentRoom()) {
        const auto& zones = ses.world().level().zones;
        if ((size_t)room->zone < zones.size()) {
            std::string z = upperUtf8(zones[(size_t)room->zone].name);
            float tw = r_->textWidth(z, 15 * s, gfx::FontFace::Bold);
            r_->rect(cx - tw * 0.5f - 10 * s, cy + r + 9 * s, tw + 20 * s, 26 * s, rgba8(12, 11, 9, 140), 13 * s);
            r_->text(cx, cy + r + 12 * s, 15 * s, z, rgba8(196, 184, 150, 230), gfx::FontFace::Bold, gfx::Align::Center);
        }
    }
}

void Ui::drawBigMap(Host& h, game::GameSession& ses, double dt) {
    bigAnim_ = std::min(1.0f, bigAnim_ + (float)dt * 6.0f);
    float a = 1.0f - (1.0f - bigAnim_) * (1.0f - bigAnim_);
    float s = S();
    const auto& p = ses.player();
    const float r = std::min(W_, H_) * 0.35f * (0.94f + 0.06f * a);
    const float cx = W_ * 0.5f, cy = H_ * 0.46f;
    const int half = (int)std::ceil(kBigMeters) + 2, tpp = 3;
    const i64 tx = ifloor(p.x), ty = ifloor(p.y);
    auto& ren = h.renderer();
    big_.timer -= dt;
    ID3D11ShaderResourceView* srv = ren.uiImage("bigmap");
    bool moved = std::abs(big_.cx - tx) + std::abs(big_.cy - ty) > 0;
    if (!srv || ((big_.rev != ses.explore().revision() || moved) && big_.timer <= 0.0)) {
        buildMapImage(ses, tx, ty, half, tpp);
        srv = ren.uiImage("bigmap", mapPixels_.data(), (2 * half + 1) * tpp, (2 * half + 1) * tpp);
        big_ = {ses.explore().revision(), tx, ty, 0.12, half, tpp};
    }
    // das Bild ist um big_.cx/cy zentriert (evtl. ein paar Bilder alt): Mittelpunkt daraus
    const float n = (float)(2 * big_.half + 1);
    vec2 uvc{(float)(p.x - (double)(big_.cx - big_.half)) / n, (float)(p.y - (double)(big_.cy - big_.half)) / n};
    vec4 shade = rgba8(4, 4, 3, (int)(160 * a));
    r_->rect(0, 0, W_, H_, shade);
    r_->rect(cx - r - 8 * s, cy - r - 8 * s, (r + 8 * s) * 2, (r + 8 * s) * 2, rgba8(14, 13, 10, (int)(246 * a)), r + 8 * s,
             1.4f * s, rgba8(110, 100, 76, (int)(230 * a)));
    r_->imageDisc(srv, cx, cy, r, uvc, kBigMeters / n, 0.0f, {1, 1, 1, a});
    drawMapOverlay(h, ses, cx, cy, r, kBigMeters, 0.0f, true);
    // Kopfzeile und Legende
    float ty0 = cy - r - 54 * s;
    r_->text(cx, ty0, 26 * s, "K A R T E", kTitle, gfx::FontFace::Bold, gfx::Align::Center);
    r_->text(cx, cy + r + 18 * s, 17 * s,
             std::format("{} m² erkundet   ·   Position {}, {}   ·   M schließen", ses.explore().knownTiles(), ifloor(p.x),
                         ifloor(p.y)),
             kMuted, gfx::FontFace::Regular, gfx::Align::Center);
    float lx = cx + r + 30 * s, ly = cy - 70 * s;
    if (lx + 170 * s > W_) lx = cx - r + 12 * s, ly = cy + r - 120 * s;  // schmale Fenster: Legende in die Karte
    auto legend = [&](vec4 col, const char* label) {
        r_->rect(lx, ly + 4 * s, 14 * s, 14 * s, col, 3 * s);
        r_->text(lx + 24 * s, ly, 17 * s, label, kText);
        ly += 28 * s;
    };
    legend(rgba8(236, 218, 170), "Wand");
    legend(rgba8(214, 156, 72), "Durchgang");
    legend(rgba8(255, 96, 196), "Automat");
    legend(rgba8(255, 226, 110), "Item");
}

}  // namespace lim::ui
