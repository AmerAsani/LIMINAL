// Materialsystem und G-Buffer-Ausgabe (gemeinsam fuer Welt- und Objektgeometrie).
#ifndef LIM_GBUFFER_HLSLI
#define LIM_GBUFFER_HLSLI

#include "common.hlsli"

struct GpuMaterial
{
    float3 albedo;      // linear
    float emissive;     // Leuchtstaerke (0 = keine)
    uint layers;        // Boden-Ebene | Decken-Ebene << 8 | Wand-Ebene << 16 | Flags << 24
    float roughness;    // Multiplikator
    float metalness;    // Zuschlag
    float variation;    // Staerke der grossflaechigen Variation (Schmutz, Flecken)
};

cbuffer LayerCB : register(b1)
{
    float4 gLayer[32];  // x: Meter je Texturkachel, y: Normalstaerke, z: Rauheit-Bias, w: 0
};

StructuredBuffer<GpuMaterial> gMaterials : register(t0);
Texture2DArray gAlbedoTex : register(t1);
Texture2DArray gNormalTex : register(t2);
Texture2DArray gOrmTex : register(t3);   // r: AO, g: Rauheit, b: Metall, a: Hoehe
Texture2D gMacroTex : register(t4);      // grossflaechiges Rauschen (r, g, b)
SamplerState gAniso : register(s0);
SamplerState gLinearWrap : register(s1);

struct GBufferOut
{
    float4 albedo : SV_Target0;    // rgb Albedo, a AO
    float4 normal : SV_Target1;    // Normale * 0.5 + 0.5
    float4 misc : SV_Target2;      // r Rauheit, g Metall, b Flackercode, a Glanz
    float3 emissive : SV_Target3;
};

// Fuellt den G-Buffer fuer eine achsenparallele oder beliebige Flaeche.
// uv: Weltkoordinaten in Metern entlang (T, B); layer: Textur-Ebene.
// V6: uvGradMeters ist die unverschobene Koordinate (Parallax): aus ihr kommen die Ableitungen
// fuer die Mip-Wahl, damit verschobene Koordinaten an Kanten nicht flimmern.
GBufferOut shadeSurface(GpuMaterial m, uint layer, float2 uvMeters, float2 uvGradMeters, float3 wpos, float3 N, float3 T,
                        float3 B, uint flickMode, uint flickSeed, float emissiveScale)
{
    float4 li = gLayer[layer];
    float2 uv = uvMeters / li.x;
    float2 gx = ddx(uvGradMeters) / li.x, gy = ddy(uvGradMeters) / li.x;
    float4 alb = gAlbedoTex.SampleGrad(gAniso, float3(uv, layer), gx, gy);
    float2 nxy = gNormalTex.SampleGrad(gAniso, float3(uv, layer), gx, gy).xy * 2.0 - 1.0;
    float4 orm = gOrmTex.SampleGrad(gAniso, float3(uv, layer), gx, gy);
    nxy *= li.y;
    float3 nt = float3(nxy, sqrt(saturate(1.0 - dot(nxy, nxy))));
    float3 n = normalize(T * nt.x + B * nt.y + N * nt.z);

    // Grossflaechige Variation gegen sichtbare Wiederholung: Flecken, Abnutzung
    float3 mac = gMacroTex.Sample(gLinearWrap, wpos.xy * 0.071 + wpos.z * 0.043).rgb;
    float3 mac2 = gMacroTex.Sample(gLinearWrap, wpos.yx * 0.23 + wpos.z * 0.11).rgb;
    float v = m.variation;
    float tint = lerp(1.0, 0.86 + 0.26 * mac.r, v) * lerp(1.0, 0.94 + 0.12 * mac2.g, v);
    float3 albedo = saturate(alb.rgb * m.albedo * tint);
    float rough = saturate(orm.g * m.roughness + li.z + (mac.b - 0.5) * 0.18 * v);

    uint flickCode = flickMode * 32u + flickSeed;
    GBufferOut o;
    o.albedo = float4(albedo, orm.r);
    o.normal = float4(n * 0.5 + 0.5, 0.0);
    o.misc = float4(rough, saturate(orm.b + m.metalness), (float)flickCode / 255.0, 1.0);
    float fl = flickerFactor(flickMode, flickSeed, gCameraPos.w) * blackoutFactor(flickSeed);
    o.emissive = m.emissive > 0.0 ? m.albedo * alb.rgb * (m.emissive * emissiveScale * fl * gFogParams.w) : 0.0;
    return o;
}

#endif
