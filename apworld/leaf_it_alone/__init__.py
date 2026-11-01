from typing import Any, Callable, Dict, List, Set

from BaseClasses import CollectionState, Entrance, Item, ItemClassification, Location, LocationProgressType, Region
from worlds.AutoWorld import WebWorld, World

from .options import LeafItAloneOptions
from .table import TABLE

CLASSES = {"progression": ItemClassification.progression, "useful": ItemClassification.useful,
           "filler": ItemClassification.filler}
MENU = "Menu"
TOKEN = "Level Cleared"
LEVEL_NAMES = list(TABLE["levels"])


def closure(level: str, region: str) -> Set[str]:
    """All items needed to enter a region, following its parent regions."""
    regions = TABLE["levels"][level]["regions"]
    need: Set[str] = set()
    for req in regions[region]:
        need |= closure(level, req) if req in regions else {req}
    return need


def parse(requires: List[str]) -> Dict[str, int]:
    """Turns 'Item', 'Itemx3' into counts. TOOL is handled by the caller."""
    out: Dict[str, int] = {}
    for req in requires:
        if req == "TOOL":
            continue
        name, _, n = req.rpartition("x")
        if name and n.isdigit():
            out[name] = int(n)
        else:
            out[req] = 1
    return out


class LeafItAloneItem(Item):
    game = "Leaf it Alone"


class LeafItAloneLocation(Location):
    game = "Leaf it Alone"


class LeafItAloneWeb(WebWorld):
    theme = "grassFlowers"


class LeafItAloneWorld(World):
    """Rake, blow and vacuum leaves across three levels. Purchases, area clears and objectives are checks."""
    game = "Leaf it Alone"
    web = LeafItAloneWeb()
    options_dataclass = LeafItAloneOptions
    options: LeafItAloneOptions
    topology_present = False
    item_name_to_id = {i["name"]: i["id"] for i in TABLE["items"]}
    location_name_to_id = {x["name"]: x["id"] for x in TABLE["locations"]}
    item_name_groups = {"Keys": {i["name"] for i in TABLE["items"] if i["name"].startswith("Key: ")},
                        "Levels": {i["name"] for i in TABLE["items"] if i["kind"] == "level"}}

    start_level: str

    def generate_early(self) -> None:
        self.start_level = self.random.choice(LEVEL_NAMES) if self.options.random_starting_level else "Hometown"
        self.multiworld.push_precollected(self.create_item(f"Level: {self.start_level}"))
        # Flattened per area so the money rule only tests plain item counts.
        self.area_rules = {lv: [(a["leaves"], self.access_counts(lv, a), a["leaves"] >= 15000 or lv == "Sakura" and a["leaves"] >= 25000)
                                for a in data["areas"]] for lv, data in TABLE["levels"].items()}

    def access_counts(self, level: str, area: Dict[str, Any]) -> Dict[str, int]:
        need = {name: 1 for name in closure(level, area["region"])}
        need.update(parse(area["requires"]))
        return need

    def has_counts(self, state: CollectionState, need: Dict[str, int]) -> bool:
        return all(state.has(k, self.player, n) for k, n in need.items())

    def has_tool(self, state: CollectionState) -> bool:
        return state.has_any(TABLE["tools"], self.player)

    def income(self, state: CollectionState, level: str) -> float:
        """Money the player can earn in a level with what they hold now."""
        data = TABLE["levels"][level]
        tool = self.has_tool(state)
        leaves = sum(n for n, need, big in self.area_rules[level] if (tool or not big) and self.has_counts(state, need))
        total = leaves * data["price"] * (1 + 0.25 * state.count(f"Income Boost ({level})", self.player))
        return total + state.count(f"Money Bundle ({level})", self.player) * TABLE["bundle_amount"]

    def create_regions(self) -> None:
        menu = Region(MENU, self.player, self.multiworld)
        self.multiworld.regions.append(menu)
        by_region: Dict[str, Region] = {}
        for level, data in TABLE["levels"].items():
            for name in data["regions"]:
                region = Region(f"{level}: {name}", self.player, self.multiworld)
                by_region[region.name] = region
                self.multiworld.regions.append(region)
                need = {k: 1 for k in closure(level, name)}
                need[f"Level: {level}"] = 1
                entrance = Entrance(self.player, f"To {region.name}", menu)
                menu.exits.append(entrance)
                entrance.connect(region)
                entrance.access_rule = self.entrance_rule(level, name, need)
        for row in TABLE["locations"]:
            region = by_region[f"{row['level']}: {row['region']}"]
            location = LeafItAloneLocation(self.player, row["name"], row["id"], region)
            region.locations.append(location)
            location.access_rule = self.location_rule(row)
            if row.get("excluded"):
                location.progress_type = LocationProgressType.EXCLUDED
            if row["kind"] == "clean":
                location.place_locked_item(self.create_item(TOKEN))

    def entrance_rule(self, level: str, name: str, need: Dict[str, int]) -> Callable[[CollectionState], bool]:
        if name == "Tree":  # the tree door also costs 500
            return lambda state: self.has_counts(state, need) and self.income(state, level) >= 500
        return lambda state: self.has_counts(state, need)

    def location_rule(self, row: Dict[str, Any]) -> Callable[[CollectionState], bool]:
        need = parse(row["requires"])
        tool = "TOOL" in row["requires"]
        level = row["level"]
        earn = row.get("earn")

        def rule(state: CollectionState) -> bool:
            if tool and not self.has_tool(state):
                return False
            if not self.has_counts(state, need):
                return False
            return earn is None or self.income(state, level) >= earn
        return rule

    def create_item(self, name: str) -> LeafItAloneItem:
        row = next(i for i in TABLE["items"] if i["name"] == name)
        return LeafItAloneItem(name, CLASSES[row["cls"]], row["id"], self.player)

    def create_items(self) -> None:
        pool: List[Item] = []
        for row in TABLE["items"]:
            if row["name"] == TOKEN or row["cls"] == "filler":
                continue
            if row["name"] == f"Level: {self.start_level}":
                continue
            pool += [self.create_item(row["name"]) for _ in range(row["count"])]
        open_slots = len(self.multiworld.get_unfilled_locations(self.player)) - len(pool)
        pool += [self.create_filler() for _ in range(open_slots)]
        self.multiworld.itempool += pool

    def get_filler_item_name(self) -> str:
        return "Money" if self.random.random() < 0.7 else "Nothing"

    def set_rules(self) -> None:
        self.multiworld.completion_condition[self.player] = lambda state: state.has(TOKEN, self.player, 3)

    def fill_slot_data(self) -> Dict[str, Any]:
        return {"start_scene": TABLE["levels"][self.start_level]["scene"]}
