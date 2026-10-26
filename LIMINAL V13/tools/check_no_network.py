"""V13: Prueft, dass die ausgelieferten Programme keinen Netzwerkcode enthalten (MULTIPLAYER_ENTSCHEIDUNG.md).

    python tools/check_no_network.py [bin-ordner]

1. Importtabelle (llvm-objdump -p aus .toolchain): keine Netzwerk-DLL (ws2_32, wsock32, winhttp, wininet, urlmon,
   mswsock, iphlpapi, dnsapi, winsock).
2. Zeichenketten im Programm: kein Name einer Netzwerk-DLL und kein Winsock-Aufruf (auch nicht fuer LoadLibrary).
Bekannte Ausnahme: der Starter LIMINAL.exe laedt auf alten Windows-Versionen ohne Universal C Runtime - nur nach
Rueckfrage - das Visual-C++-Paket von Microsoft (HTTPS, aka.ms) und fuehrt es nur aus, wenn es von
"Microsoft Corporation" signiert ist (V13, BUGLISTE.md B-003). Dafuer urlmon per LoadLibrary. Kein Port, kein Spielverkehr.
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
print('SAUBER' if bad == 0 else f'FUNDE: {bad}')
sys.exit(1 if bad else 0)
