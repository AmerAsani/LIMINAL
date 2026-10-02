// Beleuchtung (Tiled Deferred, Compute Shader).
//
// Je 16x16-Pixel-Kachel werden die Lichter bestimmt, die sie beruehren koennen;
// dann wird jedes Pixel physikalisch basiert beleuchtet (GGX + Lambert).
//
// Schatten: Die Welt ist ein Hoehenfeld aus Kacheln (Boden/Decke je 1 x 1 m).
// Ein Strahl vom Oberflaechenpunkt zur Leuchte laeuft per DDA durch dieses Feld
// und prueft, ob er irgendwo unter einen Boden oder ueber eine Decke geraet.
// Das ist fuer diese Welt exakt: Waende, Saeulen, Regale, Tuerstuerze und
// Treppen werfen korrekte Schatten, und kein Licht dringt durch Waende. Der
// Zielpunkt wandert pro Bild ueber die Leuchtflaeche - das TAA mittelt daraus
// weiche Halbschatten.
#include "common.hlsli"

#define TILE 16
#define MAX_TILE_LIGHTS 128

struct GpuLight
{
    float4 posRange;      // xyz Position, w Reichweite
    float4 colorRadius;   // rgb Farbe * Staerke (inkl. Flackern), w Radius der Leuchtflaeche
    float4 dirSpill;      // xyz Abstrahlrichtung, w Streulicht zur Seite (0..1)
    float4 viewPosFlags;  // xyz Position im Sichtraum, w Flags (asuint): 1 = Schatten
    float4 areaU;         // halbe Ausdehnung der Leuchtflaeche (Achse 1), w weicher Kern der Daempfung (m^2)
    float4 areaV;         // halbe Ausdehnung (Achse 2)
};

Texture2D<float4> gAlbedoBuf : register(t0);
Texture2D<float4> gNormalBuf : register(t1);
Texture2D<float4> gMiscBuf : register(t2);
Texture2D<float3> gEmissiveBuf : register(t3);
Texture2D<float> gDepthBuf : register(t4);
Texture2D<float> gAoBuf : register(t5);
Texture2D<float2> gHeight : register(t6);     // Ring: (Boden, Decke) je Kachel
Texture2D<float4> gAmbient : register(t7);    // Ring: getoentes Umgebungslicht je Kachelecke
StructuredBuffer<GpuLight> gLights : register(t8);
RWTexture2D<float4> gOut : register(u0);
SamplerState gLinearWrap : register(s1);

// Lichtauswahl je Kachel. Die Lichter kommen nach Abstand zur Kamera sortiert an
// (Index 0 = naechstes). Getroffene Lichter werden als Bitmaske gesammelt und in
// Indexreihenfolge zur Liste verdichtet: Laeuft eine Kachel ueber, fallen immer die
// entferntesten weg - stabil von Bild zu Bild, kein Flackern. Zusaetzlich teilt eine
// Tiefenmaske (32 Stufen zwischen naechster und fernster Oberflaeche der Kachel)
// Lichter aus, die nur in Luecken zwischen den Oberflaechen liegen (2.5D-Culling);
// das haelt die Listen bei schraegem Blick durch grosse Hallen kurz.
#define MAX_LIGHTS 2048
#define MASK_WORDS (MAX_LIGHTS / 32)
groupshared uint gsMinZ;
groupshared uint gsMaxZ;
groupshared uint gsDepthMask;
groupshared uint gsCount;
groupshared uint gsMask[MASK_WORDS];
groupshared uint gsPop[MASK_WORDS];
groupshared uint gsList[MAX_TILE_LIGHTS];
// Lichthof im Nebel: fuer das ganze Bild dieselben naechsten Lichter (keine Kachelkanten),
// die letzten davon weich ausgeblendet, damit Rangwechsel beim Gehen nicht springen.
#define MAX_VOL_LIGHTS 40
#define VOL_FADE 10.0
groupshared uint gsVolCount;
groupshared uint gsVol[MAX_VOL_LIGHTS];

// --- BRDF ---------------------------------------------------------------------------
float D_GGX(float NoH, float a)
{
    float a2 = a * a;
    float d = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * d * d + 1e-7);
}
float V_Smith(float NoV, float NoL, float a)
{
    float a2 = a * a;
    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (gv + gl + 1e-7);
}
float3 F_Schlick(float3 f0, float VoH) { return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0); }

// --- Hoehenfeld-Schatten ---------------------------------------------------------------
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

// --- Kachel-Frustum ---------------------------------------------------------------------
float4 sidePlane(float3 n) { return float4(normalize(n), 0.0); }

[numthreads(TILE, TILE, 1)]
void CSMain(uint3 gid : SV_GroupID, uint3 dtid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    if (gi == 0)
    {
        gsMinZ = 0x7F7FFFFF;
        gsMaxZ = 0;
        gsDepthMask = 0;
        gsCount = 0;
        gsVolCount = 0;
    }
    if (gi < MASK_WORDS)
        gsMask[gi] = 0;
    GroupMemoryBarrierWithGroupSync();

    uint2 px = dtid.xy;
    bool inside = px.x < (uint)gScreen.x && px.y < (uint)gScreen.y;
    float depth = inside ? gDepthBuf[px] : 0.0;
    float vz = depth > 0.0 ? linearDepth(depth) : 0.0;
    if (depth > 0.0)
    {
        InterlockedMin(gsMinZ, asuint(vz));
        InterlockedMax(gsMaxZ, asuint(vz));
    }
    GroupMemoryBarrierWithGroupSync();

    float minZ = asfloat(gsMinZ), maxZ = asfloat(gsMaxZ);
    float binScale = 32.0 / max(maxZ - minZ, 1e-4);
    if (depth > 0.0)
        InterlockedOr(gsDepthMask, 1u << min((uint)((vz - minZ) * binScale), 31u));
    GroupMemoryBarrierWithGroupSync();

    // Lichter gegen das Kachel-Frustum testen
    if (gsMaxZ > 0)
    {
        float2 p0 = (float2)(gid.xy * TILE) * gScreen.zw;
        float2 p1 = (float2)(gid.xy * TILE + TILE) * gScreen.zw;
        float xl = p0.x * 2.0 - 1.0 - gJitter.x, xr = p1.x * 2.0 - 1.0 - gJitter.x;
        float yt = 1.0 - p0.y * 2.0 - gJitter.y, yb = 1.0 - p1.y * 2.0 - gJitter.y;
        float4 pl[4] = {
            sidePlane(float3(gProj.x, 0.0, -xl)),
            sidePlane(float3(-gProj.x, 0.0, xr)),
            sidePlane(float3(0.0, gProj.y, -yb)),
            sidePlane(float3(0.0, -gProj.y, yt)) };
        uint count = min((uint)gLightParams.x, (uint)MAX_LIGHTS);
        uint depthMask = gsDepthMask;
        for (uint li = gi; li < count; li += TILE * TILE)
        {
            float3 c = gLights[li].viewPosFlags.xyz;
            float r = gLights[li].posRange.w;
            bool side = true;
            [unroll] for (int k = 0; k < 4; ++k)
                side = side && dot(pl[k].xyz, c) >= -r;
            // Lichthof: die naechsten Leuchten vor der entferntesten Oberflaeche
            if (li < MAX_VOL_LIGHTS && side && c.z - r <= maxZ && c.z + r > 0.0)
            {
                uint vs;
                InterlockedAdd(gsVolCount, 1, vs);
                gsVol[vs] = li;
            }
            float z0 = max(c.z - r, minZ), z1 = min(c.z + r, maxZ);
            if (side && z0 <= z1)
            {
                uint b0 = min((uint)((z0 - minZ) * binScale), 31u);
                uint b1 = min((uint)((z1 - minZ) * binScale), 31u);
                uint lightMask = (0xFFFFFFFFu >> (31u - b1)) & (0xFFFFFFFFu << b0);
                if ((lightMask & depthMask) != 0)
                    InterlockedOr(gsMask[li >> 5], 1u << (li & 31u));
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();
    // Bitmaske in Indexreihenfolge (= naechste Lichter zuerst) zur Liste verdichten
    if (gi < MASK_WORDS)
        gsPop[gi] = countbits(gsMask[gi]);
    GroupMemoryBarrierWithGroupSync();
    if (gi < MASK_WORDS)
    {
        uint base = 0;
        for (uint w = 0; w < gi; ++w)
            base += gsPop[w];
        if (gi == MASK_WORDS - 1)
            gsCount = base + gsPop[gi];
        uint bits = gsMask[gi];
        while (bits != 0 && base < MAX_TILE_LIGHTS)
        {
            uint b = firstbitlow(bits);
            gsList[base++] = gi * 32 + b;
            bits &= bits - 1;
        }
    }
    GroupMemoryBarrierWithGroupSync();
    if (!inside)
        return;

    if (depth <= 0.0)
    {
        // Debug 9: Pixel ohne Geometrie (Loecher im Netz) magenta
        gOut[px] = (uint)gDebug.x == 9 ? float4(1, 0, 1, 1) : float4(gFogColor.rgb, 1.0);
        return;
    }

    float2 uv = ((float2)px + 0.5) * gScreen.zw;
    float3 P = worldFromDepth(uv, depth, gInvViewProj);
    float4 alb = gAlbedoBuf[px];
    float3 N = normalize(gNormalBuf[px].xyz * 2.0 - 1.0);
    float4 misc = gMiscBuf[px];
    float rough = max(misc.r, 0.045);
    float metal = misc.g;
    uint flick = (uint)(misc.b * 255.0 + 0.5);
    float3 albedo = alb.rgb;
    float matAO = alb.a;
    float ssao = gAoBuf[px];
    float3 V = gCameraPos.xyz - P;
    float dist = length(V);
    V /= dist;
    float NoV = saturate(dot(N, V)) + 1e-4;
    float3 f0 = lerp(0.04, albedo, metal);
    float3 diffColor = albedo * (1.0 - metal);
    float a = rough * rough;

    float3 color = gEmissiveBuf[px];
    float3 direct = 0.0;
    float shadowSum = 0.0, shadowCount = 0.0;
    uint dbg = (uint)gDebug.x;
    float3 shadowOrigin = P + N * 0.03;
    float noise = ign((float2)px, gRingParams.z);
    int steps = gLightParams.z >= 2.0 ? 40 : 20;

    uint n = min(gsCount, (uint)MAX_TILE_LIGHTS);
    for (uint k = 0; k < n; ++k)
    {
        GpuLight L = gLights[gsList[k]];
        float3 toL = L.posRange.xyz - P;
        float d2 = dot(toL, toL);
        float range = L.posRange.w;
        if (d2 >= range * range)
            continue;
        float d = sqrt(d2);
        float3 l = toL / d;
        float NoL = dot(N, l);
        if (NoL <= 0.0)
            continue;
        // Lambert-Strahler mit etwas Streulicht zur Seite
        float emit = lerp(L.dirSpill.w, 1.0, saturate(dot(-l, L.dirSpill.xyz)));
        float x = d / range;
        float window = saturate(1.0 - x * x * x * x);
        float atten = window * window / (d2 + 0.25 + L.areaU.w);
        float3 radiance = L.colorRadius.rgb * (atten * emit);
        if (max(radiance.r, max(radiance.g, radiance.b)) * NoL < 2e-4)
            continue;
        if ((asuint(L.viewPosFlags.w) & 1u) != 0 && gLightParams.z > 0.0)
        {
            // Zielpunkt auf der Leuchtflaeche (wechselt je Bild und Pixel)
            float r1 = frac(noise + (float)k * 0.618034) * 2.0 - 1.0;
            float r2 = frac(noise * 1.618034 + (float)k * 0.381966) * 2.0 - 1.0;
            float3 q = L.posRange.xyz + L.areaU.xyz * r1 + L.areaV.xyz * r2;
            float sh = traceShadow(shadowOrigin, q, steps);
            shadowSum += 1.0 - sh;
            shadowCount += 1.0;
            radiance *= sh;
            if (max(radiance.r, max(radiance.g, radiance.b)) <= 0.0)
                continue;
        }
        NoL = saturate(NoL);
        float3 h = normalize(l + V);
        float NoH = saturate(dot(N, h));
        float VoH = saturate(dot(V, h));
        // Flaechenlicht: Rauheit fuer die Glanzlichter anheben (Karis)
        float aL = saturate(a + L.colorRadius.w / (3.0 * d));
        float norm = (a / aL) * (a / aL);
        float3 F = F_Schlick(f0, VoH);
        float3 spec = D_GGX(NoH, aL) * V_Smith(NoV, NoL, a) * F * norm;
        float3 diff = diffColor * (1.0 / PI) * (1.0 - F);
        direct += (diff + spec) * radiance * NoL;
    }

    // Umgebungslicht (vorberechnet je Kachelecke, weich interpoliert)
    float2 auv = (P.xy + N.xy * 0.45 + 0.5) * gRingParams.y;
    float3 amb = gAmbient.SampleLevel(gLinearWrap, auv, 0).rgb * gFogParams.z;
    amb *= flickerFactor(flick >> 5, flick & 31u, gCameraPos.w);
    float occ = matAO * ssao;
    // ein Teil der Verdeckung auch auf direktes Licht: Kontaktschatten in Ecken und unter Objekten
    color += direct * lerp(1.0, occ, gDebug.y);
    float3 Fa = f0 + (max(1.0 - rough, f0) - f0) * pow(1.0 - NoV, 5.0);
    float3 ambient = amb * (diffColor * occ + Fa * occ * 0.35);
    color += ambient;
    if (dbg != 0)
    {
        float3 dv = 0;
        if (dbg == 1) dv = direct;
        else if (dbg == 2) dv = ambient;
        else if (dbg == 3) dv = N * 0.5 + 0.5;
        else if (dbg == 4) dv = ssao.xxx;
        else if (dbg == 5) dv = shadowCount > 0.0 ? (shadowSum / shadowCount).xxx : float3(0, 0, 0.3);
        else if (dbg == 6) dv = gsCount > MAX_TILE_LIGHTS ? float3(1, 0, 0) : ((float)n / MAX_TILE_LIGHTS).xxx;
        else if (dbg == 8) dv = gsVolCount > MAX_VOL_LIGHTS ? float3(1, 0, 0) : ((float)gsVolCount / MAX_VOL_LIGHTS).xxx;
        else if (dbg == 9) dv = albedo * 0.5;
        else if (dbg == 7)
        {
            float2 hf = gHeight.Load(int3((int2)floor(P.xy + N.xy * 0.03) & ((int)gRingParams.x - 1), 0));
            dv = hf.x > 1000.0 ? float3(0.4, 0, 0) : float3(0, 0.1 + hf.y * 0.05, 0);
        }
        gOut[px] = float4(dv, 1.0);
        return;
    }

    // Lichthof im Nebel: Einfachstreuung, analytisch entlang des Blickstrahls.
    // Sichtbarkeit am naechstgelegenen Strahlpunkt verhindert Leuchten durch Waende.
    float3 inscatter = 0.0;
    if (gDebug.z > 0.0 && gFogColor.a > 0.0)
    {
        float3 cam = gCameraPos.xyz;
        float3 rd = -V;
        uint m = min(gsVolCount, (uint)MAX_VOL_LIGHTS);
        for (uint k = 0; k < m; ++k)
        {
            GpuLight L = gLights[gsVol[k]];
            float R = L.posRange.w;
            float3 toL = L.posRange.xyz - cam;
            float t0 = dot(toL, rd);
            float h2 = max(dot(toL, toL) - t0 * t0, 0.03);
            if (h2 >= R * R) continue;
            float h = sqrt(h2 + L.areaU.w);  // weicher Kern wie bei der Oberflaechenbeleuchtung
            // nur der Teil des Strahls innerhalb der Reichweite, weich ausgeblendet
            float halfLen = sqrt(R * R - h2);
            float ta = max(0.0, t0 - halfLen), tb = min(dist, t0 + halfLen);
            if (tb <= ta) continue;
            float w = 1.0 - h2 / (R * R);
            float integ = (atan((tb - t0) / h) - atan((ta - t0) / h)) / h * w * w;
            // Rangausblendung, Abschwaechung durch den Nebel zwischen Kamera und Lichthof,
            // Ausblenden am Rand der Sichtweite wie bei Oberflaechen
            float tc = clamp(t0, ta, tb);
            float fadeC = saturate((tc - gFogParams.x * gFogParams.y) / max(gFogParams.x * (1.0 - gFogParams.y), 1.0));
            integ *= saturate(((float)MAX_VOL_LIGHTS - (float)gsVol[k]) / VOL_FADE) * exp(-tc * gFogColor.a) *
                     (1.0 - fadeC * fadeC);
            if (integ <= 1e-3) continue;
            float3 pc = cam + rd * clamp(t0, ta + 0.02, tb - 0.02);
            float vis = gLightParams.z > 0.0 ? traceShadow(pc, L.posRange.xyz, 24) : 1.0;
            inscatter += L.colorRadius.rgb * (integ * vis);
        }
        inscatter *= gFogColor.a * gDebug.z * 0.012;
    }

    // Nebel und Ausblenden am Rand der Sichtweite
    float fogK = 1.0 - exp(-dist * gFogColor.a);
    float fade = saturate((dist - gFogParams.x * gFogParams.y) / max(gFogParams.x * (1.0 - gFogParams.y), 1.0));
    fogK = max(fogK, fade * fade);
    color = lerp(color, gFogColor.rgb, fogK) + inscatter;
    gOut[px] = float4(color, 1.0);
}
