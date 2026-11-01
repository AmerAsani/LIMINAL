// Abschluss: Belichtung, Bloom, Tonemapping (AgX), Farbgebung, Vignette,
// Filmkorn, roter Rand bei 0 % Sanity (wie V4), Menue-Hintergrund und die
// Retro-Darstellungen aus V4 (Terminal-Halbbloecke, ASCII, Monochrom).
#include "common.hlsli"

Texture2D gHdr : register(t0);
Texture2D gBloom : register(t1);
StructuredBuffer<float> gExposureBuf : register(t2);
Texture2D gBlurred : register(t3);
Texture2D<float> gGlyphs : register(t4);   // 20 Zeichen nebeneinander (Helligkeitsstufen)
SamplerState gLinearClamp : register(s2);
SamplerState gPointClamp : register(s3);

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// --- AgX (Minimal-Variante nach B. Wrensch) ------------------------------------------------
float3 agxContrast(float3 x)
{
    float3 x2 = x * x, x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
float3 agxTonemap(float3 v)
{
    const float3x3 m = float3x3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                                0.0784335999999992, 0.878468636469772, 0.0784336,
                                0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const float3x3 mi = float3x3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                                 -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                                 -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    const float minEv = -12.47393, maxEv = 4.026069;
    v = mul(max(v, 1e-10), m);
    v = (clamp(log2(v), minEv, maxEv) - minEv) / (maxEv - minEv);
    v = saturate(agxContrast(v));
    // Look: etwas mehr Kontrast, leicht zurueckgenommene Saettigung (ausgeblichene Neonroehren)
    float l = luminance(v);
    v = pow(max(v, 0.0), 1.08);
    v = l + 1.08 * (v - l);
    v = mul(v, mi);
    return pow(saturate(v), 2.2);  // linear fuer das sRGB-Ziel
}

float3 toDisplay(float3 lin) { return pow(max(lin, 0.0), 1.0 / 2.2); }
float3 toLinear(float3 d) { return pow(max(d, 0.0), 2.2); }

// V6: kontrastadaptives Nachschaerfen (nach AMD CAS) gegen die Weichheit von TAA und
// interner Aufloesung. Gerechnet wird in einem komprimierten Helligkeitsraum (x / (1 + x)),
// damit helle Leuchten keine Ueberschwinger und Saeume bekommen.
float3 casComp(float3 x) { return x / (1.0 + x); }
float3 casExpand(float3 y) { return y / max(1.0 - y, 1e-4); }
float3 sharpen(float3 c, float2 uv, float exposure, float amount)
{
    float2 t = gScreen.zw;  // Texel der internen Aufloesung
    float3 cc = casComp(c * exposure);
    float3 n = casComp(gHdr.SampleLevel(gLinearClamp, uv + float2(0.0, -t.y), 0).rgb * exposure);
    float3 s = casComp(gHdr.SampleLevel(gLinearClamp, uv + float2(0.0, t.y), 0).rgb * exposure);
    float3 e = casComp(gHdr.SampleLevel(gLinearClamp, uv + float2(t.x, 0.0), 0).rgb * exposure);
    float3 w = casComp(gHdr.SampleLevel(gLinearClamp, uv + float2(-t.x, 0.0), 0).rgb * exposure);
    float3 mn = min(cc, min(min(n, s), min(e, w)));
    float3 mx = max(cc, max(max(n, s), max(e, w)));
    float3 amp = sqrt(saturate(min(mn, 1.0 - mx) / max(mx, 1e-4)));
    float3 wgt = -amp * lerp(0.125, 0.2, amount);
    float3 r = saturate((cc + (n + s + e + w) * wgt) / (1.0 + 4.0 * wgt));
    return casExpand(min(r, 0.999)) / max(exposure, 1e-6);
}

float3 sceneColor(float2 uv, float exposure)
{
    float3 c;
    float ca = gPost2.w;
    if (ca > 0.0)
    {
        float2 d = (uv - 0.5) * ca * 0.006;
        c.r = gHdr.SampleLevel(gLinearClamp, uv - d, 0).r;
        c.g = gHdr.SampleLevel(gLinearClamp, uv, 0).g;
        c.b = gHdr.SampleLevel(gLinearClamp, uv + d, 0).b;
    }
    else
        c = gHdr.SampleLevel(gLinearClamp, uv, 0).rgb;
    if (gTaa.z > 0.0)
        c = sharpen(c, uv, exposure, gTaa.z);
    // V6: leichte Halation - weites Streulicht um Leuchten ist etwas waermer als die Quelle
    float3 bloom = gBloom.SampleLevel(gLinearClamp, uv, 0).rgb * float3(1.045, 0.99, 0.92);
    c = lerp(c, bloom, gPost3.w);
    return agxTonemap(c * exposure);
}

// Roter Bildschirmrand (Formel aus V4 renderer.red_edge)
float redMask(float2 uv, float level, float pulse)
{
    if (level < 0.02) return 0.0;
    float L = level;
    float start = 1.12 - 0.78 * L;
    float span = max(0.05, 1.42 - start);
    float2 n = uv * 2.0 - 1.0;
    float s = (sqrt(n.x * n.x * 0.85 + n.y * n.y) - start) / span;
    if (s <= 0.0) return 0.0;
    float q = pow(saturate(s), 1.4) * (0.45 + 0.55 * L);
    return saturate(q + pulse * (2.0 / 12.0) * step(0.001, q));
}

// V13.3 (KREATUR_Crawler.md, KREATUR_L1_Erscheinungen.md, KREATUR_GameMaster.md): gExtra.y Pilz (Sicht schwimmt,
// weisser Flaum waechst vom Rand ein, Fieber), gExtra.z Garden (gruener Rand aus Blattwerk), gExtra.w Bildschirmgesicht
// der Spielleiterin (Ausdruck * 2 + Staerke 0..1)
float vn2(float2 p)
{
    float2 c = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    uint2 u = (uint2)(int2)(c + 4096.0);
    float a = hash01(u.x * 73856093u ^ u.y * 19349663u), b = hash01((u.x + 1u) * 73856093u ^ u.y * 19349663u);
    float c0 = hash01(u.x * 73856093u ^ (u.y + 1u) * 19349663u), d = hash01((u.x + 1u) * 73856093u ^ (u.y + 1u) * 19349663u);
    return lerp(lerp(a, b, f.x), lerp(c0, d, f.x), f.y);
}
float fbm2(float2 p) { return 0.5 * vn2(p) + 0.3 * vn2(p * 2.07 + 13.1) + 0.2 * vn2(p * 4.31 + 7.7); }
float sdSeg(float2 p, float2 a, float2 b)
{
    float2 pa = p - a, ba = b - a;
    return length(pa - ba * saturate(dot(pa, ba) / dot(ba, ba)));
}
// Bildschirmgesicht: zwei rote X, darunter ein Mund (0 Grinsen, 1 verzogen, 2 lachend offen, 3 leer)
float3 jesterFace(float2 uv, float3 col, float inten, float expr, float t)
{
    float aspect = gPost3.y / max(gPost3.z, 1.0);
    float2 p = (uv - 0.5) * float2(aspect, 1.0);
    float scan = 0.82 + 0.18 * sin(uv.y * gPost3.z * 1.6);
    float3 glass = float3(0.012, 0.014, 0.02) * scan + 0.025 * vn2(uv * 300.0 + t * 40.0);
    float glow = 0.0;
    if (expr < 2.5)
    {
        for (int k = 0; k < 2; ++k)
        {
            float2 e = float2(k == 0 ? -0.2 : 0.2, -0.1);
            float2 q = p - e;
            float d = min(sdSeg(q, float2(-0.075, -0.075), float2(0.075, 0.075)), sdSeg(q, float2(-0.075, 0.075), float2(0.075, -0.075)));
            glow = max(glow, smoothstep(0.022, 0.012, d));
            glow = max(glow, 0.35 * smoothstep(0.08, 0.0, d));
        }
        float2 m = p - float2(0.0, 0.14);
        float dm;
        if (expr < 0.5)  // Cartoon-Grinsen
            dm = abs(length(m - float2(0.0, -0.17)) - 0.26) + step(m.y + 0.03, 0.0) * 1.0;
        else if (expr < 1.5)  // verzogen
            dm = abs(length(m - float2(0.0, 0.2)) - 0.24) + step(0.0, m.y - 0.035) * 1.0 + step(0.17, abs(m.x));
        else  // offen lachend
            dm = abs(length(m * float2(1.0, 1.6) - float2(0.0, 0.02)) - 0.17);
        glow = max(glow, smoothstep(0.022, 0.012, dm));
        glow = max(glow, 0.3 * smoothstep(0.07, 0.0, dm));
    }
    float3 face = glass + float3(1.0, 0.09, 0.06) * glow * (0.85 + 0.15 * sin(t * 37.0)) * 1.6;
    face *= 1.0 - 0.6 * pow(saturate(dot(p, p) * 1.2), 1.4);
    return lerp(col, face, inten);
}

float4 PSMain(PSIn i) : SV_Target
{
    float2 uv = i.uv;
    float2 outSize = gPost3.yz;
    float exposure = gExposureBuf[0] * exp2(gPost2.x);
    uint mode = (uint)gPost3.x;
    float3 col;

    const float fung = gExtra.y, gard = gExtra.z, tNow = gCameraPos.w;
    if (mode == 0u)
    {
        // V13.3: Pilz - die Sicht schwimmt; spaeter ein versetztes Doppelbild
        float2 suv = uv;
        if (fung > 0.001)
            suv += fung * fung * 0.006 * float2(sin(tNow * 1.3 + uv.y * 9.0), cos(tNow * 1.05 + uv.x * 7.0));
        col = sceneColor(suv, exposure);
        if (fung > 0.6)
            col = lerp(col, sceneColor(suv + float2(0.012, 0.004) * sin(tNow * 0.7), exposure), (fung - 0.6) * 0.6);
    }
    else
    {
        // Retro-Darstellung: Zeichenzellen wie im Terminal von V4 (Zellen 1:2)
        float cellW = max(4.0, round(outSize.y / 110.0));
        float2 cell = float2(cellW, cellW * 2.0);
        float2 pix = uv * outSize;
        float2 c0 = floor(pix / cell);
        float2 local = frac(pix / cell);
        if (mode == 1u)
        {
            // Halbblock "▀": obere und untere Zellhaelfte mit eigener Farbe, 15-Bit-Farben
            float2 half = float2(0.5, local.y < 0.5 ? 0.25 : 0.75);
            float3 c = sceneColor((c0 + half) * cell / outSize, exposure);
            col = toLinear(floor(toDisplay(c) * 31.0 + 0.5) / 31.0);
        }
        else
        {
            float3 c = sceneColor((c0 + 0.5) * cell / outSize, exposure);
            float lum = luminance(toDisplay(c));
            float levels = mode == 2u ? 8.0 : 10.0;
            float gi = min(levels - 1.0, floor(lum * (mode == 2u ? 9.0 : 13.0)));
            float base = mode == 2u ? 0.0 : 8.0;
            float2 guv = float2((base + gi + local.x) / 20.0, local.y);
            float g = gGlyphs.SampleLevel(gLinearClamp, guv, 0);
            float3 fg = mode == 2u ? c * 1.25 + 0.02 : lum.xxx * float3(0.85, 1.0, 0.85);
            float3 bg = mode == 2u ? c * 0.4 : 0.0;
            col = lerp(bg, fg, g);
        }
    }

    // Farbgebung. V6: Split-Toning in Anzeigewerten - Schatten leicht kuehl-gruenlich (Streulicht
    // der Neonroehren), Lichter warm; die Mitteltoene bleiben neutral. Gibt Tiefe ohne bunter zu werden.
    if (mode == 0u)
    {
        float3 d = toDisplay(col);
        float ld = luminance(d);
        float ws = 1.0 - smoothstep(0.02, 0.42, ld);
        float wh = smoothstep(0.5, 0.95, ld);
        d *= lerp(1.0.xxx, float3(0.95, 1.0, 1.02), ws) * lerp(1.0.xxx, float3(1.03, 1.0, 0.94), wh);
        col = toLinear(d);
    }
    else
        col *= float3(1.015, 1.0, 0.975);
    // Entsaettigung bei schwindendem Verstand
    float l = luminance(col);
    col = lerp(col, l.xxx, gPost.w);

    // Vignette
    float2 cv = uv - 0.5;
    col *= 1.0 - gPost2.z * pow(saturate(dot(cv, cv) * 2.1), 1.6);

    // Menue: weichgezeichneter, abgedunkelter Hintergrund
    if (gPost.z > 0.001)
    {
        float3 b = agxTonemap(gBlurred.SampleLevel(gLinearClamp, uv, 0).rgb * exposure);
        col = lerp(col, b * 0.32, gPost.z);
    }

    // V13.3: Pilz - wattiger, weisser Belag waechst vom Rand ein, davor feine Faeden, die zur Mitte kriechen; Fieber
    // toent warm. Polarkoordinaten: Faeden laufen radial (Rauschen laengs des Winkels gestaucht)
    if (fung > 0.001)
    {
        const float aspect = gPost3.y / max(gPost3.z, 1.0);
        const float2 p = (uv - 0.5) * float2(aspect, 1.0);
        const float r = length(p) / (0.5 * length(float2(aspect, 1.0)));
        const float ang = atan2(p.y, p.x);
        const float cotton = fbm2(p * 7.0 + 3.7) * 0.65 + fbm2(p * 19.0 + 11.0) * 0.35;
        const float fiber = fbm2(float2(ang * 21.0, r * 2.2 - tNow * 0.004));
        const float front = 1.12 - fung * 0.5;
        const float mass = smoothstep(front - 0.05, front + 0.07, r + (cotton - 0.5) * 0.32);
        const float threads = smoothstep(0.62, 0.8, fiber) * smoothstep(front - 0.26, front, r) * (1.0 - mass);
        float3 d = toDisplay(col);
        d = lerp(d, d * float3(1.06, 0.95, 0.86), smoothstep(0.5, 1.0, fung) * 0.5);
        const float3 white = float3(0.84, 0.83, 0.77) * (0.62 + 0.45 * cotton);
        d = lerp(d, white, mass * (0.6 + 0.38 * fung));
        d = lerp(d, float3(0.88, 0.87, 0.82), threads * 0.55 * fung);
        col = toLinear(d);
    }
    // V13.3: Garden - Blaetter wachsen vom Rand herein (zwei Lagen spitzer Blaetter mit Mittelrippe, dahinter dichtes
    // Laub), unten am dichtesten
    if (gard > 0.001)
    {
        const float aspect = gPost3.y / max(gPost3.z, 1.0);
        const float2 p = (uv - 0.5) * float2(aspect, 1.0);
        const float r = length(p) / (0.5 * length(float2(aspect, 1.0))) * (1.0 + 0.18 * saturate(uv.y - 0.5) * 2.0);
        const float ang = atan2(p.y, p.x);
        float3 d = toDisplay(col);
        d = lerp(d, d * float3(0.9, 1.04, 0.86), gard * 0.35);
        float cover = 0.0, tone = 0.0;
        [unroll] for (int L = 0; L < 3; ++L)
        {
            const float N = 44.0 + 17.0 * L;
            const float a = ang / 6.2831853 * N + 0.37 * L;
            const float ci = floor(a);
            const float fa = frac(a) - 0.5;
            const float h = hash01((uint)(ci + 4096.0) * 747796405u + (uint)L * 2891336453u);
            const float h2 = hash01((uint)(ci + 9000.0) * 2246822519u + (uint)L * 3266489917u);
            const float wob = (fbm2(float2(ang * 1.7 + L * 5.0, 2.0)) - 0.5) * 0.22;
            const float base = 1.08 - gard * (0.4 + 0.07 * L) + (h - 0.5) * 0.09 + wob;
            const float len = 0.06 + 0.08 * h2;
            const float ly = (base - r) / len;                         // 0 Blattgrund, 1 Spitze (zur Mitte)
            const float lx = (fa + (h - 0.5) * 0.7 * ly * ly) / (0.34 + 0.16 * h2);  // gebogen
            const float half = 0.5 * pow(sin(saturate(ly) * 3.14159), 0.8) * (1.0 - 0.3 * ly);
            const float leaf = (1.0 - smoothstep(half - 0.06, half + 0.015, abs(lx))) * step(0.0, ly) * step(ly, 1.0);
            const float rib = (1.0 - smoothstep(0.012, 0.04, abs(lx))) * leaf;
            const float behind = smoothstep(base - 0.02, base + 0.04, r);
            const float tex = fbm2(p * (23.0 + 9.0 * L) + L * 7.1);
            const float t = leaf > behind ? (0.35 + 0.55 * h) * (1.0 - 0.4 * rib) * (0.75 + 0.25 * ly) : 0.15 + 0.35 * tex;
            const float c = max(leaf, behind);
            tone = lerp(tone, t * (0.7 + 0.15 * L), c);
            cover = max(cover, c);
        }
        const float3 leafCol = lerp(float3(0.012, 0.03, 0.01), float3(0.085, 0.18, 0.055), tone);
        d = lerp(d, leafCol, cover * (0.75 + 0.25 * gard));
        col = toLinear(d);
    }
    // V13.3: das Gesicht der Spielleiterin fuellt den Bildschirm
    if (gExtra.w > 0.001)
    {
        float expr = floor(gExtra.w * 0.5);
        col = jesterFace(uv, col, saturate(gExtra.w - expr * 2.0), expr, tNow);
    }

    // Roter Rand mit Herzschlag
    float red = redMask(uv, gPost.x, gPost.y);
    if (red > 0.0)
    {
        float3 d = toDisplay(col);
        d = float3(d.r + (0.82 - d.r) * red, d.g * (1.0 - red) + 0.03 * red, d.b * (1.0 - red) + 0.02 * red);
        col = toLinear(d);
    }

    // Filmkorn und Dithering gegen Farbstufen. V6: Korn in Anzeigewerten, am staerksten in den
    // Mitteltoenen (wie Film), schwach in tiefen Schatten und Lichtern
    float n = ign(i.pos.xy, gRingParams.z) - 0.5;
    float3 disp = toDisplay(col);
    float lg = luminance(disp);
    disp += n * gPost2.y * 0.045 * (0.15 + 3.4 * lg * (1.0 - lg));
    disp += n / 255.0;
    return float4(toLinear(disp), 1.0);
}
