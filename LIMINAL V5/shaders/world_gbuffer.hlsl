// G-Buffer-Pass fuer die Weltgeometrie (Chunks). Vertex: Position + 32 Bit
// gepackte Daten (Normalenachse, Material, Flackern) - siehe world/chunk.hpp.
#include "gbuffer.hlsli"

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
    return shadeSurface(m, layer, uv, i.wpos, N, T, B, flickMode, flickSeed, extra);
}
