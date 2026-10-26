// V11: Geometrie-Baukasten fuer die Ausstattung (Leuchtenkoerper, Rohre, Schilder, Moebel,
// Fahrzeuge ...). Reine CPU-Geometrie ohne GPU-Typen; die Welt fasst die Teile je Chunk zu einem
// Netz zusammen (world/chunk_builder), der Renderer zeichnet es (shaders/props_gbuffer.hlsl).
//
// Koordinaten wie im Spiel: z oben. Ein Modell steht mit dem Ursprung am Fusspunkt bzw. an der
// Befestigung und blickt entlang +x (Drehung beim Platzieren um z).
#pragma once

#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"

namespace lim::props {

struct Vtx {
    vec3 p, n;
    vec2 uv;  // nur fuer Teile mit Atlas (Schilder, Bilder); sonst Weltkoordinaten im Shader
};

struct Mesh {
    std::vector<Vtx> v;
    std::vector<u32> i;
    void add(const Mesh& o);
    void transform(const mat4& m);  // Punkte und Normalen
    void translate(vec3 d);
    Aabb bounds() const;
    size_t tris() const { return i.size() / 3; }
};

// --- Grundformen ----------------------------------------------------------------------------------
Mesh box(vec3 lo, vec3 hi);
// Kasten mit abgefasten Kanten (bevel < halbe kuerzeste Kante): wirkt nicht mehr wie ein Klotz
Mesh bevelBox(vec3 lo, vec3 hi, float bevel);
Mesh cylinder(vec3 a, vec3 b, float r, int seg, bool caps = true);
Mesh cone(vec3 a, vec3 b, float ra, float rb, int seg, bool caps = true);
Mesh sphere(vec3 c, vec3 r, int seg, int rings);
// Flaeche mit Atlas-Koordinaten: Ursprung o, Kanten eu (u-Richtung) und ev (v nach unten)
Mesh quad(vec3 o, vec3 eu, vec3 ev, vec2 uv0, vec2 uv1, bool doubleSided = false);
// Prisma aus einem Umriss in der xy-Ebene (gegen den Uhrzeigersinn), von z0 bis z1
Mesh prism(const std::vector<vec2>& outline, float z0, float z1);
// Rohr entlang eines Polygonzugs mit gerundeten Ecken (Rohrbogen)
Mesh pipePath(const std::vector<vec3>& pts, float r, int seg, float bendR);
// Loft: Querschnitte (je Station ein geschlossener Umriss in yz) entlang x; glatte Normalen
struct Section {
    float x;
    std::vector<vec2> ring;  // (y, z), gleiche Punktzahl je Station
};
Mesh loft(const std::vector<Section>& s, bool capEnds);
// gerundetes Rechteck als Querschnitt (Breite w, Hoehe von z0 bis z1, Rundung r)
std::vector<vec2> roundRect(float w, float z0, float z1, float r, int perCorner);

// glatte Normalen (Punkte mit gleicher Lage innerhalb von eps teilen sich die Normale)
void smoothNormals(Mesh& m, float creaseDeg = 40.0f);

}  // namespace lim::props
