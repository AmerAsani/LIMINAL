// V13.2/V13.3 Koop: Seite im Hauptmenue (hosten, beitreten), Mitspieler im Pausenmenue, Zeile im Spiel.
// V13.3: Beitritt per Code ueber die Lobby (ohne Router-Einstellungen); direkt (LAN/IP) mit langem Code fuer
// Fortgeschrittene. Verbindungsqualitaet (Ping, Verlust, Weg) in der Spielerliste.
#include <algorithm>
#include <cmath>
#include <format>

#include <windows.h>

#include "gameplay/content.hpp"
#include "gameplay/session.hpp"
#include "input/input.hpp"
#include "render/font.hpp"
#include "render/renderer.hpp"
#include "ui/host.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace lim::ui {

using namespace theme;
using input::Action;

namespace {
const char* pathName(int p) { return p == 1 ? "direkt" : p == 2 ? "direkt durch NAT" : p == 3 ? "über Relay" : ""; }
}  // namespace

void Ui::titleCoop(Host& h, float x, float y, float w) {
    float s = S();
    const auto& diffs = h.content().difficulties();
    newDiff_ = std::clamp(newDiff_, 0, (int)diffs.size() - 1);
    enum Row { RMode, RName, RCode, RWay, RAddr, RRelay, RDiff, RRun, RGo, RBack };
    const bool canResume = !coopJoin_ && h.hasCoopSave();
    if (!canResume) coopResume_ = false;
    std::vector<Row> rows = coopJoin_ ? std::vector<Row>{RMode, RName, RCode, RWay, RAddr, RRelay, RGo, RBack}
                                      : std::vector<Row>{RMode, RName, RWay, RRelay, RRun, RDiff, RGo, RBack};
    if (!coopJoin_ && !canResume) rows.erase(std::find(rows.begin(), rows.end(), RRun));
    if (!coopLobby_) {  // direkt: kein Relay; Adresse nur beim Beitreten
        rows.erase(std::find(rows.begin(), rows.end(), RRelay));
    } else if (coopJoin_) {
        rows.erase(std::find(rows.begin(), rows.end(), RAddr));
    }
    const int n = (int)rows.size();
    coopSel_ = std::clamp(coopSel_, 0, n - 1);
    Row cur = rows[(size_t)coopSel_];
    std::string* field = cur == RName ? &coopName_ : cur == RAddr ? &coopAddr_ : cur == RCode ? &coopCode_ : nullptr;
    const size_t maxLen = cur == RName ? 16 : cur == RAddr ? 64 : 40;
    const bool typing = field != nullptr;
    if (typing) {
        for (char32_t c : in_->text()) {
            if (field->size() >= maxLen || c < 32 || c > 126) continue;  // nur ASCII (Name, Adresse, Code)
            if (cur == RName && !(std::isalnum((int)c) || c == ' ' || c == '_' || c == '-')) continue;
            if (cur == RCode && !(std::isalnum((int)c) || c == '-' || c == ' ')) continue;
            field->push_back(cur == RCode ? (char)std::toupper((int)c) : (char)c);
        }
        if (in_->pressed(VK_BACK) && !field->empty()) field->pop_back();
        // Einfuegen (Strg+V) fuer Adresse und Code
        if (in_->down(VK_CONTROL) && in_->pressed('V') && (cur == RAddr || cur == RCode)) {
            std::string clip = in_->clipboardText();
            for (char c : clip)
                if (c > 32 && c < 127 && field->size() < maxLen) field->push_back(cur == RCode ? (char)std::toupper((unsigned char)c) : c);
        }
    }
    const bool up = typing ? in_->pressed(VK_UP) : in_->actionPressed(Action::UiUp);
    const bool down = typing ? (in_->pressed(VK_DOWN) || in_->pressed(VK_TAB)) : in_->actionPressed(Action::UiDown);
    if (up) coopSel_ = (coopSel_ + n - 1) % n, h.playSound("ui_move");
    if (down) coopSel_ = (coopSel_ + 1) % n, h.playSound("ui_move");
    cur = rows[(size_t)coopSel_];
    const int dir = in_->actionPressed(Action::UiLeft) ? -1 : (in_->actionPressed(Action::UiRight) ? 1 : 0);
    if (dir && !typing) {
        if (cur == RMode) coopJoin_ = !coopJoin_, coopSel_ = 0, h.playSound("ui_move");
        if (cur == RWay) coopLobby_ = !coopLobby_, h.playSound("ui_move");
        if (cur == RRelay) coopRelay_ = !coopRelay_, h.playSound("ui_move");
        if (cur == RDiff) newDiff_ = (newDiff_ + dir + (int)diffs.size()) % (int)diffs.size(), h.playSound("ui_move");
        if (cur == RRun) coopResume_ = !coopResume_, h.playSound("ui_move");
    }
    bool confirm = typing ? in_->pressed(VK_RETURN) : in_->actionPressed(Action::UiConfirm);
    const float rowH = 54 * s;
    const std::string cursor = (typing && std::fmod(now_, 1.0) < 0.5) ? "▌" : " ";
    for (int i = 0; i < n; ++i) {
        const float ry = y + 6 * s + i * rowH;
        if (in_->cursorMoved && hovered(x + 24 * s, ry, w - 48 * s, rowH)) coopSel_ = i;
        if (clicked(x + 24 * s, ry, w - 48 * s, rowH)) coopSel_ = i, confirm = true;
        const bool sel = i == coopSel_;
        const Row row = rows[(size_t)i];
        if (sel) {
            r_->rect(x + 24 * s, ry + 3 * s, w - 48 * s, rowH - 6 * s, kSelRow, 3 * s);
            r_->rect(x + 24 * s, ry + 12 * s, 3 * s, rowH - 24 * s, kAccent, 1.5f * s);
        }
        static const char* labels[] = {"Modus", "Name", "Beitrittscode", "Verbindung", "Adresse des Hosts", "Eigene IP verbergen", "Schwierigkeit",
                                       "Lauf", "", "Zurück"};
        std::string label = labels[row];
        if (row == RGo) label = coopJoin_ ? "Beitreten" : "Spiel hosten";
        const vec4 tc = sel ? kSelText : (row == RBack ? kMuted : kText);
        r_->text(x + 46 * s, ry + 14 * s, kFsRow * s, label, tc);
        std::string val;
        bool placeholder = false;
        auto choice = [&](const char* a, const char* b, bool first) { return sel ? std::format("◂  {}  ▸", first ? a : b) : std::string(first ? a : b); };
        if (row == RMode) val = choice("Beitreten", "Hosten", coopJoin_);
        else if (row == RName) placeholder = coopName_.empty(), val = (placeholder ? std::string("Wanderer") : coopName_) + (sel ? cursor : " ");
        else if (row == RCode)
            placeholder = coopCode_.empty(), val = (placeholder ? std::string(coopLobby_ ? "vom Host, z. B. K7QF-M2XA-9FJT" : "der lange Code (24 Zeichen)") : coopCode_) +
                                                   (sel ? cursor : " ");
        else if (row == RWay) val = choice("Über die Lobby (Internet)", "Direkt (LAN / IP-Adresse)", coopLobby_);
        else if (row == RAddr) placeholder = coopAddr_.empty(), val = (placeholder ? std::string("z. B. 192.168.0.20:27500") : coopAddr_) + (sel ? cursor : " ");
        else if (row == RRelay) val = choice("Ja – nur über das Relay", "Nein – direkter Weg, wenn möglich", coopRelay_);
        else if (row == RDiff) val = coopResume_ ? std::string("wie im Koop-Stand") : (sel ? "◂  " + diffs[(size_t)newDiff_].label + "  ▸" : diffs[(size_t)newDiff_].label);
        else if (row == RRun) val = choice("Koop-Stand fortsetzen", "Neuer Lauf", coopResume_);
        r_->text(x + 360 * s, ry + 14 * s, kFsRow * s * (row == RCode || row == RWay ? 0.86f : 1.0f), val, placeholder ? kMuted : (sel ? kAccent : kValue));
    }
    float dy = y + 6 * s + n * rowH + 18 * s;
    const std::string lobby = h.coopLobbyName();
    std::vector<std::string> info;
    if (coopLobby_) {
        if (lobby.empty())
            info = {"Keine Lobby eingetragen. Die Adresse einer Lobby gehört in koop.json (siehe MULTIPLAYER_SETUP.md).",
                    "Ohne Lobby: »Direkt« im selben Netz (LAN) oder mit Portfreigabe."};
        else
            info = {std::format("Lobby: {}. Ohne Router-Einstellungen: ein direkter Weg wird gesucht, sonst geht es über das Relay.", lobby),
                    coopJoin_ ? "Den Code nennt dir der Host (Pausenmenü → Mitspieler)."
                              : "Bis zu vier Spieler. Der Code steht nach dem Start im Pausenmenü unter »Mitspieler«.",
                    "Übertragen werden Spielername und Spielzustand, verschlüsselt (DTLS).",
                    "Die Lobby sieht deine IP-Adresse, aber nicht den Spielverkehr."};
    } else {
        info = coopJoin_ ? std::vector<std::string>{"Adresse und der lange Code (24 Zeichen) kommen vom Host. Im LAN genügt das; übers Internet",
                                                    "braucht der Host eine Portfreigabe (TCP und UDP, Standard 27500)."}
                         : std::vector<std::string>{"Direkt: Windows fragt beim ersten Mal, ob das Spiel Verbindungen annehmen darf.",
                                                    "Gib Gästen deine LAN-Adresse (ipconfig) und den langen Code aus dem Pausenmenü."};
    }
    if (coopRelay_ && coopLobby_) info.push_back("Nur Relay: der Mitspieler sieht deine IP-Adresse nicht, dafür etwas mehr Verzögerung.");
    for (const auto& l : info) {
        r_->text(x + 46 * s, dy, 17 * s, l, kMuted);
        dy += 26 * s;
    }
    if (!coopMsg_.empty()) r_->text(x + 46 * s, dy + 10 * s, 19 * s, coopMsg_, coopMsg_.rfind("Verbinde", 0) == 0 || coopMsg_.find("…") != std::string::npos ? kValue : kDanger);
    if (confirm) {
        h.playSound("ui_select");
        const Row r = rows[(size_t)coopSel_];
        if (r == RBack) page_ = "main", coopMsg_.clear();
        else if (r == RMode) coopJoin_ = !coopJoin_, coopSel_ = 0;
        else if (r == RWay) coopLobby_ = !coopLobby_;
        else if (r == RRelay) coopRelay_ = !coopRelay_;
        else if (r == RRun) coopResume_ = !coopResume_;
        else if (r == RGo) {
            std::string name = coopName_.empty() ? std::string("Wanderer") : coopName_;
            while (!name.empty() && name.back() == ' ') name.pop_back();
            std::string err;
            const bool ok = coopJoin_ ? h.joinCoop(name, coopCode_, coopLobby_, coopAddr_, coopRelay_ && coopLobby_, err)
                                      : h.hostCoop(name, diffs[(size_t)newDiff_].key, coopLobby_, coopRelay_ && coopLobby_, coopResume_, err);
            if (!ok) coopMsg_ = err;
        } else {
            coopSel_ = std::min(coopSel_ + 1, n - 1);
        }
    }
    if (in_->pressed(VK_ESCAPE) || (!typing && in_->actionPressed(Action::UiBack))) {
        h.playSound("ui_back");
        page_ = "main";
        coopMsg_.clear();
    }
}

void Ui::coopPlayersPage(Host& h, float x, float y, float w) {
    float s = S();
    const Host::CoopView v = h.coopView();
    float dy = y + 10 * s;
    if (!v.active) {
        r_->text(x + 46 * s, dy, kFsBody * s, "Kein Koop-Spiel.", kMuted);
    } else {
        if (v.host) {
            r_->text(x + 46 * s, dy, kFsCaps * s, v.direct ? "Z U M   B E I T R E T E N   ( D I R E K T )" : "B E I T R I T T S C O D E", kAccent,
                     gfx::FontFace::Bold);
            dy += 30 * s;
            const std::string shown = v.direct ? v.codeLong : v.code;
            if (shown.empty()) {
                r_->text(x + 46 * s, dy, 22 * s, v.lobbyStatus.empty() ? std::string("…") : v.lobbyStatus, v.lobbyStatus.find("nicht") != std::string::npos ? kDanger : kMuted);
                dy += 34 * s;
            } else {
                r_->text(x + 46 * s, dy, (v.direct ? 24 : 32) * s, shown, kValue, gfx::FontFace::Bold);
                dy += (v.direct ? 34 : 44) * s;
                if (v.direct) {
                    r_->text(x + 46 * s, dy, kFsBody * s, std::format("Adresse: IP dieses Rechners (ipconfig), Port {} (TCP und UDP)", v.port), kText);
                    dy += 28 * s;
                }
                r_->text(x + 46 * s, dy, kFsSmall * s, "Strg+C kopiert den Code. Nur an Mitspieler geben; er gilt nur für diese Sitzung.", kMuted);
                dy += 22 * s;
                if (!v.direct) {
                    r_->text(x + 46 * s, dy, kFsSmall * s, "Lange Form (prüft zusätzlich den Host): " + v.codeLong, kMuted);
                    dy += 22 * s;
                }
                dy += 14 * s;
                if (in_->down(VK_CONTROL) && in_->pressed('C')) {
                    h.copyText(shown);
                    h.playSound("ui_select");
                    toast("Beitrittscode kopiert.", 2.0);
                }
            }
        }
        r_->text(x + 46 * s, dy, kFsCaps * s, "S P I E L E R", kAccent, gfx::FontFace::Bold);
        dy += 30 * s;
        std::vector<Item> items;
        for (const auto& p : v.players) {
            std::string state = !p.connected ? "getrennt" : !p.alive ? "tot" : p.ready ? "am Ausgang" : "unterwegs";
            std::string net;
            if (p.rttMs > 0 || p.path) {
                net = std::format(", {} ms", p.rttMs);
                if (p.path) net += std::string(", ") + pathName(p.path);
                if (p.lossPct > 0) net += std::format(", Verlust {} %", p.lossPct);
            }
            std::string label = std::format("{}{}  –  {}{}", p.name, p.self ? " (du)" : (p.id == 0 ? " (Host)" : ""), state, net);
            Item it{label, nullptr, nullptr, nullptr, "", false};
            if (v.host && !p.self) {
                const int id = p.id;
                it.adjust = [&h, id](int d) { h.kickCoop(id, d > 0); };
                it.desc = "←  entfernen      →  sperren (für diese Sitzung, auch in der Lobby)";
            }
            items.push_back(it);
        }
        items.push_back({"Zurück", nullptr, nullptr, [this] { page_ = "main"; }, "", true});
        runList(h, items, lists_[10], x + 24 * s, dy, w - 48 * s, 46 * s, 6);
        const auto& sel = items[(size_t)std::clamp(lists_[10].sel, 0, (int)items.size() - 1)];
        if (!sel.desc.empty()) r_->text(x + 46 * s, dy + 6 * 46 * s + 12 * s, kFsSmall * s, sel.desc, kMuted);
    }
    if (in_->actionPressed(Action::UiBack)) {
        h.playSound("ui_back");
        page_ = "main";
    }
}

void Ui::drawCoopHud(Host& h) {
    const Host::CoopView v = h.coopView();
    if (!v.active) return;
    float s = S();
    // Namen ueber den Mitspielern (die Koerper sehen alle gleich aus)
    for (const NameTag& t : nameTags_) {
        const float tw = r_->textWidth(t.name, 15 * s);
        const float x = t.x * W_, y = t.y * H_;
        r_->rect(x - tw * 0.5f - 8 * s, y - 4 * s, tw + 16 * s, 24 * s, vec4{0, 0, 0, 0.45f * t.alpha}, 3 * s);
        r_->text(x, y, 15 * s, t.name, vec4{kValue.x, kValue.y, kValue.z, t.alpha}, gfx::FontFace::Regular, gfx::Align::Center);
    }
    // Spieler oben links (klein): Name, Zustand; schlechte Verbindung markiert
    float y = 24 * s;
    r_->text(24 * s, y, kFsCaps * s, "K O O P", kAccent, gfx::FontFace::Bold);
    y += 22 * s;
    for (const auto& p : v.players) {
        const bool weak = p.connected && (p.rttMs > 250 || p.lossPct >= 5);
        const vec4 c = !p.connected ? kMuted : !p.alive ? kDanger : p.ready ? kAccent : kText;
        r_->text(24 * s, y, kFsSmall * s,
                 std::format("{}{}{}", p.name, !p.connected ? " (getrennt)" : !p.alive ? " (tot)" : p.ready ? " (am Ausgang)" : "",
                             weak ? std::format("  · {} ms, Verlust {} %", p.rttMs, p.lossPct) : std::string()),
                 weak ? kDanger : c);
        y += 20 * s;
    }
    if (v.host && !v.lobbyStatus.empty()) {
        r_->text(24 * s, y + 4 * s, kFsSmall * s, v.lobbyStatus, kMuted);
        y += 24 * s;
    }
    // Zustand (Ausgang, Tod, Zuschauen) oben in der Mitte
    std::string line = v.status;
    if (v.dead) {
        line = v.spectating.empty() ? std::format("Tot – Wiedereinstieg in {} s", v.respawnIn)
                                    : std::format("Tot – du siehst {} zu (Maus: wechseln) · zurück in {} s", v.spectating, v.respawnIn);
    }
    if (!line.empty()) {
        const float tw = r_->textWidth(line, 18 * s);
        r_->rect(W_ * 0.5f - tw * 0.5f - 16 * s, 70 * s, tw + 32 * s, 36 * s, kPanelSoft, 3 * s);
        r_->text(W_ * 0.5f, 78 * s, 18 * s, line, v.dead ? kDanger : kValue, gfx::FontFace::Regular, gfx::Align::Center);
    }
}

}  // namespace lim::ui
