// Eine Kachel (1 x 1 m) der Welt. Entspricht der Zelle aus V4
// (boden, decke, boden_mat, decken_mat, wand_mat, licht, besitzer) plus
// einigen Angaben, die der 3D-Renderer braucht.
#pragma once

#include "core/core.hpp"

namespace lim::world {

class Room;
struct Door;

enum TileFlags : u8 {
    TF_SOLID = 1 << 0,     // massive Wand
    TF_LAMP = 1 << 1,      // Deckenleuchte
    TF_WALLLAMP = 1 << 2,  // Wandleuchte an der Raumwand
    TF_STAIR = 1 << 3,     // Treppenstufe
    TF_FEATURE = 1 << 4,   // Einbau (Kiste, Regal, Theke ...)
    TF_MACHINE = 1 << 5,   // leuchtender Automat
    TF_DOOR = 1 << 6,      // Durchgang
};

struct Tile {
    float floor = 0.0f, ceil = 0.0f;  // Boden- und Deckenhoehe (m)
    u16 fmat = 0;                     // Boden (bzw. Oberseite eines Einbaus)
    u16 cmat = 0;                     // Decke
    u16 wmat = 0;                     // Seitenflaechen dieser Kachel (Stufen, Einbauten)
    u16 roomWall = 0;                 // Material fuer volle Waende (Raumwand inkl. Baendern)
    float light = 0.0f;               // Helligkeit wie in V4 (0 .. 1.6)
    u8 flags = TF_SOLID;
    const Room* room = nullptr;       // besitzender Raum (bei Tueren: nullptr)
    const Door* door = nullptr;

    bool solid() const { return (flags & TF_SOLID) != 0; }
    bool open() const { return (flags & TF_SOLID) == 0; }
    float clearance() const { return ceil - floor; }
};

}  // namespace lim::world
