"""Entities HellForge read live component records for (a session's stats-values.tsv, written by the loader for every
component table it knows): which of the given objects have an entity in the running game, and its components.

  python -I live_entities.py <stats-values.tsv> <hash16 or name> [...]
"""
import collections, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

sys.stdout.reconfigure(encoding="utf-8")
ents = collections.defaultdict(set)
for line in open(sys.argv[1], encoding="utf-8", errors="ignore"):
    p = line.split("\t")
    if len(p) > 3 and p[0] == "component":
        ents[p[2].lower()].add(p[1].removesuffix("ComponentData"))
print("live entities:", len(ents), " component tables:", len({c for v in ents.values() for c in v}))
for arg in sys.argv[2:]:
    h = arg.lower() if hd.HEX16.match(arg.lower()) else hd.h64(arg)
    print(f"-- {arg} ({h}): {', '.join(sorted(ents[h])) if h in ents else 'NO LIVE RECORDS'}")
