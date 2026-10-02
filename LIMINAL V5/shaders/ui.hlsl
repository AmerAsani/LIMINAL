// Benutzeroberflaeche: gebuendelte 2D-Rechtecke.
//   Modus 0: Flaeche/abgerundetes Rechteck mit Rand (SDF, weiche Kanten)
//   Modus 1: Text (Distanzfeld-Schriftatlas)
//   Modus 2: Bild (Vorschaubilder, Icons)
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

// Alle Farben sind vormultipliziert (premultiplied alpha), linear.
float4 PSMain(VSOut i) : SV_Target
{
    uint mode = (uint)i.extra.x;
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
