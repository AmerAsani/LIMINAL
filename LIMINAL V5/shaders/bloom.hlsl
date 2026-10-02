// Physikalisch plausibles Bloom (Mip-Kette wie in Call of Duty/Unreal):
// 13-Tap-Verkleinerung (erste Stufe mit Karis-Mittelung gegen Glitzern) und
// Zelt-Filter beim Vergroessern, jeweils additiv in die groessere Stufe.
#include "common.hlsli"

Texture2D gSrc : register(t0);
SamplerState gLinearClamp : register(s2);

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float karis(float3 c) { return 1.0 / (1.0 + luminance(c)); }

// gPass0.xy: Texelgroesse der Quelle, gPass0.z: 1 = Karis-Mittelung
float4 DownPS(PSIn i) : SV_Target
{
    float2 t = gPass0.xy;
    float2 uv = i.uv;
    float3 a = gSrc.SampleLevel(gLinearClamp, uv + t * float2(-2, -2), 0).rgb;
    float3 b = gSrc.SampleLevel(gLinearClamp, uv + t * float2(0, -2), 0).rgb;
    float3 c = gSrc.SampleLevel(gLinearClamp, uv + t * float2(2, -2), 0).rgb;
    float3 d = gSrc.SampleLevel(gLinearClamp, uv + t * float2(-2, 0), 0).rgb;
    float3 e = gSrc.SampleLevel(gLinearClamp, uv, 0).rgb;
    float3 f = gSrc.SampleLevel(gLinearClamp, uv + t * float2(2, 0), 0).rgb;
    float3 g = gSrc.SampleLevel(gLinearClamp, uv + t * float2(-2, 2), 0).rgb;
    float3 h = gSrc.SampleLevel(gLinearClamp, uv + t * float2(0, 2), 0).rgb;
    float3 k = gSrc.SampleLevel(gLinearClamp, uv + t * float2(2, 2), 0).rgb;
    float3 l = gSrc.SampleLevel(gLinearClamp, uv + t * float2(-1, -1), 0).rgb;
    float3 m = gSrc.SampleLevel(gLinearClamp, uv + t * float2(1, -1), 0).rgb;
    float3 n = gSrc.SampleLevel(gLinearClamp, uv + t * float2(-1, 1), 0).rgb;
    float3 o = gSrc.SampleLevel(gLinearClamp, uv + t * float2(1, 1), 0).rgb;
    float3 r;
    if (gPass0.z > 0.5)
    {
        float3 g0 = (a + b + d + e) * 0.25, g1 = (b + c + e + f) * 0.25;
        float3 g2 = (d + e + g + h) * 0.25, g3 = (e + f + h + k) * 0.25, g4 = (l + m + n + o) * 0.25;
        float w0 = karis(g0), w1 = karis(g1), w2 = karis(g2), w3 = karis(g3), w4 = karis(g4);
        r = (g0 * w0 * 0.125 + g1 * w1 * 0.125 + g2 * w2 * 0.125 + g3 * w3 * 0.125 + g4 * w4 * 0.5) /
            (w0 * 0.125 + w1 * 0.125 + w2 * 0.125 + w3 * 0.125 + w4 * 0.5);
    }
    else
    {
        r = e * 0.125 + (a + c + g + k) * 0.03125 + (b + d + f + h) * 0.0625 + (l + m + n + o) * 0.125;
    }
    return float4(max(r, 0.0), 1.0);
}

// gPass0.xy: Texelgroesse der Quelle (kleinere Stufe), gPass0.w: Filterradius
float4 UpPS(PSIn i) : SV_Target
{
    float2 t = gPass0.xy * gPass0.w;
    float2 uv = i.uv;
    float3 r = gSrc.SampleLevel(gLinearClamp, uv, 0).rgb * 4.0;
    r += (gSrc.SampleLevel(gLinearClamp, uv + float2(-t.x, 0), 0).rgb + gSrc.SampleLevel(gLinearClamp, uv + float2(t.x, 0), 0).rgb +
          gSrc.SampleLevel(gLinearClamp, uv + float2(0, -t.y), 0).rgb + gSrc.SampleLevel(gLinearClamp, uv + float2(0, t.y), 0).rgb) * 2.0;
    r += gSrc.SampleLevel(gLinearClamp, uv + float2(-t.x, -t.y), 0).rgb + gSrc.SampleLevel(gLinearClamp, uv + float2(t.x, -t.y), 0).rgb +
         gSrc.SampleLevel(gLinearClamp, uv + float2(-t.x, t.y), 0).rgb + gSrc.SampleLevel(gLinearClamp, uv + float2(t.x, t.y), 0).rgb;
    return float4(r / 16.0, 1.0);
}
