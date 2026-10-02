"""Baut die startbare Windows-Version von LIMINAL V3.

Ergebnis (im Ordner "LIMINAL V3"):
    LIMINAL.exe          Starter mit eigenem Icon (C#, mit Windows-Bordmitteln kompiliert)
    runtime/             eigenstaendige Python-Laufzeit (aus der lokalen Installation kopiert,
                         Standardbibliothek vorkompiliert als python3XX.zip)
    liminal/, liminal3d.py, assets/

Ablauf:
    1. Icon erzeugen (make_icon.py)
    2. Laufzeit zusammenstellen: python.exe, pythonXY.dll, VC-Runtime, benoetigte
       Erweiterungsmodule (.pyd), Standardbibliothek als ZIP, ._pth-Datei
       (isolierter Modus: ignoriert installierte Pythons und Umgebungsvariablen)
    3. LIMINAL.exe mit csc.exe (.NET Framework, in Windows enthalten) kompilieren

Aufruf mit einem normalen CPython fuer Windows:
    python build/build_exe.py
"""

import glob
import os
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RUNTIME = os.path.join(ROOT, "runtime")

# Nicht benoetigte Teile der Standardbibliothek (spart Platz)
SKIP_LIB = {
    "site-packages", "test", "tests", "idlelib", "tkinter", "turtledemo", "ensurepip", "venv",
    "pydoc_data", "lib2to3", "__pycache__", "sqlite3", "_pyrepl", "turtle.py", "idle_test",
}
# Nicht benoetigte Erweiterungsmodule (Tk, SSL, SQLite ...)
SKIP_DLL = ("_tkinter", "tcl", "tk", "_ssl", "libssl", "libcrypto", "_hashlib", "sqlite3", "_sqlite3",
            "_uuid", "_wmi", "winsound", "_msi", "py.ico", "pyc.ico")


def log(msg):
    print("  " + msg)


def build_icon():
    log("Icon erzeugen ...")
    subprocess.check_call([sys.executable, "-B", os.path.join(HERE, "make_icon.py")])


def build_runtime():
    base = sys.base_prefix
    ver = "python%d%d" % sys.version_info[:2]
    log("Laufzeit aus %s (%s)" % (base, sys.version.split()[0]))
    if os.path.isdir(RUNTIME):
        shutil.rmtree(RUNTIME)
    os.makedirs(RUNTIME)
    for name in ("python.exe", ver + ".dll", "python3.dll", "vcruntime140.dll", "vcruntime140_1.dll", "LICENSE.txt"):
        src = os.path.join(base, name)
        if os.path.exists(src):
            shutil.copy2(src, RUNTIME)
    for src in glob.glob(os.path.join(base, "DLLs", "*")):
        name = os.path.basename(src).lower()
        if name.endswith((".pyd", ".dll")) and not name.startswith(SKIP_DLL):
            shutil.copy2(src, RUNTIME)

    # Standardbibliothek vorkompiliert in ein ZIP packen
    lib = os.path.join(base, "Lib")
    zpath = os.path.join(RUNTIME, ver + ".zip")
    count = 0
    with zipfile.PyZipFile(zpath, "w", compression=zipfile.ZIP_DEFLATED, optimize=0) as zf:
        for entry in sorted(os.listdir(lib)):
            if entry in SKIP_LIB:
                continue
            path = os.path.join(lib, entry)
            if os.path.isdir(path):
                if not os.path.exists(os.path.join(path, "__init__.py")):
                    continue
                zf.writepy(path, filterfunc=lambda p: not any(part in SKIP_LIB for part in p.split(os.sep)))
                count += 1
            elif entry.endswith(".py"):
                zf.writepy(path)
                count += 1
    log("Standardbibliothek: %d Module/Pakete -> %s (%.1f MB)" % (count, os.path.basename(zpath),
                                                                    os.path.getsize(zpath) / 1e6))

    # ._pth: isolierter Modus, Suchpfad = Stdlib-ZIP, Laufzeitordner, Spielordner
    with open(os.path.join(RUNTIME, ver + "._pth"), "w", encoding="utf-8") as f:
        f.write(ver + ".zip\n.\n..\n")


def find_csc():
    root = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Microsoft.NET")
    for fw in ("Framework64", "Framework"):
        for path in sorted(glob.glob(os.path.join(root, fw, "v4*", "csc.exe")), reverse=True):
            return path
    raise SystemExit("csc.exe (.NET Framework 4) wurde nicht gefunden.")


def build_launcher():
    csc = find_csc()
    out = os.path.join(ROOT, "LIMINAL.exe")
    icon = os.path.join(ROOT, "assets", "liminal.ico")
    log("Starter kompilieren mit %s" % csc)
    subprocess.check_call([csc, "/nologo", "/target:exe", "/optimize+", "/platform:anycpu",
                           "/win32icon:" + icon, "/out:" + out, os.path.join(HERE, "launcher.cs")])
    log("-> %s (%.0f KB)" % (out, os.path.getsize(out) / 1024))


def smoke_test():
    """Startet die gebundelte Laufzeit mit leerer Umgebung (kein PATH zu anderen Pythons)."""
    env = {"SystemRoot": os.environ.get("SystemRoot", r"C:\Windows"),
           "PATH": os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")}
    py = os.path.join(RUNTIME, "python.exe")
    code = ("import sys, ctypes, json, argparse, random, html; import liminal.game as g, liminal.savegame, liminal.ui, liminal.items; "
            "print('ok', sys.version.split()[0], sys.prefix)")
    res = subprocess.run([py, "-B", "-c", code], cwd=ROOT, env=env, capture_output=True, text=True)
    log("Selbsttest: " + (res.stdout.strip() or res.stderr.strip()))
    if res.returncode != 0:
        raise SystemExit("Selbsttest fehlgeschlagen")


def main():
    if os.name != "nt":
        raise SystemExit("Die EXE kann nur unter Windows gebaut werden.")
    print("LIMINAL V3 - Build")
    build_icon()
    build_runtime()
    build_launcher()
    smoke_test()
    print("Fertig. Starten mit: LIMINAL.exe")


if __name__ == "__main__":
    main()
