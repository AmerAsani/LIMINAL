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
    float4 gLayer[116];  // V12: 112 Ebenen (Welt 0-90, Wesen ab 96, Mikro ab 107 - V13.2, ab 108 - V13.3, ab 109 - V13.3 Abnahme Level 3; 116 Plaetze). x: Meter je Kachel, y: Normalstaerke, z: Rauheit-Bias
};

StructuredBuffer<GpuMaterial> gMaterials : register(t0);
Texture2DArray gAlbedoTex : register(t1);
Texture2DArray gNormalTex : register(t2);
Texture2DArray gOrmTex : register(t3);   // r: AO, g: Rauheit, b: Metall, a: Hoehe
Texture2D gMacroTex : register(t4);      // grossflaechiges Rauschen (r, g, b)
SamplerState gAniso : register(s0);
SamplerState gLinearWrap : register(s1);
StructuredBuffer<float4> gPuddles : register(t11);  // V13.3: je Pfuetze (x, y, Boden z, Radius), (Streckung, Winkel, Zufall, Hof)

// V13.3: Pfuetzen als Abziehbild auf Boeden. Unregelmaessiger, weich auslaufender Rand ohne Polygonkante, ein nasser, dunklerer
// Hof, eine spiegelglatte Wasserflaeche (Fresnel und Spiegelung kommen aus Beleuchtung und SSR), zur Mitte dunkler (Tiefe) und
// Ringe von Tropfen, die alle paar Sekunden an wechselnden Stellen einschlagen.
void applyPuddles(float3 wpos, float3 N, inout float3 albedo, inout float3 n, inout float rough, inout float metal)
{
    const uint count = (uint)gExtra.x;
    if (count == 0 || N.z < 0.9)
        return;
    float water = 0.0, wet = 0.0, depth = 0.0;
    float2 ripple = 0.0;
    for (uint i = 0; i < count; ++i)
    {
        const float4 a = gPuddles[i * 2], b = gPuddles[i * 2 + 1];
        if (abs(wpos.z - a.z) > 0.12)
            continue;
        const float2 d = wpos.xy - a.xy;
        const float reach = a.w * max(b.w, 1.0) * 1.15;
        if (dot(d, d) > reach * reach)
            continue;
        const float c = cos(-b.y), s = sin(-b.y);
        float2 q = float2(d.x * c - d.y * s, d.x * s + d.y * c);
        q.y /= max(b.x, 0.3);
        const float r = length(q);
        const float ang = atan2(q.y, q.x);
        const float nz = gMacroTex.SampleLevel(gLinearWrap, wpos.xy * 0.45 + b.z * 3.1, 0).r - 0.5;
        const float edge = a.w * (0.80 + 0.11 * sin(ang * 3.0 + b.z * 6.28) + 0.06 * sin(ang * 7.0 + b.z * 17.0) + 0.3 * nz);
        const float w = 1.0 - smoothstep(edge * 0.84, edge, r);
        const float h = 1.0 - smoothstep(edge * 0.95, edge * b.w, r);
        water = max(water, w);
        wet = max(wet, h);
        depth = max(depth, w * saturate(1.0 - r / max(edge, 1e-3)));
        [unroll] for (int k = 0; k < 2; ++k)
        {
            const float period = 2.4 + 1.9 * frac(b.z * 13.7 + k * 0.37);
            const float tt = gCameraPos.w + period * frac(b.z * 7.3 + k * 0.61);
            const float age = fmod(tt, period);
            const uint slot = (uint)(tt / period);
            const uint sd = slot * 7919u + (uint)k * 104729u + (uint)(b.z * 65536.0);
            const float2 at = a.xy + (float2(hash01(sd), hash01(sd ^ 0x9E3779B9u)) - 0.5) * a.w * 0.9;
            const float2 dd = wpos.xy - at;
            const float dr = length(dd);
            const float x = (dr - age * 0.30) * 40.0;
            const float wave = 0.32 * exp(-age * 1.5) * sin(x) * exp(-x * x * 0.015);
            ripple += (dr > 1e-4 ? dd / dr : float2(0.0, 0.0)) * wave * w;
        }
    }
    if (wet <= 0.0)
        return;
    albedo *= lerp(1.0, 0.6, wet);                       // nasser Hof: poroese Flaechen dunkler
    rough = lerp(rough, rough * 0.5, wet);
    albedo *= lerp(1.0, 0.55 - 0.2 * depth, water);      // Wasser: dunkler zur Mitte
    rough = lerp(rough, 0.03, water);
    metal = lerp(metal, 0.0, water);
    n = normalize(lerp(n, normalize(float3(ripple, 1.0)), water));
}

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
    // V13.3: Wasserflaeche (Rauheitsfaktor 0,02 nur bei Wasser): sanfte, wandernde Wellen; der Boden darunter (Textur der
    // Flaeche) wirkt durch die Brechung leicht verzogen
    const bool water = m.roughness < 0.025 && N.z > 0.5;
    // V13.3 (Abnahme Level 37): klare Becken tragen ihre Tiefe (m) im Metall-Zuschlag; 0 = dunkles Meer wie bisher
    const float poolDepth = water ? m.metalness : 0.0;
    float3 waveN = float3(0.0, 0.0, 1.0);
    float blur = 1.0;
    if (water)
    {
        const float t = gCameraPos.w;
        const float2 p1 = wpos.xy * 0.07 + float2(t * 0.006, t * 0.004);
        const float2 p2 = wpos.xy * 0.19 - float2(t * 0.009, -t * 0.005);
        const float e = 0.015;
        const float h0 = gMacroTex.SampleLevel(gLinearWrap, p1, 0).g + 0.45 * gMacroTex.SampleLevel(gLinearWrap, p2, 0).b;
        const float hx = gMacroTex.SampleLevel(gLinearWrap, p1 + float2(e, 0.0), 0).g + 0.45 * gMacroTex.SampleLevel(gLinearWrap, p2 + float2(e, 0.0), 0).b;
        const float hy = gMacroTex.SampleLevel(gLinearWrap, p1 + float2(0.0, e), 0).g + 0.45 * gMacroTex.SampleLevel(gLinearWrap, p2 + float2(0.0, e), 0).b;
        waveN = normalize(float3(-(hx - h0) / e * 0.014, -(hy - h0) / e * 0.014, 1.0));
        if (poolDepth > 0.0)
        {
            // Tropfen: in 60 % der 4-m-Zellen schlaegt alle 7-13 s an zufaelliger Stelle einer ein (Klang "drip" der
            // Ebene); die Ringe laufen mit 0,35 m/s aus und klingen in gut 2 s ab. Sichtbar werden sie wie in echt vor
            // allem in Spiegelung und Glanz (Bogen aus Lichtpunkten). Ausgewertet werden die vier naechsten Zellen.
            const float2 cc = floor(wpos.xy / 4.0 - 0.5);
            float2 rip = 0.0;
            [unroll] for (int k = 0; k < 4; ++k)
            {
                const float2 cell = cc + float2(k & 1, k >> 1);
                const uint cs = (uint)(int)(cell.x + 4096.0) * 73856093u ^ (uint)(int)(cell.y + 4096.0) * 19349663u;
                if (hash01(cs ^ 0x51ED27u) > 0.6)
                    continue;
                const float period = 7.0 + 6.0 * hash01(cs ^ 0x2C1B3Cu);
                const float tt = t + period * hash01(cs ^ 0x7F4A7Cu);
                const float age = fmod(tt, period);
                const uint sd = (uint)(tt / period) * 7919u ^ cs;
                const float2 at = (cell + 0.12 + 0.76 * float2(hash01(sd), hash01(sd ^ 0x9E3779B9u))) * 4.0;
                const float2 dd = wpos.xy - at;
                const float dr = length(dd);
                const float x = (dr - age * 0.35) * 34.0;
                const float wave = 0.45 * exp(-age * 1.6) * sin(x) * exp(-x * x * 0.02) / (1.0 + dr * 2.0);
                rip += (dr > 1e-4 ? dd / dr : float2(0.0, 0.0)) * wave;
            }
            waveN = normalize(waveN + float3(rip, 0.0));
            // Tiefenwirkung: der Boden liegt poolDepth tiefer; der Blickstrahl knickt an der Flaeche ab (Brechzahl 1,33),
            // der Boden erscheint verschoben (Parallaxe) und in der Tiefe weicher
            const float3 v = normalize(wpos - gCameraPos.xyz);
            const float sinI = length(v.xy);
            const float sinT = sinI / 1.333;
            const float tanT = sinT / sqrt(max(1.0 - sinT * sinT, 1e-3));
            uvMeters += (sinI > 1e-4 ? v.xy / sinI : float2(0.0, 0.0)) * (poolDepth * tanT);
            blur = 1.0 + poolDepth * 0.9;
        }
        uvMeters += waveN.xy * (0.12 + 0.08 * poolDepth);
    }
    float4 li = gLayer[layer];
    float2 uv = uvMeters / li.x;
    float2 gx = ddx(uvGradMeters) / li.x * blur, gy = ddy(uvGradMeters) / li.x * blur;
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
        // V13.3 (Abnahme Level 36): gLayer.w = 2 - schmaler Uebergang, vom Helligkeitsunterschied so gefuehrt, dass die
        // dunkleren Koerner beider Versaetze ganz bleiben (die Grenze laeuft um sie herum). Breit ueberblendet zeigte Terrazzo
        // halb durchscheinende, doppelte Koerner und wirkte unscharf; alle anderen Ebenen (w = 1) unveraendert
        const float bw = li.w > 1.5 ? 0.08 : 0.3, bk = li.w > 1.5 ? -0.25 : 0.1;
        const float b = smoothstep(0.5 - bw, 0.5 + bw, f - bk * dot(a1.rgb - a2.rgb, float3(1.0, 1.0, 1.0)));
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
    float metal = saturate(orm.b + m.metalness);
    applyPuddles(wpos, N, albedo, n, rough, metal);  // V13.3
    if (water)
    {
        n = waveN;
        rough = 0.025;
        metal = 0.0;
        orm.r = 1.0;
    }

    uint flickCode = flickMode * 32u + flickSeed;
    GBufferOut o;
    o.albedo = float4(albedo, orm.r);
    o.normal = float4(n * 0.5 + 0.5, 0.0);
    o.misc = float4(rough, metal, (float)flickCode / 255.0, 1.0);
    // V13.3 (Abnahme Level 0): im Stromausfall erlischt die Leuchtflaeche quadratisch - die 3 % Rest des Lichts
    // leuchteten nach der Belichtungsanpassung im dunklen Raum noch weiss (Leuchten "an", Raum dunkel)
    const float bo = blackoutFactorAt(flickSeed, wpos);
    float fl = flickerFactor(flickMode, flickSeed, gCameraPos.w) * bo * bo;
    o.emissive = m.emissive > 0.0 ? m.albedo * alb.rgb * (m.emissive * emissiveScale * fl * gFogParams.w) : 0.0;
    return o;
}

#endif
