# Kreatur: Red Knight (Entity 71) – Level 3

Quellen (nur Wikidot, gelesen am 02.11.2026, CC BY-SA 3.0; eigene Worte, keine Bilder, keine Zitate):
- [entity-71](https://backrooms-wiki.wikidot.com/entity-71) „The Red Knight“, Rev. 44 (zuletzt bearbeitet 11.07.2026), Autor CadaverCommander; Tags u. a. humanoid, unique.
- [level-3](https://backrooms-wiki.wikidot.com/level-3) Rev. 18 (26.09.2026), Vorfallbericht GB-03 (Base Gamma).

## Entscheidung: Körper und Auftritt auf Level 3 – ja

- **Körper:** ja. Die Wesensseite beschreibt ihn als Ritter in Plattenrüstung (Stil des späten 16. Jahrhunderts), mittelgroß und von gewöhnlichem Bau, mit einem Zweihänder, den er wegen der Größe auf der Schulter trägt, manchmal mit Schild, mit rotem Umhang mit Goldsaum, am Ende zerrissen; die Rüstung ist alt und verbeult. Er ist sichtbar, hörbar (das Klirren seiner Schritte), greifbar (er hebt Menschen hoch, wird in Wände geschleudert, blutet aus Einstichen). Kein Mensch und keine Gruppe – ein einzelnes Wesen.
- **Level 3:** ja. Der Vorfallbericht GB-03 der Ebenenseite: Am 10.11.2017 rannte er sichtbar in Panik durch das Hauptatrium von Base Gamma, die Rüstung schwer verbeult, mit Spuren der Purpifizierung. Nur 10 Sekunden Aufnahme blieben, der Rest war auf unbekannte Weise zerstört; viele sahen ihn vorbeilaufen, aber niemand erinnert sich, wie er kam oder ging. Absicht unbekannt; die verbreitete Vermutung: eine antimemetische Kraft oder ein solches Wesen war in der Basis, und er kam, um sich darum zu kümmern. Keine Opfer.
- Damit ist die alte Zeile in `ENTITY_ABGLEICH.md` („nur in Vorfallberichten, bewusst weggelassen“) überholt: er wird gebaut.

## Was die Seiten sagen (eigene Worte)

- **Erscheinung:** Plattenrüstung, wie man sie gegen Ende des 16. Jahrhunderts trug; ein Zweihänder auf der Schulter; manchmal ein Schild; roter Umhang mit Goldsaum, am Ende ausgefranst. Alles gut erhalten, aber sichtlich alt, Dellen in der Rüstung. Durch das Visier sieht man nur Dunkelheit. Er klingt männlich, spricht jede Sprache, redet wenig.
- **Kräfte:** übermenschlich stark und schnell (beim Kampf glühen seine Stiefel rot und schlagen Funken), aber nicht unverwundbar – Berichte von gebrochenen Gliedern, Dellen und Löchern, Hinken; aus Einstichen in der Rüstung sickert in einer Erzählung eine Flüssigkeit „von unbeschreiblicher Farbe“. Er schneidet mit dem Schwert einen leuchtend weißen Riss in die Luft, einen Durchgang zu anderen Ebenen.
- **Verhalten:** Er greift Menschen nicht an; er taucht oft auf, wenn jemand neu in die Backrooms fällt oder in Not ist, stellt sich zwischen Opfer und Wesen und rettet Neuankömmlinge durch seinen Riss. Auf Level 3 (GB-03) dagegen: er rannte in Panik vorbei – vor etwas, das niemand sah.

## Umsetzung

**Rolle:** keine neue `Presence::Role`, kein neues `anim::Entity` – der Ritter ist **Variante 4 der Rolle Wretch** (gleiches Zweibeiner-Skelett `bp`, Modell `ent_redknight`, eigenes Verhalten über `variant == kKnightVariant`). Die Variante zählt nicht als Wretch (Auftrittsplan der Wretches, Ansprechen, Erstsichtung als Bedrohung, Schrittklang ausgenommen). Eine eigene Rolle ist **nicht zwingend** nötig.

**Ablauf** (`gameplay/presence_knight.cpp`, Level-JSON `presence.red_knight_every: 900`; Interpretation des Berichts GB-03):
1. **Anlauf, ungesehen:** selten (Mittel 15 min, erster Lauf nach 7–15 min, höchstens zwei je Besuch, nicht während einer Jagd). Er erscheint außer Sicht 20–33 m Weg entfernt. **Geräusch vor Sicht:** Sabatons auf Beton, nachschlagende Beinlamellen, klirrendes Kettengeflecht, Keuchen im geschlossenen Helm (beim ersten Hören ein Hinweis).
2. **Lauf in Panik** (5 m/s, knapp langsamer als der Sprint des Spielers) auf einem Weg, der 2,5–5 m am Spieler vorbeiführt (nie durch ihn: beide Wegstücke werden vorab abgelaufen, mindestens 2,2 m Abstand); im Lauf sieht er immer wieder über die Schulter zurück.
3. **Halt und Hieb:** am Wegpunkt beim Spieler bremst er (Platten rasseln), dreht sich zurück und haut beidhändig über die Schulter in die **leere Luft** hinter sich – gegen etwas Unsichtbares (die antimemetische Kraft). Nie zum Spieler hin (mindestens 60° daneben), nur ab 2,5 m Abstand und wenn er den Spieler sieht.
4. **Blick:** 0,9 s sieht er den Spieler an, keuchend – dann rennt er weiter.
5. **Riss:** am Ziel (16–33 m, dem Anlauf gegenüber), möglichst ungesehen (sonst bis zu zweimal weiter weg), dreht er sich und schneidet von links unten nach rechts oben einen Riss in die Luft (Wiki: der Hieb von der Hüfte zum Schlüsselbein): weißes Licht flammt auf und verglimmt um die Ecke, ein heller Riss, ein gläserner Akkord, ein Sog – er ist fort. Niemand sieht ihn kommen oder gehen (Wiki).
6. **Der Verfolger:** 2,5–4 s später zieht **unsichtbar** das vorbei, wovor er floh, seinen Weg entlang (6,5 m/s): Druck in den Ohren (tiefes, schwebendes Grollen, ein feiner Ton wie Tinnitus), die Leuchten flackern Raum um Raum; kommt es näher als 8 m, zehrt es einmal am Verstand (Vision, −3). Nichts greift an. *(Interpretation von „antimemetisch“: man sieht es nicht, man merkt nur, dass etwas fehlt.)*

**Er greift nie an**, richtet keinen Schaden an, ist kein Ziel; er erscheint nie im Blickfeld (Spawn außer Sicht; im Mehrspieler verwirft die Regie Auftritte, die ein Mitspieler sähe).

## Modell (`tools/blender/creatures/redknight.py`, `data/models/ent_redknight_l0..l2.glb`: 41 988 / 14 062 / 5 759 Dreiecke, 12 Netze)

Skelett des Wretch (`SKELETON = 'ent_wretch'`, 1,73 m, mit Helm gut 1,8 m). Die Platten sind eigene Netze je Körperteil (Helm, Rumpf, Schultern, Arme, Beine, Schwertstahl – zusammengeführt zu einem Zeichenaufruf), damit sie nicht weich wie ein Körper ineinanderfließen.
- **Helm:** geschlossener Helm mit Kamm, Stirnplatte mit Sehschlitz (Mittelsteg), darunter das vorspringende Visier aus zwei Wangenflächen, die sich vorn in einem Grat treffen, Atemlöcher rechts, Kinnreff, Drehpunkt-Nieten; hinter Schlitz und Löchern nur Schwärze (Wiki: durch das Visier sieht man nichts). Halsberge in drei Lamellen.
- **Rumpf:** eigener glatter Brustpanzer mit kräftigem Mittelgrat und Gänsebauch, Schulterjoch, gerollte Kanten an Hals und Armlöchern; drei ausgestellte Bauchreifen; je Bein vier Beintaschen vorn und Seitenlamellen.
- **Arme:** geschobene Schulterstücke (vier Lamellen, Nieten, Brechrand am Hals), Oberarm- und Unterarmröhren mit Lamellenkanten und Wulsten, Ellbogenkachel mit Muschel, Stulpenhandschuhe mit Fingerlamellen (beide Fäuste geschlossen), Lederriemen.
- **Beine:** Diechlinge mit Lamellenkanten, Kniebuckel mit Muschel, Beinröhren, breite „Bärentatzen“-Schuhe mit Rist-Lamellen.
- **Darunter:** Kettengeflecht (Achseln, Armbeugen, Kniekehlen), Kettenschurz mit ausgefranstem Saum.
- **Umhang:** rote Wolle mit Goldsaum (Seiten und Kragen, Spangen), von den Schultern bis zur Wade, unten in 16 Fetzen, fünf Löcher; gebunden an Brust, Rücken, Becken und – unten links/rechts – an die Oberschenkel, so schlägt er im Lauf mit den Beinen.
- **Schwert:** Zweihänder, 1,6 m, an der rechten Hand: Birnenknauf, langer Ledergriff mit Wicklung, Parierstange mit eingerollten Enden, Fehlschärfe mit Parierhaken, Klinge mit Hohlkehle auf beiden Seiten und sechs Scharten.
- **Schaden:** sieben tiefe Dellen (Brust zweimal, Helm, rechte Schulter, linkes Schienbein, Rücken, rechter Oberschenkel), 14 Kratzer, zwei Einstiche (Bauch links, Rücken rechts) mit dunkler, öliger Flüssigkeit, die herabläuft; Ruß um eine Delle. **Purpurspuren** (Wiki GB-03): in jeder Delle eine violette Glasur mit drei glühenden Adern, eine Spur auch auf der Klinge nahe der Spitze; dazu ein schwaches violettes Licht am Körper.
- **Materialien** (`FM_ENT_RK_*`): alter Stahl (triplanar, Metall 0,8 – der Schmutz streut, sonst wäre er im Raumlicht schwarz), geschwärzte Stellen, Kettengeflecht (stumpf), Umhang (verblichene rote Wolle), Goldfaden, Klinge (heller, glatter), Purpur-Glasur (leuchtet leicht), Purpur-Adern (leuchten hell); Leder und Messing der Plague Goblins, die Schwärze der Ouroboros-Arbeiter, die ölige Flüssigkeit der Rudel-Hounds.

## Animation (`anim/creature_knight.cpp`, Stil „rk“ in `updateBiped`)

Aufrecht, schwer, weit ausgreifend; ab 2,2 m/s Rennen mit Flugphase. Eingaben über die vorhandenen Kanäle:
- **Idle/Tragen:** die Klinge liegt flach auf der rechten Schulter (Griff vor der Brust), im Lauf federt sie mit dem Schritt; die linke Hand pumpt.
- **Wahrnehmen** (`perceive`): Blick über die Schulter zurück im Lauf; 0,9 s zum Spieler.
- **„Jagen“ = Flucht:** Rennen mit 5 m/s, Panik (`alert`): fliegender Atem, der Kopf zuckt in kleinen Stößen.
- **Angriff** (`strike`): beidhändig über die rechte Schulter ausholen, schräg nach links unten durchziehen, Nachschwung – gegen nichts.
- **Rückzug:** Weiterlaufen, Blick zurück.
- **Verschwinden** (`strike` + `wave`): der Schnitt von links unten nach rechts oben, dann ist er fort.
- **Ruhe** (`crouch`, für Vorführung und Bilder): auf das Schwert gestützt, Spitze am Boden, beide Hände am Griff (Wiki: er lehnt keuchend auf der Klinge).

## Klang (`audio/synth_knight.cpp`, alles synthetisch)

- `rk_step`: Sabaton auf Beton (harter Stahlschlag), dumpfer Tritt, nachschlagende Beinlamellen, Kettengeflecht – je Schritt, laut (34 m), mit Tonhöhenstreuung. Beim ersten Hören: „Metall klirrt im Laufschritt – jemand in Rüstung rennt.“
- `rk_breath`: Keuchen im geschlossenen Helm (eng, metallische Resonanzen bei 940 Hz und 2,15 kHz, gequältes Stöhnen) – im Lauf alle 0,9–1,3 s.
- `rk_halt`: abruptes Bremsen – Platten rasseln, Stahlsohlen schaben.
- `rk_slash`: der Zweihänder durch leere Luft – tiefes Fauchen, das anschwillt, am Ende knistert die Luft, als risse sie ein wenig.
- `rk_rift`: der Schnitt in die Luft – heller, steigender Riss, gläserner Akkord, tiefer Sog, ein sinkendes Zischen beim Schließen.
- `rk_unseen`: der Verfolger – tiefes, schwebendes Grollen (31/33,5 Hz), ein feiner Ton wie Tinnitus (6,15 kHz). Beim ersten Hören: „Da ist etwas. Du siehst es nicht – du hörst nur, dass es nicht da ist.“

## Zahlen

| Wert | |
|---|---|
| Häufigkeit | Mittel 900 s (erster Lauf nach 405–900 s), höchstens 2 je Besuch, nicht während einer Jagd |
| Anlauf | außer Sicht, 20–33 m Weg |
| Vorbeilauf | Wegpunkt 2,5–5 m vom Spieler, beide Wegstücke vorab abgelaufen: mindestens 2,2 m |
| Tempo | 5,0 m/s (Spieler: Sprint 5,7 m/s) |
| Hieb | ab 2,5 m, nur mit Sicht, ≥ 60° neben der Richtung zum Spieler; 1,65 s Halt |
| Ziel | 16–33 m, dem Anlauf gegenüber; gesehen: bis zu zweimal weiter, spätestens nach 12 s gesehen trotzdem fort |
| Riss | Licht weiß, 3,2-fach, 9 m, 0,12 s auf, dann verglimmend (2,5 s) |
| Verfolger | nach 2,5–4 s, 6,5 m/s seinen Weg entlang; Flackern alle 0,45 s im Umkreis von 16 m; Klang alle 1,3 s bis 30 m; einmal Vision (−3 Verstand) unter 8 m |
| Schaden | keiner |

## Mehrspieler

Host rechnet; der Gast bekommt den Ritter als Puppe der Rolle Wretch mit Variante 4 (Modell `ent_redknight`) und die Kanäle `strike`, `wave`, `alert`, `perceive`, `crouch` über `Presence::NetState`; die Klänge kommen wie bei den Level-3-Wretches als Klang mit Namen (`kinNetRoute`, `knightLineBySound`); das Licht des Risses rechnet der Gast aus den Kanälen der Puppe. Der Verfolger zehrt am Verstand der Gäste über das Vision-Ereignis (Host). Protokoll unverändert (`coop_test --quick` grün).

## Gleichgewicht

Keine Gefahr, kein Schaden, keine Vorräte. Ein Lauf kostet höchstens eine Vision (−3 Verstand). Eigener Zufallsgenerator: die Würfe der anderen Wesen bleiben, wie sie waren. Er zählt nicht als Wretch (der Auftrittsplan der Wretches bleibt unverändert) und nicht als Bedrohung (keine Erstsichtung, keine Jagdmusik).

## Interpretation (gekennzeichnet)

Die ganze Szene ist eine Auslegung von GB-03: dass er an einem Wanderer vorbeiläuft, der Hieb in die leere Luft, der Blick, der Riss außer Sicht, der unsichtbare Verfolger mit flackernden Leuchten und Druck in den Ohren, das violette Glimmen, die Häufigkeit, alle Zahlen und Klänge. Wiki-belegt sind: Rüstung, Schwert auf der Schulter, roter Umhang mit Goldsaum, Dellen, Purpurspuren, Panik, Klirren der Schritte, der weiße Riss, kein Angriff auf Menschen, niemand sieht ihn kommen oder gehen.

## Wow-Check

Erst hört man ihn – Stahl auf Beton im Laufschritt, ein Keuchen in Blech –, dann stürmt ein verbeulter Ritter mit rotem Umhang durch den Ziegelgang, violettes Licht glimmt aus seinen Dellen, er bremst, wirbelt herum und haut in die leere Luft hinter sich, sieht dich eine Sekunde lang durch einen schwarzen Schlitz an und ist fort; Sekunden später flackern die Röhren, einer nach der anderen, und etwas, das du nicht siehst, zieht an dir vorbei. **Ehrlich:** Wow für die Szene (`r5_lauf_lampe_hieb.jpg`, `r5_lauf_lampe_dreht.jpg`) – das Unheimliche ist, wovor *er* flieht. Aus nächster Nähe sind die Platten glatter als gehämmerter Stahl (keine Gravur, kaum Hammerschlag) – Restmangel.

## Kritikrunden

Bilder: `docs/screens/creatures/l3_redknight/` (Vorführung: nah/mittel/von hinten, mit Lampe, Hieb, gestützt, Lauf; echter Lauf in Level 3: ohne und mit Lampe).

| Runde | Was ist schlecht | Was ändert sich |
|---|---|---|
| 1 (`r1_blender.jpg`, `r1_vorn_khaki.jpg`, `r1_hinten_khaki.jpg`) | Im Spiel liest sich die Rüstung khaki wie fleckiger Stein (Zahnschmelz-Textur); der Kettenschurz wirkt wie eine schwarze Unterhose; der Umhang in Blender wie ein Lattenzaun (Reihen aus Kästen); die Kamera stand hinter ihm. | Stahl triplanar (Metall) statt Zahnschmelz; Beintaschen vier Lamellen, breiter, Seitenlamellen; Kettenschurz kürzer mit ausgefranstem Saum; Umhang aus Streifen. |
| 2 (`r2_blender_umhang_gitter.jpg`, `r2_nah_stahl_schnabel.jpg`, `r2_mittel_schwarz.jpg`) | Umhang aus Röhren: an den Nähten dünn – ein Gitter aus Löchern; Rüstung jetzt Stahl, aber im Raumlicht fast schwarz; das Visier wie ein Entenschnabel; Brustmuskeln drücken durch den Harnisch (Gummipuppe); Schwert steht senkrecht. | Umhang aus überlappenden Platten gleicher Dicke; Schulterstücke größer mit Brechrand; Schwert flacher auf die Schulter. |
| 3 (`r3_nah_helm_brust.jpg`, `r3_lampe.jpg`) | Röhren an Armen und Beinen wie glatte Schläuche; Helm jetzt kantig und lesbar, Rumpf ohne Muskeln; mit Lampe glänzt er wie ein Ritter. | Eigener Brustpanzer (Mittelgrat, gerollte Kanten), kantiges Visier aus zwei Wangenflächen, Lamellenkanten an Ober- und Unterarm und Diechling. |
| 4 (`r4_lauf_hieb_dunkel.jpg`, `r4_lauf_blick_dunkel.jpg`) | Echter Lauf in Level 3: der erste Versuch fand keinen Weg (am Start eine Sackgasse) und meldete nichts; danach läuft er wie geplant (4,4 m, Hieb, Blick, Riss ungesehen) – aber im Dunkeln ist er zu dunkel, Rüstung und Kettengeflecht schwarz wie Stoff. | Erzwungener Lauf versucht es jede Sekunde, spätere Versuche lockerer, Meldung „kein Weg“; Metall 0,8 (Platten) und 0,5 (Kettengeflecht), damit Schmutz das Raumlicht streut. |
| 5 (`r5_mittel.jpg`, `r5_nah.jpg`, `r5_hieb.jpg`, `r5_lauf_lampe_kommt.jpg`, `r5_lauf_lampe_hieb.jpg`, `r5_lauf_lampe_dreht.jpg`, `r5_lauf_lampe_weg.jpg`) | Grauer Stahl, rote Fetzen, violette Dellen – im Gang liest sich die ganze Szene; Restmangel: aus der Nähe glatte Flächen, im Hieb wirken die Arme etwas steif. | Abgenommen. |

## Tests (`gameplay_test`, „V13.3: Level 3 – Red Knight“)

Level 3 setzt `red_knight_every`, Level 1 nicht; erzwungener Lauf in der Halle Aquila (mit der Einstellung von Level 3): er erscheint außer Sicht, man hört ihn, bevor man ihn sieht (57 Schritte), er läuft am Wanderer vorbei (nächster Abstand 3,0 m), haut genau einmal in die leere Luft, schneidet einen Riss und ist fort, greift nie an, kein Schaden; danach zieht der Verfolger vorbei (8 Klänge, 13 Flackern) und zehrt genau einmal am Verstand (Verstand nach 31 s: −10,9 gegen −7,8 an derselben Stelle ohne Ritter – der Unterschied ist die eine Vision); ein Lauf. Level 1: 30 s im Zeitraffer kein Ritter. Level 3 im Zeitraffer (240 s × 20): er kommt von selbst, höchstens zweimal.
