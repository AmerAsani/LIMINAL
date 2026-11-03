// V13.3 Teil 4 Level 188 (KREATUR_Horcher.md): Klangnummern und Saetze des Horchers (presence_listener.cpp, synth_listener.cpp).
// Die Praesenz meldet jeden Klang als Ereignis Mimic mit der Staerke kListenerCueBase + Nummer; Einzelspieler und Host spielen
// ihn nach der Nummer, die Gaeste hoeren denselben Klang ueber kinNetRoute (Protokoll unveraendert).
#pragma once

#include <string>

namespace lim::game {

struct KinLine;

namespace l188 {
constexpr float kListenerCueBase = 900.0f;  // kKinCue (100) + 800
enum ListenerCue : int {
    HC_KNOCK0, HC_KNOCK1, HC_KNOCK2,  // ein Schlag der Faust gegen eine Mahagonituer (von der anderen Seite)
    HC_LISTEN,   // Antwort gehoert: hinter der Tuer ein nasses Saugen - es lauscht an der Tuer
    HC_LOST,     // keine Antwort: hinter der Tuer entfernt sich etwas
    HC_DOOR,     // hinter dir geht eine Tuer auf (Klinke, langsames Knarren)
    HC_UNFOLD,   // es entfaltet sich aus der Tuer: Gelenke, Knoechel auf Beton
    HC_STEP0, HC_STEP1, HC_STEP2, HC_STEP3,  // Faust und nackte Fuesse
    HC_BREATH,   // es lauscht: Einsaugen durch den Gehoergang, Klicken
    HC_HUM,      // tiefes Brummen mit Klicken (es schleicht)
    HC_HEARD,    // es hat etwas gehoert: nasses Schnalzen, das Ohr schnappt herum
    HC_RUSH,     // es jagt: Pfeifen durch den Gehoergang, schwere Schlaege
    HC_WARN,     // Ankuendigung: drei Schlaege in die Luft, ein steigendes Zischen
    HC_STRIKE,   // die Faust trifft
    HC_MISS,     // die Faust schlaegt auf den Beton
    HC_FLINCH,   // Licht auf dem Ohr: ein Kreischen, Knorpel knistert
    HC_GONE,     // eine Tuer schliesst sich leise, die Klinke
    HC_SCRAPE,   // es sucht: die Faust tastet ueber den Boden, klopft kurz
    HC_COUNT
};
const KinLine* listenerLine(int cue);
const KinLine* listenerLineBySound(const std::string& s);
}  // namespace l188

}  // namespace lim::game
