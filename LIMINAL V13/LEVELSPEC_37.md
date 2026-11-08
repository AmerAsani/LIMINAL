# LEVELSPEC 37 – „Sublimity“

- **Quelle:** https://backrooms-wiki.wikidot.com/level-37
- **Revision:** 52, zuletzt geändert am 18.08.2026, 02:36. Abgerufen am 26.10.2026.
- **Autor:** egglord; Kritik: Sariastuff.
- **Bilder der Seite:** u/AnarkyMusic (CC BY-SA 4.0, „Poolrooms“ 1–4). Nicht verwendet.
- **Lizenz:** CC BY-SA 3.0. Nur Fakten in eigenen Worten (`LICENSES.md`).

## 1. Was das Wiki belegt (eigene Worte)

**Bau**
- Ein großer, zusammenhängender Komplex aus Räumen und Gängen, geflutet mit leicht warmem, sanft bewegtem Wasser.
- Der Zweck ist unklar, die Anordnung scheinbar willkürlich; makellos, ohne Schäden.
- Die Bereiche sind sehr verschieden in Größe und Form: gleichförmige Becken und Flure, offene, unregelmäßige Räume. Die Verbindungen ergeben keinen Sinn.
- Fast alles ist mit weißen Keramikfliesen belegt, gleich und unbeschädigt, praktisch unzerstörbar.
- Die einzige andere Farbe ist das Blaugrün des Wassers.

**Wasser und Licht**
- Wasser lauwarm und blaugrün. Viele offene Wasserflächen haben keinen Rand zum Herausklettern.
- Säulen ragen in manchen Bereichen aus dem Wasser, zu viele für einen Zweck.
- Manche Treppen führen direkt in tiefe Gruben voller Wasser. Tiefe Bereiche erreicht man über Treppen oder Löcher unter Wasser; sie sind schlecht beleuchtet und haben wenige Ausgänge.
- Licht kommt aus unregelmäßigen Winkeln; manche Bereiche sind ganz dunkel.
- Spuren von Bittersalz und unbekannte Stoffe im Wasser wirken entspannend.
- Das Wasser kräuselt sich ständig leicht, auch ungestört.

**Klang**
- Geräusche verstummen plötzlich, es hallt nicht normal nach, Sprache klingt gedämpft.
- Das Wasser ist meist ruhig, selten so laut, dass es alles übertönt.

**Empfinden**
- Viele spüren Ruhe und Frieden, andere Furcht, besonders nahe den dunklen Ecken.

**Wesen und Gefahren**
- Keine Wesen, nie andere Wanderer.
- Hauptgefahr ist Ertrinken, besonders in tiefen Gruben und dunklen Bereichen ohne Ausgang.

**Gruppen:** keine Stützpunkte.

**Ein- und Ausgänge:** Die Seite nennt Level 58, 119, 233, 130, 25, 43 – keines davon gehört zur Auswahl.

## 2. Was LIMINAL bewusst ergänzt

- **Zugang** (`LEVELAUSWAHL.md`, eigene Ergänzung):
  - Der Metro-Wagen aus Level 903 hält in einem gefluteten Tunnel; sein vorderes Ende steckt in der Fliesenwand. Wiki 903: die Wagen fahren durch feste Materie.
  - Das hintere Wagenende (Bildzeichen Wagen) führt zurück nach Level 903.
  - Die Stufen an den Wagentüren sind gefliest.
- **Aufteilung:** 338 Räume aus einer festen Zufallsteilung (BSP), alle knöcheltief geflutet (0,3 m):
  - 12 Hallen mit einem Säulenraster alle 3 m;
  - 34 Gruben 4,5 m tief, in die die Engine Treppen an den Öffnungen legt;
  - Gänge, 31 dunkle Räume und Gänge.
- **Licht:**
  - Wandleuchten tief über dem Wasser, schräg in den Raum;
  - Deckenlicht aus einer Ecke;
  - Licht von unten aus dem Wasser;
  - in den Gruben meist kein Licht (40 % mit Licht von unten).
- **Wasser:** Die Engine kennt keine durchsichtigen Flächen. Flaches Wasser ist deshalb ein helles Türkis (so wirkt klares Wasser über weißen Fliesen), tiefes ein dunkles Blaugrün.
- **Wasser (V13.3, Abnahme):** Wiki: das Wasser kräuselt sich ständig leicht; Licht aus seltsamen Winkeln.
  - Lichtnetz (Kaustik): auf dem Grund unter dem Wasser, unter Wasser in den Gruben (mit der Tiefe schwächer) und an
    Decke, Säulen und Wänden über Lampen unter Wasser (nah am Wasser scharf, nach oben weicher).
  - Gespiegeltes Licht: die Wasserfläche wirft das Licht der Lampen über dem Wasser – auch der Taschenlampe – als
    wanderndes Netz an Wände und Decke (Fresnel, Schatten).
  - Lampen unter Wasser liegen als Schein in der Fläche, mit hellem, gemustertem Grund ringsum (Totalreflexion an der
    Unterseite).
  - Tiefe: der Boden erscheint um die Wassertiefe versetzt (Brechung) und in der Tiefe weicher; schmale Wasserlinie an
    der Wand; Tropfen schlagen alle 7–13 s ein (Ringe im Glanz).
  - Umsetzung: `shaders/lighting.hlsl`, `shaders/gbuffer.hlsli`; Wasserspiegel klarer Becken im Umgebungs-Ring, Tiefe
    im Material der Wasserfläche. Nur für Becken (`water_look` 1, 2); das dunkle Meer (Level 0) bleibt unverändert.
- **Verstand:**
  - Im Wasser erholt er sich (−0,2 bis −0,35 je Sekunde, Wiki: entspannend).
  - In Gruben und dunklen Bereichen sinkt er (+0,15 bis +0,2, Wiki: Furcht).
- **Klang:** Becken-Rauschen, Luft, Tropfen; ein kurzer Hall, der abrupt abbricht („gate“).
- **Funde:** keine (Wiki: keine Gegenstände). Eine Notiz auf einer Stufe am Wagen.

## 3. Konflikte und Grenzen

- **Ausgänge** zu 130, 43 und 233: nicht dargestellt (nicht in der Auswahl). Der Wagen ist der einzige Weg hinaus.
- **Löcher unter Wasser:** nicht gebaut; tiefe Bereiche erreicht man über die Treppen.
- **Ränder:** Die Engine lässt aus jeder Grube über die Treppen heraus. Wasserflächen ganz ohne Rand gibt es deshalb nicht.
- **Ertrinken:** wie in V11. Wer zu lange taucht, ertrinkt; geprüft im Test (Grube in Level 37).
