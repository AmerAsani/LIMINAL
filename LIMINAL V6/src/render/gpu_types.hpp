// Datenstrukturen, die 1:1 den Konstanten- und Strukturpuffern der Shader
// entsprechen (shaders/*.hlsl). Reihenfolge und Groessen muessen uebereinstimmen.
#pragma once

#include "core/math.hpp"

namespace lim::gfx {

struct FrameConstants {
    mat4 viewProj;
    mat4 viewProjNoJitter;
    mat4 invViewProj;
    mat4 prevViewProjNoJitter;
    mat4 invViewProjNoJitter;
    mat4 view;
    mat4 attachedReproject;  // V6: Weltpunkt eines mitbewegten Objekts (eigener Koerper) -> Lage im Vorbild
    vec4 cameraPos;    // w = Zeit
    vec4 screen;       // w, h, 1/w, 1/h
    vec4 jitter;       // xy aktuell (NDC), zw vorher
    vec4 fogColor;     // rgb, a = Dichte
    vec4 fogParams;    // Sichtweite, Beginn Ausblendung, Umgebungslicht, Emissiv-Staerke
    vec4 lightParams;  // Anzahl, Staerke, Schattenqualitaet, SSAO-Staerke
    vec4 ringParams;   // Ringgroesse, 1/Ringgroesse, Bildnummer, Nahebene
    vec4 proj;         // sx, sy, Nahebene, 0
    vec4 post;         // Rot-Rand, Puls, Menue-Abdunklung, Entsaettigung
    vec4 post2;        // Belichtungs-Bias, Filmkorn, Vignette, chromatische Aberration
    vec4 post3;        // Darstellungsmodus, Ausgabe-Breite, Ausgabe-Hoehe, Bloom-Staerke
    vec4 taa;          // Verlauf verwerfen, Mischfaktor, Schaerfen, Zeitschritt
    vec4 debug;        // Debug-Ansicht, AO auf Direktlicht
};
static_assert(sizeof(FrameConstants) == 7 * 64 + 13 * 16);

struct PassConstants {
    vec4 p0, p1;
};

struct GpuMaterial {
    float albedo[3];
    float emissive;
    u32 layers;
    float roughness;
    float metalness;
    float variation;
};
static_assert(sizeof(GpuMaterial) == 32);

struct GpuLight {
    vec4 posRange;
    vec4 colorRadius;
    vec4 dirSpill;
    vec4 viewPosFlags;
    vec4 areaU;
    vec4 areaV;
};
static_assert(sizeof(GpuLight) == 96);

struct ObjectConstants {
    mat4 world;
    u32 material;
    float highlight;
    float attached;  // V6: 1 = bewegt sich mit dem Spieler (Markierung im G-Buffer fuer TAA)
    float pad;
};
static_assert(sizeof(ObjectConstants) == 80);

// Vertex fuer frei platzierte Objekte (Items, Requisiten)
struct MeshVertex {
    vec3 pos;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
};
static_assert(sizeof(MeshVertex) == 48);

}  // namespace lim::gfx
