# LIMINAL V10 – Änderungen gegenüber V9

V9 hat der Welt ein Ziel gegeben. Der Gegner war aber ein schwarzes Strichmännchen, und die Halluzinationen waren Abziehbilder. V10 ersetzt sie durch **Entities aus den Backrooms-Wikis**: Jede lebt in dem Bereich, in den sie laut den Quellen gehört, sieht aus, wie sie beschrieben wird, und verhält sich so. Die Welt selbst ist unverändert: gleiche Seeds, gleiche Räume, bitgenau.

> Eine erste V10-Fassung hatte stattdessen erfundene Figuren (der Angestellte, der Beobachter, der Andere, später Pfahl, Laterne, Kriecher, Abgewandte). Sie sind vollständig entfernt, samt Modellen, Texturen und Klängen. Geblieben ist nur die Technik dahinter.

## Recherche und Zuordnung
- **Quellen:** Backrooms Wiki auf Wikidot (Hauptquelle; Seiten zu Level 0, 1 und 5 sowie zu Entity 3, 4, 5, 8, 9, 10 und 15) und auf Fandom (Vergleich; Entity List, Level 0 und 1, Smilers, Deathmoths, Hounds). Eine eigene „Backrooms Foundation“-Seite wurde nicht gefunden.
- **Dokumentiert** in [docs/ENTITAETEN.md](docs/ENTITAETEN.md): was die Quellen je Entity sagen, wo sie sich widersprechen, welche Auslegung gewählt wurde und warum Clump, Skin-Stealer, Faceling und Wretch nicht umgesetzt sind.
- **Zuordnung:** Innenräume = Level 0 (keine bestätigten Entities, nur dunkle Gestalten). Tiefgeschoss = Level 1 (Stromausfälle, Smiler, Hound). Hotel = Level 5 (Deathmoths mit Nestern, selten Hound).

## Die Entities
- **Smiler** (Entity 3): Nur Augen und ein langes Lächeln voller spitzer Zähne leuchten. Der Körper ist eine formlose schwarze Rauchmasse ohne Beine. Lebt im Dunkeln, kommt bei Stromausfällen. Das Licht der Lampe lockt ihn. Blickkontakt hält ihn fest. Ungesehen kommt er durch das Dunkel näher, nie ins Licht. Rennen in seiner Nähe löst einen Angriff aus. Kehrt das Licht zurück, erlischt er.
- **Hound** (Entity 8): Ein Mensch auf allen vieren. Knochig, Hände mit Schwimmhäuten, pfotenartige Füße. Langes schwarzes Haar verdeckt Kopf und Gesicht und läuft als Mähne den Rücken entlang. Weiße Augen, großes Maul mit zwei Zahnreihen, räudige Haut, hinkt. Streift umher, hört Rennen, sieht das Licht. Hat er dich, knurrt und pirscht er. Blickkontakt schüchtert ihn eine Weile ein. Den Rücken zugekehrt oder rennend: Bellen, Angriff, danach Rückzug. Verliert er die Spur, winselt er.
- **Deathmoths** (Entity 4): Etwa 45 cm Spannweite, zwei gezackte, grau behaarte Flügelpaare mit Muster, brauner Brustpanzer, behaarter Hinterleib mit Nadelborsten, gefiederte Fühler, Rüssel, bedornte Beine. Fleischig-harzige Nester überwuchern die Türen dunkler Hotelzimmer. Die Motten sitzen reglos an Wand und Decke. Licht oder Nähe scheucht den Schwarm auf. Er umkreist die Lampe, einzelne Motten stechen zu. Ohne Licht kreisen sie um die Deckenleuchten und kehren zum Nest zurück, durch die Tür statt durch Wände.
- **Gestalt** (Level 0): Ein dunkler, formloser Umriss am Ende eines Gangs. Verändert sich nie im Blick, ist fort, sobald man wegsieht, und greift nie an.

## Fair, nicht billig
- Keine Entity erscheint im Blickfeld oder neben dir. Jeder Angriff kündigt sich an: Knurren und Bellen, das Dröhnen des Smilers (eigene Ergänzung, keine Stimme), das Flattern.
- „Bewegt sich nur ungesehen“ gilt nur beim Smiler (Blickkontakt) und bei der Gestalt. Hound und Motten bewegen sich offen.
- Nach dem ersten Treffer nennt eine Einblendung den Rat aus dem Wiki.

## Technik
- **Neu:** `anim/creature` (fünf Skelette), `anim/creature_anim` (Vierbeiner mit Hinken, Flug mit zwei Flügelpaaren, Rauchmasse, Haarketten mit Kollision), `render/entity_models` (Modelle), `render/moth_wing.hpp` (Flügelumriss für Netz und Textur), `render/texture_entity` (acht Materialebenen bis 4096 px plus vier Detailnormalen).
- **Geändert:** `gameplay/presence` (Regie je Bereich, Verhalten, Stromausfall, Schwarm mit Wegesuche), `gameplay/haunt` (echter Stromausfall auch ohne Halluzinationen), `audio/synth` (neue Klänge), Shader (Leuchtfarbe aus dem Material), Hilfe, Notizen, Einstellungen.
- **Spieler:** realistischer Körper (Kapuzenjacke, Jeans) oder das Strichmännchen. Die Animator-Korrektur bei der Landung des Fußes bleibt.
- **Tests:** `gameplay_test` 193 Prüfungen (neu: Hound, Smiler mit Stromausfall, Mottenschwarm mit Nest, Gestalt, Bereichsregel), `audio_test` 59, `worldgen_test` 29 658, `supply_balance_test` OK.

---

# LIMINAL V9 – Änderungen gegenüber V8

V8 hat aus den Leveln eine zusammenhängende Welt gemacht, aber in dieser Welt gab es nichts zu erreichen und nichts zu fürchten außer der eigenen Sanity. V9 gibt ihr **ein Ziel** und **einen Gegner**. Die Welt selbst ist unverändert: gleiche Seeds, gleiche Räume, bitgenau.

## Kurzfassung

- **Ein Weg hinaus:** In jeder Ebene hängen tief im Inneren Sicherungskästen. Mit einer Sicherung aus jeder Ebene (Büros, Tiefgeschoss, Hotel) fährt ein Aufzug weiter als je zuvor, ins Licht. Danach kommt ein Abspann mit dem Laufbuch.
- **Der Andere ist echt:** Eine große, gesichtslose Gestalt folgt dir. Sie bewegt sich nur, wenn du sie nicht siehst. Im Dunkeln hilft nur die Taschenlampe.
- **Hinweise:** Neue Notizen, die wichtigsten zuerst, dazu eine leise Einleitung, die Zielanzeige im Pausenmenü und Markierungen auf der Karte.

---

## Der Weg hinaus

- **Sicherungskästen** (`data/world.json` → `fuses`, Prefab `data/prefabs/fusebox.json`):
  - Wahrscheinlichkeit 1,2 % je Raum (keine Gänge, Decke ab 2,4 m). Nur in der **eigenen** Schicht einer Ebene und dort nur zwischen 22 % und 85 % ihrer Tiefe, also nie an den Nahtstellen.
  - Der Kasten sitzt an einer freien Wandstelle: keine Tür dahinter, keine Wandleuchte, Nachbarkacheln frei.
  - **Gemessen** (`layout_stats --world --radius 8`, 5 Seeds), nächster Kasten:

    | Ebene | nächster Kasten |
    |---|---|
    | Büros | 82–188 m |
    | Tiefgeschoss | 339–478 m |
    | Hotel | 832–975 m |

  - **Aussehen:**
    - Stahlkasten mit offener Tür und Warnschild „VORSICHT STARKSTROM“ (auch an der Wand darüber).
    - Kabelrohr bis in die Decke, verzinkte Montageplatte „HAUPTSICHERUNG“.
    - Bakelithalter mit Messingklemmen, Schmelzsicherung aus Porzellan („63A“), orange Kontrolllampe.
  - **Atmosphäre:**
    - Netzbrummen mit Knistern als räumliche Quelle (22 m).
    - Unruhiges oranges Licht mit seltenen Funken.
    - Hinweis „Irgendwo ganz in der Nähe brummt Starkstrom …“ ab 30 m.
  - **Karte:** Gesehene Kästen werden gemerkt und gespeichert, orange (geleert grau). Volle Kästen erscheinen auch als Marke am Kartenrand.
  - **E** nimmt die Sicherung heraus (eine je Ebene): Klemme, Funke, das Brummen bricht ab. Der Kasten bleibt leer, auch nach dem Laden (`picked`).
- **Sicherungen** sind Items (`fuse_office`, `fuse_cellar`, `fuse_hotel`) mit eigenem Modell und Symbol. Sie lassen sich ablegen, aber nicht „benutzen“.
- **Der Aufzug:**
  - In der ersten Kabine: „Unter dem Tastenfeld: drei leere Fassungen für Sicherungen. Daneben, eingeritzt: »RAUS«.“ Später meldet die Kabine, welche Sicherungen noch fehlen.
  - Mit allen drei: **„Sicherungen einsetzen“**. Die Fahrt dauert elf Sekunden, ohne Ortswechsel.
  - Unterwegs rüttelt die Kabine immer stärker, das Sichtfeld weitet sich, das Kabinenlicht wird bis zu achtmal heller, das Bild bleicht aus.
  - Ein eigener Klang: Der Antrieb läuft hoch, ein Akkord schwillt an.
  - Die Sicherungen werden erst am Ende verbraucht. Ein Autosave während der Fahrt kann also nichts kosten.
- **Abspann „DRAUSSEN“** (neuer Oberflächenmodus):
  - Weißblende, dann ein warm werdender heller Grund mit kurzem Text.
  - Danach das Laufbuch Zeile für Zeile: Name, Schwierigkeit, Zeit bis zum Entkommen, Weg, Räume, Ebenen, Übergänge, Notizen, Halluzinationen, Begegnungen mit dem Anderen, Seed. Dazu Wind und ferne Vögel.
  - Enter überspringt. Danach: **Weiter erkunden** (die Tür geht auf, Einblendung aus Weiß) oder **Zum Hauptmenü**. Der Stand wird gespeichert (`runlog.escaped`, `escape_time`).
- **Pausenmenü:** neuer Block „DER WEG HINAUS“ mit ✓/○ je Ebene und einem kurzen Ziel. Das Laufbuch hat eine dritte Zeile.

## Der Andere

- Modul `gameplay/stalker.cpp`, gehört zur Sitzung.
- **Seine Regel:** Er bewegt sich nur, wenn du ihn nicht siehst. Als gesehen gilt er, wenn mindestens ein Punkt (Kopf, Brust oder Knie) alle drei Bedingungen erfüllt:
  1. Er liegt höchstens etwa 50° neben deiner Blickrichtung.
  2. Die Sichtlinie ist frei.
  3. Er ist beleuchtet: Kachelhelligkeit ab 0,2 und kein Stromausfall in seinem Raum, oder er steht im Kegel der Taschenlampe (26°, 22 m).
- **Gesehen:** Er erstarrt mitten im Schritt (die Pose bleibt stehen). Beim ersten Blick je Jagd ertönt ein Stich, und die Sanity sinkt um 4 %.
  - Anstarren kostet zusätzlich 0,45 % Sanity pro Sekunde. Seine Nähe kostet bis zu 0,3 % pro Sekunde und entzieht dem Bild die Farbe.
- **Ungesehen:** Er folgt dem Gefälle eines Distanzfelds (Dijkstra über 81 × 81 geladene Kacheln, Stufen bis 0,65 m, schräg nur an freien Ecken vorbei). Ab 30 m Weg geht er mit 1,5-fachem Tempo.
  - Hörbar sind schwere, dumpfe Schritte (28 m), ein tiefes, atmendes Brummen an seiner Stelle (verdeckt hinter Wänden) und ab etwa 14 m dein Herzschlag.
- **Zugriff** (ungesehen, unter 0,95 m): Health sinkt um `stalker_hit`, Sanity um 20 %.
  - Ein Schlag im Bild: rote Ränder, Blende aus Schwarz.
  - Danach verschwindet er für 120–240 s × `stalker_pause` (bei niedriger Sanity kürzer).
  - Beim ersten Mal erklärt ein Hinweis die Regel. Bei 0 % Health: Todesbildschirm „ER HAT DICH ERWISCHT“.
- **Auftreten:**
  - Der erste Auftritt kommt nach 180 s, nach dem Laden nach 90 s.
  - Er erscheint 20–34 m Weg entfernt, ungesehen (auch nicht im Licht sichtbar), bevorzugt hinter dir.
  - Nach 150 s, bei über 52 m Abstand oder wenn er abgeschnitten ist, verliert er die Spur.
  - Bei Aufzugfahrten ist er sofort fort.
  - In der Demo und mit der Einstellung **„Verfolger“** (aus) erscheint er nicht.
- **Schwierigkeit** (`data/difficulty.json`):

  | | Tempo (m/s) | Pausen | Zugriff |
  |---|---|---|---|
  | Leicht | 3,2 | × 1,6 | −25 % |
  | Mittel | 4,0 | × 1,0 | −35 % |
  | Schwer | 4,7 | × 0,7 | −45 % |

- Er ist dieselbe gestreckte, gesichtslose Figur wie die Halluzination aus V7. Die Halluzination verschwindet, wenn man näher kommt. Er nicht.

## Hinweise und Oberfläche

- **Notizen** (`data/notes.json`): sechs neue für die Welt.
  - Vier haben Vorrang (`"priority"`): „Drei Sicherungen“, „Nicht wegsehen“, „Im Dunkeln“, „Er fährt nicht mit“. Man findet sie zuerst, solange sie nicht im Notizbuch stehen.
  - Dazu kommen „RAUS“ und „Orange“.
- **Einleitung:** Bei einem neuen Lauf erscheinen drei leise Zeilen nacheinander in der oberen Bildhälfte.
- **Hilfe:** neue Abschnitte „Der Weg hinaus“ und „Der Andere“.
- **Einstellung „Verfolger“** unter Grafik & Anzeige (`settings_v9.json`: `stalker`).
- **Laufbuch:** neue Felder `encounters`, `caught`, `fuses`, `escaped`, `escape_time`. Sie erscheinen im Pausenmenü, auf dem Todesbildschirm und im Abspann.

## Klang und Grafik

- **Acht neue prozedurale Klänge:** `fuse_buzz` (Schleife), `fuse_take`, `stalker_step`, `stalker_drone` (Schleife), `stalker_sting`, `stalker_attack`, `escape_ride`, `outside` (Schleife).
- **Neue Texturebene** `L_FUSE` (Atlas):
  - Kastenlack mit Kratzern und Rost
  - Warnschild
  - verzinkte Montageplatte mit Bakelit
  - Porzellan mit Aufdruck
  - Messing
- **Feste GPU-Materialien:** 32 statt 16, neu sind `FM_FUSEBOX`, `FM_FUSE_GLOW` (leuchtend) und `FM_ITEM_FUSE`. Die Materialbank fasst dafür 16 352 statt 16 368 Weltmaterialien.
- **Modelle:** `fusebox`, `fusebox_empty`, `fuse`, dazu das Inventarsymbol „fuse“.

## Start ohne Starter

- Im Paket ist `LIMINAL.exe` jetzt **direkt das Spiel** (eine Kopie von `LiminalGame.exe`).
- Den früheren Starter (`launcher/`) blockierte Smart App Control mit „Bösartige Binärreputation“ (Code Integrity, Ereignis 3077). Das ist ein Fehlalarm, ausgelöst durch die eingebaute Download- und Admin-Funktion für die Microsoft-Laufzeit. Das Spiel selbst wird zugelassen.
- Startfehler (z. B. ohne Direct3D 11) meldet das Spiel selbst in einem Fenster.
- Der Quellcode des Starters bleibt im Projekt.

## Versionen und Tests

- Neue Dateien:
  - `saves_v9`, Spielstandversion 9
  - `settings_v9.json` (beim ersten Start aus V8 übernommen, sonst V7 … V3)
  - `liminal_v9.log`, `benchmark_v9.txt`, `liminal_v9_*.dmp`
- **Alte Stände:** V8-Stände erscheinen mit „(V8)“ und laufen in derselben Welt weiter. V3–V7 werden wie bisher gelesen.
- **Tests:**
  - `gameplay_test`: 163 Prüfungen (44 neue für V9).
  - `worldgen_test`: 29 658 Prüfungen, bitgenau wie V8.
  - `audio_test` und `supply_balance_test` sind unverändert grün.
  - `layout_stats --world` zählt Sicherungskästen je Ebene und prüft mit `--check`, dass jede Ebene in jedem Seed einen hat.
- **Neue Skriptbefehle:** `stalker N`, `facestalker [versatz]`, `stalkerinfo`, `give item`, `health N`. `expect` kennt jetzt auch `Note` und `Ending`.
