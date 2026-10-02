// Abschluss: Belichtung, Bloom, Tonemapping (AgX), Farbgebung, Vignette,
// Filmkorn, roter Rand bei 0 % Sanity (wie V4), Menue-Hintergrund und die
// Retro-Darstellungen aus V4 (Terminal-Halbbloecke, ASCII, Monochrom).
#include "common.hlsli"

Texture2D gHdr : register(t0);
Texture2D gBloom : register(t1);
StructuredBuffer<float> gExposureBuf : register(t2);
Texture2D gBlurred : register(t3);
Texture2D<float> gGlyphs : register(t4);   // 20 Zeichen nebeneinander (Helligkeitsstufen)
SamplerState gLinearClamp : register(s2);
SamplerState gPointClamp : register(s3);

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// --- AgX (Minimal-Variante nach B. Wrensch) ------------------------------------------------
float3 agxContrast(float3 x)
{
    float3 x2 = x * x, x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
float3 agxTonemap(float3 v)
{
    const float3x3 m = float3x3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                                0.0784335999999992, 0.878468636469772, 0.0784336,
                                0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const float3x3 mi = float3x3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                                 -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                                 -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    const float minEv = -12.47393, maxEv = 4.026069;
    v = mul(max(v, 1e-10), m);
    v = (clamp(log2(v), minEv, maxEv) - minEv) / (maxEv - minEv);
    v = saturate(agxContrast(v));
    // Look: etwas mehr Kontrast, leicht zurueckgenommene Saettigung (ausgeblichene Neonroehren)
    float l = luminance(v);
    v = pow(max(v, 0.0), 1.08);
    v = l + 1.08 * (v - l);
    v = mul(v, mi);
    return pow(saturate(v), 2.2);  // linear fuer das sRGB-Ziel
}

float3 toDisplay(float3 lin) { return pow(max(lin, 0.0), 1.0 / 2.2); }
float3 toLinear(float3 d) { return pow(max(d, 0.0), 2.2); }

float3 sceneColor(float2 uv, float exposure)
{
    float3 c;
    float ca = gPost2.w;
    if (ca > 0.0)
    {
        float2 d = (uv - 0.5) * ca * 0.006;
        c.r = gHdr.SampleLevel(gLinearClamp, uv - d, 0).r;
        c.g = gHdr.SampleLevel(gLinearClamp, uv, 0).g;
        c.b = gHdr.SampleLevel(gLinearClamp, uv + d, 0).b;
    }
    else
        c = gHdr.SampleLevel(gLinearClamp, uv, 0).rgb;
    float3 bloom = gBloom.SampleLevel(gLinearClamp, uv, 0).rgb;
    c = lerp(c, bloom, gPost3.w);
    return agxTonemap(c * exposure);
}

// Roter Bildschirmrand (Formel aus V4 renderer.red_edge)
float redMask(float2 uv, float level, float pulse)
{
    if (level < 0.02) return 0.0;
    float L = level;
    float start = 1.12 - 0.78 * L;
    float span = max(0.05, 1.42 - start);
    float2 n = uv * 2.0 - 1.0;
    float s = (sqrt(n.x * n.x * 0.85 + n.y * n.y) - start) / span;
    if (s <= 0.0) return 0.0;
    float q = pow(saturate(s), 1.4) * (0.45 + 0.55 * L);
    return saturate(q + pulse * (2.0 / 12.0) * step(0.001, q));
}

float4 PSMain(PSIn i) : SV_Target
{
    float2 uv = i.uv;
    float2 outSize = gPost3.yz;
    float exposure = gExposureBuf[0] * exp2(gPost2.x);
    uint mode = (uint)gPost3.x;
    float3 col;

    if (mode == 0u)
    {
        col = sceneColor(uv, exposure);
    }
    else
    {
        // Retro-Darstellung: Zeichenzellen wie im Terminal von V4 (Zellen 1:2)
        float cellW = max(4.0, round(outSize.y / 110.0));
        float2 cell = float2(cellW, cellW * 2.0);
        float2 pix = uv * outSize;
        float2 c0 = floor(pix / cell);
        float2 local = frac(pix / cell);
        if (mode == 1u)
        {
            // Halbblock "▀": obere und untere Zellhaelfte mit eigener Farbe, 15-Bit-Farben
            float2 half = float2(0.5, local.y < 0.5 ? 0.25 : 0.75);
            float3 c = sceneColor((c0 + half) * cell / outSize, exposure);
            col = toLinear(floor(toDisplay(c) * 31.0 + 0.5) / 31.0);
        }
        else
        {
            float3 c = sceneColor((c0 + 0.5) * cell / outSize, exposure);
            float lum = luminance(toDisplay(c));
            float levels = mode == 2u ? 8.0 : 10.0;
            float gi = min(levels - 1.0, floor(lum * (mode == 2u ? 9.0 : 13.0)));
            float base = mode == 2u ? 0.0 : 8.0;
            float2 guv = float2((base + gi + local.x) / 20.0, local.y);
            float g = gGlyphs.SampleLevel(gLinearClamp, guv, 0);
            float3 fg = mode == 2u ? c * 1.25 + 0.02 : lum.xxx * float3(0.85, 1.0, 0.85);
            float3 bg = mode == 2u ? c * 0.4 : 0.0;
            col = lerp(bg, fg, g);
        }
    }

    // Farbgebung: minimal warm, Entsaettigung bei schwindendem Verstand
    col *= float3(1.015, 1.0, 0.975);
    float l = luminance(col);
    col = lerp(col, l.xxx, gPost.w);

    // Vignette
    float2 cv = uv - 0.5;
    col *= 1.0 - gPost2.z * pow(saturate(dot(cv, cv) * 2.1), 1.6);

    // Menue: weichgezeichneter, abgedunkelter Hintergrund
    if (gPost.z > 0.001)
    {
        float3 b = agxTonemap(gBlurred.SampleLevel(gLinearClamp, uv, 0).rgb * exposure);
        col = lerp(col, b * 0.32, gPost.z);
    }

    // Roter Rand mit Herzschlag
    float red = redMask(uv, gPost.x, gPost.y);
    if (red > 0.0)
    {
        float3 d = toDisplay(col);
        d = float3(d.r + (0.82 - d.r) * red, d.g * (1.0 - red) + 0.03 * red, d.b * (1.0 - red) + 0.02 * red);
        col = toLinear(d);
    }

    // Filmkorn und Dithering gegen Farbstufen
    float n = ign(i.pos.xy, gRingParams.z) - 0.5;
    col += col * n * gPost2.y * 0.09;
    float3 disp = toDisplay(col) + n / 255.0;
    return float4(toLinear(disp), 1.0);
}
