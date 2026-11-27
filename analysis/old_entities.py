"""Entity definitions of mission objects in a 2024 community dump (shalzuth/HelldiversData, one commit per manifest):
for each object, the component tables (data/entities/<Name>ComponentData.json) that list it. Compared with the 2026-09
dump (Darctor, 1.007.100) as a stand-in for the current game. Read from GitHub into memory; nothing is saved.

  python -I old_entities.py <commit> <hash16 or name> [...]

The 2024-06-01 manifest (5604538564913335971) is commit 5a2e4b30a2.
"""
import concurrent.futures as cf, json, os, sys, urllib.request
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

sys.stdout.reconfigure(encoding="utf-8")
API = "https://api.github.com/repos/shalzuth/HelldiversData/git/trees/{}?recursive=1"
RAW = "https://raw.githubusercontent.com/shalzuth/HelldiversData/{}/{}"
NEW = r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002\Darctor_Helldivers2_RawData\Data\settings\EntityComponentMap.json"


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "curl"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return r.read().decode("utf-8", errors="replace")


commit = sys.argv[1]
targets = {}
for arg in sys.argv[2:]:
    h = int(arg, 16) if hd.HEX16.match(arg.lower()) else int(hd.h64(arg), 16)
    targets[arg] = h
tree = json.loads(fetch(API.format(commit)))["tree"]
files = [x["path"] for x in tree if x["type"] == "blob" and x["path"].startswith("data/entities/")
         and x["path"].endswith("ComponentData.json")]
found = {a: [] for a in targets}


def scan(path):
    text = fetch(RAW.format(commit, path))
    name = path.rsplit("/", 1)[1].removesuffix("ComponentData.json")
    return name, [a for a, h in targets.items() if f'"{h}"' in text or f"{h}," in text or f"{h}}}" in text or f": {h}" in text]


with cf.ThreadPoolExecutor(8) as ex:
    for name, hits in ex.map(scan, files):
        for a in hits:
            found[a].append(name)
new = json.load(open(NEW, encoding="utf-8"))[0]["EntityComponentMap"]
new_ents = {int(e["hash"]): [c.split("<=>")[1].strip().removesuffix("ComponentData") for c in e["components"]] for e in new["entities"]}
print(f"2024 component tables scanned: {len(files)}")
for a, h in targets.items():
    n = new_ents.get(h)
    print(f"-- {a}")
    print(f"   2024: {', '.join(sorted(found[a])) or 'in no component table'}")
    print(f"   2026: {', '.join(n) if n is not None else 'not in dump'}")
