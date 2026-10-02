// V6: Volumetrisches Licht (Einfachstreuung im Dunst) in reduzierter Aufloesung.
//
// Je Pixel und Leuchte wird das Streulicht entlang des Blickstrahls analytisch
// integriert (Punktlicht im homogenen Medium, Integral ueber 1/Abstand^2). Neu in V6:
// - Sichtbarkeit an mehreren Punkten des Strahls statt an einem: die Punkte werden genau
//   nach der Streu-Intensitaet verteilt (gleichmaessig im Winkel um die Leuchte), so dass ihr
//   Mittel die schattierte Integration erwartungstreu schaetzt. Ergebnis: Lichtschaechte
//   durch Tueren, zwischen Saeulen und Regalen - Schatten aus dem Hoehenfeld der Welt.
// - Abstrahlrichtung der Leuchten: Deckenlampen werfen Lichtkegel nach unten.
// - Dunst: langsam treibendes Dichte-Rauschen, keine voellig gleichfoermige Luft.
// Die Beleuchtung skaliert das Ergebnis tiefenbewusst hoch, TAA mittelt das Rauschmuster.
//
//   gPass0: Breite, Hoehe, 1/Breite, 1/Hoehe (Volumen-Aufloesung)
//   gPass1: x Teiler, y Anzahl Leuchten, z Sichtbarkeitspunkte, w Schattenschritte
#include "common.hlsli"

struct GpuLight
{
    float4 posRange;
    float4 colorRadius;
    float4 dirSpill;
    float4 viewPosFlags;
    float4 areaU;
    float4 areaV;
};

Texture2D<float> gDepthBuf : register(t0);
StructuredBuffer<GpuLight> gLights : register(t1);
Texture2D<float2> gHeight : register(t2);
Texture2D gMacroTex : register(t3);
RWTexture2D<float4> gVolOut : register(u0);  // rgb Streulicht, a lineare Tiefe
SamplerState gLinearWrap : register(s1);

#define VOL_FADE 10.0

float traceShadow(float3 p, float3 q, int maxSteps)
{
    float2 d = q.xy - p.xy;
    float dz = q.z - p.z;
    int2 cell = (int2)floor(p.xy);
    int2 endCell = (int2)floor(q.xy);
    int2 stp = int2(d.x >= 0.0 ? 1 : -1, d.y >= 0.0 ? 1 : -1);
    float2 ad = max(abs(d), 1e-6);
    float2 tDelta = 1.0 / ad;
    float2 nb = (float2)cell + float2(stp.x > 0 ? 1.0 : 0.0, stp.y > 0 ? 1.0 : 0.0);
    float2 tMax = abs(nb - p.xy) * tDelta;
    int mask = (int)gRingParams.x - 1;
    float t0 = 0.0;
    [loop]
    for (int i = 0; i < maxSteps; ++i)
    {
        float t1 = min(min(tMax.x, tMax.y), 1.0);
        float2 fc = gHeight.Load(int3(cell & mask, 0));
        float za = p.z + dz * t0;
        float zb = p.z + dz * t1;
        if (min(za, zb) < fc.x - 0.012 || max(za, zb) > fc.y + 0.012)
            return 0.0;
        if (t1 >= 1.0 || all(cell == endCell))
            return 1.0;
        if (tMax.x < tMax.y) { cell.x += stp.x; t0 = tMax.x; tMax.x += tDelta.x; }
        else { cell.y += stp.y; t0 = tMax.y; tMax.y += tDelta.y; }
    }
    return 1.0;
}

// Dichte des Dunstes: langsam treibende, weiche Schwaden (0.55 .. 1.45)
float hazeDensity(float3 w, float t)
{
    float2 uv = w.xy * 0.045 + w.z * 0.03 + float2(t * 0.004, t * 0.0027);
    float2 uv2 = w.yx * 0.11 - w.z * 0.05 + float2(-t * 0.006, t * 0.005);
    float n = gMacroTex.SampleLevel(gLinearWrap, uv, 0).r * 0.65 + gMacroTex.SampleLevel(gLinearWrap, uv2, 0).g * 0.35;
    return 0.55 + 0.9 * saturate((n - 0.3) / 0.4);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    int2 p = (int2)id.xy;
    if (p.x >= (int)gPass0.x || p.y >= (int)gPass0.y) return;
    int scale = (int)gPass1.x;
    int2 maxF = int2((int)gScreen.x - 1, (int)gScreen.y - 1);
    int2 fp = min(p * scale + (scale >> 1), maxF);
    float depth = gDepthBuf[fp];
    float2 uv = ((float2)fp + 0.5) * gScreen.zw;
    float3 cam = gCameraPos.xyz;
    float dist, vz;
    float3 rd;
    if (depth > 0.0)
    {
        float3 P = worldFromDepth(uv, depth, gInvViewProj);
        rd = P - cam;
        dist = length(rd);
        rd /= max(dist, 1e-5);
        vz = linearDepth(depth);
    }
    else
    {
        float3 far = worldFromDepth(uv, 1e-6, gInvViewProj);
        rd = normalize(far - cam);
        dist = gFogParams.x;
        vz = 1e4;
    }
    float density = gFogColor.a;
    if (gDebug.z <= 0.0 || density <= 0.0)
    {
        gVolOut[p] = float4(0, 0, 0, vz);
        return;
    }
    float t = gCameraPos.w;
    float jit = ign((float2)p, gRingParams.z);
    uint n = min((uint)gLightParams.x, (uint)gPass1.y);
    int nv = (int)gPass1.z;
    int steps = (int)gPass1.w;
    bool shadows = gLightParams.z > 0.0;
    float3 inscatter = 0.0;
    [loop] for (uint k = 0; k < n; ++k)
    {
        GpuLight L = gLights[k];
        float R = L.posRange.w;
        float3 toL = L.posRange.xyz - cam;
        float t0 = dot(toL, rd);
        float h2 = max(dot(toL, toL) - t0 * t0, 0.03);
        if (h2 >= R * R) continue;
        float h = sqrt(h2 + L.areaU.w);  // weicher Kern wie bei der Oberflaechenbeleuchtung
        float halfLen = sqrt(R * R - h2);
        float ta = max(0.0, t0 - halfLen), tb = min(dist, t0 + halfLen);
        if (tb <= ta) continue;
        float w = 1.0 - h2 / (R * R);
        float thA = atan((ta - t0) / h), thB = atan((tb - t0) / h);
        float integ = (thB - thA) / h * w * w;
        // Rangausblendung, Abschwaechung durch den Nebel, Ausblenden am Rand der Sichtweite
        float tc = clamp(t0, ta, tb);
        float fadeC = saturate((tc - gFogParams.x * gFogParams.y) / max(gFogParams.x * (1.0 - gFogParams.y), 1.0));
        integ *= saturate(((float)n - (float)k) / VOL_FADE) * exp(-tc * density) * (1.0 - fadeC * fadeC);
        if (integ <= 1e-3) continue;
        // Sichtbarkeit, Abstrahlrichtung und Dunst an Punkten, die gleichmaessig im Winkel um die
        // Leuchte verteilt sind (= nach der Streu-Intensitaet gewichtet)
        float acc = 0.0;
        [loop] for (int j = 0; j < nv; ++j)
        {
            float th = lerp(thA, thB, ((float)j + frac(jit + (float)k * 0.618034)) / (float)nv);
            float ts = clamp(t0 + h * tan(th), ta + 0.02, tb - 0.02);
            float3 ps = cam + rd * ts;
            float3 dl = normalize(L.posRange.xyz - ps);
            float emit = lerp(L.dirSpill.w, 1.0, saturate(dot(-dl, L.dirSpill.xyz)));
            float vis = shadows ? traceShadow(ps, L.posRange.xyz, steps) : 1.0;
            acc += vis * emit * hazeDensity(ps, t);
        }
        inscatter += L.colorRadius.rgb * (integ * acc / (float)nv);
    }
    inscatter *= density * gDebug.z * 0.026;
    gVolOut[p] = float4(inscatter, vz);
}
