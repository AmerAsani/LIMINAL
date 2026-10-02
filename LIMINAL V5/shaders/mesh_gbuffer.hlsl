// G-Buffer-Pass fuer frei platzierte Objekte (Items, Requisiten).
#include "gbuffer.hlsli"

cbuffer ObjectCB : register(b2)
{
    float4x4 gWorld;
    uint gMaterial;
    float gHighlight;   // Hervorhebung (Ziel zum Aufnehmen), 0..1
    float2 gPad;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;  // xyz Tangente, w Vorzeichen der Bitangente
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 svpos : SV_Position;
    float3 wpos : TEXCOORD0;
    float3 n : TEXCOORD1;
    float4 t : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 w = mul(gWorld, float4(i.pos, 1.0));
    o.svpos = mul(gViewProj, w);
    o.wpos = w.xyz;
    o.n = mul((float3x3)gWorld, i.normal);
    o.t = float4(mul((float3x3)gWorld, i.tangent.xyz), i.tangent.w);
    o.uv = i.uv;
    return o;
}

GBufferOut PSMain(VSOut i, bool front : SV_IsFrontFace)
{
    GpuMaterial m = gMaterials[gMaterial];
    float3 N = normalize(i.n);
    float3 T = normalize(i.t.xyz - N * dot(i.t.xyz, N));
    float3 B = cross(N, T) * i.t.w;
    uint layer = m.layers & 0xFFu;
    // Objekt-UVs sind bereits in Textureinheiten: Ebene mit Massstab 1 verwenden
    GBufferOut o = shadeSurface(m, layer, i.uv * gLayer[layer].x, i.wpos, N, T, B, 0u, 0u, 1.0);
    // sanftes Aufleuchten, wenn das Item anvisiert wird
    o.emissive += o.albedo.rgb * gHighlight * 0.9;
    return o;
}
