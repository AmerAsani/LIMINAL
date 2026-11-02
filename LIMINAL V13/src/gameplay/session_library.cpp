// V13.3 Teil 4 Level 5.3: die Bibliothek in der Sitzung (gameplay/library_l53.hpp; KREATUR_Gentleman.md,
// KREATUR_Bibliothekswissen.md).
//
//   Wissen     Folio auf dem Lesepult (rote Lichter, Entity 141) und Karteikarten auf dem Katalogschrank (das Volk in den
//              Anzuegen, Entity 189): Funde (Notizen mit mode "fund") an den Spawnpunkten trace/lore der Karte. Wer das Folio
//              liest, dem spielt das Diktiergeraet am Pult eine Stimme vor (nur dieser Rechner).
//   Notizbuch  das Notizbuch, das der Hausherr hinter sich wirft, wird dort ein Fund, sobald es aus seinem Bild verschwindet
//              (Phase 5..6 -> 0; Gast: an der Puppe); wer es aufhebt, solange er da ist, den bemerkt er (Host).
//   Zorn       ist er durch die Tuer, liegt vor dem Regal rechts von seinem Platz ein Haufen herabgestuerzter Buecher.
//   Stille     waehrend er spricht, schweigt die Bibliothek (app.cpp: die Klangbetten treten zurueck).
// Mehrspieler: Funde sind gewoehnliche Notizen (der Host entscheidet das Aufheben); Haufen und Stille rechnet jeder Rechner aus
// der Puppe (Zustand, Kanal glow). Protokoll unveraendert.
#include <cmath>

#include "core/log.hpp"
#include "gameplay/session.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {
int libNote(const Content& c, const std::string& title) {
    const auto& n = c.notes();
    for (size_t i = 0; i < n.size(); ++i)
        if (n[i].title == title) return (int)i;
    return -1;
}
}  // namespace

void GameSession::libraryNote(vec3 pos, float yaw, const std::string& title, const std::string& id, const std::string& model,
                              const std::string& prompt) {
    const int note = libNote(content_, title);
    if (note < 0 || isPicked(id) || !world_->tileIfLoaded(ifloor(pos.x), ifloor(pos.y))) return;
    bool there = false;
    reg_.each<NoteFind>([&](ecs::Entity, NoteFind& nf) { there = there || nf.id == id; });
    if (there) return;
    ecs::Entity n = prefabs_.spawn(reg_, "note", pos, yaw);
    if (n == ecs::kNull) return;
    if (auto* nf = reg_.get<NoteFind>(n)) nf->index = note, nf->id = id;
    if (auto* m = reg_.get<MeshRenderer>(n)) m->model = model;
    if (auto* in = reg_.get<Interactable>(n)) in->prompt = prompt;
    reg_.add<ChunkOwned>(n, ChunkOwned{world::chunkOf(ifloor(pos.x), ifloor(pos.y))});
}

void GameSession::libraryPile() {
    const vec3 p = lib53_.pileAt();
    if (!world_->tileIfLoaded(ifloor(p.x), ifloor(p.y))) return;
    ecs::Entity e = reg_.create();
    reg_.add<Transform>(e, Transform{p, lib53_.standYaw() + 0.6f, 1.0f});
    reg_.add<MeshRenderer>(e, MeshRenderer{"prop:books_fallen"});
    reg_.add<ChunkOwned>(e, ChunkOwned{world::chunkOf(ifloor(p.x), ifloor(p.y))});
    ecs::Entity e2 = reg_.create();  // ein zweiter, kleinerer Haufen ein Stueck weiter (die Kaskade)
    const vec3 f{std::cos(lib53_.standYaw()), std::sin(lib53_.standYaw()), 0.0f};
    reg_.add<Transform>(e2, Transform{p + f * 1.3f, lib53_.standYaw() - 1.1f, 0.8f});
    reg_.add<MeshRenderer>(e2, MeshRenderer{"prop:books_fallen"});
    reg_.add<ChunkOwned>(e2, ChunkOwned{world::chunkOf(ifloor(p.x), ifloor(p.y))});
}

void GameSession::spawnLibrary(const world::ChunkData& cd) {
    if (!lib53_.any()) return;
    auto inChunk = [&](vec3 p) {
        const i64 tx = ifloor(p.x), ty = ifloor(p.y);
        return tx >= cd.x0() && tx < cd.x0() + world::CS && ty >= cd.y0() && ty < cd.y0() + world::CS;
    };
    for (const Library53::Lore& L : lib53_.lore())
        if (inChunk(L.pos)) libraryNote(L.pos, L.yaw, L.title, L.id, L.model, L.prompt);
    if (lib53_.thrown && inChunk(lib53_.landing()))
        libraryNote(lib53_.landing(), lib53_.standYaw() + 2.2f, Library53::kNotebook, Library53::kNotebookId, "prop:gm_notebook", "Notizbuch aufheben");
    if (lib53_.wrath && inChunk(lib53_.pileAt())) libraryPile();
}

void GameSession::updateLibrary(double dt) {
    (void)dt;
    if (!lib53_.any()) return;
    if (lib53_.scene()) presence_.gentScene(lib53_.door(), lib53_.stand(), lib53_.standYaw());
    const Presence::GentInfo gi = presence_.gentInfo();
    lib53_.hush = gi.hush;
    // das geworfene Notizbuch: sobald es aus seinem Bild verschwindet, liegt dort der Fund
    // (Gast, der erst waehrend der Ansprache dazukommt: die Ansprache folgt immer dem Wurf)
    const bool speaking = netRole == NetRole::Guest && gi.active && gi.state == anim::gm::S_SPEAK;
    if (!lib53_.thrown && ((lib53_.prevBook >= 5.0f && gi.book < 5.0f) || (gi.landed && !gi.active) || speaking)) {
        lib53_.thrown = true;
        libraryNote(lib53_.landing(), lib53_.standYaw() + 2.2f, Library53::kNotebook, Library53::kNotebookId, "prop:gm_notebook", "Notizbuch aufheben");
        log::info("Bibliothek: das Notizbuch liegt bei ({:.1f}, {:.1f})", lib53_.landing().x, lib53_.landing().y);
    }
    lib53_.prevBook = gi.active ? gi.book : 0.0f;
    // wer es aufhebt, solange er noch da ist, den bemerkt er (der Host rechnet; der Naechste ist sein Ziel)
    if (lib53_.thrown && !lib53_.provoked && isPicked(Library53::kNotebookId)) {
        lib53_.provoked = true;
        if (netRole != NetRole::Guest && gi.active && gi.state <= anim::gm::S_LEAVE) presence_.gentProvoke(0);
    }
    // Zorn: Buecher stuerzen aus dem Regal
    if (gi.wrath && !lib53_.wrath) {
        lib53_.wrath = true;
        libraryPile();
    }
    // das Diktiergeraet am Lesepult: wer das Folio liest, hoert eine bekannte Stimme - zu glatt (rote Lichter, Entity 141)
    if (!lib53_.dictaPlayed)
        for (const Library53::Lore& L : lib53_.lore())
            if (L.id == "trace:l53_lore141" && isPicked(L.id)) {
                lib53_.dictaPlayed = true;
                GameEvent ev{GameEvent::SoundAt};
                ev.pos = L.pos + vec3{0, 0, 0.05f};
                ev.sound = "lib_dicta";
                ev.intensity = 0.75f;
                events_.push_back(ev);
            }
}

}  // namespace lim::game
