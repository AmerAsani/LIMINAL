// V11: G-Buffer-Pass der Ausstattung (Leuchtenkoerper, Rohre, Schilder, Fahrzeuge, Moebel ...).
//
// Die Teile eines Chunks liegen in einem Netz (world/chunk.hpp, PropVertex): Material je Vertex.
// Texturen werden wie bei den Waenden aus Weltkoordinaten projiziert (Achse der groessten
// Normalenkomponente, gleiche Tangentenrichtungen wie world_gbuffer.hlsl) - Teile brauchen dafuer
// keine eigenen UVs. Schilder, Beschriftungen und Bilder benutzen den Atlas L_SIGNS (UV im Vertex);
// Schablonenschrift schneidet den Grund weg. Leuchtflaechen flackern und fallen mit ihrem Stromkreis
// aus wie die Leuchten der Raeume.
#include "gbuffer.hlsli"

struct VSIn
{
    float3 pos : POSITION;
    float4 n : NORMAL;
    float2 uv : TEXCOORD0;
    uint data : TEXCOORD1;
};

struct VSOut
{
    float4 svpos : SV_Position;
    float3 wpos : TEXCOORD0;
    float3 n : TEXCOORD1;
    float2 uv : TEXCOORD2;
    nointerpolation uint data : TEXCOORD3;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.svpos = mul(gViewProj, float4(i.pos, 1.0));
    o.wpos = i.pos;
    o.n = i.n.xyz;
    o.uv = i.uv;
    o.data = i.data;
    return o;
}

GBufferOut PSMain(VSOut i, bool front : SV_IsFrontFace)
{
    const uint mat = i.data & 0x3FFFu;
    const uint flickMode = (i.data >> 14) & 3u;
    const uint flickSeed = (i.data >> 16) & 31u;
    const float glow = (float)((i.data >> 21) & 127u) / 63.5;
    GpuMaterial m = gMaterials[mat];
    float3 N = normalize(i.n);
    if (!front)
        N = -N;
    const bool atlas = ((m.layers >> 27) & 1u) != 0u;
    const bool cutout = ((m.layers >> 28) & 1u) != 0u;
    uint layer;
    float2 uvm;
    float3 T, B;
    if (atlas)
    {
        layer = m.layers & 0xFFu;
        uvm = i.uv * gLayer[layer].x;
        // Tangentenrahmen aus den Ableitungen (Atlasflaechen sind eben)
        float3 dp1 = ddx(i.wpos), dp2 = ddy(i.wpos);
        float2 du1 = ddx(i.uv), du2 = ddy(i.uv);
        float3 p2 = cross(dp2, N), p1 = cross(N, dp1);
        T = p2 * du1.x + p1 * du2.x;
        B = p2 * du1.y + p1 * du2.y;
        float inv = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-12));
        T *= inv;
        B *= inv;
    }
    else
    {
        const float3 a = abs(N);
        if (a.z >= a.x && a.z >= a.y)
        {
            layer = N.z > 0.0 ? (m.layers & 0xFFu) : ((m.layers >> 8) & 0xFFu);
            T = float3(1, 0, 0);
            B = N.z > 0.0 ? float3(0, 1, 0) : float3(0, -1, 0);
        }
        else if (a.x >= a.y)
        {
            layer = (m.layers >> 16) & 0xFFu;
            T = N.x > 0.0 ? float3(0, -1, 0) : float3(0, 1, 0);
            B = float3(0, 0, -1);
        }
        else
        {
            layer = (m.layers >> 16) & 0xFFu;
            T = N.y > 0.0 ? float3(1, 0, 0) : float3(-1, 0, 0);
            B = float3(0, 0, -1);
        }
        uvm = float2(dot(i.wpos, T), dot(i.wpos, B));
        // Rahmen auf die tatsaechliche Normale drehen (schraege Flaechen, Rohre)
        T = normalize(T - N * dot(N, T));
        B = normalize(cross(N, T)) * (dot(cross(N, T), B) < 0.0 ? -1.0 : 1.0);
    }
    GBufferOut o = shadeSurface(m, layer, uvm, uvm, i.wpos, N, T, B, flickMode, flickSeed, glow > 0.0 ? glow : 1.0);
    if (cutout)
    {
        // Schablone: Strich hell, Grund dunkel (Albedo ist bereits mit der Materialfarbe getoent)
        float lum = dot(o.albedo.rgb, float3(0.333, 0.333, 0.333));
        float ref = max(dot(m.albedo, float3(0.333, 0.333, 0.333)), 0.05);
        if (lum < ref * 0.45)
            discard;
    }
    o.normal.w = 0.0;
    return o;
}
