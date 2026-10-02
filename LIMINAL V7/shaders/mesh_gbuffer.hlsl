// G-Buffer-Pass fuer frei platzierte Objekte (Items, Requisiten, V6: eigener Koerper).
#include "gbuffer.hlsli"

cbuffer ObjectCB : register(b2)
{
    float4x4 gWorld;
    uint gMaterial;
    float gHighlight;   // Hervorhebung (Ziel zum Aufnehmen), 0..1
    float gAttached;    // V6: 1 = bewegt sich mit dem Spieler (TAA projiziert diese Pixel anders zurueck)
    float gPad;
};

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
