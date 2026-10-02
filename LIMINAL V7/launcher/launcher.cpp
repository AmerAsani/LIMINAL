// LIMINAL.exe - Starter/Bootstrapper
//
// Prueft vor dem Start, ob der PC alles Noetige mitbringt, und startet dann
// LiminalGame.exe. Fehlt etwas, erklaert der Starter verstaendlich, was los
// ist, und bietet - wo moeglich - die offizielle Installation von Microsoft an:
//
//   * Windows-Version (Windows 10/11 empfohlen)
//   * Universal C Runtime (in Windows 10/11 enthalten; fuer aeltere Systeme das
//     offizielle "Microsoft Visual C++ Redistributable" - Download von
//     aka.ms, Signaturpruefung, Installation erst nach Rueckfrage)
//   * Direct3D 11 mit Hardwarebeschleunigung (sonst Angebot: Software-Modus)
//   * Vollstaendigkeit der Installation (Spiel, Daten, Shader)
//
// Der Starter selbst benutzt keine C-Laufzeitbibliothek, damit er auch dann
// laeuft, wenn genau diese fehlt. Nur Windows-Systemfunktionen.
#include <windows.h>
#include <d3d11.h>
#include <shellapi.h>
#include <softpub.h>
#include <wintrust.h>

extern "C" {
// Vom Compiler ggf. erzeugte Aufrufe (keine CRT vorhanden)
void* memset(void* d, int c, size_t n) {
    unsigned char* p = (unsigned char*)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}
void* memcpy(void* d, const void* s, size_t n) {
    unsigned char* p = (unsigned char*)d;
    const unsigned char* q = (const unsigned char*)s;
    while (n--) *p++ = *q++;
    return d;
}
}

namespace {

const wchar_t* kTitle = L"LIMINAL";
wchar_t gDir[MAX_PATH];

void cat(wchar_t* dst, const wchar_t* src, int cap) {
    int n = lstrlenW(dst);
    while (*src && n < cap - 1) dst[n++] = *src++;
    dst[n] = 0;
}

void pathJoin(wchar_t* out, const wchar_t* name) {
    lstrcpyW(out, gDir);
    cat(out, name, MAX_PATH);
}

bool fileExists(const wchar_t* p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES;
}

int ask(const wchar_t* text, UINT flags) { return MessageBoxW(nullptr, text, kTitle, flags | MB_SETFOREGROUND); }

// --- Windows-Version ----------------------------------------------------------------------
bool windowsVersion(DWORD& major, DWORD& minor, DWORD& build) {
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    auto fn = nt ? (RtlGetVersionFn)(void*)GetProcAddress(nt, "RtlGetVersion") : nullptr;
    OSVERSIONINFOW vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (!fn || fn(&vi) != 0) return false;
    major = vi.dwMajorVersion;
    minor = vi.dwMinorVersion;
    build = vi.dwBuildNumber;
    return true;
}

// --- Universal C Runtime --------------------------------------------------------------------
bool hasUcrt() {
    HMODULE m = LoadLibraryExW(L"ucrtbase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m) return false;
    FreeLibrary(m);
    return true;
}

// Authenticode-Signatur einer Datei pruefen (gueltige, vertrauenswuerdige Kette)
bool signatureValid(const wchar_t* file) {
    WINTRUST_FILE_INFO fi;
    memset(&fi, 0, sizeof(fi));
    fi.cbStruct = sizeof(fi);
    fi.pcwszFilePath = file;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA wd;
    memset(&wd, 0, sizeof(wd));
    wd.cbStruct = sizeof(wd);
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE;
    wd.pFile = &fi;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    LONG r = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &wd);
    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &wd);
    return r == 0;
}

typedef HRESULT(WINAPI* URLDownloadToFileWFn)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPVOID);

bool installRedist() {
    const wchar_t* url = L"https://aka.ms/vs/17/release/vc_redist.x64.exe";
    wchar_t tmp[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, tmp);
    if (!n || n > MAX_PATH - 40) return false;
    cat(tmp, L"LIMINAL_vc_redist.x64.exe", MAX_PATH);
    HMODULE urlmon = LoadLibraryExW(L"urlmon.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto dl = urlmon ? (URLDownloadToFileWFn)(void*)GetProcAddress(urlmon, "URLDownloadToFileW") : nullptr;
    if (!dl || FAILED(dl(nullptr, url, tmp, 0, nullptr))) {
        ask(L"Der Download ist fehlgeschlagen. Bitte das \"Microsoft Visual C++ Redistributable (x64)\" "
            L"direkt bei Microsoft herunterladen:\n\nhttps://aka.ms/vs/17/release/vc_redist.x64.exe",
            MB_OK | MB_ICONWARNING);
        return false;
    }
    if (!signatureValid(tmp)) {
        DeleteFileW(tmp);
        ask(L"Die heruntergeladene Datei hat keine gueltige digitale Signatur und wurde geloescht.", MB_OK | MB_ICONERROR);
        return false;
    }
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";  // Windows fragt nach Administratorrechten
    sei.lpFile = tmp;
    sei.lpParameters = L"/install /passive /norestart";
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return false;
    WaitForSingleObject(sei.hProcess, INFINITE);
    CloseHandle(sei.hProcess);
    DeleteFileW(tmp);
    return hasUcrt();
}

// --- Direct3D 11 --------------------------------------------------------------------------------
typedef HRESULT(WINAPI* D3D11CreateDeviceFn)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
                                             UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

// 0 = Hardware ok, 1 = nur Software (WARP), 2 = gar nicht
int checkD3D11() {
    HMODULE d3d = LoadLibraryExW(L"d3d11.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!d3d) return 2;
    auto create = (D3D11CreateDeviceFn)(void*)GetProcAddress(d3d, "D3D11CreateDevice");
    if (!create) return 2;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0, got;
    ID3D11Device* dev = nullptr;
    if (SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev, &got, nullptr))) {
        dev->Release();
        return 0;
    }
    if (SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev, &got, nullptr))) {
        dev->Release();
        return 1;
    }
    return 2;
}

// Argumente des Starters (ohne Programmnamen)
const wchar_t* argsTail() {
    const wchar_t* c = GetCommandLineW();
    bool quoted = false;
    while (*c && (quoted || (*c != L' ' && *c != L'\t'))) {
        if (*c == L'"') quoted = !quoted;
        ++c;
    }
    while (*c == L' ' || *c == L'\t') ++c;
    return c;
}

int run() {
    // Ordner des Starters
    DWORD n = GetModuleFileNameW(nullptr, gDir, MAX_PATH);
    if (!n || n >= MAX_PATH) return 1;
    for (int i = (int)n - 1; i >= 0; --i)
        if (gDir[i] == L'\\' || gDir[i] == L'/') {
            gDir[i + 1] = 0;
            break;
        }
    SetCurrentDirectoryW(gDir);

    // 1. Installation vollstaendig?
    wchar_t game[MAX_PATH], data[MAX_PATH], shaders[MAX_PATH];
    pathJoin(game, L"LiminalGame.exe");
    pathJoin(data, L"data\\levels\\level0.json");
    pathJoin(shaders, L"shaders\\lighting_CSMain.cso");
    if (!fileExists(game) || !fileExists(data) || !fileExists(shaders)) {
        ask(L"Die Installation ist unvollstaendig: Neben LIMINAL.exe fehlen LiminalGame.exe oder die Ordner "
            L"\"data\" bzw. \"shaders\".\n\nBitte den kompletten Spielordner behalten (fuer den Desktop am besten "
            L"eine Verknuepfung auf LIMINAL.exe anlegen) oder das Spiel neu entpacken.",
            MB_OK | MB_ICONERROR);
        return 2;
    }

    // 2. Windows-Version
    DWORD major = 0, minor = 0, build = 0;
    if (windowsVersion(major, minor, build)) {
        if (major < 6 || (major == 6 && minor < 1)) {
            ask(L"LIMINAL benoetigt mindestens Windows 7, empfohlen wird Windows 10 oder 11.", MB_OK | MB_ICONERROR);
            return 3;
        }
        if (major < 10 &&
            ask(L"Dieses Windows ist aelter als Windows 10. LIMINAL wird hier nicht offiziell unterstuetzt und "
                L"benoetigt u. U. zusaetzliche Updates von Microsoft.\n\nTrotzdem fortfahren?",
                MB_YESNO | MB_ICONWARNING) != IDYES)
            return 3;
    }

    // 3. Universal C Runtime
    if (!hasUcrt()) {
        int r = ask(L"Auf diesem PC fehlt die \"Universal C Runtime\" von Microsoft (in Windows 10/11 bereits "
                    L"enthalten).\n\nSoll das offizielle \"Microsoft Visual C++ Redistributable (x64)\" jetzt von "
                    L"microsoft.com heruntergeladen und installiert werden? Windows fragt dabei nach "
                    L"Administratorrechten.",
                    MB_YESNO | MB_ICONQUESTION);
        if (r != IDYES || !installRedist()) {
            if (!hasUcrt()) {
                ask(L"Ohne die Universal C Runtime kann LIMINAL nicht starten.", MB_OK | MB_ICONERROR);
                return 4;
            }
        }
    }

    // 4. Grafik
    bool warp = false;
    int gfx = checkD3D11();
    if (gfx == 2) {
        ask(L"Direct3D 11 ist auf diesem PC nicht verfuegbar.\n\nBitte Windows aktualisieren und den neuesten "
            L"Grafiktreiber des Herstellers (NVIDIA, AMD oder Intel) installieren.",
            MB_OK | MB_ICONERROR);
        return 5;
    }
    if (gfx == 1) {
        if (ask(L"Es wurde keine Grafikkarte mit Direct3D 11 gefunden (oder der Treiber ist veraltet).\n\n"
                L"LIMINAL kann im Software-Modus starten - das ist sehr langsam. Besser: den neuesten Grafiktreiber "
                L"des Herstellers installieren.\n\nIm Software-Modus starten?",
                MB_YESNO | MB_ICONWARNING) != IDYES)
            return 5;
        warp = true;
    }

    // 5. Spiel starten (gleiche Argumente wie der Starter)
    static wchar_t cmd[8192];
    cmd[0] = 0;
    cat(cmd, L"\"", 8192);
    cat(cmd, game, 8192);
    cat(cmd, L"\" ", 8192);
    cat(cmd, argsTail(), 8192);
    if (warp) cat(cmd, L" --warp", 8192);
    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(game, cmd, nullptr, nullptr, FALSE, 0, nullptr, gDir, &si, &pi)) {
        DWORD e = GetLastError();
        if (e == ERROR_ACCESS_DENIED || e == 0x10DC /*ERROR_VIRUS_INFECTED*/ || e == 4551 /*Anwendungssteuerung*/)
            ask(L"Windows hat den Start von LiminalGame.exe blockiert (z. B. durch Smart App Control oder einen "
                L"Virenscanner). Bitte die Datei in den Windows-Sicherheitseinstellungen zulassen.",
                MB_OK | MB_ICONERROR);
        else
            ask(L"LiminalGame.exe konnte nicht gestartet werden.", MB_OK | MB_ICONERROR);
        return 6;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    // 0 = normal, 1/3 = Fehler wurde vom Spiel selbst gemeldet
    if (code != 0 && code != 1 && code != 3) {
        ask(L"LIMINAL wurde unerwartet beendet.\n\nDas Protokoll liegt unter\n%LOCALAPPDATA%\\LIMINAL\\logs\\liminal_v7.log",
            MB_OK | MB_ICONWARNING);
    }
    return (int)code;
}

}  // namespace

extern "C" int __stdcall LauncherMain() { ExitProcess((UINT)run()); }
