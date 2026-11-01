"""V13: Prueft, dass die ausgelieferten Programme keinen Netzwerkcode enthalten (MULTIPLAYER_ENTSCHEIDUNG.md).

    python tools/check_no_network.py [bin-ordner]

1. Importtabelle (llvm-objdump -p aus .toolchain): keine Netzwerk-DLL (ws2_32, wsock32, winhttp, wininet, urlmon,
   mswsock, iphlpapi, dnsapi, winsock).
2. Zeichenketten im Programm: kein Name einer Netzwerk-DLL und kein Winsock-Aufruf (auch nicht fuer LoadLibrary).
Bekannte Ausnahme: der Starter LIMINAL.exe laedt auf alten Windows-Versionen ohne Universal C Runtime - nur nach
Rueckfrage - das Visual-C++-Paket von Microsoft (HTTPS, aka.ms) und fuehrt es nur aus, wenn es von
"Microsoft Corporation" signiert ist (V13, BUGLISTE.md B-003). Dafuer urlmon per LoadLibrary. Kein Port, kein Spielverkehr.
V13.2/V13.3 (Koop): Netzwerk gibt es nur in liminal_net.dll. Das Spiel laedt sie erst beim Hosten oder Beitreten
(LoadLibrary, MULTIPLAYER_ARCHITEKTUR.md Abschnitt 7) und darf sie nicht statisch importieren. Die DLL darf ws2_32
(UDP/TCP fuer WebRTC und die eingebaute Lobby), iphlpapi (Netzwerkschnittstellen fuer ICE) und winhttp (WebSocket zur
Lobby, TLS-Pruefung durch Windows) importieren, aber keine Download-Bibliothek (wininet, urlmon) und keine
Namensaufloesung ausser ueber Winsock (dnsapi); sie exportiert genau die elf ln_-Funktionen.
liminal_lobby.exe ist der Lobby- und Relay-Server zum Selbstbetreiben (ein Server, kein Teil des Spiels): er wird nur
darauf geprueft, dass das Spiel ihn nirgends nennt.
Ausgang 0 = sauber, 1 = Fund, 2 = Werkzeug fehlt.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
BIN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'Release', 'bin')
OBJDUMP = os.path.join(ROOT, '.toolchain', 'llvm-mingw', 'bin', 'llvm-objdump.exe')
NET_DLLS = ('ws2_32', 'wsock32', 'winhttp', 'wininet', 'urlmon', 'mswsock', 'iphlpapi', 'dnsapi', 'winsock')
NET_CALLS = (b'WSAStartup', b'getaddrinfo', b'WinHttpOpen', b'InternetOpen', b'URLDownloadToFile', b'bind', b'listen')
EXCEPTIONS = {'LIMINAL.exe': {'urlmon', 'URLDownloadToFile'}}

if not os.path.exists(OBJDUMP):
    print('llvm-objdump fehlt:', OBJDUMP)
    sys.exit(2)
bad = 0
for exe in ('LiminalGame.exe', 'LIMINAL.exe'):
    path = os.path.join(BIN, exe)
    if not os.path.exists(path):
        print('fehlt:', path)
        bad += 1
        continue
    out = subprocess.run([OBJDUMP, '-p', path], capture_output=True, text=True).stdout
    dlls = sorted({m.lower() for m in re.findall(r'DLL Name:\s*(\S+)', out)})
    hits = [d for d in dlls if any(n in d for n in NET_DLLS)]
    data = open(path, 'rb').read()
    low = data.lower()
    wide = lambda t: t.encode('utf-16-le')  # Windows-Zeichenketten (L"...") stehen als UTF-16 im Programm
    strs = ([n for n in NET_DLLS if n.encode() in low or wide(n) in low] +
            [c.decode() for c in NET_CALLS if re.search(b'(?<![A-Za-z_])' + c + b'(?![a-z_])', data) or wide(c.decode()) in data])
    known = [x for x in strs if x in EXCEPTIONS.get(exe, set())]
    strs = [x for x in strs if x not in known]
    print(f'{exe}: {len(dlls)} DLLs importiert, Netzwerk-DLLs: {hits or "keine"}, Netzwerk-Zeichenketten: {strs or "keine"}'
          + (f', bekannte Ausnahme: {known}' if known else ''))
    bad += len(hits) + len(strs)
# V13.2: die Netzwerk-DLL des Koop-Modus
EXPORTS = {'ln_host', 'ln_join', 'ln_poll', 'ln_send', 'ln_drop', 'ln_stats', 'ln_code', 'ln_port', 'ln_close', 'ln_random', 'ln_version'}
game = subprocess.run([OBJDUMP, '-p', os.path.join(BIN, 'LiminalGame.exe')], capture_output=True, text=True).stdout
if 'liminal_net' in game.lower():
    print('LiminalGame.exe importiert liminal_net.dll statisch (verboten: nur LoadLibrary beim Hosten/Beitreten)')
    bad += 1
if b'liminal_lobby' in open(os.path.join(BIN, 'LiminalGame.exe'), 'rb').read().lower():
    print('LiminalGame.exe nennt liminal_lobby (verboten: das Spiel startet keinen Server)')
    bad += 1
dll = os.path.join(BIN, 'liminal_net.dll')
if os.path.exists(dll):
    out = subprocess.run([OBJDUMP, '-p', dll], capture_output=True, text=True).stdout
    dlls = sorted({m.lower() for m in re.findall(r'DLL Name:\s*(\S+)', out)})
    web = [d for d in dlls if any(n in d for n in ('wininet', 'urlmon', 'dnsapi'))]
    exports = set(re.findall(r'\]\s+(ln_\w+)', out)) | set(re.findall(r'^\s+\d+\s+\w+\s+(\w+)$', out, re.M))
    exports = {e for e in exports if e.startswith('ln_') or e in EXPORTS}
    extra = sorted(exports - EXPORTS)
    missing = sorted(EXPORTS - exports)
    print(f'liminal_net.dll: {len(dlls)} DLLs importiert ({", ".join(d for d in dlls if not d.startswith("api-ms"))}), '
          f'Download-Bibliotheken: {web or "keine"}, Exporte: {len(exports)}' + (f', fremd: {extra}' if extra else '') +
          (f', fehlen: {missing}' if missing else ''))
    bad += len(web) + len(extra) + len(missing)
else:
    print('liminal_net.dll fehlt (Koop nicht moeglich)')
print('SAUBER' if bad == 0 else f'FUNDE: {bad}')
sys.exit(1 if bad else 0)
