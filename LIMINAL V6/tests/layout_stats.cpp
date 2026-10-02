// V6: misst die raeumliche Struktur der Welt - alter Aufbau (V4/V5) gegen den V6-Aufbau.
//
//   layout_stats <data-Ordner> [--seeds a,b,c] [--radius R] [--map <ordner>] [--check]
//
// Je Level und Aufbau werden (2R+1)^2 Sektoren je Seed erzeugt und als Belegungsraster
// (offen = Raum oder Durchgang) ausgewertet:
//   Raumdichte (Raeume je Hektar), Raumgroessen, Anteil kleiner Raeume, Flurbreiten,
//   Freiraum (Abstand zur naechsten Wand), Sichtachsen (Strahlen durch das Raster),
//   Versorgungsraeume je Hektar.
// --map schreibt fuer den ersten Seed je Aufbau eine Draufsicht als BMP (1 Pixel = 1 m).
// --check prueft die Ziele des V6-Aufbaus (weniger, groessere Raeume, laengere Sichtachsen,
// Versorgung bleibt erreichbar) und liefert sonst Exitcode 1.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "core/jobs.hpp"
#include "core/rng.hpp"
#include "gameplay/content.hpp"
#include "world/world.hpp"

using namespace lim;
using namespace lim::world;

struct Stats {
    double area = 0;            // betrachtete Flaeche (m^2)
    double open = 0;            // offene Kacheln
    int rooms = 0, corridors = 0, supply = 0, doors = 0, openings = 0;
    std::vector<double> roomAreas;
    double corridorWidth = 0;
    double clearanceSum = 0;
    double spacious = 0;        // offene Kacheln mit >= 3 m Abstand zur Wand
    double raySum = 0, rayMaxSum = 0;
    int rayPoints = 0, rays = 0;
};

struct Grid {
    i64 x0, y0;
    int w, h;
    std::vector<u8> cell;  // 0 Wand, 1 Raum, 2 Flur, 3 Tuer, 4 Oeffnung, 5 Versorgung
    std::vector<u8> zone;
    u8 at(i64 x, i64 y) const {
        if (x < x0 || y < y0 || x >= x0 + w || y >= y0 + h) return 0;
        return cell[(size_t)((y - y0) * w + (x - x0))];
    }
};

static void writeBmp(const std::string& path, const Grid& g, const LevelDef& L) {
    const int W = g.w, H = g.h;
    const int row = (W * 3 + 3) & ~3;
    std::vector<unsigned char> px((size_t)row * H, 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            u8 c = g.cell[i];
            double r = 0.06, gg = 0.06, b = 0.07;
            if (c != 0) {
                const Rgb& f = L.zones[g.zone[i]].wall;
                double k = c == 2 ? 0.95 : 0.7;
                r = f[0] * k, gg = f[1] * k, b = f[2] * k;
                if (c == 3) r = 0.95, gg = 0.55, b = 0.15;
                if (c == 4) r = 1.0, gg = 0.9, b = 0.35;
                if (c == 5) r = 0.85, gg = 0.2, b = 0.2;
            }
            unsigned char* p = &px[(size_t)(H - 1 - y) * row + (size_t)x * 3];
            p[0] = (unsigned char)std::clamp(b * 255.0, 0.0, 255.0);
            p[1] = (unsigned char)std::clamp(gg * 255.0, 0.0, 255.0);
            p[2] = (unsigned char)std::clamp(r * 255.0, 0.0, 255.0);
        }
    std::ofstream f(path, std::ios::binary);
    auto u32le = [&](u32 v) { f.put((char)(v & 255)).put((char)(v >> 8 & 255)).put((char)(v >> 16 & 255)).put((char)(v >> 24 & 255)); };
    auto u16le = [&](u16 v) { f.put((char)(v & 255)).put((char)(v >> 8 & 255)); };
    f.put('B').put('M');
    u32le((u32)(54 + px.size()));
    u32le(0);
    u32le(54);
    u32le(40);
    u32le((u32)W);
    u32le((u32)H);
    u16le(1);
    u16le(24);
    u32le(0);
    u32le((u32)px.size());
    u32le(2835);
    u32le(2835);
    u32le(0);
    u32le(0);
    f.write((const char*)px.data(), (std::streamsize)px.size());
}

static void analyze(std::shared_ptr<const LevelDef> lvl, i64 seed, int R, JobSystem& jobs, Stats& st, const std::string& mapPath) {
    World world(lvl, seed, 0.28, 0.80, jobs, 4);
    const i64 S = lvl->sectorSize;
    Grid g;
    g.x0 = -R * S;
    g.y0 = -R * S;
    g.w = (int)((2 * R + 1) * S);
    g.h = g.w;
    g.cell.assign((size_t)g.w * g.h, 0);
    g.zone.assign((size_t)g.w * g.h, 0);
    for (int sy = -R; sy <= R; ++sy)
        for (int sx = -R; sx <= R; ++sx) {
            Sector& sec = world.sector(sx, sy);
            for (const auto& rp : sec.rooms) {
                const Room& r = *rp;
                u8 v = r.isCorridor ? 2 : (r.type->supply ? 5 : 1);
                for (i64 y = r.y0; y < r.y1; ++y)
                    for (i64 x = r.x0; x < r.x1; ++x) {
                        size_t i = (size_t)((y - g.y0) * g.w + (x - g.x0));
                        g.cell[i] = v;
                        g.zone[i] = (u8)r.zone;
                    }
                if (r.isCorridor) {
                    ++st.corridors;
                    st.corridorWidth += (double)std::min(r.w(), r.h());
                } else {
                    ++st.rooms;
                    st.roomAreas.push_back((double)(r.w() * r.h()));
                }
                if (r.type->supply) ++st.supply;
            }
            for (const auto& dp : sec.doors) {
                const Door& d = *dp;
                if (!d.owned) continue;
                ++st.doors;
                if (d.kind == DoorKind::Opening) ++st.openings;
                for (i64 y = d.y0; y < d.y1; ++y)
                    for (i64 x = d.x0; x < d.x1; ++x) {
                        if (x < g.x0 || y < g.y0 || x >= g.x0 + g.w || y >= g.y0 + g.h) continue;
                        size_t i = (size_t)((y - g.y0) * g.w + (x - g.x0));
                        g.cell[i] = d.kind == DoorKind::Opening ? 4 : 3;
                        const Room* lr = d.localRoom();
                        g.zone[i] = lr ? (u8)lr->zone : 0;
                    }
            }
        }
    st.area += (double)g.w * g.h;
    // Freiraum: Abstand zur naechsten Wand (Schachbrett-Metrik, Breitensuche)
    std::vector<int> dist((size_t)g.w * g.h, -1);
    std::deque<int> q;
    for (int i = 0; i < g.w * g.h; ++i)
        if (g.cell[(size_t)i] == 0) dist[(size_t)i] = 0, q.push_back(i);
    while (!q.empty()) {
        int i = q.front();
        q.pop_front();
        int x = i % g.w, y = i / g.w;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= g.w || ny >= g.h) continue;
                int j = ny * g.w + nx;
                if (dist[(size_t)j] < 0) dist[(size_t)j] = dist[(size_t)i] + 1, q.push_back(j);
            }
    }
    for (int i = 0; i < g.w * g.h; ++i)
        if (g.cell[(size_t)i] != 0) {
            st.open += 1;
            st.clearanceSum += dist[(size_t)i];
            if (dist[(size_t)i] >= 3) st.spacious += 1;
        }
    // Sichtachsen: Strahlen in 16 Richtungen von gleichmaessig verteilten offenen Punkten
    Rng rng(hash64(seed, "sight"));
    const int margin = (int)S / 2;
    for (int n = 0; n < 2500; ++n) {
        int x = (int)rng.randint(margin, g.w - margin - 1), y = (int)rng.randint(margin, g.h - margin - 1);
        if (g.cell[(size_t)(y * g.w + x)] == 0) continue;
        double best = 0;
        for (int k = 0; k < 16; ++k) {
            double a = k * (2.0 * 3.14159265358979 / 16.0);
            double dx = std::cos(a), dy = std::sin(a), len = 0;
            for (double t = 0.5; t < 150.0; t += 0.5) {
                if (g.at(g.x0 + (i64)std::floor(x + 0.5 + dx * t), g.y0 + (i64)std::floor(y + 0.5 + dy * t)) == 0) break;
                len = t;
            }
            st.raySum += len;
            ++st.rays;
            best = std::max(best, len);
        }
        st.rayMaxSum += best;
        ++st.rayPoints;
    }
    if (!mapPath.empty()) writeBmp(mapPath, g, *lvl);
}

struct Summary {
    double roomsPerHa, medianArea, smallShare, corrWidth, openShare, clearance, spaciousShare, ray, rayMax, supplyPerHa,
        openingShare;
};

static Summary summarize(Stats& st) {
    Summary s{};
    double ha = st.area / 10000.0;
    s.roomsPerHa = (st.rooms + st.corridors) / ha;
    std::sort(st.roomAreas.begin(), st.roomAreas.end());
    s.medianArea = st.roomAreas.empty() ? 0 : st.roomAreas[st.roomAreas.size() / 2];
    double small = 0, total = 0;
    for (double a : st.roomAreas) {
        total += a;
        if (a < 40) small += a;
    }
    s.smallShare = total > 0 ? small / total : 0;
    s.corrWidth = st.corridors ? st.corridorWidth / st.corridors : 0;
    s.openShare = st.open / st.area;
    s.clearance = st.open > 0 ? st.clearanceSum / st.open : 0;
    s.spaciousShare = st.open > 0 ? st.spacious / st.open : 0;
    s.ray = st.rays ? st.raySum / st.rays : 0;
    s.rayMax = st.rayPoints ? st.rayMaxSum / st.rayPoints : 0;
    s.supplyPerHa = st.supply / ha;
    s.openingShare = st.doors ? (double)st.openings / st.doors : 0;
    return s;
}

static void print(const char* tag, const Summary& s) {
    std::printf("  %-6s Raeume/ha %5.1f | Median-Raum %5.0f m2 | kleine Raeume %4.1f %% | Flurbreite %4.1f | "
                "offen %4.1f %% | Wandabstand %4.2f | weitlaeufig %4.1f %% | Sicht %5.1f / max %5.1f m | "
                "Oeffnungen %4.1f %% | Versorgung/ha %4.2f\n",
                tag, s.roomsPerHa, s.medianArea, s.smallShare * 100, s.corrWidth, s.openShare * 100, s.clearance,
                s.spaciousShare * 100, s.ray, s.rayMax, s.openingShare * 100, s.supplyPerHa);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Aufruf: layout_stats <data-Ordner> [--seeds a,b] [--radius R] [--map ordner] [--check]\n");
        return 2;
    }
    std::vector<long long> seeds = {1, 7, 1234, 4242, 31337};
    int R = 2;
    std::string mapDir;
    bool doCheck = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--radius" && i + 1 < argc) R = std::atoi(argv[++i]);
        else if (a == "--map" && i + 1 < argc) mapDir = argv[++i];
        else if (a == "--check") doCheck = true;
        else if (a == "--seeds" && i + 1 < argc) {
            seeds.clear();
            std::string s = argv[++i];
            for (size_t p = 0; p <= s.size();) {
                size_t e = s.find(',', p);
                if (e == std::string::npos) e = s.size();
                seeds.push_back(std::atoll(s.substr(p, e - p).c_str()));
                p = e + 1;
            }
        }
    }
    game::Content content;
    std::string err;
    if (!content.load(argv[1], err)) {
        std::printf("Inhalte nicht ladbar: %s\n", err.c_str());
        return 2;
    }
    JobSystem jobs(2);
    int fails = 0;
    for (const auto& lv : content.levels()) {
        std::printf("%s (%d Seeds, %dx%d Sektoren):\n", lv.def->id.c_str(), (int)seeds.size(), 2 * R + 1, 2 * R + 1);
        Stats a, b;
        for (size_t k = 0; k < seeds.size(); ++k) {
            std::string ma = (!mapDir.empty() && k == 0) ? std::format("{}/{}_alt.bmp", mapDir, lv.def->id) : "";
            std::string mb = (!mapDir.empty() && k == 0) ? std::format("{}/{}_v6.bmp", mapDir, lv.def->id) : "";
            analyze(lv.legacy, seeds[k], R, jobs, a, ma);
            analyze(lv.def, seeds[k], R, jobs, b, mb);
        }
        Summary sa = summarize(a), sb = summarize(b);
        print("V5", sa);
        print("V6", sb);
        if (doCheck && lv.def->layout >= kLayoutV6) {
            auto need = [&](bool ok, const char* what) {
                if (!ok) {
                    std::printf("  FEHLER: %s\n", what);
                    ++fails;
                }
            };
            need(sb.roomsPerHa <= sa.roomsPerHa * 0.85, "nicht genug weniger Raeume je Flaeche");
            need(sb.medianArea >= sa.medianArea * 1.25, "Raeume nicht deutlich groesser");
            need(sb.rayMax >= sa.rayMax * 1.15, "Sichtachsen nicht deutlich laenger");
            need(sb.spaciousShare >= sa.spaciousShare * 1.15, "nicht weitlaeufiger");
            need(sb.supplyPerHa >= sa.supplyPerHa * 0.6, "zu wenige Versorgungsraeume");
        }
    }
    if (doCheck) std::printf("%s (%d Fehler)\n", fails ? "FEHLGESCHLAGEN" : "OK", fails);
    return fails ? 1 : 0;
}
