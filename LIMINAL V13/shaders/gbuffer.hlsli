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
    float4 gLayer[112];  // V12: 112 Ebenen (Welt 0-90, Wesen ab 96, Mikro ab 107 - V13.2). x: Meter je Kachel, y: Normalstaerke, z: Rauheit-Bias
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
    // V13: Kachelbruch (nach I. Quilez, "texture repetition", Variante 3). Ein grossflaechiger Index waehlt je Stelle
    // einen von acht Versaetzen der Textur; zwischen zwei benachbarten wird weich ueberblendet (gesteuert vom
    // Helligkeitsunterschied, damit kein Brei entsteht). Nur fuer Ebenen ohne Raster (gLayer.w = 1: Teppich, Beton,
    // Putz ...); Ziegel, Fliesen, Deckenplatten und Tapetenmuster behalten ihr Raster (gLayer.w = 0, ein Abruf).
    float4 alb, orm;
    float2 nxy;
    [branch] if (li.w > 0.5)
    {
        const float k = gMacroTex.SampleLevel(gLinearWrap, uv * 0.093 + float2(0.37, 0.11), 0).g * 8.0;
        const float ia = floor(k), f = k - ia;
        const float2 oa = frac(sin(float2(3.0, 7.0) * ia) * 43.17), ob = frac(sin(float2(3.0, 7.0) * (ia + 1.0)) * 43.17);
        const float4 a1 = gAlbedoTex.SampleGrad(gAniso, float3(uv + oa, layer), gx, gy);
        const float4 a2 = gAlbedoTex.SampleGrad(gAniso, float3(uv + ob, layer), gx, gy);
        const float b = smoothstep(0.2, 0.8, f - 0.1 * dot(a1.rgb - a2.rgb, float3(1.0, 1.0, 1.0)));
        const float2 n1 = gNormalTex.SampleGrad(gAniso, float3(uv + oa, layer), gx, gy).xy;
        const float2 n2 = gNormalTex.SampleGrad(gAniso, float3(uv + ob, layer), gx, gy).xy;
        alb = lerp(a1, a2, b);
        nxy = lerp(n1, n2, b) * 2.0 - 1.0;
        orm = lerp(gOrmTex.SampleGrad(gAniso, float3(uv + oa, layer), gx, gy), gOrmTex.SampleGrad(gAniso, float3(uv + ob, layer), gx, gy), b);
    }
    else
    {
        alb = gAlbedoTex.SampleGrad(gAniso, float3(uv, layer), gx, gy);
        nxy = gNormalTex.SampleGrad(gAniso, float3(uv, layer), gx, gy).xy * 2.0 - 1.0;
        orm = gOrmTex.SampleGrad(gAniso, float3(uv, layer), gx, gy);
    }
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
    float fl = flickerFactor(flickMode, flickSeed, gCameraPos.w) * blackoutFactorAt(flickSeed, wpos);
    o.emissive = m.emissive > 0.0 ? m.albedo * alb.rgb * (m.emissive * emissiveScale * fl * gFogParams.w) : 0.0;
    return o;
}

#endif
