// V6: Staub in der Luft - feine Schwebeteilchen, die im Licht der Lampen aufleuchten.
//
// Die Teilchen liegen in einem Wuerfel (Kantenlaenge E) um die Kamera und wiederholen
// sich darin: Jedes hat eine feste Weltposition (modulo E) und treibt langsam, beim Gehen
// wandern sie also natuerlich vorbei statt mitzulaufen. Beleuchtet werden sie von den
// naechsten Leuchten (Liste der Beleuchtung, nach Abstand sortiert) und vom vorberechneten
// Umgebungslicht. Gezeichnet als winzige, weiche Scheiben, additiv in das HDR-Bild vor
// Spiegelungen und TAA; der Tiefentest verdeckt Teilchen hinter Waenden.
//
//   gPass0: x Anzahl, y Kantenlaenge E (m), z Helligkeit, w frei
//   gPass1: x Anzahl der beruecksichtigten Leuchten
#include "common.hlsli"

struct GpuLight
{
    float4 posRange;
    float4 colorRadius;
    float4 dirSpill;
    float4 viewPosFlags;
    float4 areaU;
    float4 areaV;
};

StructuredBuffer<GpuLight> gLights : register(t0);
Texture2D<float4> gAmbient : register(t1);
SamplerState gLinearWrap : register(s1);

struct VSOut
{
    float4 pos : SV_Position;
    float2 local : TEXCOORD0;
    float3 color : TEXCOORD1;
};

static const float2 kCorner[6] = { float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, -1), float2(1, 1), float2(-1, 1) };

VSOut VSMain(uint vid : SV_VertexID)
{
    VSOut o;
    uint i = vid / 6u;
    float2 cr = kCorner[vid % 6u];
    float E = gPass0.y;
    float t = gCameraPos.w;
    float3 cam = gCameraPos.xyz;

    // feste Lage je Teilchen plus langsames Treiben (Luftzug) und leichtes Absinken
    float3 base = float3(hash01(i * 3u + 1u), hash01(i * 3u + 2u), hash01(i * 3u + 3u)) * E;
    float ph = hash01(i * 7u + 11u) * 6.2831853;
    float3 drift = float3(sin(t * 0.21 + ph), cos(t * 0.17 + ph * 1.3), sin(t * 0.13 + ph * 0.7)) * 0.35;
    drift.z -= t * 0.012;
    float3 rel = frac((base + drift - cam) / E) - 0.5;  // -0.5 .. 0.5 um die Kamera
    float3 p = cam + rel * E;

    // Ausblenden: am Rand des Wuerfels (kein Aufploppen), sehr nah an der Kamera, in der Ferne
    float3 edge = 0.5 - abs(rel);
    float fade = saturate(min(edge.x, min(edge.y, edge.z)) * E / 0.6);
    float d = length(p - cam);
    fade *= saturate((d - 0.25) / 0.5) * saturate((E * 0.48 - d) / 1.0);

    // Licht der naechsten Leuchten (ohne Schatten; hinter Waenden verdeckt der Tiefentest)
    float3 L = 0.0;
    uint n = min((uint)gPass1.x, 24u);
    [loop] for (uint li = 0; li < n; ++li)
    {
        GpuLight g = gLights[li];
        float3 tl = g.posRange.xyz - p;
        float d2 = dot(tl, tl);
        float r = g.posRange.w;
        if (d2 >= r * r) continue;
        float x = sqrt(d2) / r;
        float win = saturate(1.0 - x * x * x * x);
        float emit = lerp(g.dirSpill.w, 1.0, saturate(dot(-tl * rsqrt(max(d2, 1e-6)), g.dirSpill.xyz)));
        L += g.colorRadius.rgb * (win * win / (d2 + 0.25 + g.areaU.w)) * emit;
    }
    float3 amb = gAmbient.SampleLevel(gLinearWrap, (p.xy + 0.5) * gRingParams.y, 0).rgb * gFogParams.z;
    // jedes Teilchen glitzert in eigenem Takt (dreht seine Flaeche zum Licht)
    float tw = 0.35 + 0.65 * pow(0.5 + 0.5 * sin(t * (0.6 + 1.4 * hash01(i * 5u + 7u)) + ph * 3.0), 3.0);

    // Groesse: wenige Millimeter, aber mindestens gut ein Pixel (dann entsprechend dunkler)
    float4 c = mul(gViewProj, float4(p, 1.0));
    float radius = 0.0016 + 0.0016 * hash01(i * 13u + 5u);
    float px = radius * gProj.y * gScreen.y * 0.5 / max(c.w, 1e-3);
    float pxClamped = max(px, 1.25);
    float energy = (px * px) / (pxClamped * pxClamped);
    o.pos = c;
    o.pos.xy += cr * pxClamped * 2.0 * gScreen.zw * c.w;
    o.local = cr;
    o.color = (L * 0.6 + amb * 0.4) * gPass0.z * fade * tw * energy;
    if (c.w <= 0.05 || fade <= 0.0)
        o.pos = float4(0, 0, -1, 1);  // ausserhalb des Bildes verwerfen
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float a = saturate(1.0 - dot(i.local, i.local));
    return float4(i.color * (a * a), 0.0);
}
