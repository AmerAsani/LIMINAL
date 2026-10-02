// Automatische Belichtung: mittlere logarithmische Helligkeit (Mitte gewichtet)
// und weiche Anpassung wie ein Auge, das sich an Hell und Dunkel gewoehnt.
// Ergebnis: gExposure[0] = aktueller Belichtungsfaktor, [1] = mittlere Helligkeit.
#include "common.hlsli"

Texture2D<float4> gHdr : register(t0);
RWStructuredBuffer<float> gExposure : register(u0);

groupshared float gsSum[1024];
groupshared float gsW[1024];

// gPass0: x = Mindest-EV, y = Hoechst-EV, z = Anpassungsgeschwindigkeit, w = Anteil der Automatik
// gPass1: x = feste Belichtung, y = Zielhelligkeit
[numthreads(32, 32, 1)]
void CSMain(uint3 tid : SV_GroupThreadID, uint gi : SV_GroupIndex)
{
    float sum = 0.0, wsum = 0.0;
    [unroll] for (int k = 0; k < 4; ++k)
    {
        float2 g = (float2)(tid.xy * 2 + uint2(k & 1, k >> 1)) + 0.5;
        float2 uv = g / 64.0;
        int2 px = (int2)(uv * gScreen.xy);
        float l = luminance(gHdr[px].rgb);
        float2 c = uv - 0.5;
        float w = 1.0 - saturate(dot(c, c) * 2.2);  // Bildmitte zaehlt mehr
        sum += log2(max(l, 1e-4)) * w;
        wsum += w;
    }
    gsSum[gi] = sum;
    gsW[gi] = wsum;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = 512; s > 0; s >>= 1)
    {
        if (gi < s)
        {
            gsSum[gi] += gsSum[gi + s];
            gsW[gi] += gsW[gi + s];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (gi == 0)
    {
        float avgLog = gsSum[0] / max(gsW[0], 1e-4);
        float avg = exp2(avgLog);
        float autoExp = gPass1.y / max(avg, 1e-4);
        float ev = clamp(log2(autoExp), gPass0.x, gPass0.y);
        float target = exp2(lerp(log2(gPass1.x), ev, gPass0.w));
        float prev = gExposure[0];
        if (!(prev > 0.0) || gTaa.x > 0.5)
            prev = target;
        float k = 1.0 - exp(-gTaa.w * gPass0.z);
        gExposure[0] = exp2(lerp(log2(prev), log2(target), k));
        gExposure[1] = avg;
    }
}
