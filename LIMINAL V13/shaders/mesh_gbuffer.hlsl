// G-Buffer-Pass fuer frei platzierte Objekte (Items, Requisiten, V6: eigener Koerper,
// V10: Figuren mit Skinning und triplanaren Materialien).
#include "gbuffer.hlsli"

cbuffer ObjectCB : register(b2)
{
    float4x4 gWorld;
    uint gMaterial;
    float gHighlight;   // Hervorhebung (Ziel zum Aufnehmen), 0..1
    float gAttached;    // V6: 1 = bewegt sich mit dem Spieler (TAA projiziert diese Pixel anders zurueck),
                        // V10: 1/3 = bewegte Figur (TAA nimmt weniger Verlauf)
    uint gBoneOffset;   // V10: erste Zeile der Knochenmatrizen dieser Figur
    float4 gClip;       // V13: Schnittkugel (xyz Mitte, w Radius; 0 = keine)
};

// V10: Knochenmatrizen aller Figuren dieses Bildes (je Matrix 3 Zeilen, float3x4)
StructuredBuffer<float4> gBones : register(t0);

// V10: Texturen der Wesen (render/texture_entity.hpp): Farbe (BC1), Normalen (BC5), Verdeckung und
// Rauheit (BC5), feine Detailnormalen. Ebenen-Konstanten ab gLayer[48] (Wesen) und gLayer[56] (Mikro).
Texture2DArray gEntAlbedo : register(t6);
Texture2DArray gEntNormal : register(t7);
Texture2DArray gEntOrm : register(t8);
Texture2DArray gEntMicro : register(t9);
static const uint kEntLayerSlot = 96, kMicroLayerSlot = 107;  // V12, V13.2 (texture_entity.hpp)

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;  // xyz Tangente, w Vorzeichen der Bitangente
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 svpos : SV_Position;
    float3 wpos : TEXCOORD0;
    float3 n : TEXCOORD1;
    float4 t : TEXCOORD2;
    float2 uv : TEXCOORD3;
    float3 bindPos : TEXCOORD4;  // V10: Ruheposition (triplanare Texturen haften an der Haut)
    float3 bindN : TEXCOORD5;    // V10: Normale der Ruhepose
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 w = mul(gWorld, float4(i.pos, 1.0));
    o.svpos = mul(gViewProj, w);
    o.wpos = w.xyz;
    o.n = mul((float3x3)gWorld, i.normal);
    o.t = float4(mul((float3x3)gWorld, i.tangent.xyz), i.tangent.w);
    o.uv = i.uv;
    o.bindPos = i.pos;
    o.bindN = i.normal;
    return o;
}

// V10: Figuren - Linear Blend Skinning mit bis zu vier Knochen je Punkt
struct SkinIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;   // triplanare Materialien: xyz = Normale der Ruhepose
    float2 uv : TEXCOORD0;
    float3 bindPos : TEXCOORD1;
    uint4 bones : BLENDINDICES;
    float4 weights : BLENDWEIGHT;
};

// V13.2 (B-001): Dual-Quaternion-Skinning fuer Menschen (Bit 31 von gBoneOffset). Lineares Mischen der Matrizen laesst
// Punkte zwischen zwei Knochen (Ellbogen innen, Schulter vorn) bei starker Beugung zusammenfallen und umklappen - die
// Innenseite des Stoffs wurde als dunkle Zacken sichtbar. Dual Quaternions halten die Punkte auf Boegen um das Gelenk.
// Gleichmaessige Skalierung (die verborgene Hand schrumpft auf 2 %) wird getrennt linear gemischt.
float4 quatFromRot(float3x3 m)
{
    float tr = m[0][0] + m[1][1] + m[2][2];
    float4 q;
    if (tr > 0.0)
    {
        float s = sqrt(tr + 1.0) * 2.0;
        q = float4((m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s, 0.25 * s);
    }
    else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
    {
        float s = sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
        q = float4(0.25 * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s, (m[2][1] - m[1][2]) / s);
    }
    else if (m[1][1] > m[2][2])
    {
        float s = sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
        q = float4((m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s, (m[0][2] - m[2][0]) / s);
    }
    else
    {
        float s = sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
        q = float4((m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s, (m[1][0] - m[0][1]) / s);
    }
    return q;
}
float4 quatMul(float4 a, float4 b) { return float4(a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz), a.w * b.w - dot(a.xyz, b.xyz)); }
float3 quatRot(float4 q, float3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }

VSOut VSSkinned(SkinIn i)
{
    float3x4 m = (float3x4)0;
    float3 p, nrm, tng;
    const bool dq = (gBoneOffset & 0x80000000u) != 0u;
    const uint off = gBoneOffset & 0x7FFFFFFFu;
    if (dq)
    {
        float4 r0 = 0, d0 = 0, rFirst = 0;
        float sc = 0.0;
        [unroll] for (int k = 0; k < 4; ++k)
        {
            uint b = off + i.bones[k] * 3u;
            float3x4 mk = float3x4(gBones[b], gBones[b + 1u], gBones[b + 2u]);
            float s = length(float3(mk[0][0], mk[1][0], mk[2][0]));
            float3x3 rk = (float3x3)mk / max(s, 1e-6);
            float4 q = quatFromRot(rk);
            if (k == 0) rFirst = q;
            float sgn = dot(q, rFirst) < 0.0 ? -1.0 : 1.0;  // kuerzester Weg
            float4 d = 0.5 * quatMul(float4(mk[0][3], mk[1][3], mk[2][3], 0.0), q);
            r0 += i.weights[k] * sgn * q;
            d0 += i.weights[k] * sgn * d;
            sc += i.weights[k] * s;
        }
        float len = max(length(r0), 1e-6);
        r0 /= len;
        d0 /= len;
        float3 t = 2.0 * (r0.w * d0.xyz - d0.w * r0.xyz + cross(r0.xyz, d0.xyz));
        p = quatRot(r0, i.pos * sc) + t;
        nrm = quatRot(r0, i.normal);
        tng = quatRot(r0, i.tangent.xyz);
    }
    else
    {
        [unroll] for (int k = 0; k < 4; ++k)
        {
            uint b = off + i.bones[k] * 3u;
            m += i.weights[k] * float3x4(gBones[b], gBones[b + 1u], gBones[b + 2u]);
        }
        p = mul(m, float4(i.pos, 1.0));
        nrm = mul((float3x3)m, i.normal);
        tng = mul((float3x3)m, i.tangent.xyz);
    }
    VSOut o;
    float4 w = mul(gWorld, float4(p, 1.0));
    o.svpos = mul(gViewProj, w);
    o.wpos = w.xyz;
    o.n = mul((float3x3)gWorld, nrm);
    o.t = float4(mul((float3x3)gWorld, tng), i.tangent.w);
    o.uv = i.uv;
    o.bindPos = i.bindPos;
    o.bindN = i.tangent.xyz;
    return o;
}

// V10: triplanare Texturierung in der Ruhepose (Haut, Stoff, Haar). Die gestoerte Normale wird
// in der Ruhepose bestimmt (Whiteout-Mischung) und dann mit der Drehung Ruhe- -> aktuelle Normale
// in die Welt gebracht.
GBufferOut shadeTriplanar(GpuMaterial m, uint layer, float3 bp, float3 bn, float3 N, float2 tint)
{
    float4 li = gLayer[layer];
    float3 q = bp / li.x;
    bn = normalize(bn);
    float3 bw = pow(abs(bn), 4.0);
    bw /= (bw.x + bw.y + bw.z);
    float2 uvX = q.zy, uvY = q.xz, uvZ = q.xy;
    float4 aX = gAlbedoTex.Sample(gAniso, float3(uvX, layer));
    float4 aY = gAlbedoTex.Sample(gAniso, float3(uvY, layer));
    float4 aZ = gAlbedoTex.Sample(gAniso, float3(uvZ, layer));
    float4 oX = gOrmTex.Sample(gAniso, float3(uvX, layer));
    float4 oY = gOrmTex.Sample(gAniso, float3(uvY, layer));
    float4 oZ = gOrmTex.Sample(gAniso, float3(uvZ, layer));
    float3 nX = float3((gNormalTex.Sample(gAniso, float3(uvX, layer)).xy * 2.0 - 1.0) * li.y, 0.0);
    float3 nY = float3((gNormalTex.Sample(gAniso, float3(uvY, layer)).xy * 2.0 - 1.0) * li.y, 0.0);
    float3 nZ = float3((gNormalTex.Sample(gAniso, float3(uvZ, layer)).xy * 2.0 - 1.0) * li.y, 0.0);
    nX.z = sqrt(saturate(1.0 - dot(nX.xy, nX.xy)));
    nY.z = sqrt(saturate(1.0 - dot(nY.xy, nY.xy)));
    nZ.z = sqrt(saturate(1.0 - dot(nZ.xy, nZ.xy)));
    nX = float3(nX.xy + bn.zy, abs(nX.z) * bn.x);
    nY = float3(nY.xy + bn.xz, abs(nY.z) * bn.y);
    nZ = float3(nZ.xy + bn.xy, abs(nZ.z) * bn.z);
    float3 nb = normalize(nX.zyx * bw.x + nY.xzy * bw.y + nZ.xyz * bw.z);
    // Ruhe- -> aktuelle Normale (Rodrigues)
    float3 ax = cross(bn, N);
    float sn = length(ax), cs = dot(bn, N);
    float3 n = nb;
    if (sn > 1e-4)
    {
        ax /= sn;
        n = nb * cs + cross(ax, nb) * sn + ax * dot(ax, nb) * (1.0 - cs);
    }
    else if (cs < 0.0)
        n = -nb;
    float4 alb = aX * bw.x + aY * bw.y + aZ * bw.z;
    float4 orm = oX * bw.x + oY * bw.y + oZ * bw.z;
    // grossflaechige Variation an der Figur festgemacht (wandert nicht mit der Welt)
    float3 mac = gMacroTex.Sample(gLinearWrap, bp.xy * 0.9 + bp.z * 0.37).rgb;
    float tintV = lerp(1.0, 0.88 + 0.24 * mac.r, m.variation);
    GBufferOut o;
    // Faerbung je Punkt (Gesicht): x Roete (Lippen, Wangen), y Dunkelheit (Augenhoehlen, Brauen, Bart)
    float3 tc = lerp(1.0.xxx, float3(1.12, 0.8, 0.78), tint.x) * (1.0 - 0.5 * tint.y);
    o.albedo = float4(saturate(alb.rgb * m.albedo * tintV * tc), orm.r * (1.0 - 0.25 * tint.y));
    o.normal = float4(normalize(n) * 0.5 + 0.5, 0.0);
    float rough = saturate(orm.g * m.roughness + li.z + (mac.b - 0.5) * 0.15 * m.variation);
    o.misc = float4(rough, saturate(orm.b + m.metalness), 0.0, 1.0);
    o.emissive = 0.0;
    return o;
}

// V10: die Wesen. Triplanar in der Ruhepose (v laeuft an den Seiten nach oben - Fasern und
// Ringfalten haben eine Richtung), grosse Kachel (~1 m) plus feine Detailnormalen. Dazu je Punkt
// (uv): Verdeckung -> Schmutz in Falten und Ritzen, Woelbung -> abgegriffene, hellere Kanten; und
// Staub auf allem, was nach oben zeigt.
GBufferOut shadeEntity(GpuMaterial m, float3 bp, float3 bn, float3 N, float2 cav)
{
    const uint layer = m.layers & 0xFFu, micro = (m.layers >> 8) & 0xFFu;
    const float4 li = gLayer[kEntLayerSlot + layer], lm = gLayer[kMicroLayerSlot + micro];
    const float3 q = bp / li.x, qm = bp / lm.x;
    bn = normalize(bn);
    float3 bw = pow(abs(bn), 5.0);
    bw /= (bw.x + bw.y + bw.z);
    const float2 uv[3] = {q.yz, q.xz, q.xy};
    const float2 mv[3] = {qm.yz, qm.xz, qm.xy};
    float3 alb = 0.0, nb = 0.0;
    float2 orm = 0.0;
    [unroll] for (int k = 0; k < 3; ++k)
    {
        const float w = bw[k];
        if (w < 0.01)
            continue;
        alb += gEntAlbedo.Sample(gAniso, float3(uv[k], layer)).rgb * w;
        orm += gEntOrm.Sample(gAniso, float3(uv[k], layer)).xy * w;
        float2 t2 = (gEntNormal.Sample(gAniso, float3(uv[k], layer)).xy * 2.0 - 1.0) * li.y +
                    (gEntMicro.Sample(gAniso, float3(mv[k], micro)).xy * 2.0 - 1.0) * lm.y;
        const float tz = sqrt(saturate(1.0 - dot(t2, t2)));
        float3 n;
        if (k == 0)
            n = float3(tz * bn.x, t2.x + bn.y, t2.y + bn.z);
        else if (k == 1)
            n = float3(t2.x + bn.x, tz * bn.y, t2.y + bn.z);
        else
            n = float3(t2.x + bn.x, t2.y + bn.y, tz * bn.z);
        nb += normalize(n) * w;
    }
    nb = normalize(nb);
    // Ruhe- -> aktuelle Normale (Rodrigues)
    float3 ax = cross(bn, N);
    const float sn = length(ax), cs = dot(bn, N);
    float3 n = nb;
    if (sn > 1e-4)
    {
        ax /= sn;
        n = nb * cs + cross(ax, nb) * sn + ax * dot(ax, nb) * (1.0 - cs);
    }
    else if (cs < 0.0)
        n = -nb;
    const float cavity = saturate(cav.x), edge = saturate(cav.y);
    float3 col = alb * lerp(1.0, 0.5, cavity);
    col = lerp(col, saturate(col * 1.45 + 0.015), edge * 0.55);
    const float dust = saturate((bn.z - 0.35) * 1.8) * m.variation * (0.6 + 0.4 * (1.0 - cavity));
    col = lerp(col, float3(0.40, 0.385, 0.35), dust * 0.32);
    GBufferOut o;
    o.albedo = float4(saturate(col * m.albedo), orm.x * lerp(1.0, 0.55, cavity));
    o.normal = float4(normalize(n) * 0.5 + 0.5, 0.0);
    o.misc = float4(saturate(orm.y * m.roughness + cavity * 0.12 + dust * 0.18 - edge * 0.12), saturate(m.metalness), 0.0, 1.0);
    // Leuchten (Augen und Zaehne des Smilers, Augen des Hounds): Risse und Ritzen bleiben dunkler;
    // Staerke je Bild aus gHighlight
    float3 glow = 0.0;
    if (m.emissive > 0.0)
        glow = col * m.albedo * (m.emissive * gHighlight * (1.0 - 0.6 * cavity) * saturate(dot(alb, float3(0.3, 0.5, 0.2)) * 3.2 - 0.25));
    o.emissive = glow;
    return o;
}

// V6: Glasfront des Automaten mit Faechern dahinter (Interior Mapping).
// Der Sehstrahl laeuft im Tangentenraum der Scheibe in einen Kasten (Fenster x Tiefe): zuerst
// trifft er die Ebene der Produkte (Ausschnitt-Grafik mit Maske in der Hoehe), sonst Rueckwand,
// Seitenwaende oder Boden/Decke des Fachs. Die Faecher leuchten (Emission), die Scheibe selbst ist
// fast schwarz und sehr glatt - sie spiegelt Leuchten und Raum (SSR) und zeigt Fingerspuren.
// Masse wie render/vending_layout.hpp.
static const float kWinW = 0.60, kWinH = 0.98, kRowsN = 5.0, kBoxDepth = 0.5, kProductZ = 0.07;

GBufferOut glassInterior(GpuMaterial m, uint layer, float2 uv, float3 wpos, float3 N, float3 T, float3 B)
{
    float3 V = normalize(gCameraPos.xyz - wpos);
    float3 r = -float3(dot(V, T), dot(V, B), dot(V, N));  // x rechts, y nach unten, z aus der Scheibe
    r.z = min(r.z, -0.04);
    float3 p0 = float3(uv.x * kWinW, uv.y * kWinH, 0.0);
    const float rh = kWinH / kRowsN;
    float2 gx = ddx(uv), gy = ddy(uv);
    float3 radiance = 0.0;
    // 1) Produkte (deckend, wo die Maske gesetzt ist)
    float3 pp = p0 + r * (-kProductZ / r.z);
    bool hit = false;
    if (pp.x > 0.0 && pp.x < kWinW && pp.y > 0.0 && pp.y < kWinH)
    {
        float2 tuv = pp.xy / float2(kWinW, kWinH);
        float mask = gOrmTex.SampleGrad(gAniso, float3(tuv, layer), gx, gy).a;
        if (mask > 0.45)
        {
            float3 alb = gAlbedoTex.SampleGrad(gAniso, float3(tuv, layer), gx, gy).rgb;
            float rowY = frac(pp.y / rh);
            float light = 0.55 + 0.45 * (1.0 - rowY);  // Leuchtband oben im Fach
            radiance = alb * light;
            hit = true;
        }
    }
    if (!hit)
    {
        // 2) Fach: Reihe am Schnittpunkt mit der Produktebene (am Rand an der Scheibe)
        float rowRef = (pp.y > 0.0 && pp.y < kWinH) ? pp.y : p0.y;
        float row = clamp(floor(rowRef / rh), 0.0, kRowsN - 1.0);
        float yTop = row * rh, yBot = yTop + rh;
        float tBack = -kBoxDepth / r.z;
        float tSide = r.x > 0.0 ? (kWinW - p0.x) / r.x : (r.x < 0.0 ? -p0.x / r.x : 1e9);
        float tRow = r.y > 0.0 ? (yBot - p0.y) / r.y : (r.y < 0.0 ? (yTop - p0.y) / r.y : 1e9);
        tSide = tSide > 0.0 ? tSide : 1e9;
        tRow = tRow > 0.0 ? tRow : 1e9;
        float t = min(tBack, min(tSide, tRow));
        float3 h = p0 + r * t;
        float depth = saturate(-h.z / kBoxDepth);
        float3 c;
        if (t == tRow && r.y > 0.0)       c = float3(0.16, 0.165, 0.175) * (0.75 + 0.25 * frac(h.x * 40.0));  // Fachboden (Blech, geriffelt)
        else if (t == tRow)               c = (h.z > -0.05) ? float3(2.2, 2.15, 2.0) : float3(0.05, 0.05, 0.055);  // Leuchtband / Unterseite
        else if (t == tSide)              c = float3(0.045, 0.045, 0.05);
        else                              c = float3(0.055, 0.052, 0.05) * (0.85 + 0.15 * frac(h.x * 8.0));  // Rueckwand
        float rowY = saturate((h.y - yTop) / rh);
        float light = (0.35 + 0.65 * exp(-depth * 2.2)) * (0.6 + 0.4 * (1.0 - rowY));
        radiance = c * light;
    }
    // Scheibe: Fingerspuren in Griffhoehe, Staub am unteren Rand
    float3 mac = gMacroTex.Sample(gLinearWrap, wpos.xy * 0.9 + wpos.z * 0.7).rgb;
    float smudge = smoothstep(0.55, 0.85, mac.r) * smoothstep(0.2, 0.75, uv.y);
    float dust = smoothstep(0.85, 1.0, uv.y);
    GBufferOut o;
    o.albedo = float4(0.02.xxx + dust * 0.1, 1.0);
    o.normal = float4(N * 0.5 + 0.5, 0.0);
    o.misc = float4(saturate(0.04 + 0.35 * smudge + 0.4 * dust), 0.0, 0.0, 1.0);
    o.emissive = radiance * m.albedo * (m.emissive * gFogParams.w) * (1.0 - 0.25 * smudge - 0.5 * dust);
    return o;
}

// V11: glattes Wertrauschen fuer den Rauchrand
float smokeHash(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);
}
float smokeNoise(float3 p)
{
    const float3 c = floor(p), f = frac(p), u = f * f * (3.0 - 2.0 * f);
    const float a = lerp(lerp(smokeHash(c), smokeHash(c + float3(1, 0, 0)), u.x),
                         lerp(smokeHash(c + float3(0, 1, 0)), smokeHash(c + float3(1, 1, 0)), u.x), u.y);
    const float b = lerp(lerp(smokeHash(c + float3(0, 0, 1)), smokeHash(c + float3(1, 0, 1)), u.x),
                         lerp(smokeHash(c + float3(0, 1, 1)), smokeHash(c + float3(1, 1, 1)), u.x), u.y);
    return lerp(a, b, u.z);
}

GBufferOut PSMain(VSOut i, bool front : SV_IsFrontFace)
{
    // V6: eigener Koerper - Teile direkt vor der Kamera (Schultern, Hals) weich ausblenden
    // (gerastertes Ausblenden, das TAA glaettet), sonst fuellen sie riesig das Bild
    if (gAttached > 0.5 && gWave.w < 0.5)
    {
        float dc = length(i.wpos - gCameraPos.xyz);
        float fade = saturate((dc - 0.10) / 0.07);
        if (ign(i.svpos.xy, gRingParams.z) >= fade)
            discard;
    }
    // V13: Haut und Aermel des Koerpers in der Kugel ums Handgelenk der Hand mit Skelett (die Hand deckt dort)
    if (gClip.w > 0.0 && dot(i.wpos - gClip.xyz, i.wpos - gClip.xyz) < gClip.w * gClip.w)
        discard;
    GpuMaterial m = gMaterials[gMaterial];
    float3 N = normalize(i.n);
    // V11: Rauch (Smiler, Gestalt): eine Saeule tieferer Dunkelheit, die den Hintergrund nur teilweise
    // verdeckt. Gerastert ausduennen - zum Umriss, zum Saum, zum Scheitel, an abstehenden Zungen und mit
    // Abstand vom Gesicht; dicht bleibt nur das Dunkel hinter Augen und Zaehnen. Das Muster treibt langsam
    // aufwaerts. TAA mischt es ueber viele Bilder zu weichem Rauch (Pixel gelten als ruhend, siehe unten).
    const bool smoke = ((m.layers >> 29) & 1u) != 0u;
    if (smoke)
    {
        const float3 bp = i.bindPos;
        const float facing = abs(dot(N, normalize(gCameraPos.xyz - i.wpos)));
        const float n = smokeNoise(bp * float3(6.0, 6.0, 2.4) - float3(0.0, 0.0, gCameraPos.w * 0.45));
        // V13: die Gestalt (Bit 31) bleibt im Koerper dicht - ein Mensch im Gegenlicht; nur Rand, Saum und Fasern duennen
        // aus, und der Rand etwas schmaler als beim Smiler (die Loecher an Schluesselbein und Achsel waren Luecken im Modell)
        const bool silhouette = (m.layers >> 31) != 0u;
        const float rim = silhouette ? 1.0 - smoothstep(0.04, 0.45, facing) : 1.0 - smoothstep(0.06, 0.6, facing);  // weicher Rand, innen dicht
        const float hem = 1.0 - smoothstep(0.08, 0.7, bp.z);
        const float crown = smoothstep(1.98, 2.22, bp.z);
        const float stray = smoothstep(0.3, 0.55, length(bp.xy));
        const float body = silhouette ? 0.0 : 0.42 * smoothstep(0.28, 0.75, length((bp - float3(0.12, 0.0, 1.83)) * float3(1.0, 0.75, 1.0)));
        const float thin = saturate(max(max(rim * 0.95, body), max(max(hem, crown), stray)) * (0.55 + 0.9 * n) - 0.06);
        if (ign(i.svpos.xy, gRingParams.z) < thin)
            discard;
    }
    // V13.3: durchscheinende Haut (Wrangler, world_renderer.hpp kMatMembraneShift): Bits 16-23 = Deckkraft. Gerastert ausduennen;
    // schraeg gesehen (laengerer Weg durch die Haut) dichter. Das TAA mischt es zu einer milchigen Haut, durch die man das Innere sieht
    const uint memb = (m.layers >> 16) & 0xFFu;
    float membWin = 0.0;
    if (memb != 0u && ((m.layers >> 26) & 1u))
    {
        const float facing = abs(dot(N, normalize(gCameraPos.xyz - i.wpos)));
        // duenne Stellen ("Fenster", ~30 cm, an der Haut haftend): dort sieht man klar hinein
        const float3 bq = i.bindPos;  // zwei gedrehte Oktaven - keine achsparallelen Kanten
        const float wn = 0.62 * smokeNoise(float3(bq.x * 0.8 + bq.y * 0.6, bq.y * 0.8 - bq.x * 0.6, bq.z) * 3.3 + 17.0) +
                         0.38 * smokeNoise(float3(bq.x * 0.6 - bq.z * 0.8, bq.y, bq.z * 0.6 + bq.x * 0.8) * 6.7 + 5.0);
        membWin = smoothstep(0.52, 0.7, wn);
        const float op = lerp(1.0, (memb / 255.0) * (1.0 - 0.62 * membWin), smoothstep(0.12, 0.7, facing));
        if (ign(i.svpos.xy, gRingParams.z) >= op)
            discard;
    }
    float3 T = normalize(i.t.xyz - N * dot(i.t.xyz, N));
    float3 B = cross(N, T) * i.t.w;
    uint layer = m.layers & 0xFFu;
    if ((m.layers >> 26) & 1u)  // V10: die Wesen
    {
        GBufferOut g = shadeEntity(m, i.bindPos, i.bindN, N, i.uv);
        g.normal.w = gAttached;
        if (memb != 0u)  // V13.3: durchscheinend - in der Mitte schimmert das dunkle Innere durch, der Rand leuchtet hell
        {
            const float facing = abs(dot(N, normalize(gCameraPos.xyz - i.wpos)));
            const float core = smoothstep(0.3, 0.95, facing) * (1.0 - 0.5 * saturate(i.uv.x));
            g.albedo.rgb = lerp(g.albedo.rgb, g.albedo.rgb * float3(0.4, 0.27, 0.3), core * (0.5 + 0.3 * membWin));
            g.misc.r = lerp(g.misc.r, 0.12, membWin * 0.6);  // die duennen Stellen glaenzen nass
            g.albedo.rgb = lerp(g.albedo.rgb, saturate(g.albedo.rgb * 1.4 + 0.03), (1.0 - smoothstep(0.08, 0.45, facing)) * 0.55);
        }
        // V13: Haut der Wesen laesst Licht eindringen (in Ritzen weniger)
        if ((m.layers >> 30) & 1u)
            g.misc.a = 1.0 - 0.7 * (1.0 - 0.6 * saturate(i.uv.x));
        return g;
    }
    if ((m.layers >> 25) & 1u)  // V10: Figuren (Haut, Stoff, Haar)
    {
        GBufferOut g = shadeTriplanar(m, layer, i.bindPos, i.bindN, N, i.uv);
        g.normal.w = smoke ? 0.0 : gAttached;  // V11: Rauch mit langem Verlauf (zieht leicht nach)
        // V13: Haut laesst Licht eindringen (Anteil im Glanzkanal, lighting.hlsl); in Falten und Ritzen weniger
        if ((m.layers >> 30) & 1u)
            g.misc.a = 1.0 - 0.8 * (1.0 - 0.5 * saturate(i.uv.y));
        return g;
    }
    if ((m.layers >> 24) & 1u)  // V6: Glasfront mit Faechern (Automat)
    {
        GBufferOut g = glassInterior(m, layer, i.uv, i.wpos, N, T, B);
        g.normal.w = gAttached;
        return g;
    }
    // Objekt-UVs sind bereits in Textureinheiten: Ebene mit Massstab 1 verwenden
    float2 uvm = i.uv * gLayer[layer].x;
    GBufferOut o = shadeSurface(m, layer, uvm, uvm, i.wpos, N, T, B, 0u, 0u, 1.0);
    // sanftes Aufleuchten, wenn das Item anvisiert wird
    o.emissive += o.albedo.rgb * gHighlight * 0.9;
    o.normal.w = gAttached;  // Markierung (2-Bit-Alpha des Normalenpuffers)
    return o;
}
