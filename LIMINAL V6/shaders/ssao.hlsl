// V6: Umgebungsverdeckung und indirektes Licht im Bildraum.
//
// GTAO (Ground-Truth Ambient Occlusion, Jimenez et al. 2016): je Pixel werden in zwei
// gedrehten Schnittebenen die Horizonte zu beiden Seiten gesucht und die kosinusgewichtete
// Sichtbarkeit der Hemisphaere analytisch integriert - physikalisch fundiert statt einer
// Zaehlung von Stichproben. Dazu ein Licht-Bounce (Screen-Space Indirect Lighting): Jede
// Stichprobe, die den Horizont anhebt, gibt das Licht ihrer Oberflaeche aus dem letzten Bild
// (TAA-Verlauf, rueckprojiziert) weiter - Waende faerben Boeden, Lichtkegel hellen Ecken auf.
//
// Rechnet je nach Qualitaet in halber oder voller Aufloesung; die Beleuchtung skaliert
// tiefenbewusst hoch. Das Rauschmuster wechselt je Bild, das TAA mittelt es.
//
//   gPass0: Breite, Hoehe, 1/Breite, 1/Hoehe (AO-Aufloesung)
//   gPass1: x Teiler (1 oder 2), y Schritte je Seite, z Staerke indirektes Licht, w Radius (m)
#include "common.hlsli"

Texture2D<float> gDepthBuf : register(t0);
Texture2D<float4> gNormalBuf : register(t1);
Texture2D<float4> gHistory : register(t2);    // letztes Bild (HDR, linear)
StructuredBuffer<float> gExposureBuf : register(t3);
Texture2D<float4> gAoIn : register(t4);       // Weichzeichner: Rohwerte
Texture2D<float> gAoDepthIn : register(t5);   // Weichzeichner: lineare Tiefe
RWTexture2D<float4> gAoOut : register(u0);    // rgb indirektes Licht, a Sichtbarkeit
RWTexture2D<float> gAoDepthOut : register(u1);
SamplerState gLinearClamp : register(s2);

static const float HALF_PI = 1.5707963;

float3 viewPosFull(int2 q, float depth)
{
    float z = linearDepth(depth);
    float2 ndc = float2(((float)q.x + 0.5) * gScreen.z * 2.0 - 1.0, 1.0 - ((float)q.y + 0.5) * gScreen.w * 2.0);
    return float3((ndc.x - gJitter.x) * z / gProj.x, (ndc.y - gJitter.y) * z / gProj.y, z);
}

float3 viewAt(int2 q, int2 maxF)
{
    q = clamp(q, int2(0, 0), maxF);
    float d = gDepthBuf[q];
    return d > 0.0 ? viewPosFull(q, d) : float3(0.0, 0.0, 1e6);
}

// Geometrische Normale aus dem Tiefenpuffer (je Achse der Nachbar mit kleinerem Tiefensprung).
// Die G-Buffer-Normale enthaelt Texturdetails - mit ihr laege die eigene Flaeche teils ueber
// dem Horizont (Selbstverdeckung, Selbstbeleuchtung).
float3 geometricNormal(int2 fp, float3 P, int2 maxF)
{
    float3 pr = viewAt(fp + int2(1, 0), maxF), pl = viewAt(fp - int2(1, 0), maxF);
    float3 pd = viewAt(fp + int2(0, 1), maxF), pu = viewAt(fp - int2(0, 1), maxF);
    float3 dx = abs(pr.z - P.z) < abs(P.z - pl.z) ? pr - P : P - pl;
    float3 dy = abs(pd.z - P.z) < abs(P.z - pu.z) ? pd - P : P - pu;
    float3 n = normalize(cross(dy, dx));
    return dot(n, -P) < 0.0 ? -n : n;
}

float fastAcos(float x)
{
    float ax = abs(x);
    float r = (-0.156583 * ax + HALF_PI) * sqrt(saturate(1.0 - ax));
    return x >= 0.0 ? r : PI - r;
}

[numthreads(8, 8, 1)]
void AoCS(uint3 id : SV_DispatchThreadID)
{
    int2 p = (int2)id.xy;
    if (p.x >= (int)gPass0.x || p.y >= (int)gPass0.y) return;
    int scale = (int)gPass1.x;
    int2 maxF = int2((int)gScreen.x - 1, (int)gScreen.y - 1);
    int2 fp = min(p * scale + (scale >> 1), maxF);
    float depth = gDepthBuf[fp];
    if (depth <= 0.0)
    {
        gAoOut[p] = float4(0, 0, 0, 1);
        gAoDepthOut[p] = 1e6;
        return;
    }
    float3 P = viewPosFull(fp, depth);
    gAoDepthOut[p] = P.z;
    float3 Nd = normalize(mul((float3x3)gView, gNormalBuf[fp].xyz * 2.0 - 1.0));
    float3 N = geometricNormal(fp, P, maxF);
    // an Kanten (Tiefensprung) ist die geometrische Normale unzuverlaessig: dann G-Buffer
    if (dot(N, Nd) < 0.5) N = Nd;
    float3 V = normalize(-P);

    const float R = gPass1.w;
    float rPx = R * gProj.x * 0.5 * gScreen.x / P.z;  // Radius in vollen Pixeln
    if (rPx < 1.5)
    {
        gAoOut[p] = float4(0, 0, 0, 1);
        return;
    }
    rPx = min(rPx, 160.0);
    const int steps = (int)gPass1.y;
    float noise = ign((float2)p, gRingParams.z);
    float noise2 = ign((float2)p.yx + 31.0, gRingParams.z + 11.0);
    float exposure = gExposureBuf[0];
    float giOn = gPass1.z;

    float vis = 0.0;
    float3 gi = 0.0;
    const int dirs = 2;
    [unroll] for (int d = 0; d < dirs; ++d)
    {
        float phi = (noise + (float)d / dirs) * PI;
        float2 dir = float2(cos(phi), sin(phi));
        // Schnittebene im Sichtraum (Bildschirm-y zeigt nach unten, Sicht-y nach oben)
        float3 dir3 = float3(dir.x, -dir.y, 0.0);
        float3 ortho = dir3 - V * dot(dir3, V);
        float3 axis = normalize(cross(ortho, V));
        float3 Np = N - axis * dot(N, axis);
        float npLen = max(length(Np), 1e-4);
        float cosN = saturate(dot(Np, V) / npLen);
        // Winkel der projizierten Normalen zur Blickrichtung, positiv zur Seite +dir
        float n = (dot(Np, ortho) >= 0.0 ? 1.0 : -1.0) * fastAcos(cosN);

        float h[2];
        [unroll] for (int side = 0; side < 2; ++side)
        {
            float sgn = side == 0 ? 1.0 : -1.0;
            float lowCos = cos(n + sgn * HALF_PI);  // Horizont ohne Verdeckung (Rand der Normalen-Hemisphaere)
            float maxCos = lowCos;
            [loop] for (int s = 0; s < steps; ++s)
            {
                float t = ((float)s + noise2) / (float)steps;
                t = t * t;  // dichter nahe am Pixel
                float2 off = dir * sgn * max(t * rPx, 1.0 + (float)s);
                int2 q = clamp(fp + int2(round(off)), int2(0, 0), maxF);
                float dq = gDepthBuf[q];
                if (dq <= 0.0) continue;
                float3 S = viewPosFull(q, dq);
                float3 dS = S - P;
                float len2 = dot(dS, dS);
                float invLen = rsqrt(max(len2, 1e-6));
                float w = saturate(1.0 - len2 / (R * R));
                float c = lerp(lowCos, dot(dS, V) * invLen, w);
                if (c > maxCos)
                {
                    // Licht dieser Oberflaeche aus dem letzten Bild (nur was den Horizont hebt, ist sichtbar)
                    if (giOn > 0.0)
                    {
                        float3 wS = worldFromDepth(((float2)q + 0.5) * gScreen.zw, dq, gInvViewProj);
                        float4 pc = mul(gPrevViewProjNoJitter, float4(wS, 1.0));
                        float2 puv = float2(pc.x / pc.w * 0.5 + 0.5, 0.5 - pc.y / pc.w * 0.5);
                        if (pc.w > 0.0 && all(puv > 0.0) && all(puv < 1.0))
                        {
                            float3 L = gHistory.SampleLevel(gLinearClamp, puv, 0).rgb;
                            // sehr Helles (Leuchten) begrenzen: deren Licht rechnet die direkte Beleuchtung
                            float lum = luminance(L) * exposure;
                            L *= min(1.0, 1.5 / max(lum, 1e-4));
                            float cosN2 = saturate(dot(N, dS * invLen));
                            gi += L * ((c - maxCos) * cosN2 * w);
                        }
                    }
                    maxCos = c;
                }
            }
            h[side] = n + clamp(sgn * fastAcos(maxCos) - n, -HALF_PI, HALF_PI);
        }
        // kosinusgewichtete Sichtbarkeit der Schnittebene (analytisch)
        float a0 = 0.25 * (-cos(2.0 * h[0] - n) + cosN + 2.0 * h[0] * sin(n));
        float a1 = 0.25 * (-cos(2.0 * h[1] - n) + cosN + 2.0 * h[1] * sin(n));
        vis += npLen * (a0 + a1);
    }
    // Kontrast wie ueblich bei GTAO leicht anheben (physikalisch ist die Verdeckung in
    // Innenraeumen schwach, Ecken sollen sich aber deutlich absetzen)
    vis = pow(saturate(vis / dirs), 1.8);
    gi = gi * (giOn / dirs);
    gAoOut[p] = float4(gi, vis);
}

// Tiefenbewusster 4x4-Weichzeichner fuer Sichtbarkeit und indirektes Licht
[numthreads(8, 8, 1)]
void BlurCS(uint3 id : SV_DispatchThreadID)
{
    int2 p = (int2)id.xy;
    if (p.x >= (int)gPass0.x || p.y >= (int)gPass0.y) return;
    float z0 = gAoDepthIn[p];
    if (z0 > 1e5)
    {
        gAoOut[p] = float4(0, 0, 0, 1);
        return;
    }
    int2 maxP = int2((int)gPass0.x - 1, (int)gPass0.y - 1);
    float4 sum = 0.0;
    float wsum = 0.0;
    [unroll] for (int y = -2; y <= 1; ++y)
    [unroll] for (int x = -2; x <= 1; ++x)
    {
        int2 q = clamp(p + int2(x, y), int2(0, 0), maxP);
        float z = gAoDepthIn[q];
        float w = exp(-abs(z - z0) / (0.03 * z0 + 0.02));
        sum += gAoIn[q] * w;
        wsum += w;
    }
    gAoOut[p] = sum / max(wsum, 1e-4);
}
