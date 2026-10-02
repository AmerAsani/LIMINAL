// Pfade und Dateizugriff. Alle Pfade sind UTF-8-Strings; intern wird fuer
// Windows nach UTF-16 umgewandelt. Es gibt keine fest eingebauten Pfade: alles
// wird relativ zur EXE oder zu den Benutzerordnern von Windows ermittelt.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace lim::paths {

std::wstring widen(const std::string& utf8);
std::string narrow(const std::wstring& utf16);

std::string join(const std::string& a, const std::string& b);
std::string parent(const std::string& path);
std::string fileName(const std::string& path);
bool exists(const std::string& path);
bool createDirectories(const std::string& path);
bool removeFile(const std::string& path);

// Ordner der laufenden EXE (Spiel- oder Starterdatei).
const std::string& exeDir();
// Mitgelieferte Spieldaten (data/, shaders/, assets/) liegen neben der EXE.
std::string dataDir();
std::string shaderDir();
// %APPDATA%\LIMINAL - Spielstaende und Einstellungen (wandert mit dem Profil).
std::string userDataDir();
// %LOCALAPPDATA%\LIMINAL - Protokolle und Caches.
std::string localDataDir();
// Bilder\LIMINAL - Screenshots.
std::string screenshotDir();

std::optional<std::string> readFile(const std::string& path);
std::optional<std::vector<unsigned char>> readBinary(const std::string& path);
// Schreibt zuerst in eine temporaere Datei, sichert die alte Fassung als .bak
// und ersetzt dann atomar. Liefert false bei Fehlern.
bool writeFileAtomic(const std::string& path, const std::string& content, bool keepBackup);
// Dateien in einem Ordner, deren Name mit prefix beginnt und mit suffix endet.
std::vector<std::string> listFiles(const std::string& dir, const std::string& prefix, const std::string& suffix);

}  // namespace lim::paths
