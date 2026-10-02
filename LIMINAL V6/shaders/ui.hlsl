// Benutzeroberflaeche: gebuendelte 2D-Rechtecke.
//   Modus 0: Flaeche/abgerundetes Rechteck mit Rand (SDF, weiche Kanten)
//   Modus 1: Text (Distanzfeld-Schriftatlas)
//   Modus 2: Bild (Vorschaubilder, Icons)
//   Modus 3: V6 rundes, gedrehtes Bild (Minimap), weicher Kreisrand
//   Modus 4: V6 Richtungspfeil (Distanzfeld, beliebig gedreht, mit Rand)
//   Modus 5: V6 weicher Sichtkegel
cbuffer UiCB : register(b0)
{
    float2 gViewport;  // Ausgabegroesse in Pixeln
    float2 gUiPad;
};

Texture2D gFont : register(t0);
Texture2D gImage : register(t1);
SamplerState gLinearClamp : register(s2);

struct VSIn
{
    float2 pos : POSITION;       // Pixel
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 rect : TEXCOORD1;     // Modus 0: Rechteckgroesse (w, h), Radius, Randbreite
    float4 extra : TEXCOORD2;    // x Modus, y lokale Koordinate x, z lokale Koordinate y, w Schriftschaerfe
    float4 border : COLOR1;      // Randfarbe
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 rect : TEXCOORD1;
    float4 extra : TEXCOORD2;
    float4 border : COLOR1;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = float4(i.pos / gViewport * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.uv = i.uv;
    o.color = i.color;
    o.rect = i.rect;
    o.extra = i.extra;
    o.border = i.border;
    return o;
}

float sdRoundBox(float2 p, float2 halfSize, float r)
{
    float2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// Abstand zu einem (auch konkaven) Viereck (nach I. Quilez), negativ innen
float sdQuad(float2 p, float2 v0, float2 v1, float2 v2, float2 v3)
{
    float2 v[4] = { v0, v1, v2, v3 };
    float d = dot(p - v[0], p - v[0]);
    float s = 1.0;
    [unroll] for (int k = 0; k < 4; ++k)
    {
        int j = (k + 3) % 4;
        float2 e = v[j] - v[k];
        float2 w = p - v[k];
        float2 b = w - e * saturate(dot(w, e) / dot(e, e));
        d = min(d, dot(b, b));
        bool c0 = p.y >= v[k].y, c1 = p.y < v[j].y, c2 = e.x * w.y > e.y * w.x;
        if ((c0 && c1 && c2) || (!c0 && !c1 && !c2)) s = -s;
    }
    return s * sqrt(d);
}

// Bildschirmrichtung -> Rahmen des Symbols (Winkel 0 = oben, im Uhrzeigersinn)
float2 toLocal(float2 p, float a)
{
    float s = sin(a), c = cos(a);
    return float2(p.x * c + p.y * s, -p.x * s + p.y * c);
}

// Alle Farben sind vormultipliziert (premultiplied alpha), linear.
float4 PSMain(VSOut i) : SV_Target
{
    uint mode = (uint)i.extra.x;
    if (mode == 3u)
    {
        float2 p = i.uv;
        float d = length(p);
        float mask = saturate((1.0 - d) / max(fwidth(d), 1e-4));
        float s = sin(i.rect.w), c = cos(i.rect.w);
        float2 q = float2(p.x * c - p.y * s, p.x * s + p.y * c);
        float4 t = gImage.SampleLevel(gLinearClamp, i.rect.xy + q * i.rect.z, 0);
        return float4(t.rgb * t.a, t.a) * i.color * mask;
    }
    if (mode == 4u)
    {
        float2 q = toLocal(i.uv, i.rect.z);
        float d = sdQuad(q, float2(0.0, -0.8), float2(0.58, 0.64), float2(0.0, 0.3), float2(-0.58, 0.64));
        float aa = max(fwidth(d), 1e-4);
        float inner = saturate(0.5 - d / aa);
        float outer = saturate(0.5 - (d - 0.15) / aa);
        return i.color * inner + i.border * (outer - inner);
    }
    if (mode == 5u)
    {
        float2 p = i.uv;
        float r = length(p);
        float2 dir = float2(sin(i.rect.z), -cos(i.rect.z));
        float ang = acos(clamp(dot(p / max(r, 1e-4), dir), -1.0, 1.0));
        float a = smoothstep(i.rect.y, i.rect.y * 0.55, ang) * smoothstep(1.0, 0.2, r) * smoothstep(0.03, 0.15, r);
        return i.color * a;
    }
    if (mode == 1u)
    {
        float d = gFont.SampleLevel(gLinearClamp, i.uv, 0).r;
        float w = max(fwidth(d), 1e-4) * i.extra.w;
        float a = smoothstep(0.5 - w, 0.5 + w, d);
        return i.color * a;
    }
    if (mode == 2u)
    {
        float4 t = gImage.SampleLevel(gLinearClamp, i.uv, 0);
        return float4(t.rgb * t.a, t.a) * i.color;
    }
    float2 size = i.rect.xy;
    float2 p = i.extra.yz - size * 0.5;
    float r = min(i.rect.z, min(size.x, size.y) * 0.5);
    float d = sdRoundBox(p, size * 0.5, r);
    float aa = max(fwidth(d), 0.5);
    float fill = saturate(0.5 - d / aa);
    float4 c = i.color * fill;
    if (i.rect.w > 0.0)
    {
        float inner = saturate(0.5 - (d + i.rect.w) / aa);
        float edge = fill - inner;
        c = i.color * inner + i.border * edge;
    }
    return c;
}
