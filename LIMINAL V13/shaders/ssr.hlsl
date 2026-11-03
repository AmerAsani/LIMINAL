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
SamplerState gLinearWrap : register(s1);
SamplerState gLinearClamp : register(s2);
// V13.3: fuer die Ersatzspiegelung der Decke
Texture2D<float2> gHeightRing : register(t5);    // Ring: (Boden, Decke) je Kachel, massiv = Boden > 1000
Texture2D<float4> gAmbientRing : register(t6);   // Ring: Umgebungslicht je Kachelecke
struct GpuLightR
{
    float4 posRange, colorRadius, dirSpill, viewPosFlags, areaU, areaV;
};
StructuredBuffer<GpuLightR> gLightsR : register(t7);

// V13.3: Verlaesst der gespiegelte Strahl das Bild (bei Pfuetzen und glatten Boeden fast immer: die Decke ist nicht im
// Bild), spiegelt sich die Decke des Raums: der Strahl trifft die Deckenebene (Hoehenring), dort Umgebungslicht und die
// Leuchtflaechen der naechsten Deckenleuchten. ok = false, wenn keine Decke darueber liegt.
float3 ceilingMirror(float3 Pw, float3 Rw, out bool ok)
{
    ok = false;
    if (Rw.z < 0.04)
        return 0.0;
    const int mask = (int)gRingParams.x - 1;
    const float2 fc = gHeightRing.Load(int3((int2)floor(Pw.xy) & mask, 0));
    if (fc.x > 1000.0 || fc.y <= Pw.z + 0.3)
        return 0.0;
    const float t = (fc.y - Pw.z) / Rw.z;
    if (t > 45.0)
        return 0.0;
    const float3 Q = Pw + Rw * t;
    // V13.3 (Abnahme Level 0): der Strahl geht nicht durch Waende - die Zellen von Pw bis Q der Reihe nach (DDA). Vorher
    // zaehlte nur die Zelle unter dem Treffpunkt: Wasser an Wandecken der Dunkelzone spiegelte die Leuchten weit
    // entfernter Raeume durch die Wand (helle Schlieren im Dunkeln). Gesperrt: Wand, Decke unter dem Strahl, Boden darueber.
    {
        const float2 dxy = Q.xy - Pw.xy;
        int2 c = (int2)floor(Pw.xy);
        const int2 c1 = (int2)floor(Q.xy);
        const int2 st = int2(dxy.x >= 0.0 ? 1 : -1, dxy.y >= 0.0 ? 1 : -1);
        const float2 inv = 1.0 / max(abs(dxy), 1e-5);
        float2 tm = float2(dxy.x >= 0.0 ? (float)c.x + 1.0 - Pw.x : Pw.x - (float)c.x,
                           dxy.y >= 0.0 ? (float)c.y + 1.0 - Pw.y : Pw.y - (float)c.y) * inv;
        [loop] for (int k = 0; k < 96 && any(c != c1); ++k)
        {
            const float s = min(tm.x, tm.y);  // Anteil der Strecke beim Eintritt in die naechste Zelle
            if (tm.x < tm.y) { c.x += st.x; tm.x += inv.x; }
            else             { c.y += st.y; tm.y += inv.y; }
            const float2 fk = gHeightRing.Load(int3(c & mask, 0));
            const float zk = Pw.z + Rw.z * t * s;
            if (fk.x > 1000.0 || fk.y < zk || fk.x > zk)
            {
                ok = true;
                return gAmbientRing.SampleLevel(gLinearWrap, (Pw.xy + 0.5) * gRingParams.y, 0).rgb * gFogParams.z * 1.3;
            }
        }
    }
    const float2 fq = gHeightRing.Load(int3((int2)floor(Q.xy) & mask, 0));
    if (fq.x > 1000.0)
    {
        // Strahl trifft eine Wand/Stuetze vor der Decke: deren Flaeche im Umgebungslicht (keine schwarzen Flecken)
        ok = true;
        return gAmbientRing.SampleLevel(gLinearWrap, (Pw.xy + 0.5) * gRingParams.y, 0).rgb * gFogParams.z * 1.3;
    }
    float3 col = gAmbientRing.SampleLevel(gLinearWrap, (Q.xy + 0.5) * gRingParams.y, 0).rgb * gFogParams.z * 1.5;
    const uint n = min((uint)gLightParams.x, 64u);
    for (uint i = 0; i < n; ++i)
    {
        const GpuLightR L = gLightsR[i];
        if ((asuint(L.viewPosFlags.w) & 2u) != 0u || L.dirSpill.z > -0.5)
            continue;  // keine Spots (Taschenlampe), nur nach unten strahlende Leuchten
        const float3 d = Q - L.posRange.xyz;
        if (abs(d.z) > 0.9)
            continue;
        float3 U = L.areaU.xyz, V = L.areaV.xyz;
        float lu = length(U), lv = length(V);
        if (lu < 0.02 || lv < 0.02)
        {
            lu = lv = 0.09;
            U = float3(lu, 0.0, 0.0);
            V = float3(0.0, lv, 0.0);
        }
        const float su = abs(dot(d, U)) / (lu * lu), sv = abs(dot(d, V)) / (lv * lv);
        const float inside = (1.0 - smoothstep(0.8, 1.08, su)) * (1.0 - smoothstep(0.8, 1.08, sv));
        col += L.colorRadius.rgb * inside * (0.55 / max(lu * lv * 4.0, 0.05));
    }
    ok = true;
    return col * saturate(1.3 - t / 35.0);
}

float3 mirrorFallback(int2 p, float2 uv, float depth, float rough, float4 misc, out bool ok)
{
    ok = false;
    const float3 nW = normalize(gNormalBuf[p].xyz * 2.0 - 1.0);
    if (nW.z < 0.8 || rough > 0.3)
        return 0.0;
    const float3 Pw = worldFromDepth(uv, depth, gInvViewProj);
    const float3 Vw = normalize(gCameraPos.xyz - Pw);
    const float3 Rw = reflect(-Vw, nW);
    const float3 c = ceilingMirror(Pw + nW * 0.02, Rw, ok);
    if (!ok)
        return 0.0;
    const float3 albedo = gAlbedoBuf[p].rgb;
    const float3 f0 = lerp(0.04, albedo, misc.g);
    const float VoN = saturate(dot(Vw, nW));
    const float3 F = f0 + (1.0 - f0) * pow(1.0 - VoN, 5.0);
    const float smooth = saturate(1.0 - rough / 0.3);
    return c * F * smooth * smooth * gAlbedoBuf[p].a;
}

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
        bool ok;
        const float3 fb = mirrorFallback(p, uv, depth, rough, misc, ok);  // V13.3
        gOut[p] = float4(base.rgb + (ok ? fb : 0.0), 1.0);
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
        bool ok;
        const float3 fb = mirrorFallback(p, uv, depth, rough, misc, ok);  // V13.3
        gOut[p] = float4(base.rgb + (ok ? fb : 0.0), 1.0);
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
