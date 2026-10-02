"""Erzeugt Referenzdaten der V4-Weltgenerierung fuer den C++-Port (V5).

Aufruf (mit der Python-Laufzeit aus V4, -B verhindert __pycache__ im V4-Ordner):
    "LIMINAL V4\\runtime\\python.exe" -B tools\\v4_reference\\dump_ref.py "LIMINAL V4" tests\\data\\v4_reference.json

Der V4-Ordner wird nur gelesen. Pruefung: build\\Release\\tests\\worldgen_test.exe data\\levels\\level0.json tests\\data\\v4_reference.json
"""
import json
import os
import sys

V4_DIR = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "v4_reference.json")
sys.dont_write_bytecode = True
sys.path.insert(0, V4_DIR)

from liminal.world import World, CS  # noqa: E402
from liminal.rng import hash64, hfloat, Rng, fbm  # noqa: E402
from liminal.zones import ZoneField  # noqa: E402


def r6(v):
    return round(v, 6)


def dump_rng():
    out = {}
    out["hash64"] = [str(hash64(1, 2, 3)), str(hash64(7, "portal", 0, -3, 5)), str(hash64(-1)), str(hash64("zone"))]
    out["hfloat"] = [r6(hfloat(7, 11, -2, 3)), r6(hfloat(12345, 31, 4, 5))]
    rng = Rng(0xDEADBEEF)
    out["rng"] = [str(rng.next64()) for _ in range(4)] + [rng.randint(3, 17) for _ in range(4)] + [r6(rng.random()) for _ in range(4)]
    out["fbm"] = [r6(fbm(99, 1.25, -3.5, 2)), r6(fbm(5, -100.3, 42.7, 3))]
    zf = ZoneField(7)
    out["zone_w"] = [[r6(w) for w in zf.weights(x, y)] for x, y in ((0, 0), (300, -200), (-1000, 450))]
    return out


def dump_world(seed, sectors, chunks):
    w = World(seed)
    res = {"seed": seed, "sectors": [], "chunks": []}
    for sx, sy in sectors:
        sec = w.sector(sx, sy)
        rooms = []
        for r in sec.rooms:
            rooms.append({
                "rect": [r.x0, r.y0, r.x1, r.y1], "type": r.type.key, "zone": r.zone,
                "floor": r6(r.floor), "height": r6(r.height), "light": r.light_style,
                "ambient": r6(r.ambient), "lamp_int": r6(r.lamp_int), "avg": r6(r.avg_light),
                "fog": [r6(c) for c in r.fog], "fog_d": r6(r.fog_density), "flicker": r.flicker,
                "wall": [r6(c) for c in w.bank[r.wmat].rgb], "floor_rgb": [r6(c) for c in w.bank[r.fmat].rgb],
                "items": [[it.id, it.kind, r6(it.x), r6(it.y), r6(it.z)] for it in r.items],
            })
        doors = [[d.x0, d.y0, d.x1, d.y1, d.axis, d.kind, r6(d.sill), r6(d.top), int(d.portal), int(d.owned)]
                 for d in sec.doors]
        res["sectors"].append({"sx": sx, "sy": sy, "rooms": rooms, "doors": doors})
    for cx, cy in chunks:
        cells = w.load_chunk_now((cx, cy))
        tiles = []
        for c in cells:
            if c is None:
                tiles.append(None)
            else:
                tiles.append([r6(c[0]), r6(c[1]), r6(c[5]), [r6(v) for v in w.bank[c[2]].rgb],
                              [r6(v) for v in w.bank[c[4]].rgb], int(w.bank[c[3]].emissive)])
        corners = [r6(v) for v in w.corners[(cx, cy)]]
        res["chunks"].append({"cx": cx, "cy": cy, "cells": tiles, "corners": corners})
    x, y, z, a = w.find_spawn()
    res["spawn"] = [r6(x), r6(y), r6(z), r6(a)]
    return res


def main():
    out = {"rng": dump_rng(), "worlds": []}
    out["worlds"].append(dump_world(7, [(0, 0), (1, 0), (-1, -1), (2, -3)], [(0, 0), (2, 3), (-1, -1), (5, 5), (-4, 2)]))
    out["worlds"].append(dump_world(4242, [(0, 0), (-2, 1), (6, 6)], [(1, 1), (3, -2), (-6, 0)]))
    out["worlds"].append(dump_world(987654321, [(0, 0), (10, -7)], [(0, 1), (62, -40)]))
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f)
    print("ok", sum(len(s["rooms"]) for wd in out["worlds"] for s in wd["sectors"]), "rooms")


main()
