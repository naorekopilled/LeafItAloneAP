#!/usr/bin/env python3
"""Builds the one location/item table from dumped assets plus curated logic.

Input: core.json from assets_dump.py. Output: apworld/leaf_it_alone/table.py and
client/src/gen_table.h. Add new content here; everything else reads the output.
"""
import json, sys, math

core, out_py, out_h = sys.argv[1:4]
d = json.load(open(core))
by = {}
for k, rows in d.items():
    for r in rows:
        if r["tt"]:
            by[r["pid"]] = (k, r["tt"])
get = lambda p: by.get(p.get("m_PathID"), (None, None))[1]
refs = lambda t: {x["rid"]: x for x in t.get("references", {}).get("RefIds", [])}

BASE = 19_731_000
LEVELS = {"Hometown": {"scene": 0, "price": 0.004, "bundle": "hometowndetails", "areas": "areas2"},
          "Castle": {"scene": 1, "price": 0.004, "bundle": "castledetails", "areas": "areas1"},
          "Sakura": {"scene": 3, "price": 0.01, "bundle": None, "areas": "areas"}}

# Region -> requirements. Start pockets come from a collision flood of each scene from its spawn point.
# Anything the flood could not place sits later than it may need to: assumed access must never exceed real access.
K = lambda s: f"Key: {s}"
REGIONS = {
    "Hometown": {
        "Yard": [],
        "Garage": ["Yard", "Ability: Sprint"],  # the Shed button opens the door, then you have to outrun it
        "Shed": ["Yard", K("Shed")],
        "Backyard": ["Shed", K("Grill")],
        "Greenhouse": ["Backyard", K("Greenhouse")],
        "Secret Room": ["Greenhouse", "Garage", K("Secret Room")],
    },
    "Castle": {
        "Gate": [],
        "Courtyard": ["Gate", K("Courtyard")],
        "Hallway": ["Courtyard", K("Hallway")],
        "Dinette": ["Hallway", K("Dinette")],
        "Love Garden": ["Dinette", K("Love Garden")],
        "Throne Room": ["Love Garden", K("Throne Room Love Seal"), K("Throne Room Leaf Seal")],
    },
    "Sakura": {
        "Statues": [],
        "Temple": ["Statues", K("Temple")],
        "Shrines": ["Temple", K("Shrines")],
        "Lanterns": ["Temple", K("Lanterns")],
        "Inner Gardens": ["Shrines", "Lanterns"],
        "Park": ["Inner Gardens", K("Park")],
        "Dog Run": ["Park", K("Dog Run")],
        "Tree": ["Dog Run", K("Tree")],
    },
}
AREA_REGION = {
    "Entrance": "Yard", "Frontyard": "Yard", "Driveway": "Yard", "Porch": "Yard", "Court": "Yard",
    "Garage": "Garage", "Basement": "Garage", "Swings": "Garage", "Shed": "Shed", "Grill": "Backyard", "Pool": "Backyard",
    "Greenhouse": "Greenhouse", "Maze": "Greenhouse",
    "Gate": "Gate", "Courtyard": "Courtyard", "Stairs": "Courtyard", "Hallway": "Hallway",
    "Fresco": "Hallway", "Range": "Hallway",
    "Dinette": "Dinette", "Terrace": "Dinette", "Armory": "Dinette", "Library": "Dinette", "LoveGarden": "Love Garden", "Ruins": "Love Garden",
    "Labyrinth": "Love Garden", "LeafGarden": "Love Garden",
    "Statues": "Statues", "Fountain": "Park", "Temple": "Temple", "Shrines": "Shrines",
    "Lanterns": "Lanterns", "Garden": "Inner Gardens", "Playground": "Inner Gardens",
    "Alley": "Park", "OpenAir": "Park", "Picnic": "Park", "DogRun": "Dog Run", "Recreation": "Dog Run",
}
AREA_EXTRA = {"Range": ["Activity: Archery Range"]}
BIG_AREA = {"Hometown": 15000, "Castle": 15000, "Sakura": 25000}
TOOLS = ["Rake", "Leaf Blower", "Mini Tractor"]

KEYS = {"UnlockShed": ("Hometown", "Shed", 1), "UnlockGrill": ("Hometown", "Grill", 3),
        "UnlockGreenhouse": ("Hometown", "Greenhouse", 2), "UnlockSecretRoom": ("Hometown", "Secret Room", 4),
        "UnlockCourtyard": ("Castle", "Courtyard", 14), "UnlockHallway": ("Castle", "Hallway", 19),
        "UnlockDinette": ("Castle", "Dinette", 17), "UnlockLoveGarden": ("Castle", "Love Garden", 20),
        "UnlockThroneRoom_LoveGarden": ("Castle", "Throne Room Love Seal", 16),
        "UnlockThroneRoom_LeafGarden": ("Castle", "Throne Room Leaf Seal", 23),
        "UnlockTemple": ("Sakura", "Temple", 25), "UnlockTree": ("Sakura", "Tree", 35),
        "UnlockDogRun": ("Sakura", "Dog Run", 33)}
ABILITY = {"UnlockSprint": "Sprint", "UnlockCrouch": "Crouch", "UnlockBagSealing": "Bag Sealing",
           "UnlockLanternLighting": "Lantern Lighting"}
ACTIVITY = {"UnlockSlidingPuzzle": "Sliding Puzzle", "UnlockMemory": "Memory Game", "UnlockMosaics": "Mosaics",
            "UnlockArcheryRange": "Archery Range", "UnlockPetalSort": "Petal Sort"}

# name -> (region, extra requirements). Objectives absent here are not locations.
OBJ = {
    "JumpIntoLeafPile": ("Yard", ["TOOL"]), "ScoreBasketball": ("Garage", ["Ability: Bag Sealing"]),
    "SealBags": ("Yard", ["Ability: Bag Sealing", "TOOL"]), "ThrowLeafBag": ("Yard", ["Ability: Bag Sealing"]),
    "FindAllCollectables_Hometown": ("Greenhouse", ["Ability: Crouch", "Ability: Sprint"]), "UnlockGarage": ("Garage", []),
    "UnlockShed": ("Shed", []), "UnlockGrill": ("Backyard", []), "UnlockGreenhouse": ("Greenhouse", []),
    "UnlockSecretRoom": ("Secret Room", []),
    "UnlockCourtyard": ("Courtyard", []), "UnlockHallway": ("Hallway", []), "UnlockDinette": ("Dinette", []),
    "UnlockLoveGarden": ("Love Garden", []), "UnlockThroneRoom": ("Throne Room", []),
    "PassArcheryTest": ("Hallway", ["Activity: Archery Range"]), "SortPetals": ("Hallway", ["Activity: Petal Sort"]),
    "CompleteMemoryGame": ("Dinette", ["Activity: Memory Game"]),
    "AssembleMosaics": ("Love Garden", ["Activity: Mosaics"]), "ReassembleMosaic": ("Love Garden", ["Activity: Mosaics"]),
    "SolveSlidingPuzzle": ("Love Garden", ["Activity: Sliding Puzzle"]),
    "FindAllCollectables_Castle": ("Love Garden", ["Ability: Crouch"]),
    "ScoreExactArcheryPoints": ("Hallway", ["Activity: Archery Range"]), "RingBell": ("Dinette", []),
    "PutBooksOnBookshelf": ("Dinette", []),
    "SellWorthyBag": ("Love Garden", ["Progressive Bag (Castle)x2", "Progressive Bag Capacity (Castle)x5", "Income Boost (Castle)x3"]),
    "UnlockTemple": ("Temple", []), "UnlockShrines": ("Shrines", []), "UnlockLanterns": ("Lanterns", []),
    "UnlockPark": ("Park", []), "UnlockDogRun": ("Dog Run", []), "UnlockTree": ("Tree", []),
    "SuckUpLeaves": ("Park", ["Leaf Blower", "Progressive Leaf Blower Suction Mode (Sakura)"]),
    "FindAllCollectables_Sakura": ("Dog Run", ["Ability: Crouch"]),
    "SpinAllPinwheels": ("Statues", ["Leaf Blower"]),
    "CollectLeavesAtDrivingSpeed": ("Dog Run", ["Mini Tractor", "Progressive Tractor Swift Mode (Sakura)"]),
    "DumpPresetLeafBags": ("Dog Run", []),
    "LightUpLanterns": ("Inner Gardens", ["Ability: Lantern Lighting"]),
}
EXCLUDED_LOCS = {"SellWorthyBag"}  # bag value threshold unverified, so only filler may land here
CLEAN = {"Hometown": "CleanUpAllLeaves_Hometown", "Castle": "CleanUpAllLeaves_Castle", "Sakura": "CleanUpAllLeaves_Sakura"}

SHOP = {  # level -> [(item line, actor id, price source actor)] in exhibit chain order
    "Hometown": [("Rake", "Rake", "Yard"), ("Leaf Blower", "LeafBlower", "Yard"), ("Medium Bag", "MediumLeafBag", "Yard"), ("Big Bag", "BigLeafBag", "Yard")],
    "Castle": [("Leaf Blower", "LeafBlower", "Hallway"), ("Big Bag", "BigLeafBag", "Hallway"), ("Giant Bag", "GiantLeafBag", "Hallway")],
    "Sakura": [("Mini Tractor", "Tractor", "Dog Run")],
}
ACTORS = {r["tt"]["m_Name"]: r["tt"] for r in d["actors"]}
def price(actor, level):
    rr = refs(ACTORS[actor])
    for x in rr.values():
        if x["type"]["class"] == "ItemProperty":
            p = x["data"]["pricesByScene"]
            sc = [i for i, s in enumerate(p["values"])]
            names = []
            for key in p["keys"]:
                names.append(key if isinstance(key, str) else get(key)["m_Name"])
            return round(dict(zip(names, p["values"]))[level], 2)

rewards = {}
for b in ("rewards", "rewards1", "rewards2"):
    for r in d[b]:
        rewards[r["tt"]["m_Name"]] = r["tt"]
PRETTY = {"LoveGarden": "Love Garden", "LeafGarden": "Leaf Garden", "DogRun": "Dog Run", "OpenAir": "Open Air"}
pretty = lambda s: PRETTY.get(s, s)
line_name = lambda s: "".join((" " + c if c.isupper() and i else c) for i, c in enumerate(s)).replace("  ", " ")
TITLE = {"BagSealingTIme": "Bag Sealing Time", "HandHoldInteraction": "Hand Hold", "LeafBlowerAirstreamWidth": "Airstream Width", "LeafBlowerSpreadFactor": "Air Spread", "LeafBlowerPower": "Blower Power", "RakeHandleLength": "Handle Length", "RakeSweepWidth": "Sweep Width", "RakeInteractionStrength": "Leaf Stickiness", "RakeUsageCooldown": "Rake Cooldown", "HandPickupInterval": "Quick Grab", "HandCapacity": "Greedy Grip"}

items, locs = [], []
def add_item(name, cls, count=1, **kw):
    items.append({"name": name, "cls": cls, "count": count, **kw})
loc_lines = {}
shop_entries = {lv: [] for lv in LEVELS}
for lv, L in LEVELS.items():
    t = d[L["bundle"]][0]["tt"] if L["bundle"] else [r["tt"] for r in d["scenes"] if r["tt"]["m_Name"] == "SakuraDetails"][0]
    rr = refs(t)
    props = {rr[p["rid"]]["type"]["class"]: rr[p["rid"]]["data"] for p in t["properties"]}
    seen = set()
    first_region = list(REGIONS[lv])[0]
    for u in props["UpgradesSceneProperty"]["upgrades"]:
        s = get(u)
        if s["m_Name"] in seen:
            continue
        seen.add(s["m_Name"])
        base = s["m_Name"].replace(f"_{lv}", "")
        title = TITLE.get(base, line_name(base))
        cls = "progression" if base in ("TractorSwiftMode", "LeafBlowerSuctionMode", "BagCapacity") else "useful"
        raw = refs(s)[s["descriptor"]["rid"]]["data"]
        tiers = []
        if "upgradeGroup" in raw:
            st = [get(x) for x in get(raw["upgradeGroup"])["settings"]]
        else:
            st = [get(raw[k]) for k in raw if isinstance(raw[k], dict) and "m_PathID" in raw[k] and get(raw[k]) and "upgrade" in get(raw[k])]
        for x in st:
            up = refs(x)[x["upgrade"]["rid"]]["data"]
            m = get(x["modifier"])
            tiers.append({"cost": round(up["cost"], 2), "modifier": m["uniqueId"]})
        item = f"Progressive {title} ({lv})"
        add_item(item, cls, len(tiers), kind="upgrade", level=lv, tiers=[z["modifier"] for z in tiers], upgrade_type=s["uniqueId"])
        for i, z in enumerate(tiers):
            nm = f"{lv} Shop: {title} {i+1}"
            locs.append({"name": nm, "level": lv, "kind": "tier", "game_id": z["modifier"], "region": first_region, "requires": [], "cost": z["cost"], "line": title})
            shop_entries[lv].append(nm)
    for line, actor, region in SHOP[lv]:
        nm = f"{lv} Shop: {line}"
        locs.append({"name": nm, "level": lv, "kind": "item", "game_id": ACTORS[actor]["uniqueId"], "region": region, "requires": [], "cost": price(actor, lv), "line": line})
        shop_entries[lv].append(nm)
    # areas
    L["area_rows"] = []
    for r in d[L["areas"]]:
        a = r["tt"]
        if a.get("isMetaArea"):
            continue
        reg = AREA_REGION[a["m_Name"]]
        L["area_rows"].append({"name": a["m_Name"], "leaves": a["leavesCount"], "region": reg, "requires": AREA_EXTRA.get(a["m_Name"], [])})
        for key in sorted(k for k in rewards if k.rsplit("_", 1)[0] == a["m_Name"]):
            req = list(AREA_EXTRA.get(a["m_Name"], []))
            if a["leavesCount"] >= BIG_AREA[lv]:
                req.append("TOOL")
            rw = rewards[key]
            n = key.rsplit("_", 1)[1]
            locs.append({"name": f"{lv} Clear {pretty(a['m_Name'])}" + ("" if n == "1" else f" Reward {n}"), "level": lv, "kind": "reward", "game_id": rw["uniqueId"], "region": reg, "requires": req})
    # objectives
    for o in props["ObjectivesSceneProperty"]["objectives"]:
        s = get(o)
        n = s["m_Name"]
        if n in OBJ:
            reg, req = OBJ[n]
            locs.append({"name": f"{lv} Objective: {line_name(n.replace('_' + lv, '')).strip()}", "level": lv, "kind": "objective", "game_id": s["uniqueId"], "region": reg, "requires": req, "excluded": n in EXCLUDED_LOCS})
        if n == CLEAN[lv]:
            req = ["TOOL"] + (["Activity: Archery Range"] if lv == "Castle" else [])
            locs.append({"name": f"{lv} Clean Up All Leaves", "level": lv, "kind": "clean", "game_id": s["uniqueId"], "region": list(REGIONS[lv])[-1], "requires": req})

# reward-derived items
mods = {r["tt"]["m_Name"]: r["tt"] for b in ("modifiers", "modifiers1", "modifiers2") for r in d[b]}
for name, (lv, label, feature) in KEYS.items():
    add_item(K(label), "progression", kind="modifier", level=lv, modifiers=[mods[name]["uniqueId"]])
m = mods["UnlockShrinesLanterns"]["uniqueId"]
add_item(K("Shrines"), "progression", kind="feature", level="Sakura", features=[27])
add_item(K("Lanterns"), "progression", kind="feature", level="Sakura", features=[29])
add_item(K("Park"), "progression", kind="modifier", level="Sakura", modifiers=[mods["UnlockPark_Garden"]["uniqueId"]])
for name, label in ABILITY.items():
    add_item(f"Ability: {label}", "progression" if label in ("Sprint", "Bag Sealing", "Crouch", "Lantern Lighting") else "useful", kind="modifier", level={"UnlockLanternLighting": "Sakura"}.get(name, "Hometown"), modifiers=[mods[name]["uniqueId"]])
for name, label in ACTIVITY.items():
    add_item(f"Activity: {label}", "progression", kind="modifier", level="Castle", modifiers=[mods[name]["uniqueId"]])
add_item("Income Boost (Hometown)", "progression", 3, kind="tiered", level="Hometown", modifiers=[36, 35, 39])
add_item("Income Boost (Castle)", "progression", 3, kind="tiered", level="Castle", modifiers=[102, 103, 104])
add_item("Move Speed (Hometown)", "useful", 3, kind="tiered", level="Hometown", modifiers=[26, 27, 28])
add_item("Sprint Speed (Castle)", "useful", kind="modifier", level="Castle", modifiers=[106])
add_item("Sprint Speed (Sakura)", "useful", kind="modifier", level="Sakura", modifiers=[109])
add_item("Upgrade Discount (Sakura)", "useful", 2, kind="tiered", level="Sakura", modifiers=[150, 151])
add_item("Amplify Upgrades (Hometown)", "useful", kind="modifier", level="Hometown", modifiers=[52])
add_item("Amplify Suction Range (Sakura)", "useful", kind="modifier", level="Sakura", modifiers=[149])
add_item("Rake", "progression", kind="tool", actor=ACTORS["Rake"]["uniqueId"])
add_item("Leaf Blower", "progression", kind="tool", actor=ACTORS["LeafBlower"]["uniqueId"])
add_item("Mini Tractor", "progression", kind="tool", actor=ACTORS["Tractor"]["uniqueId"])
add_item("Progressive Bag (Hometown)", "useful", 2, kind="bag", level="Hometown", actors=[ACTORS["MediumLeafBag"]["uniqueId"], ACTORS["BigLeafBag"]["uniqueId"]])
add_item("Progressive Bag (Castle)", "progression", 2, kind="bag", level="Castle", actors=[ACTORS["BigLeafBag"]["uniqueId"], ACTORS["GiantLeafBag"]["uniqueId"]])
for lv in LEVELS:
    add_item(f"Level: {lv}", "progression", kind="level", scene=LEVELS[lv]["scene"], level=lv)
add_item("Level Cleared", "progression", 3, kind="token")
BUNDLE = 25.0
LEVEL_MAX_INCOME = {}
for lv, L in LEVELS.items():
    leaves = sum(a["leaves"] for a in L["area_rows"])
    boosts = 3 if lv != "Sakura" else 0
    income = leaves * L["price"] * (1 + 0.25 * boosts)
    spend = sum(x["cost"] for x in locs if x["level"] == lv and "cost" in x) + (500 if lv == "Sakura" else 0)
    target = spend * 1.1
    n = max(0, math.ceil((target - income) / BUNDLE))
    LEVEL_MAX_INCOME[lv] = {"income": round(income, 2), "spend": round(spend, 2), "bundles": n}
    if n:
        add_item(f"Money Bundle ({lv})", "progression", n, kind="money", level=lv, amount=BUNDLE)
add_item("Money", "filler", 0, kind="money_any", amount=5.0)
add_item("Nothing", "filler", 0, kind="nothing")

for lv in LEVELS:
    # cumulative cost, cheapest first: what must be earned to have paid for this entry
    es = sorted([x for x in locs if x["level"] == lv and "cost" in x], key=lambda x: x["cost"])
    run = 0.0
    for x in es:
        run += x["cost"]
        x["earn"] = round(run, 2)

for i, it in enumerate(items):
    it["id"] = BASE + i
for i, x in enumerate(locs):
    x["id"] = BASE + 1000 + i

table = {"levels": {k: {"scene": v["scene"], "price": v["price"], "areas": v["area_rows"], "regions": REGIONS[k]} for k, v in LEVELS.items()},
         "tools": TOOLS, "items": items, "locations": locs, "bundle_amount": BUNDLE, "income_summary": LEVEL_MAX_INCOME}
with open(out_py, "w") as f:
    f.write('"""Generated by scripts/gen_table.py. Edit the generator, not this file."""\n')
    f.write("TABLE = " + repr(table) + "\n")
kinds = {"tier": 0, "item": 1, "reward": 2, "objective": 3, "clean": 4}
ik = {"upgrade": 0, "modifier": 1, "tiered": 1, "feature": 2, "tool": 3, "bag": 4, "level": 5, "token": 6, "money": 7, "money_any": 7, "nothing": 8}
with open(out_h, "w") as f:
    f.write("// Generated by scripts/gen_table.py. Do not edit.\n#pragma once\n#include <cstdint>\n")
    f.write("struct LocRow { int64_t ap; int kind; int game; int scene; const char* name; };\n")
    f.write("struct ItemRow { int64_t ap; int kind; int scene; float amount; int n; const int* ids; const char* name; };\n")
    for it in items:
        ids = it.get("tiers") or it.get("modifiers") or it.get("features") or it.get("actors") or ([it["actor"]] if "actor" in it else ([it["scene"]] if it["kind"] == "level" else []))
        if ids:
            f.write(f"static const int ids_{it['id']}[] = {{{','.join(map(str, ids))}}};\n")
    f.write("static const ItemRow ITEMS[] = {\n")
    for it in items:
        ids = it.get("tiers") or it.get("modifiers") or it.get("features") or it.get("actors") or ([it["actor"]] if "actor" in it else ([it["scene"]] if it["kind"] == "level" else []))
        sc = LEVELS[it["level"]]["scene"] if "level" in it else -1
        f.write(f"  {{{it['id']},{ik[it['kind']]},{sc},{float(it.get('amount', 0))}f,{len(ids)},{'ids_' + str(it['id']) if ids else 'nullptr'},{json.dumps(it['name'])}}},\n")
    f.write("};\nstatic const LocRow LOCS[] = {\n")
    for x in locs:
        f.write(f"  {{{x['id']},{kinds[x['kind']]},{x['game_id']},{LEVELS[x['level']]['scene']},{json.dumps(x['name'])}}},\n")
    f.write("};\n")
print(len(locs), "locations", sum(i["count"] for i in items), "items before filler", LEVEL_MAX_INCOME)
import collections
print(collections.Counter(x["kind"] for x in locs), collections.Counter(x["level"] for x in locs))
print([x["name"] for x in locs if x["kind"] in ("objective",)][:6], len(items))
