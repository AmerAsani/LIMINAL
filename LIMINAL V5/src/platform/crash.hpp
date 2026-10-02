// Absturzbehandlung: schreibt bei unerwarteten Fehlern einen Minidump nach
// %LOCALAPPDATA%\LIMINAL\crash\ und einen Eintrag ins Protokoll. Das hilft,
// Fehler auf fremden PCs nachzuvollziehen, ohne Debugger.
#pragma once

namespace lim::plat {

void installCrashHandler();

}  // namespace lim::plat
