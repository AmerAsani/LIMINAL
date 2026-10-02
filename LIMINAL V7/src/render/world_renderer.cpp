#include "render/world_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/log.hpp"
#include "render/flicker.hpp"
#include "render/renderer.hpp"
#include "render/texture_gen.hpp"

namespace lim::gfx {

u16 floatToHalf(float f) {
    u32 x;
    std::memcpy(&x, &f, 4);
    u32 sign = (x >> 16) & 0x8000u;
    int exp = (int)((x >> 23) & 0xFF) - 127 + 15;
    u32 mant = x & 0x7FFFFFu;
    if (exp <= 0) return (u16)sign;
    if (exp >= 31) return (u16)(sign | 0x7BFFu);  // groesster endlicher Wert
    return (u16)(sign | ((u32)exp << 10) | ((mant + 0x1000u) >> 13));
}

static float srgbToLinear(double c) {
    float v = (float)c;
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

GpuMaterial toGpuMaterial(const world::Material& m) {
    using world::PlanePattern;
    using world::Surface;
    using world::WallPattern;
    GpuMaterial g{};
    for (int c = 0; c < 3; ++c) g.albedo[c] = srgbToLinear(m.rgb[(size_t)c]);
    u32 up = L_CONCRETE, down = L_PLASTER, wall = L_PLASTER;
    switch (m.plane) {
        case PlanePattern::None: up = L_CONCRETE, down = L_PLASTER; break;
        case PlanePattern::Carpet: up = down = L_CARPET; break;
        case PlanePattern::Slabs: up = L_SLABS, down = L_CONCRETE; break;
        case PlanePattern::BigTiles: up = down = L_BIGTILES; break;
        case PlanePattern::Grate: up = down = L_GRATE; break;
        case PlanePattern::Tiles: up = L_FLOORTILE, down = L_CEILTILE; break;
        case PlanePattern::Checker: up = down = L_CHECKER; break;
        case PlanePattern::Lines: up = down = L_SLABS; break;
        default: break;
    }
    switch (m.wall) {
        case WallPattern::Plain: wall = L_PLASTER; break;
        case WallPattern::Wallpaper: wall = L_WALLPAPER; break;
        case WallPattern::Blocks: wall = L_BLOCKS; break;
        case WallPattern::Bricks: wall = L_BRICKS; break;
        case WallPattern::Panels: wall = L_PANELS; break;
        case WallPattern::Concrete: wall = L_CONCRETE; break;
        default: break;
    }
    g.roughness = 1.0f;
    g.metalness = 0.0f;
    auto all = [&](u32 l) { up = down = wall = l; };
    switch (m.surface) {
        case Surface::Partition: all(L_FABRIC); break;
        case Surface::Shelf: all(L_METAL); break;
        case Surface::Crate:
            all(L_CRATE);
            // V6: Fichte statt Orange (nur Darstellung; die Weltdaten bleiben unveraendert)
            for (int c = 0; c < 3; ++c) g.albedo[c] = g.albedo[c] * 0.55f + 0.45f * (g.albedo[0] * 0.2126f + g.albedo[1] * 0.7152f + g.albedo[2] * 0.0722f) * 1.05f;
            break;
        case Surface::Machine: all(L_METAL); g.roughness = 1.2f; break;
        case Surface::Stage: up = L_BIGTILES; break;
        case Surface::Monolith: all(L_MONOLITH); break;
        case Surface::Counter: all(L_WOOD); break;
        case Surface::CounterTop: all(L_LAMINATE); break;
        case Surface::Vending: all(L_VENDING); break;
        case Surface::Trim: all(L_TRIM); break;
        case Surface::Metal: all(L_METAL); break;
        default: break;
    }
    if (m.emissive) {
        if (m.surface == Surface::Default) all(L_DIFFUSER);
        g.emissive = m.surface == Surface::Vending ? 3.2f : 11.0f;
    }
    g.layers = up | (down << 8) | (wall << 16);
    g.variation = kLayers[wall].variation;
    return g;
}

bool WorldRenderer::init(Gpu& gpu, int ringSize) {
    ringSize_ = ringSize;
    height_ = gpu.createTexture(ringSize, ringSize, DXGI_FORMAT_R16G16_FLOAT, TEX_SRV);
    ambient_ = gpu.createTexture(ringSize, ringSize, DXGI_FORMAT_R16G16B16A16_FLOAT, TEX_SRV);
    if (!height_.valid() || !ambient_.valid()) return false;
    int slots = ringSize / world::CS;
    ringOwner_.assign((size_t)slots * slots, world::ChunkKey{});
    ringUsed_.assign((size_t)slots * slots, false);
    resetRing(gpu, 0, 0, ringSize, ringSize);

    // Feste Materialien: Items (Farbe steckt vollstaendig in der Textur)
    materialData_.assign(kFixedMaterials, GpuMaterial{{1, 1, 1}, 0, 0, 1, 0, 0});
    materialData_[FM_ITEM_ENERGY].layers = L_ITEM_ENERGY | (L_ITEM_ENERGY << 8) | (L_ITEM_ENERGY << 16);
    materialData_[FM_ITEM_ALMOND].layers = L_ITEM_ALMOND | (L_ITEM_ALMOND << 8) | (L_ITEM_ALMOND << 16);
    // V6: Strichmaennchen - schwarz und matt wie mit dem Filzstift gezogen (nur ein Hauch Glanz,
    // damit die Striche in hellen Raeumen nicht flach wirken). Augen und Grinsen hell, damit das
    // Gesicht auf dem schwarzen Kopf sichtbar ist.
    materialData_[FM_BODY] = GpuMaterial{{0.008f, 0.008f, 0.009f}, 0, L_PLASTER | (L_PLASTER << 8) | (L_PLASTER << 16), 0.95f, 0, 0.0f};
    materialData_[FM_BODY_FACE] = GpuMaterial{{0.85f, 0.83f, 0.78f}, 0, L_PLASTER | (L_PLASTER << 8) | (L_PLASTER << 16), 1.0f, 0, 0.0f};
    // V6: Automat - Gehaeuse (Atlas), Leuchtschild und Anzeige (gleicher Atlas, leuchtend),
    // Glasfront mit Faechern dahinter (Interior Mapping im Mesh-Shader)
    const u32 body = L_VEND_BODY | (L_VEND_BODY << 8) | (L_VEND_BODY << 16);
    materialData_[FM_VEND_BODY] = GpuMaterial{{1, 1, 1}, 0, body, 1.0f, 0, 0.0f};
    materialData_[FM_VEND_GLOW] = GpuMaterial{{1, 0.97f, 0.92f}, 2.6f, body, 1.0f, 0, 0.0f};
    materialData_[FM_VEND_GLASS] =
        GpuMaterial{{1, 0.96f, 0.9f}, 2.4f, L_VENDING | (L_VENDING << 8) | (L_VENDING << 16) | kMatFlagInteriorGlass, 1.0f, 0, 0.0f};
    // V7: Taschenlampe, Fundstuecke, Notausgang, Gestalt
    auto all = [](u32 l) { return l | (l << 8) | (l << 16); };
    materialData_[FM_FLASH_BODY] = GpuMaterial{{0.045f, 0.045f, 0.05f}, 0, all(L_METAL), 0.85f, 0, 0.0f};
    materialData_[FM_FLASH_LENS] = GpuMaterial{{1.0f, 0.94f, 0.82f}, 7.0f, all(L_DIFFUSER), 1.0f, 0, 0.0f};
    materialData_[FM_ITEM_BATTERY] = GpuMaterial{{1, 1, 1}, 0, all(L_ITEM_BATTERY), 1.0f, 0, 0.0f};
    materialData_[FM_ITEM_NOTE] = GpuMaterial{{1, 1, 1}, 0, all(L_ITEM_NOTE), 1.0f, 0, 0.0f};
    materialData_[FM_EXIT_FRAME] = GpuMaterial{{1, 1, 1}, 0, all(L_EXIT), 1.0f, 0, 0.0f};
    materialData_[FM_EXIT_DOOR] = GpuMaterial{{1, 1, 1}, 0, all(L_EXIT), 1.0f, 0, 0.0f};
    materialData_[FM_EXIT_SIGN] = GpuMaterial{{0.9f, 1.0f, 0.92f}, 3.5f, all(L_EXIT), 1.0f, 0, 0.0f};
    materialData_[FM_GHOST] = GpuMaterial{{0.003f, 0.003f, 0.004f}, 0, all(L_PLASTER), 1.0f, 0, 0.0f};
    writeMaterials(gpu);
    lights_ = gpu.createStructured(sizeof(GpuLight), 2048, true, false);
    return height_.valid() && ambient_.valid() && lights_.valid();
}

void WorldRenderer::clear(Gpu& gpu) {
    chunks_.clear();
    materialData_.resize(kFixedMaterials);
    syncedWorldMaterials_ = 0;
    // Neue Welt: keine Reste der vorigen im Hoehenfeld (sonst falsche Schatten am Sichtrand)
    std::fill(ringUsed_.begin(), ringUsed_.end(), false);
    resetRing(gpu, 0, 0, ringSize_, ringSize_);
}

// Unbekannte Bereiche: massiv, dunkel (Schattenstrahlen enden dort)
void WorldRenderer::resetRing(Gpu& gpu, int rx, int ry, int w, int h) {
    std::vector<u16> hf((size_t)w * h * 2);
    for (size_t i = 0; i < hf.size(); i += 2) {
        hf[i] = floatToHalf(60000.0f);
        hf[i + 1] = floatToHalf(-60000.0f);
    }
    std::vector<u16> am((size_t)w * h * 4, 0);
    D3D11_BOX box{(UINT)rx, (UINT)ry, 0, (UINT)(rx + w), (UINT)(ry + h), 1};
    gpu.ctx()->UpdateSubresource(height_.tex.get(), 0, &box, hf.data(), (UINT)w * 4, 0);
    gpu.ctx()->UpdateSubresource(ambient_.tex.get(), 0, &box, am.data(), (UINT)w * 8, 0);
}

int WorldRenderer::ringSlot(world::ChunkKey k) const {
    int slots = ringSize_ / world::CS;
    int sx = (int)(((k.x % slots) + slots) % slots), sy = (int)(((k.y % slots) + slots) % slots);
    return sy * slots + sx;
}

void WorldRenderer::writeMaterials(Gpu& gpu) {
    u32 cap = materials_.valid() ? materials_.count : 0;
    if (materialData_.size() > cap) {
        u32 n = std::max<u32>(1024, (u32)materialData_.size() * 2);
        materials_ = gpu.createStructured(sizeof(GpuMaterial), n, false, false);
    }
    D3D11_BOX box{0, 0, 0, (UINT)(materialData_.size() * sizeof(GpuMaterial)), 1, 1};
    gpu.ctx()->UpdateSubresource(materials_.buf.get(), 0, &box, materialData_.data(), 0, 0);
}

void WorldRenderer::syncMaterials(Gpu& gpu, const world::MaterialBank& bank) {
    size_t n = bank.size();
    if (n <= syncedWorldMaterials_) return;
    for (size_t i = syncedWorldMaterials_; i < n; ++i) materialData_.push_back(toGpuMaterial(bank[(u16)i]));
    syncedWorldMaterials_ = n;
    writeMaterials(gpu);
}

void WorldRenderer::addChunk(Gpu& gpu, const world::ChunkData& cd) {
    GpuChunk c;
    const auto& mesh = cd.mesh;
    if (!mesh.vertices.empty()) {
        // Materialindizes auf die GPU-Tabelle verschieben (feste Materialien davor)
        std::vector<world::WorldVertex> verts = mesh.vertices;
        for (auto& v : verts) {
            u32 mat = ((v.data >> 3) & 0x3FFFu) + kFixedMaterials;
            v.data = (v.data & ~(0x3FFFu << 3)) | ((mat & 0x3FFFu) << 3);
        }
        c.vb = gpu.createVertex(verts.data(), (u32)(verts.size() * sizeof(world::WorldVertex)), sizeof(world::WorldVertex));
        c.indexCount = (u32)mesh.indices.size();
        if (verts.size() < 65536) {
            std::vector<u16> idx(mesh.indices.begin(), mesh.indices.end());
            c.ib = gpu.createIndex(idx.data(), (u32)(idx.size() * 2));
            c.index16 = true;
        } else {
            c.ib = gpu.createIndex(mesh.indices.data(), (u32)(mesh.indices.size() * 4));
        }
        c.bounds = mesh.bounds;
    }
    c.lights = cd.lights;

    // Ring-Texturen: Hoehenfeld und Umgebungslicht dieses Chunks
    const int R = ringSize_;
    const int rx = (int)(((cd.x0() % R) + R) % R), ry = (int)(((cd.y0() % R) + R) % R);
    u16 hf[world::CS * world::CS * 2];
    u16 am[world::CS * world::CS * 4];
    for (int y = 0; y < world::CS; ++y)
        for (int x = 0; x < world::CS; ++x) {
            const world::Tile& t = cd.tile(x, y);
            size_t i = (size_t)(y * world::CS + x);
            hf[i * 2] = floatToHalf(t.solid() ? 60000.0f : t.floor);
            hf[i * 2 + 1] = floatToHalf(t.solid() ? -60000.0f : t.ceil);
            vec3 a = cd.ambient[i];
            am[i * 4] = floatToHalf(a.x);
            am[i * 4 + 1] = floatToHalf(a.y);
            am[i * 4 + 2] = floatToHalf(a.z);
            am[i * 4 + 3] = floatToHalf(1.0f);
        }
    D3D11_BOX box{(UINT)rx, (UINT)ry, 0, (UINT)(rx + world::CS), (UINT)(ry + world::CS), 1};
    gpu.ctx()->UpdateSubresource(height_.tex.get(), 0, &box, hf, world::CS * 4, 0);
    gpu.ctx()->UpdateSubresource(ambient_.tex.get(), 0, &box, am, world::CS * 8, 0);
    int slot = ringSlot(cd.key);
    ringOwner_[(size_t)slot] = cd.key;
    ringUsed_[(size_t)slot] = true;
    chunks_[cd.key] = std::move(c);
}

void WorldRenderer::removeChunk(Gpu& gpu, const world::ChunkData& cd) {
    chunks_.erase(cd.key);
    // Ringfeld nur zuruecksetzen, wenn dort nicht schon ein anderer Chunk liegt
    int slot = ringSlot(cd.key);
    if (ringUsed_[(size_t)slot] && ringOwner_[(size_t)slot] == cd.key) {
        ringUsed_[(size_t)slot] = false;
        const int R = ringSize_;
        resetRing(gpu, (int)(((cd.x0() % R) + R) % R), (int)(((cd.y0() % R) + R) % R), world::CS, world::CS);
    }
}

u32 WorldRenderer::updateLights(Gpu& gpu, const mat4& view, const Frustum& frustum, vec3 camPos, float maxDist,
                                float time, float intensityScale, bool shadows, u32 maxLights,
                                const std::vector<ExtraLight>* extra) {
    struct Cand {
        float d2;
        const world::ChunkLight* l;
    };
    static std::vector<Cand> cands;
    cands.clear();
    // Reichweite: V4-Radius (waagrecht) mal Faktor; kuerzer = mehr Kontrast, weniger Arbeit.
    // V4 rechnete Licht rein waagrecht. Deckenlampen in hohen Hallen (ueber 3 m) reichen deshalb
    // um den Mehrabstand weiter und strahlen staerker, aber gebuendelter nach unten
    // (Hallenstrahler), damit der Boden so hell bleibt wie in normalen Raeumen. Ein weicher
    // Kern (grosse Leuchtflaeche) verhindert dabei grelle Flecken an Waenden direkt neben der Lampe.
    auto overOf = [](const world::ChunkLight& L) {
        return L.kind == world::ChunkLight::Ceiling ? std::clamp(L.drop - 3.0f, 0.0f, 9.0f) : 0.0f;
    };
    // Hallenlampen reichen 1,5x den Mehrabstand weiter, damit das Ausblenden am Rand der
    // Reichweite im schwachen Bereich liegt (sonst runde, scharf begrenzte Lichtflecken an Waenden).
    auto rangeOf = [&](const world::ChunkLight& L) { return std::max(L.radius * 1.7f, 5.0f) + 1.5f * overOf(L); };
    for (const auto& [k, c] : chunks_)
        for (const auto& L : c.lights) {
            float range = rangeOf(L);
            vec3 d = L.pos - camPos;
            float d2 = dot(d, d);
            float lim = maxDist + range;
            if (d2 > lim * lim) continue;
            if (!frustum.intersectsSphere(L.pos, range)) continue;
            cands.push_back({d2, &L});
        }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
    // V7: zusaetzliche Leuchten (Taschenlampe, Schilder) stehen vorn - sie sind nah und wichtig
    const size_t ne = extra ? std::min<size_t>(extra->size(), 16) : 0;
    const size_t cap = std::min<size_t>(lights_.count, maxLights);
    size_t n = std::min<size_t>(cands.size(), cap > ne ? cap - ne : 0);
    lightData_.resize(ne + n);
    for (size_t i = 0; i < ne; ++i) {
        const ExtraLight& e = (*extra)[i];
        GpuLight& g = lightData_[i];
        vec3 dir = normalize(e.dir);
        vec3 side = normalize(cross(dir, std::fabs(dir.z) < 0.9f ? vec3{0, 0, 1} : vec3{1, 0, 0}));
        vec3 up = cross(side, dir);
        g.posRange = {e.pos, e.range};
        g.colorRadius = {e.color * intensityScale, e.size};
        g.dirSpill = {dir, e.spot ? e.cosOuter : e.spill};
        vec4 vp = view * vec4(e.pos, 1.0f);
        u32 flags = (shadows && e.shadows ? 1u : 0u) | (e.spot ? 2u : 0u);
        float ff;
        std::memcpy(&ff, &flags, 4);
        g.viewPosFlags = {vp.x, vp.y, vp.z, ff};
        g.areaU = {side * e.size, e.soft};
        g.areaV = {up * e.size, e.spot ? e.cosInner : 0.0f};
    }
    for (size_t i = 0; i < n; ++i) {
        const world::ChunkLight& L = *cands[i].l;
        GpuLight& g = lightData_[ne + i];
        float range = rangeOf(L);
        float fl = flickerFactor(L.flickerMode, L.flickerSeed, time);
        if ((int)L.flickerSeed == blackoutSeed_ && L.kind != world::ChunkLight::Machine) fl *= 1.0f - blackoutAmount_;  // V7
        vec3 lin{srgbToLinear(L.color.x), srgbToLinear(L.color.y), srgbToLinear(L.color.z)};
        float power = L.intensity * intensityScale * fl;
        vec3 dir{0, 0, -1};
        float over = overOf(L);
        float boost = 1.0f + over / 3.0f;
        power *= boost * boost;
        float spill = 0.18f / boost;
        float soft2 = over * over;  // Kern der Abstandsdaempfung: 1 / (d^2 + 0.25 + soft2)
        vec3 au{L.area.x * 0.5f, 0, 0}, av{0, L.area.y * 0.5f, 0};
        if (L.kind != world::ChunkLight::Ceiling) {
            dir = normalize(vec3{(float)L.dirX, (float)L.dirY, 0.0f});
            vec3 side{-dir.y, dir.x, 0.0f};
            au = side * (L.area.x * 0.5f);
            av = vec3{0, 0, L.area.y * 0.5f};
            spill = L.kind == world::ChunkLight::Wall ? 0.25f : 0.12f;
            if (L.kind == world::ChunkLight::Machine) power = L.intensity * intensityScale * 0.75f * fl;
        }
        g.posRange = {L.pos, range};
        g.colorRadius = {lin * power, std::max(L.area.x, L.area.y) * 0.5f};
        g.dirSpill = {dir, spill};
        vec4 vp = view * vec4(L.pos, 1.0f);
        u32 flags = shadows ? 1u : 0u;
        float ff;
        std::memcpy(&ff, &flags, 4);
        g.viewPosFlags = {vp.x, vp.y, vp.z, ff};
        g.areaU = {au, soft2};
        g.areaV = {av, 0};
    }
    if (ne + n) gpu.update(lights_, lightData_.data(), (u32)((ne + n) * sizeof(GpuLight)));
    return (u32)(ne + n);
}

u32 WorldRenderer::drawChunks(Gpu& gpu, const Frustum& frustum, vec3 camPos) {
    drawList_.clear();
    for (const auto& [k, c] : chunks_) {
        if (!c.indexCount || !frustum.intersects(c.bounds)) continue;
        vec3 mid = (c.bounds.lo + c.bounds.hi) * 0.5f;
        vec3 d = mid - camPos;
        drawList_.push_back({dot(d, d), &c});
    }
    // von vorne nach hinten: frueher Tiefentest spart Pixelarbeit
    std::sort(drawList_.begin(), drawList_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    auto* ctx = gpu.ctx();
    UINT stride = sizeof(world::WorldVertex), offset = 0;
    triangles_ = 0;
    for (const auto& [d, c] : drawList_) {
        ctx->IASetVertexBuffers(0, 1, c->vb.buf.addr(), &stride, &offset);
        ctx->IASetIndexBuffer(c->ib.buf.get(), c->index16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
        ctx->DrawIndexed(c->indexCount, 0, 0);
        triangles_ += c->indexCount / 3;
    }
    return (u32)drawList_.size();
}

}  // namespace lim::gfx
