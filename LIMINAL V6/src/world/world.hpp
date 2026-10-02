// World: Streaming von Sektoren und Chunks (Port und Ausbau von V4, world.py).
//
// Pipeline (alles deterministisch, daher egal in welcher Reihenfolge):
//   Sektor erzeugen   (Arbeitsthread; nur Metadaten)
//   Sektor verbinden  (Arbeitsthread; braucht die 4 Nachbarn: Portale aufloesen,
//                      Raeume vorbereiten)
//   Chunk bauen       (Arbeitsthread; Kacheln, Licht, Netz)
//   veroeffentlichen  (Hauptthread; Ereignis onChunkLoaded -> Renderer/Gameplay)
//
// Alle Zustandsaenderungen der Karten passieren auf dem Hauptthread. Jobs halten
// shared_ptr auf die Sektoren, mit denen sie arbeiten; Entladen ist dadurch
// jederzeit sicher. Braucht das Spiel eine Kachel sofort (Kollision), wird der
// fehlende Teil synchron gebaut - wie in V4.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "core/jobs.hpp"
#include "world/chunk.hpp"
#include "world/generator.hpp"

namespace lim::world {

struct SectorKey {
    i64 x = 0, y = 0;
    bool operator==(const SectorKey& o) const { return x == o.x && y == o.y; }
};
struct SectorKeyHash {
    size_t operator()(const SectorKey& k) const { return std::hash<u64>()(((u64)(u32)k.x << 32) ^ (u32)k.y); }
};

class World {
public:
    // supplyRules: V6-Seltenheit einzelner Items (leer = Item-Verteilung exakt wie V4)
    World(std::shared_ptr<const LevelDef> level, i64 seed, double itemOddsNone, double itemOddsOne, JobSystem& jobs,
          int loadRadius, SupplyRules supplyRules = {});
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    i64 seed() const { return seed_; }
    const LevelDef& level() const { return *level_; }
    const Generator& generator() const { return *gen_; }
    MaterialBank& bank() { return bank_; }
    const MaterialBank& bank() const { return bank_; }
    int sectorSize() const { return level_->sectorSize; }

    // --- Abfragen (Hauptthread) -------------------------------------------------------
    // Kachel an (x, y); baut den Chunk notfalls sofort (wie cell() in V4).
    const Tile& tile(i64 x, i64 y);
    // Kachel nur, wenn ihr Chunk bereits geladen ist (sonst nullptr).
    const Tile* tileIfLoaded(i64 x, i64 y) const;
    // Raum an einer Position (bei Tueren der Raum auf dieser Seite).
    const Room* roomAt(double x, double y);
    const ChunkData* chunk(ChunkKey k) const;
    Sector& sector(i64 sx, i64 sy);  // mindestens erzeugt (synchron)

    // --- Streaming -------------------------------------------------------------------
    void update(double px, double py);
    void preload(double px, double py);  // wartet, bis der Laderadius vollstaendig geladen ist
    void setLoadRadius(int r) { loadRadius_ = std::max(2, r); }
    int loadRadius() const { return loadRadius_; }

    std::function<void(const ChunkData&)> onChunkLoaded;
    std::function<void(const ChunkData&)> onChunkUnloaded;

    // --- Items und Versorgungsraeume ----------------------------------------------------
    std::vector<const ItemSpawn*> itemsNear(double x, double y, double radius,
                                            const std::unordered_set<std::string>& picked) const;
    template <class Fn>
    void forEachSupplyRoom(Fn&& fn) const {
        for (const auto& [k, e] : sectors_)
            if (e.sec)
                for (const Room* r : e.sec->supplyRooms) fn(*r);
    }

    std::tuple<double, double, double, double> findSpawn();

    // --- Statistik ---------------------------------------------------------------------
    size_t loadedChunks() const { return chunks_.size(); }
    size_t loadedSectors() const { return sectors_.size(); }
    size_t loadedRooms() const;
    u64 generatedChunks() const { return generatedChunks_; }
    int jobsInFlight() const { return inFlight_; }
    // V6: synchron (auf dem Hauptthread) erzeugte Sektoren/Chunks - Ursache fuer Ruckler
    u64 syncWork() const { return syncWork_; }
    const std::unordered_map<ChunkKey, std::shared_ptr<ChunkData>, ChunkKeyHash>& chunks() const { return chunks_; }

private:
    enum class SState { Generating, Generated, Linking, Linked };
    struct SEntry {
        SState state = SState::Generating;
        std::shared_ptr<Sector> sec;
    };

    SectorKey sectorKeyOf(i64 x, i64 y) const {
        return {floorDiv(x, level_->sectorSize), floorDiv(y, level_->sectorSize)};
    }
    void requiredSectors(ChunkKey k, std::vector<SectorKey>& out) const;
    bool ensureLinked(SectorKey k);                // asynchron: plant Jobs, true wenn fertig
    void scheduleGenerate(SectorKey k);
    void scheduleLink(SectorKey k);
    void scheduleChunk(ChunkKey k);
    void linkSector(Sector& sec, const std::shared_ptr<Sector> nb[4]) const;
    std::shared_ptr<ChunkData> buildChunkNow(ChunkKey k, std::vector<std::shared_ptr<Sector>> secs) const;
    void publishChunk(std::shared_ptr<ChunkData> cd);
    void ensureLinkedSync(SectorKey k);
    const ChunkData& chunkSync(ChunkKey k);
    void unloadFar(i64 pcx, i64 pcy);

    std::shared_ptr<const LevelDef> level_;
    i64 seed_;
    MaterialBank bank_;
    std::unique_ptr<Generator> gen_;
    JobSystem& jobs_;
    int loadRadius_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    std::unordered_map<SectorKey, SEntry, SectorKeyHash> sectors_;
    std::unordered_map<ChunkKey, std::shared_ptr<ChunkData>, ChunkKeyHash> chunks_;
    std::unordered_set<ChunkKey, ChunkKeyHash> building_;
    int inFlight_ = 0;
    u64 generatedChunks_ = 0;
    u64 syncWork_ = 0;
    std::vector<SectorKey> tmpKeys_;
};

}  // namespace lim::world
