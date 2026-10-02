// Spiegelungen im Bildraum (Screen-Space Reflections).
//
// Fuer glatte Oberflaechen (polierte Fliesen, Terrazzo, Monolith, Laminat)
// wird der gespiegelte Blickstrahl im Bild verfolgt. Rauere Oberflaechen
// streuen den Strahl zufaellig (GGX-Stichprobe); das TAA mittelt daraus
// weiche, matte Spiegelungen. Ohne Treffer bleibt das Umgebungslicht.
#include "common.hlsli"

Texture2D<float4> gLit : register(t0);
Texture2D<float> gDepthBuf : register(t1);
Texture2D<float4> gNormalBuf : register(t2);
Texture2D<float4> gMiscBuf : register(t3);
Texture2D<float4> gAlbedoBuf : register(t4);
RWTexture2D<float4> gOut : register(u0);
SamplerState gLinearClamp : register(s2);

float3 viewFromDepth(float2 uv, float depth)
{
    float z = linearDepth(depth);
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    return float3((ndc.x - gJitter.x) * z / gProj.x, (ndc.y - gJitter.y) * z / gProj.y, z);
}

float2 uvFromView(float3 v)
{
    float2 ndc = float2(v.x * gProj.x / v.z + gJitter.x, v.y * gProj.y / v.z + gJitter.y);
    return float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    int2 p = (int2)id.xy;
    if (p.x >= (int)gScreen.x || p.y >= (int)gScreen.y) return;
    float4 base = gLit[p];
    float depth = gDepthBuf[p];
    float4 misc = gMiscBuf[p];
    float rough = misc.r;
    const float maxRough = 0.42;
    if (depth <= 0.0 || rough > maxRough)
    {
        gOut[p] = base;
        return;
    }
    float2 uv = ((float2)p + 0.5) * gScreen.zw;
    float3 P = viewFromDepth(uv, depth);
    float3 N = normalize(mul((float3x3)gView, gNormalBuf[p].xyz * 2.0 - 1.0));
    float3 V = normalize(-P);

    // GGX-Stichprobe fuer die Mikronormale (je Bild anders, TAA mittelt)
    float a = rough * rough;
    float r1 = ign((float2)p, gRingParams.z), r2 = ign((float2)p.yx + 17.0, gRingParams.z + 3.0);
    float phi = 6.2831853 * r1;
    float cosT = sqrt((1.0 - r2) / (1.0 + (a * a - 1.0) * r2));
    float sinT = sqrt(1.0 - cosT * cosT);
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 T = normalize(cross(up, N)), B = cross(N, T);
    float3 H = normalize(T * (sinT * cos(phi)) + B * (sinT * sin(phi)) + N * cosT);
    float3 R = reflect(-V, H);
    if (R.z <= 0.02 || dot(R, N) <= 0.0)
    {
        gOut[p] = base;
        return;
    }

    // Strahl im Sichtraum mit wachsender Schrittweite, dann Verfeinerung.
    // V6: Tiefe punktgenau lesen (linear gefilterte Tiefe erfindet an Kanten Zwischenflaechen) und
    // jeden Durchgang hinter eine Oberflaeche verfeinern; erst am verfeinerten Punkt wird ueber die
    // Dicke entschieden. Vorher wurden lange Schritte, die eine Wand um mehr als die Dicke
    // durchstiessen, verworfen - Treffer und Fehlschuss wechselten zeilenweise (Streifen in
    // Bodenspiegelungen bei flachem Blick).
    float3 pos = P + N * 0.02;
    float stepLen = 0.08 + 0.02 * P.z;
    float jitter = ign((float2)p * 1.3, gRingParams.z + 7.0);
    float3 prev = pos;
    float2 hitUv = -1;
    int2 maxPx = int2((int)gScreen.x - 1, (int)gScreen.y - 1);
    [loop]
    for (int i = 0; i < 40; ++i)
    {
        float3 cur = pos + R * (stepLen * ((float)i + jitter));
        stepLen *= 1.06;
        float2 cuv = uvFromView(cur);
        if (any(cuv < 0.0) || any(cuv > 1.0) || cur.z < gProj.z) break;
        float d = gDepthBuf[min((int2)(cuv * gScreen.xy), maxPx)];
        if (d <= 0.0) { prev = cur; continue; }
        float diff = cur.z - linearDepth(d);
        if (diff > 0.0)
        {
            // binaere Verfeinerung zwischen letztem freien und erstem verdeckten Punkt
            float3 lo = prev, hi = cur;
            float hiDiff = diff;
            [unroll] for (int k = 0; k < 6; ++k)
            {
                float3 mid = (lo + hi) * 0.5;
                float2 muv = uvFromView(mid);
                float mz = linearDepth(max(gDepthBuf[min((int2)(muv * gScreen.xy), maxPx)], 1e-7));
                if (mid.z > mz) { hi = mid; hiDiff = mid.z - mz; }
                else lo = mid;
            }
            // durchgehende Flaeche: Strahl liegt nach der Verfeinerung knapp hinter ihr. Grosser
            // Abstand = Strahl lief hinter einem schmalen Gegenstand vorbei -> weitersuchen
            if (hiDiff < 0.12 + 0.02 * hi.z + length(hi - lo))
            {
                hitUv = uvFromView(hi);
                break;
            }
        }
        prev = cur;
    }
    if (hitUv.x < 0.0)
    {
        gOut[p] = base;
        return;
    }
    float3 refl = gLit.SampleLevel(gLinearClamp, hitUv, 0).rgb;
    // Ausblenden am Bildrand und fuer rauere Flaechen
    float2 e = saturate(min(hitUv, 1.0 - hitUv) * 12.0);
    float fade = e.x * e.y * saturate(1.0 - rough / maxRough) * saturate(1.0 - (float)P.z / 60.0);
    float3 albedo = gAlbedoBuf[p].rgb;
    float3 f0 = lerp(0.04, albedo, misc.g);
    float VoH = saturate(dot(V, H));
    float3 F = f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0);
    gOut[p] = float4(base.rgb + refl * F * fade * gAlbedoBuf[p].a, 1.0);
}
