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
};

// V10: Knochenmatrizen aller Figuren dieses Bildes (je Matrix 3 Zeilen, float3x4)
StructuredBuffer<float4> gBones : register(t0);

// V10: Texturen der Wesen (render/texture_entity.hpp): Farbe (BC1), Normalen (BC5), Verdeckung und
// Rauheit (BC5), feine Detailnormalen. Ebenen-Konstanten ab gLayer[40] (Wesen) und gLayer[48] (Mikro).
Texture2DArray gEntAlbedo : register(t6);
Texture2DArray gEntNormal : register(t7);
Texture2DArray gEntOrm : register(t8);
Texture2DArray gEntMicro : register(t9);
static const uint kEntLayerSlot = 40, kMicroLayerSlot = 48;

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

VSOut VSSkinned(SkinIn i)
{
    float3x4 m = (float3x4)0;
    [unroll] for (int k = 0; k < 4; ++k)
    {
        uint b = gBoneOffset + i.bones[k] * 3u;
        m += i.weights[k] * float3x4(gBones[b], gBones[b + 1u], gBones[b + 2u]);
    }
    float3 p = mul(m, float4(i.pos, 1.0));
    VSOut o;
    float4 w = mul(gWorld, float4(p, 1.0));
    o.svpos = mul(gViewProj, w);
    o.wpos = w.xyz;
    o.n = mul((float3x3)gWorld, mul((float3x3)m, i.normal));
    o.t = float4(mul((float3x3)gWorld, mul((float3x3)m, i.tangent.xyz)), i.tangent.w);
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

GBufferOut PSMain(VSOut i, bool front : SV_IsFrontFace)
{
    // V6: eigener Koerper - Teile direkt vor der Kamera (Schultern, Hals) weich ausblenden
    // (gerastertes Ausblenden, das TAA glaettet), sonst fuellen sie riesig das Bild
    if (gAttached > 0.5)
    {
        float dc = length(i.wpos - gCameraPos.xyz);
        float fade = saturate((dc - 0.10) / 0.07);
        if (ign(i.svpos.xy, gRingParams.z) >= fade)
            discard;
    }
    GpuMaterial m = gMaterials[gMaterial];
    float3 N = normalize(i.n);
    float3 T = normalize(i.t.xyz - N * dot(i.t.xyz, N));
    float3 B = cross(N, T) * i.t.w;
    uint layer = m.layers & 0xFFu;
    if ((m.layers >> 26) & 1u)  // V10: die Wesen
    {
        GBufferOut g = shadeEntity(m, i.bindPos, i.bindN, N, i.uv);
        g.normal.w = gAttached;
        return g;
    }
    if ((m.layers >> 25) & 1u)  // V10: Figuren (Haut, Stoff, Haar)
    {
        GBufferOut g = shadeTriplanar(m, layer, i.bindPos, i.bindN, N, i.uv);
        g.normal.w = gAttached;
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
