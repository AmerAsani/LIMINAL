// Umgebungsverdeckung im Bildraum (Scalable Ambient Obscurance, vereinfacht)
// plus tiefenbewusste Weichzeichnung. Das Rauschmuster wechselt je Bild und
// wird vom TAA aufgeloest.
#include "common.hlsli"

Texture2D<float> gDepthBuf : register(t0);
Texture2D<float4> gNormalBuf : register(t1);
Texture2D<float> gAoIn : register(t2);
RWTexture2D<float> gAoOut : register(u0);

float3 viewPos(int2 p, float depth)
{
    float z = linearDepth(depth);
    float2 ndc = float2(((float)p.x + 0.5) * gScreen.z * 2.0 - 1.0, 1.0 - ((float)p.y + 0.5) * gScreen.w * 2.0);
    return float3((ndc.x - gJitter.x) * z / gProj.x, (ndc.y - gJitter.y) * z / gProj.y, z);
}

#define SAMPLES 12
static const float RADIUS = 0.55;

[numthreads(8, 8, 1)]
void AoCS(uint3 id : SV_DispatchThreadID)
{
    int2 p = (int2)id.xy;
    if (p.x >= (int)gScreen.x || p.y >= (int)gScreen.y) return;
    float depth = gDepthBuf[p];
    if (depth <= 0.0) { gAoOut[p] = 1.0; return; }
    float3 P = viewPos(p, depth);
    float3 Nw = gNormalBuf[p].xyz * 2.0 - 1.0;
    float3 N = normalize(mul((float3x3)gView, Nw));
    float rPx = RADIUS * gProj.x * 0.5 * gScreen.x / P.z;
    if (rPx < 1.0) { gAoOut[p] = 1.0; return; }
    rPx = min(rPx, 96.0);
    float angle0 = ign((float2)p, gRingParams.z) * 6.2831853;
    float sum = 0.0;
    [unroll]
    for (int i = 0; i < SAMPLES; ++i)
    {
        float t = ((float)i + 0.5) / SAMPLES;
        float ang = angle0 + (float)i * 2.3999632;  // goldener Winkel
        float r = sqrt(t) * rPx;
        int2 q = p + int2(round(float2(cos(ang), sin(ang)) * r));
        if (q.x < 0 || q.y < 0 || q.x >= (int)gScreen.x || q.y >= (int)gScreen.y) continue;
        float dq = gDepthBuf[q];
        if (dq <= 0.0) continue;
        float3 v = viewPos(q, dq) - P;
        float vv = dot(v, v);
        float vn = dot(v, N);
        float fall = max(0.0, 1.0 - vv / (RADIUS * RADIUS));
        sum += fall * max(0.0, vn - 0.015 * P.z) / (vv + 0.01);
    }
    float ao = saturate(1.0 - sum * (2.0 * gLightParams.w / SAMPLES) * 0.6);
    gAoOut[p] = ao * ao;
}

// 4x4-Weichzeichner, der Kanten (Tiefenspruenge) respektiert
[numthreads(8, 8, 1)]
void BlurCS(uint3 id : SV_DispatchThreadID)
{
    int2 p = (int2)id.xy;
    if (p.x >= (int)gScreen.x || p.y >= (int)gScreen.y) return;
    float d0 = gDepthBuf[p];
    if (d0 <= 0.0) { gAoOut[p] = 1.0; return; }
    float z0 = linearDepth(d0);
    float sum = 0.0, wsum = 0.0;
    [unroll] for (int y = -2; y <= 1; ++y)
    [unroll] for (int x = -2; x <= 1; ++x)
    {
        int2 q = clamp(p + int2(x, y), int2(0, 0), int2((int)gScreen.x - 1, (int)gScreen.y - 1));
        float dz = gDepthBuf[q];
        float z = dz > 0.0 ? linearDepth(dz) : 1e6;
        float w = exp(-abs(z - z0) / (0.04 * z0 + 0.02));
        sum += gAoIn[q] * w;
        wsum += w;
    }
    gAoOut[p] = sum / max(wsum, 1e-4);
}
