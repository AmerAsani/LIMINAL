// Komponenten des Spiels. Reine Daten ohne Logik; die Systeme in
// GameSession (session.cpp: updateWanderers, findTarget, collectMeshes,
// collectSounds ...) arbeiten darauf. Neue Komponenten: hier definieren, in
// prefabs.cpp einen Leser fuer die JSON-Beschreibung registrieren und das
// zugehoerige System in GameSession::update aufrufen.
#pragma once

#include <string>

#include "core/math.hpp"
#include "ecs/registry.hpp"
#include "world/chunk.hpp"

namespace lim::game {

struct Transform {
    vec3 pos;
    float yaw = 0.0f;
    float scale = 1.0f;
};

// Sichtbares Modell (Netz aus der MeshLibrary des Renderers)
struct MeshRenderer {
    std::string model;
    int meshId = -2;          // -2 = noch nicht aufgeloest
    float highlight = 0.0f;
    float drawDistance = 40.0f;
    bool glint = false;       // V6: leichtes Schimmern (seltene Items)
};

// Kann mit [E] aufgenommen werden
struct Pickup {
    std::string item;         // Schluessel aus data/items.json
    std::string worldId;      // stabile ID des Liegeplatzes (Spielstand)
};

// Allgemein interaktiv: Hinweistext und Reichweite
struct Interactable {
    std::string prompt;
    float range = 2.1f;
    float coneDeg = 42.0f;
};

// Dauerhafter Klang an einer Position (Automatenbrummen ...)
struct AudioEmitter {
    std::string sound;
    float volume = 1.0f;
    float radius = 10.0f;
    int voice = -1;           // vom Audiosystem vergeben
};

// V6: vom Spieler abgelegtes Item - fliegt, prallt kurz auf und bleibt dann liegen
struct Dropped {
    vec3 vel;
    float spin = 0.0f;   // Drehung um die Hochachse (rad/s)
    bool resting = false;
    std::string id;      // Eintrag in der Liste abgelegter Items (Spielstand)
};

// V7: Notizzettel eines anderen Wanderers (mit E lesen)
struct NoteFind {
    int index = -1;  // Eintrag in data/notes.json
    std::string id;  // Fundort (Spielstand)
};

// V7: Notausgang zur naechsten Ebene
struct ExitDoor {
    std::string id;
};

// Gehoert zu einem Chunk und verschwindet mit ihm
struct ChunkOwned {
    world::ChunkKey chunk;
};

// Herkunft (Prefab-Name) fuer Debug-Anzeige und Speicherung
struct Tag {
    std::string prefab;
};

// Einfache, datengesteuerte Bewegung (Beispiel fuer kuenftige NPCs/Gegner):
// wandert in Kachelwelt zwischen zufaelligen Zielen umher.
struct Wanderer {
    float speed = 1.2f;
    float timer = 0.0f;
    vec2 target{0, 0};
    u32 seed = 1;
};

}  // namespace lim::game
