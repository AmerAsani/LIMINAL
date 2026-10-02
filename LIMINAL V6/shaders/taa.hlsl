// Temporales Anti-Aliasing.
// Die Welt ist statisch; die Rueckprojektion ergibt sich daher allein aus der
// Tiefe und der Kamerabewegung. Nachbarschafts-Clipping (YCoCg, Varianz)
// verhindert Nachziehen, Catmull-Rom haelt das Bild scharf.
// V6: Pixel des eigenen Koerpers (im Normalenpuffer markiert) bewegen sich mit dem
// Spieler - fuer sie wird die Bewegung des Spielers mit herausgerechnet.
#include "common.hlsli"

Texture2D<float4> gCurrent : register(t0);
Texture2D<float4> gHistory : register(t1);
Texture2D<float> gDepthBuf : register(t2);
Texture2D<float4> gNormalBuf : register(t3);
RWTexture2D<float4> gOutTex : register(u0);
SamplerState gLinearClamp : register(s2);

float3 toYCoCg(float3 c) { return float3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b); }
float3 fromYCoCg(float3 c) { return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }
// Helligkeitsgewichtung gegen Flimmern heller Punkte
float3 compress(float3 c) { return c / (1.0 + luminance(c)); }
float3 expand(float3 c) { return c / max(1.0 - luminance(c), 1e-4); }

float3 sampleCatmullRom(float2 uv)
{
    float2 size = gScreen.xy;
    float2 pos = uv * size;
    float2 tc = floor(pos - 0.5) + 0.5;
    float2 f = pos - tc;
    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);
    float2 w12 = w1 + w2;
    float2 tc0 = (tc - 1.0) * gScreen.zw;
    float2 tc3 = (tc + 2.0) * gScreen.zw;
    float2 tc12 = (tc + w2 / w12) * gScreen.zw;
    float3 r = 0;
    r += gHistory.SampleLevel(gLinearClamp, float2(tc12.x, tc0.y), 0).rgb * (w12.x * w0.y);
    r += gHistory.SampleLevel(gLinearClamp, float2(tc0.x, tc12.y), 0).rgb * (w0.x * w12.y);
    r += gHistory.SampleLevel(gLinearClamp, float2(tc12.x, tc12.y), 0).rgb * (w12.x * w12.y);
    r += gHistory.SampleLevel(gLinearClamp, float2(tc3.x, tc12.y), 0).rgb * (w3.x * w12.y);
    r += gHistory.SampleLevel(gLinearClamp, float2(tc12.x, tc3.y), 0).rgb * (w12.x * w3.y);
    float wsum = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return max(r / wsum, 0.0);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    int2 p = (int2)id.xy;
    if (p.x >= (int)gScreen.x || p.y >= (int)gScreen.y) return;
    int2 maxP = int2((int)gScreen.x - 1, (int)gScreen.y - 1);
    float3 cur = compress(gCurrent[p].rgb);

    float3 m1 = 0, m2 = 0;
    float nearest = 0.0;
    int2 nearestP = p;
    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
    {
        int2 q = clamp(p + int2(x, y), int2(0, 0), maxP);
        float3 c = toYCoCg(compress(gCurrent[q].rgb));
        m1 += c;
        m2 += c * c;
        float dq = gDepthBuf[q];
        if (dq > nearest) { nearest = dq; nearestP = q; }  // umgekehrtes z: groesster Wert = naechster Punkt
    }
    bool attached = gNormalBuf[nearestP].w > 0.5;

    if (gTaa.x > 0.5)
    {
        gOutTex[p] = float4(expand(cur), 1.0);
        return;
    }

    float2 uv = ((float2)p + 0.5) * gScreen.zw;
    float2 uvNoJitter = uv - float2(gJitter.x, -gJitter.y) * 0.5;
    float3 history;
    bool valid;
    if (nearest > 0.0)
    {
        float3 w = worldFromDepth(uvNoJitter, nearest, gInvViewProjNoJitter);
        if (attached) w = mul(gAttachedReproject, float4(w, 1.0)).xyz;
        float4 pc = mul(gPrevViewProjNoJitter, float4(w, 1.0));
        float2 prevUv = float2(pc.x / pc.w * 0.5 + 0.5, 0.5 - pc.y / pc.w * 0.5);
        valid = pc.w > 0.0 && all(prevUv >= 0.0) && all(prevUv <= 1.0);
        history = valid ? compress(sampleCatmullRom(prevUv)) : cur;
    }
    else
    {
        history = compress(gHistory.SampleLevel(gLinearClamp, uv, 0).rgb);
        valid = true;
    }

    // Varianz-Clipping der Historie auf die aktuelle Nachbarschaft
    float3 mu = m1 / 9.0;
    float3 sigma = sqrt(max(m2 / 9.0 - mu * mu, 0.0));
    float3 lo = mu - 1.25 * sigma, hi = mu + 1.25 * sigma;
    float3 hy = toYCoCg(history);
    float3 center = 0.5 * (hi + lo), extent = 0.5 * (hi - lo) + 1e-5;
    float3 off = hy - center;
    float3 ts = abs(off / extent);
    float tmax = max(ts.x, max(ts.y, ts.z));
    if (tmax > 1.0) hy = center + off / tmax;
    history = fromYCoCg(hy);

    // Koerper: Glieder bewegen sich auch untereinander -> Verlauf etwas schneller erneuern
    float blend = valid ? (attached ? 0.22 : gTaa.y) : 1.0;
    float3 res = lerp(history, cur, blend);
    gOutTex[p] = float4(expand(max(res, 0.0)), 1.0);
}
