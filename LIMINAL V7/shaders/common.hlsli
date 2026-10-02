// Gemeinsame Konstanten und Hilfsfunktionen aller Shader.
// Muss zu render/gpu_types.hpp passen (gleiche Reihenfolge und Groessen).
#ifndef LIM_COMMON_HLSLI
#define LIM_COMMON_HLSLI

cbuffer FrameCB : register(b0)
{
    float4x4 gViewProj;          // mit TAA-Versatz
    float4x4 gViewProjNoJitter;
    float4x4 gInvViewProj;       // Inverse von gViewProj
    float4x4 gPrevViewProjNoJitter;
    float4x4 gInvViewProjNoJitter;
    float4x4 gView;
    float4x4 gAttachedReproject; // V6: mitbewegte Objekte (eigener Koerper): Weltpunkt -> Lage im Vorbild
    float4 gCameraPos;           // xyz, w = Zeit (s, periodisch)
    float4 gScreen;              // Breite, Hoehe, 1/Breite, 1/Hoehe (interne Aufloesung)
    float4 gJitter;              // xy aktueller Versatz (NDC), zw vorheriger
    float4 gFogColor;            // rgb (linear), a = Dichte
    float4 gFogParams;           // x Sichtweite, y Beginn Ausblendung (Anteil), z Umgebungslicht-Staerke, w Emissiv-Staerke
    float4 gLightParams;         // x Lichtanzahl, y Lichtstaerke, z Schattenqualitaet, w SSAO-Staerke
    float4 gRingParams;          // x Ringgroesse (Kacheln), y 1/Groesse, z Bildnummer, w Nahebene
    float4 gProj;                // x sx, y sy (Brennweite), z Nahebene, w 0
    float4 gPost;                // x Rot-Rand, y Puls, z Menue-Abdunklung, w Entsaettigung
    float4 gPost2;               // x Belichtung (Bias), y Filmkorn, z Vignette, w chromatische Aberration
    float4 gPost3;               // x Darstellungsmodus, y Ausgabe-Breite, z Ausgabe-Hoehe, w Bloom-Staerke
    float4 gTaa;                 // x Verlauf verwerfen (1), y Mischfaktor, z Nachschaerfen, w Zeitschritt (s)
    float4 gDebug;               // x Debug-Ansicht (0 aus), y AO auf Direktlicht, zw frei
};

// Konstanten einzelner Nachbearbeitungsschritte
cbuffer PassCB : register(b3)
{
    float4 gPass0;
    float4 gPass1;
};

static const float PI = 3.14159265;

// Flackern eines Raums (identisch zu render/flicker.hpp).
uint hashU(uint x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
float hash01(uint x) { return (float)(hashU(x) >> 8) * (1.0 / 16777216.0); }

float smoothNoise(uint seed, float t)
{
    float i = floor(t);
    float f = t - i;
    uint ii = (uint)(int)i;
    float a = hash01(seed * 7919u + ii);
    float b = hash01(seed * 7919u + ii + 1u);
    float u = f * f * (3.0 - 2.0 * f);
    return lerp(a, b, u);
}

float flickerFactor(uint mode, uint seed, float t)
{
    float phase = (float)seed * 0.2;
    if (mode == 1u)
        return 0.985 + 0.015 * sin(t * 1.6 + phase);
    if (mode == 2u)
    {
        float n = smoothNoise(seed + 101u, t * 1.1);
        float dip = n > 0.62 ? (n - 0.62) / 0.38 : 0.0;
        return (1.0 - 0.38 * dip * dip) * (0.975 + 0.025 * sin(t * 0.8 + phase));
    }
    return 1.0;
}

// V7: Stromausfall (Halluzination). gDebug.w = Seed + 1 + Dunkelheit * 0,9 (0 = keiner)
float blackoutLevel()
{
    return gDebug.w < 0.5 ? 0.0 : saturate((gDebug.w - floor(gDebug.w)) / 0.9);
}
float blackoutFactor(uint seed)
{
    if (gDebug.w < 0.5) return 1.0;
    return abs(floor(gDebug.w) - 1.0 - (float)seed) < 0.5 ? 1.0 - blackoutLevel() : 1.0;
}

// Interleaved Gradient Noise (Jimenez) fuer rauscharme Zufallsmuster.
float ign(float2 p, float frame)
{
    p += frame * 5.588238;
    return frac(52.9829189 * frac(0.06711056 * p.x + 0.00583715 * p.y));
}

float luminance(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

// Abstrahlung einer Leuchte in Richtung -toLight (toLight: Einheitsvektor Punkt -> Leuchte).
//   normale Leuchten: Lambert-artig mit Streulicht zur Seite (dirSpill.w)
//   V7 Spot (Flag 2, Taschenlampe): weicher Kegel zwischen cos aussen (dirSpill.w) und cos innen (coneInner)
static const uint kLightShadow = 1u, kLightSpot = 2u;
float lightEmission(float4 dirSpill, float coneInner, uint flags, float3 toLight)
{
    float c = dot(-toLight, dirSpill.xyz);
    if (flags & kLightSpot)
    {
        // Reflektorlampe: heller Kern, weicher Ring bis zum Aussenrand, ganz schwaches Streulicht
        float ring = smoothstep(dirSpill.w, coneInner, c);
        float hot = smoothstep(coneInner, 1.0 - (1.0 - coneInner) * 0.3, c);
        float spill = smoothstep(dirSpill.w - 0.15, dirSpill.w, c);
        return 0.05 * spill + 0.55 * ring + 0.4 * hot;
    }
    return lerp(dirSpill.w, 1.0, saturate(c));
}

// Weltposition aus Tiefe (umgekehrtes z) und Bildkoordinate [0,1]
float3 worldFromDepth(float2 uv, float depth, float4x4 invVP)
{
    float4 ndc = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    float4 w = mul(invVP, ndc);
    return w.xyz / w.w;
}

float linearDepth(float depth) { return gProj.z / max(depth, 1e-7); }

#endif
