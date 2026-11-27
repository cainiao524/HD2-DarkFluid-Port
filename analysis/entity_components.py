"""Entity definitions of mission objects in the community entity map (Darctor raw data, game 1.007.100, 2026-09-22):
whether the entity exists and which components it has. Also lists component types matching a pattern.

  python -I entity_components.py --types <regex>
  python -I entity_components.py <hash16 or name> [...]
"""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

MAP = r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002\Darctor_Helldivers2_RawData\Data\settings\EntityComponentMap.json"
sys.stdout.reconfigure(encoding="utf-8")
m = json.load(open(MAP, encoding="utf-8"))[0]["EntityComponentMap"]
ents = {int(e["hash"]): e for e in m["entities"]}
print("map:", m["_metadata"]["game_version"], len(ents), "entities,", m["_metadata"]["component_type_count"], "component types")
if sys.argv[1] == "--types":
    types = sorted({c for e in m["entities"] for c in e["components"]}, key=lambda c: int(c.split()[0]))
    pat = re.compile(sys.argv[2], re.I)
    for c in types:
        if pat.search(c):
            n = sum(1 for e in m["entities"] if c in e["components"])
            print(f"   {c}  ({n} entities)")
    sys.exit()
for arg in sys.argv[1:]:
    h = int(arg, 16) if hd.HEX16.match(arg.lower()) else int(hd.h64(arg), 16)
    e = ents.get(h)
    label = arg if not hd.HEX16.match(arg.lower()) else f"{h:016x}"
    if e is None:
        print(f"-- {label}: NO ENTITY")
    else:
        comps = [c.split("<=>")[1].strip().removesuffix("ComponentData") for c in e["components"]]
        print(f"-- {label} ({e['name']} / {e['name_zh']}): {', '.join(comps)}")
