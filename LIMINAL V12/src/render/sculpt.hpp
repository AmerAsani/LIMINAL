// V10: Prozedurales Modellieren mit Distanzfeldern (SDF) fuer die Figuren.
//
// Ein Koerper entsteht wie beim Bildhauer aus Grundformen - Ellipsoide (Muskeln, Schaedel),
// abgerundete Kegel (Gliedmassen), Kapseln, abgerundete Kisten (Hand, Schuh) -, die weich
// miteinander verschmelzen (smooth min) oder Vertiefungen schneiden (Augenhoehlen, Falten).
// Dazu kommen Verschiebungsfunktionen (Stoffknitter, Rippen, Haarstraehnen).
//
// Das Feld wird auf einem Gitter abgetastet (leere Bloecke werden uebersprungen) und mit
// "Surface Nets" in ein Dreiecksnetz verwandelt: je Zelle mit Vorzeichenwechsel ein Punkt, der
// anschliessend auf die Oberflaeche projiziert wird; Normalen aus dem Gradienten.
#pragma once

#include <functional>
#include <vector>

#include "core/math.hpp"

namespace lim {
class JobSystem;
}

namespace lim::gfx::sculpt {

struct Prim {
    enum Kind : unsigned char { Ellipsoid, RoundCone, RoundBox, Custom } kind = Ellipsoid;
    enum Op : unsigned char { Add, Sub, Inter } op = Add;
    float k = 0.0f;     // Weichheit der Verbindung (m)
    vec3 a, b;          // Ellipsoid/Box: a = Mitte; Kegel: a, b Endpunkte
    vec3 ax[3];         // Ellipsoid/Box: Achsen (orthonormal)
    vec3 r;             // Ellipsoid: Radien; Box: halbe Kanten; Kegel: r.x (bei a), r.y (bei b); Box: r.w = Rundung in b.x
    float round = 0.0f; // Box: Rundung
    Aabb box;           // Huelle (inkl. k)
    int custom = -1;    // V10: eigene Distanzfunktion (Index in Shape)
};

class Shape {
public:
    // Grundformen (Achsen: Standard = Weltachsen)
    Shape& ellipsoid(vec3 c, vec3 radii, float k = 0.0f, Prim::Op op = Prim::Add);
    Shape& ellipsoidAx(vec3 c, vec3 ax, vec3 ay, vec3 radii, float k = 0.0f, Prim::Op op = Prim::Add);  // az = ax x ay
    Shape& cone(vec3 a, vec3 b, float ra, float rb, float k = 0.0f, Prim::Op op = Prim::Add);
    Shape& capsule(vec3 a, vec3 b, float r, float k = 0.0f, Prim::Op op = Prim::Add) { return cone(a, b, r, r, k, op); }
    Shape& sphere(vec3 c, float r, float k = 0.0f, Prim::Op op = Prim::Add) { return ellipsoid(c, vec3(r), k, op); }
    Shape& box(vec3 c, vec3 ax, vec3 ay, vec3 half, float round, float k = 0.0f, Prim::Op op = Prim::Add);
    // eigene Distanzfunktion mit Huelle (z. B. der Kopf als radiales Hoehenfeld)
    Shape& custom(std::function<float(vec3)> f, Aabb box, float k = 0.0f, Prim::Op op = Prim::Add);
    // Verschiebung (positiv = nach innen/kleiner); wird nur nahe der Oberflaeche ausgewertet
    std::function<float(vec3)> displace;
    // Beschneiden: Punkte mit clip(p) > 0 liegen ausserhalb (z. B. Saum, Aermelende)
    std::function<float(vec3)> clip;
    float offset = 0.0f;  // gesamte Oberflaeche nach aussen verschieben (Kleidung ueber dem Koerper)

    float eval(vec3 p) const;
    Aabb bounds() const;
    bool empty() const { return prims_.empty(); }
    const std::vector<Prim>& prims() const { return prims_; }

private:
    std::vector<Prim> prims_;
    std::vector<std::function<float(vec3)>> customs_;
};

// Ergebnis der Vernetzung
struct RawMesh {
    std::vector<vec3> pos, normal;
    std::vector<unsigned> idx;
};

// Vernetzt das Feld im Bereich box mit Zellgroesse h (m). jobs: parallel (optional)
RawMesh mesh(const Shape& s, Aabb box, float h, JobSystem* jobs);
// V10 (simplify.cpp): Vereinfachung auf hoechstens targetTris Dreiecke (Quadric Error Metrics)
void simplify(RawMesh& m, size_t targetTris);
// Punkte auf die Oberflaeche zurueckziehen, Normalen aus dem Gradienten
void reproject(RawMesh& m, const Shape& s, float h, JobSystem* jobs);

// Hilfen
float smin(float a, float b, float k);
float smax(float a, float b, float k);
float noise3(vec3 p, unsigned seed);  // glattes Wertrauschen -1..1
float fbm3(vec3 p, int oct, unsigned seed);

}  // namespace lim::gfx::sculpt
