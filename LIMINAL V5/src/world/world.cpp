#include "world/world.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>

#include "core/log.hpp"
#include "core/math.hpp"

namespace lim::world {

namespace {

struct FetchCtx {
    const std::vector<std::shared_ptr<Sector>>* secs;
};

Tile fetchTile(void* ctx, i64 x, i64 y) {
    auto* c = static_cast<FetchCtx*>(ctx);
    for (const auto& s : *c->secs) {
        if (!s->contains(x, y)) continue;
        if (Room* r = s->roomAt(x, y)) return r->tile(x, y);
        if (const Door* d = s->doorAt(x, y)) return d->cell();
        return Tile{};
    }
    return Tile{};
}
}  // namespace

World::World(std::shared_ptr<const LevelDef> level, i64 seed, double oddsNone, double oddsOne, JobSystem& jobs,
             int loadRadius)
    : level_(std::move(level)), seed_(seed), jobs_(jobs), loadRadius_(std::max(2, loadRadius)) {
    gen_ = std::make_unique<Generator>(*level_, seed_, bank_, oddsNone, oddsOne);
}

World::~World() {
    *alive_ = false;
    // Laufende Jobs greifen auf Generator und Sektoren zu - erst abwarten.
    jobs_.waitIdle();
    jobs_.drainCompletions();
}

// --- Sektoren ---------------------------------------------------------------------------
void World::scheduleGenerate(SectorKey k) {
    if (sectors_.count(k)) return;
    sectors_[k] = SEntry{SState::Generating, nullptr};
    ++inFlight_;
    const Generator* gen = gen_.get();
    auto alive = alive_;
    jobs_.submit([this, gen, k, alive] {
        std::shared_ptr<Sector> sec(gen->generateSector(k.x, k.y).release());
        jobs_.completeOnMain([this, k, sec, alive] {
            if (!*alive) return;
            --inFlight_;
            auto it = sectors_.find(k);
            if (it == sectors_.end() || it->second.state != SState::Generating) return;  // inzwischen synchron erzeugt/entladen
            it->second.sec = sec;
            it->second.state = SState::Generated;
        });
    });
}

void World::linkSector(Sector& sec, const std::shared_ptr<Sector> nb[4]) const {
    // Portale mit den Raeumen im Nachbarsektor verbinden (V4: resolve_door)
    for (auto& dp : sec.doors) {
        Door& d = *dp;
        if (!d.portal || d.remoteResolved) continue;
        const Sector* remote = nullptr;
        for (int i = 0; i < 4; ++i)
            if (nb[i] && nb[i]->sx == d.remoteSx && nb[i]->sy == d.remoteSy) remote = nb[i].get();
        i64 tx, ty;
        if (d.remoteSide == 'a') {
            tx = d.axis == 0 ? d.x0 - 1 : d.x0;
            ty = d.axis == 0 ? d.y0 : d.y0 - 1;
        } else {
            tx = d.axis == 0 ? d.x1 : d.x0;
            ty = d.axis == 0 ? d.y0 : d.y1;
        }
        const Room* r = remote ? remote->roomAt(tx, ty) : nullptr;
        d.remoteAvgLight = r ? r->avgLight : 0.4;
        d.remoteResolved = true;
    }
    for (auto& r : sec.rooms) r->prepare();
}

static const SectorKey kNbOff[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

void World::scheduleLink(SectorKey k) {
    auto& e = sectors_[k];
    e.state = SState::Linking;
    std::shared_ptr<Sector> nb[4];
    for (int i = 0; i < 4; ++i) nb[i] = sectors_[{k.x + kNbOff[i].x, k.y + kNbOff[i].y}].sec;
    std::shared_ptr<Sector> sec = e.sec;
    ++inFlight_;
    auto alive = alive_;
    jobs_.submit([this, k, sec, nb0 = nb[0], nb1 = nb[1], nb2 = nb[2], nb3 = nb[3], alive] {
        const std::shared_ptr<Sector> n[4] = {nb0, nb1, nb2, nb3};
        linkSector(*sec, n);
        jobs_.completeOnMain([this, k, sec, alive] {
            if (!*alive) return;
            --inFlight_;
            sec->linked = true;
            auto it = sectors_.find(k);
            if (it != sectors_.end() && it->second.sec == sec) it->second.state = SState::Linked;
        });
    });
}

bool World::ensureLinked(SectorKey k) {
    auto it = sectors_.find(k);
    if (it == sectors_.end()) {
        scheduleGenerate(k);
        return false;
    }
    SState st = it->second.state;
    if (st == SState::Linked) return true;
    if (st != SState::Generated) return false;
    bool ready = true;
    for (const auto& o : kNbOff) {
        SectorKey n{k.x + o.x, k.y + o.y};
        auto nit = sectors_.find(n);
        if (nit == sectors_.end()) {
            scheduleGenerate(n);
            ready = false;
        } else if (nit->second.state == SState::Generating) {
            ready = false;
        }
    }
    if (ready) scheduleLink(k);
    return false;
}

Sector& World::sector(i64 sx, i64 sy) {
    SectorKey k{sx, sy};
    auto& e = sectors_[k];
    if (!e.sec) {
        e.sec = std::shared_ptr<Sector>(gen_->generateSector(sx, sy).release());
        e.state = SState::Generated;  // ein evtl. laufender Job wird bei Ankunft verworfen
    }
    return *e.sec;
}

void World::ensureLinkedSync(SectorKey k) {
    sector(k.x, k.y);
    for (const auto& o : kNbOff) sector(k.x + o.x, k.y + o.y);
    while (sectors_[k].state == SState::Linking) {  // Job bereitet gerade vor -> abwarten
        jobs_.waitIdle();
        jobs_.drainCompletions();
        for (const auto& o : kNbOff) sector(k.x + o.x, k.y + o.y);
    }
    auto& e = sectors_[k];  // erst jetzt: Abschluss-Callbacks koennen die Tabelle veraendern
    if (!e.sec) sector(k.x, k.y);
    if (e.state == SState::Linked) return;
    std::shared_ptr<Sector> nb[4];
    for (int i = 0; i < 4; ++i) nb[i] = sectors_[{k.x + kNbOff[i].x, k.y + kNbOff[i].y}].sec;
    linkSector(*e.sec, nb);
    e.sec->linked = true;
    e.state = SState::Linked;
}

// --- Chunks -----------------------------------------------------------------------------
void World::requiredSectors(ChunkKey k, std::vector<SectorKey>& out) const {
    out.clear();
    const i64 x0 = k.x * CS - 1, y0 = k.y * CS - 1, x1 = k.x * CS + CS, y1 = k.y * CS + CS;
    for (i64 y : {y0, y1})
        for (i64 x : {x0, x1}) {
            SectorKey s = sectorKeyOf(x, y);
            if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
        }
    // eigener Sektor zuerst (dient als Besitzer)
    SectorKey own = sectorKeyOf(k.x * CS, k.y * CS);
    auto it = std::find(out.begin(), out.end(), own);
    if (it != out.end()) std::iter_swap(out.begin(), it);
}

std::shared_ptr<ChunkData> World::buildChunkNow(ChunkKey k, std::vector<std::shared_ptr<Sector>> secs) const {
    auto cd = std::make_shared<ChunkData>();
    cd->key = k;
    cd->sectors = std::move(secs);
    FetchCtx ctx{&cd->sectors};
    buildChunk(*cd, &fetchTile, &ctx, bank_);
    return cd;
}

void World::scheduleChunk(ChunkKey k) {
    requiredSectors(k, tmpKeys_);
    std::vector<std::shared_ptr<Sector>> secs;
    for (const auto& sk : tmpKeys_) secs.push_back(sectors_[sk].sec);
    building_.insert(k);
    ++inFlight_;
    auto alive = alive_;
    jobs_.submit([this, k, secs = std::move(secs), alive]() mutable {
        auto cd = buildChunkNow(k, std::move(secs));
        jobs_.completeOnMain([this, k, cd, alive] {
            if (!*alive) return;
            --inFlight_;
            building_.erase(k);
            if (chunks_.count(k)) return;  // bereits synchron gebaut
            publishChunk(cd);
        });
    });
}

void World::publishChunk(std::shared_ptr<ChunkData> cd) {
    ChunkKey k = cd->key;
    chunks_[k] = cd;
    ++generatedChunks_;
    if (onChunkLoaded) onChunkLoaded(*cd);
}

const ChunkData& World::chunkSync(ChunkKey k) {
    auto it = chunks_.find(k);
    if (it != chunks_.end()) return *it->second;
    std::vector<SectorKey> keys;
    requiredSectors(k, keys);
    for (const auto& sk : keys) ensureLinkedSync(sk);
    std::vector<std::shared_ptr<Sector>> secs;
    for (const auto& sk : keys) secs.push_back(sectors_[sk].sec);
    auto cd = buildChunkNow(k, std::move(secs));
    publishChunk(cd);
    return *cd;
}

const Tile& World::tile(i64 x, i64 y) {
    ChunkKey k = chunkOf(x, y);
    auto it = chunks_.find(k);
    const ChunkData& cd = it != chunks_.end() ? *it->second : chunkSync(k);
    return cd.tile((int)(x & CS_MASK), (int)(y & CS_MASK));
}

const Tile* World::tileIfLoaded(i64 x, i64 y) const {
    auto it = chunks_.find(chunkOf(x, y));
    if (it == chunks_.end()) return nullptr;
    return &it->second->tile((int)(x & CS_MASK), (int)(y & CS_MASK));
}

const ChunkData* World::chunk(ChunkKey k) const {
    auto it = chunks_.find(k);
    return it != chunks_.end() ? it->second.get() : nullptr;
}

const Room* World::roomAt(double x, double y) {
    const Tile& t = tile(ifloor(x), ifloor(y));
    if (t.room) return t.room;
    if (t.door) return t.door->localRoom();
    return nullptr;
}

void World::update(double px, double py) {
    jobs_.drainCompletions(4.0);
    const i64 pcx = ifloor(px) >> CS_SHIFT, pcy = ifloor(py) >> CS_SHIFT;
    // Direkte Umgebung immer sofort (Kollision, Sicht)
    for (i64 dy = -1; dy <= 1; ++dy)
        for (i64 dx = -1; dx <= 1; ++dx) chunkSync({pcx + dx, pcy + dy});

    const int R = loadRadius_;
    std::vector<std::pair<i64, ChunkKey>> missing;
    for (i64 dy = -R; dy <= R; ++dy)
        for (i64 dx = -R; dx <= R; ++dx) {
            ChunkKey k{pcx + dx, pcy + dy};
            if (!chunks_.count(k) && !building_.count(k)) missing.push_back({dx * dx + dy * dy, k});
        }
    std::sort(missing.begin(), missing.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const int maxInFlight = (int)jobs_.workerCount() * 3 + 2;
    for (const auto& [d, k] : missing) {
        if (inFlight_ >= maxInFlight) break;
        requiredSectors(k, tmpKeys_);
        bool ready = true;
        for (const auto& sk : std::vector<SectorKey>(tmpKeys_)) ready = ensureLinked(sk) && ready;
        if (ready) scheduleChunk(k);
    }
    unloadFar(pcx, pcy);
}

void World::preload(double px, double py) {
    const i64 pcx = ifloor(px) >> CS_SHIFT, pcy = ifloor(py) >> CS_SHIFT;
    const int R = loadRadius_;
    while (true) {
        update(px, py);
        bool done = true;
        for (i64 dy = -R; dy <= R && done; ++dy)
            for (i64 dx = -R; dx <= R; ++dx)
                if (!chunks_.count({pcx + dx, pcy + dy})) {
                    done = false;
                    break;
                }
        if (done) break;
        jobs_.waitIdle();
        jobs_.drainCompletions();
    }
}

void World::unloadFar(i64 pcx, i64 pcy) {
    const i64 U = loadRadius_ + 2;
    std::vector<ChunkKey> far;
    for (const auto& [k, c] : chunks_)
        if (std::abs(k.x - pcx) > U || std::abs(k.y - pcy) > U) far.push_back(k);
    for (const auto& k : far) {
        auto it = chunks_.find(k);
        if (onChunkUnloaded) onChunkUnloaded(*it->second);
        chunks_.erase(it);
    }
    // Sektoren weit ausserhalb des Laderadius verwerfen (werden bei Bedarf identisch neu erzeugt)
    const i64 S = level_->sectorSize;
    const i64 psx = floorDiv(pcx * CS, S), psy = floorDiv(pcy * CS, S);
    const i64 keep = floorDiv((loadRadius_ + 3) * CS, S) + 2;
    std::vector<SectorKey> drop;
    for (const auto& [k, e] : sectors_)
        if ((std::abs(k.x - psx) > keep || std::abs(k.y - psy) > keep) && e.state != SState::Generating &&
            e.state != SState::Linking)
            drop.push_back(k);
    for (const auto& k : drop) sectors_.erase(k);
}

// --- Items ---------------------------------------------------------------------------------
std::vector<const ItemSpawn*> World::itemsNear(double x, double y, double radius,
                                               const std::unordered_set<std::string>& picked) const {
    std::vector<const ItemSpawn*> out;
    const double r2 = radius * radius;
    for (const auto& [k, e] : sectors_) {
        if (!e.sec) continue;
        for (const Room* room : e.sec->itemRooms)
            for (const ItemSpawn& it : room->items) {
                if (picked.count(it.id)) continue;
                double dx = it.x - x, dy = it.y - y;
                if (dx * dx + dy * dy <= r2) out.push_back(&it);
            }
    }
    return out;
}

size_t World::loadedRooms() const {
    size_t n = 0;
    for (const auto& [k, e] : sectors_)
        if (e.sec) n += e.sec->rooms.size();
    return n;
}

std::tuple<double, double, double, double> World::findSpawn() {
    // Startpunkt: moeglichst ein heller Flur nahe dem Ursprung (wie V4)
    Sector& sec = sector(0, 0);
    const double S = level_->sectorSize;
    const Room* best = nullptr;
    double bestScore = 0;
    for (const auto& rp : sec.rooms) {
        const Room& room = *rp;
        if (room.floor != 0.0) continue;
        auto [cx, cy] = room.center();
        double score = std::hypot(cx - S * 0.5, cy - S * 0.5);
        if (room.isCorridor) score -= 25.0;
        for (int t : level_->spawnPreferred)
            if (room.type->index == t) {
                score -= 10.0;
                break;
            }
        if (!best || score < bestScore) {
            best = &room;
            bestScore = score;
        }
    }
    if (!best) best = sec.rooms.front().get();
    auto [rx, ry] = best->center();
    for (i64 rad = 0; rad < 12; ++rad)
        for (i64 dy = -rad; dy <= rad; ++dy)
            for (i64 dx = -rad; dx <= rad; ++dx) {
                i64 x = (i64)rx + dx, y = (i64)ry + dy;
                const Tile& c = tile(x, y);
                if (c.open() && c.ceil - c.floor >= 1.9f) {
                    double angle = best->w() >= best->h() ? 0.0 : kPi * 0.5;
                    return {x + 0.5, y + 0.5, (double)c.floor, angle};
                }
            }
    return {rx, ry, 0.0, 0.0};
}

}  // namespace lim::world
