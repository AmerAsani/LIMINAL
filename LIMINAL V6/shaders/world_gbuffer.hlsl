// G-Buffer-Pass fuer die Weltgeometrie (Chunks). Vertex: Position + 32 Bit
// gepackte Daten (Normalenachse, Material, Flackern) - siehe world/chunk.hpp.
//
// V6:
// - Parallax-Occlusion-Mapping: Die Texturen tragen echte Hoehen (Meter) im Alpha-Kanal der
//   ORM-Textur. In der Naehe folgt ein Strahl im Tangentenraum dem Hoehenfeld - Fugen, Gitter,
//   Mauerwerk und Deckenplatten bekommen echte Tiefe, die sich mit dem Blickwinkel verschiebt.
// - Gebrauchsspuren, weltfest und deterministisch: Wasserflecken mit Raendern auf Decken,
//   Schmutzkante am Wandfuss, Staub in Bodenecken, Abrieb, feuchte Stellen im Teppich,
//   Wasserlaeufe an Waenden. Grundlage sind Boden-/Deckenhoehen aus dem Hoehenfeld-Ring.
//
//   gPass0: x Parallax an, y Parallax-Reichweite (m), z Tiefenmassstab, w Gebrauchsspuren an
#include "gbuffer.hlsli"

Texture2D<float2> gHeightRing : register(t5);  // (Boden, Decke) je Kachel, massiv = Boden > 1000

struct VSIn
{
    float3 pos : POSITION;
    uint data : TEXCOORD0;
};

struct VSOut
{
    float4 svpos : SV_Position;
    float3 wpos : TEXCOORD0;
    nointerpolation uint data : TEXCOORD1;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.svpos = mul(gViewProj, float4(i.pos, 1.0));
    o.wpos = i.pos;
    o.data = i.data;
    return o;
}

// Normale, Tangente (Richtung +u) und Bitangente (+v) je Achse.
// Waende: u laeuft fuer den Betrachter nach rechts, v nach unten.
static const float3 kN[6] = { float3(1,0,0), float3(-1,0,0), float3(0,1,0), float3(0,-1,0), float3(0,0,1), float3(0,0,-1) };
static const float3 kT[6] = { float3(0,-1,0), float3(0,1,0), float3(1,0,0), float3(-1,0,0), float3(1,0,0), float3(1,0,0) };
static const float3 kB[6] = { float3(0,0,-1), float3(0,0,-1), float3(0,0,-1), float3(0,0,-1), float3(0,1,0), float3(0,-1,0) };

// --- Parallax-Occlusion-Mapping ------------------------------------------------------------
// Tiefe unter der Oberkante 0..1 aus der ORM-Hoehe (a = 0.5 + h * 25, h in Metern).
// Trilinear statt anisotrop: die Schrittschleife tastet oft ab, die Hoehe ist weich genug.
float pomDepth(float2 uvm, uint layer, float2 gx, float2 gy)
{
    float a = gOrmTex.SampleGrad(gLinearWrap, float3(uvm / gLayer[layer].x, layer), gx, gy).a;
    return 1.0 - a;
}

float2 parallax(float2 uvm, uint layer, float3 wpos, float3 N, float3 T, float3 B)
{
    float3 toEye = gCameraPos.xyz - wpos;
    float dist = length(toEye);
    float fade = saturate((gPass0.y - dist) / 2.5);
    if (fade <= 0.01) return uvm;
    float3 v = toEye / dist;
    float3 vts = float3(dot(v, T), dot(v, B), dot(v, N));
    if (vts.z <= 0.02) return uvm;
    float li = gLayer[layer].x;
    float2 gx = ddx(uvm) / li, gy = ddy(uvm) / li;
    // nutzbarer Bereich: Tiefe 0.44 (hoechste Erhebungen) bis 1.0 (tiefste Fugen/Loecher)
    const float d0 = 0.44, d1 = 1.0;
    float metersRange = (d1 - d0) * 0.04 * gPass0.z * fade;
    int steps = (int)lerp(gPass1.x, 6.0, vts.z);
    float2 delta = -vts.xy / max(vts.z, 0.12) * (metersRange / (float)steps);
    float stepD = (d1 - d0) / (float)steps;
    float2 cur = uvm;
    float layerD = d0;
    float texD = pomDepth(cur, layer, gx, gy);
    float prevTexD = texD;
    [loop] for (int i = 0; i < steps && layerD < texD; ++i)
    {
        prevTexD = texD;
        cur += delta;
        layerD += stepD;
        texD = pomDepth(cur, layer, gx, gy);
    }
    // lineare Verfeinerung zwischen den beiden letzten Schritten
    float after = texD - layerD;
    float before = prevTexD - (layerD - stepD);
    float w = saturate(after / min(after - before, -1e-5));
    return lerp(cur, cur - delta, w);
}

// --- Gebrauchsspuren ---------------------------------------------------------------------------
float2 ringAt(float2 xy)
{
    int2 c = (int2)floor(xy) & ((int)gRingParams.x - 1);
    return gHeightRing.Load(int3(c, 0));
}

float hashCell(float2 c, float s)
{
    return hash01(asuint(c.x * 73.13 + c.y * 191.7 + s) * 2654435761u + asuint(c.y * 17.0 + s));
}

float macro(float2 p, int ch)
{
    float4 m = gMacroTex.SampleLevel(gLinearWrap, p, 0);
    return ch == 0 ? m.r : (ch == 1 ? m.g : m.b);
}

void weather(inout GBufferOut o, GpuMaterial m, uint axis, uint layer, float3 wpos, float3 N, float3 T)
{
    float strength = saturate(m.variation * 1.6);
    if (strength <= 0.0 || m.emissive > 0.0) return;
    float3 alb = o.albedo.rgb;
    float rough = o.misc.r;
    if (axis == 5u)
    {
        // Decke: einzelne Wasserflecken - unregelmaessiger Fleck je 3-m-Zelle (selten), innen leicht
        // vergilbt, am Rand eine dunklere braune Wasserkante, darin ein, zwei aeltere Raender
        float2 c = floor(wpos.xy / 3.0);
        if (hashCell(c, 3.0) < 0.2)
        {
            float2 ctr = c * 3.0 + 0.6 + 1.8 * float2(hashCell(c, 4.0), hashCell(c, 5.0));
            float rad = 0.45 + 0.8 * hashCell(c, 6.0);
            float2 d = wpos.xy - ctr;
            float wob = macro(wpos.xy * 0.21 + c * 0.37, 0) - 0.5;
            float fine = macro(wpos.xy * 0.9 + c * 0.11, 1) - 0.5;
            float shape = 1.0 - length(d) / rad + wob * 0.9 + fine * 0.12;  // > 0 innen
            float inner = smoothstep(0.0, 0.25, shape);
            float rim = smoothstep(-0.035, 0.0, shape) * (1.0 - smoothstep(0.0, 0.05, shape));
            float rim2 = smoothstep(0.3, 0.33, shape) * (1.0 - smoothstep(0.33, 0.37, shape));
            alb *= lerp(1.0.xxx, float3(0.9, 0.84, 0.7), inner * 0.5 * strength);
            alb *= lerp(1.0.xxx, float3(0.66, 0.54, 0.38), saturate(rim * 0.75 + rim2 * 0.35) * strength);
        }
        // leichter Staub/Rauch in Wandnaehe
        float2 r0 = ringAt(wpos.xy);
        float near = 1.0;
        [unroll] for (int k = 0; k < 4; ++k)
        {
            float2 dir = k == 0 ? float2(1, 0) : (k == 1 ? float2(-1, 0) : (k == 2 ? float2(0, 1) : float2(0, -1)));
            float2 nb = ringAt(wpos.xy + dir);
            if (nb.x > 1000.0 || nb.y < r0.y - 0.3)
            {
                float edge = dot(frac(wpos.xy) - 0.5, dir) + 0.5;  // 0 .. 1 Richtung Nachbar
                near = min(near, 1.0 - edge);
            }
        }
        alb *= 1.0 - 0.14 * (1.0 - smoothstep(0.0, 0.45, near)) * strength;
    }
    else if (axis == 4u)
    {
        // Boden: Staub und Schmutz an Waenden und Einbauten
        float2 r0 = ringAt(wpos.xy);
        float near = 1.0;
        [unroll] for (int k = 0; k < 4; ++k)
        {
            float2 dir = k == 0 ? float2(1, 0) : (k == 1 ? float2(-1, 0) : (k == 2 ? float2(0, 1) : float2(0, -1)));
            float2 nb = ringAt(wpos.xy + dir);
            if (nb.x > 1000.0 || nb.x > r0.x + 0.25)
            {
                float edge = dot(frac(wpos.xy) - 0.5, dir) + 0.5;
                near = min(near, 1.0 - edge);
            }
        }
        float n = macro(wpos.xy * 0.9, 2);
        float dirt = (1.0 - smoothstep(0.0, 0.32, near)) * (0.55 + 0.45 * n);
        alb = lerp(alb, alb * float3(0.78, 0.74, 0.68), dirt * 0.6 * strength);
        rough = saturate(rough + 0.12 * dirt);
        // Teppich: feuchte, dunkle Stellen mit leichtem Glanz
        if (layer == 1u)
        {
            float2 c = floor(wpos.xy / 4.0);
            if (hashCell(c, 7.0) < 0.22)
            {
                float wn = macro(wpos.xy * 0.16 + c * 0.53, 1);
                float wet = smoothstep(0.62, 0.68, wn);
                alb *= 1.0 - 0.32 * wet * strength;
                rough = lerp(rough, rough * 0.55, wet * strength);
            }
        }
        // Abrieb: begangene Flaechen abseits der Waende glatter
        float worn = smoothstep(0.55, 0.75, macro(wpos.xy * 0.07, 0)) * smoothstep(0.3, 0.6, near);
        rough = saturate(rough - 0.07 * worn * strength);
    }
    else
    {
        // Wand: Hoehe ueber dem Boden davor (Kachel auf der Seite der Normalen)
        float2 r = ringAt(wpos.xy + N.xy * 0.5);
        if (r.x > 1000.0) return;
        float hf = wpos.z - r.x, hc = r.y - wpos.z;
        float u = dot(wpos, T);
        float n = macro(float2(u * 0.6, wpos.z * 0.6), 2);
        // Schmutzkante am Wandfuss
        float foot = (1.0 - smoothstep(0.02, 0.32, hf)) * (0.6 + 0.4 * n);
        alb *= 1.0 - 0.24 * foot * strength;
        rough = saturate(rough + 0.1 * foot);
        // Schrammen und Abrieb auf Hueft- bis Schulterhoehe (waagrecht gestreckt)
        float band = smoothstep(0.25, 0.4, hf) * (1.0 - smoothstep(1.1, 1.3, hf));
        float sc = smoothstep(0.74, 0.8, macro(float2(u * 0.9, wpos.z * 7.0), 1));
        alb *= 1.0 - 0.1 * sc * band * strength;
        // Russ/Staub unter der Decke
        alb *= 1.0 - 0.1 * (1.0 - smoothstep(0.0, 0.5, hc)) * strength;
        // Wasserlaeufe: seltene, schmale braune Spuren von der Decke abwaerts
        float col = floor(u * 2.5);
        float2 wc = float2(col, floor(dot(wpos, N) + 0.5) + (float)axis * 4096.0);
        if (hashCell(wc, 11.0) < 0.05)
        {
            float x = frac(u * 2.5) - 0.5 + (macro(float2(u * 0.3, wpos.z * 0.35), 1) - 0.5) * 0.25;
            float len = 0.6 + 1.6 * hashCell(wc, 13.0);
            float along = 1.0 - smoothstep(0.0, len, hc);
            float wid = 0.12 + 0.1 * macro(float2(u, wpos.z * 0.5), 0);
            float streak = (1.0 - smoothstep(wid * 0.4, wid, abs(x))) * along;
            alb *= lerp(1.0.xxx, float3(0.8, 0.7, 0.55), streak * 0.6 * strength);
        }
    }
    o.albedo.rgb = alb;
    o.misc.r = rough;
}

GBufferOut PSMain(VSOut i)
{
    uint axis = i.data & 7u;
    uint mat = (i.data >> 3) & 0x3FFFu;
    uint flickMode = (i.data >> 17) & 3u;
    uint flickSeed = (i.data >> 19) & 31u;
    float extra = (float)(i.data >> 24) / 255.0;

    GpuMaterial m = gMaterials[mat];
    float3 N = kN[axis], T = kT[axis], B = kB[axis];
    uint layer;
    if (axis == 4u) layer = m.layers & 0xFFu;
    else if (axis == 5u) layer = (m.layers >> 8) & 0xFFu;
    else layer = (m.layers >> 16) & 0xFFu;
    float2 uv = float2(dot(i.wpos, T), dot(i.wpos, B));
    float2 uvP = (gPass0.x > 0.0 && m.emissive <= 0.0) ? parallax(uv, layer, i.wpos, N, T, B) : uv;
    GBufferOut o = shadeSurface(m, layer, uvP, uv, i.wpos, N, T, B, flickMode, flickSeed, extra);
    if (gPass0.w > 0.0) weather(o, m, axis, layer, i.wpos, N, T);
    return o;
}
