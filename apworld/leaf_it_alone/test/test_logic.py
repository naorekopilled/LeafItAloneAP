from ..table import TABLE
from .bases import LeafItAloneTestBase


class TestDefault(LeafItAloneTestBase):
    def test_starts_at_hometown(self) -> None:
        self.assertEqual(self.world.start_level, "Hometown")
        self.assertTrue(self.multiworld.state.has("Level: Hometown", self.player))

    def test_counts(self) -> None:
        total = len(TABLE["locations"])
        self.assertEqual(total, len(self.multiworld.get_locations(self.player)))
        self.assertEqual(self.multiworld.get_unfilled_locations(self.player).__len__() + 3, total)

    def test_locked_level_blocks_its_checks(self) -> None:
        self.assertFalse(self.can_reach_location("Castle Clear Gate"))
        self.collect_by_name("Level: Castle")
        self.assertTrue(self.can_reach_location("Castle Clear Gate"))

    def test_key_gates_area(self) -> None:
        self.assertFalse(self.can_reach_location("Hometown Clear Shed"))
        self.collect_by_name("Key: Shed")
        self.assertTrue(self.can_reach_location("Hometown Clear Shed"))

    def test_money_gates_expensive_tier(self) -> None:
        name = "Hometown Shop: Airstream Width 5"
        self.assertFalse(self.can_reach_location(name))
        self.collect_all_but(["Nothing"])
        self.assertTrue(self.can_reach_location(name))

    def test_progressive_items_have_one_copy_per_tier(self) -> None:
        for row in TABLE["items"]:
            if row["kind"] == "upgrade":
                self.assertEqual(row["count"], len(row["tiers"]))
                self.assertEqual(len(self.get_items_by_name(row["name"])), row["count"])


class TestRandomStart(LeafItAloneTestBase):
    options = {"random_starting_level": True}

    def test_start_level_unlock_is_precollected_not_in_pool(self) -> None:
        name = f"Level: {self.world.start_level}"
        self.assertTrue(self.multiworld.state.has(name, self.player))
        self.assertEqual(self.get_items_by_name(name), [])


class TestStartPockets(LeafItAloneTestBase):
    """Areas a level's spawn cannot walk to, taken from a collision flood of the scene."""

    def test_sakura_start_is_statues_only(self) -> None:
        self.collect_by_name("Level: Sakura")
        self.assertTrue(self.can_reach_location("Sakura Clear Statues"))
        for area in ("Fountain", "Alley", "Open Air", "Picnic"):
            self.assertFalse(self.can_reach_location(f"Sakura Clear {area}"), area)

    def test_fountain_needs_the_park(self) -> None:
        self.collect_by_name(["Level: Sakura", "Key: Temple", "Key: Shrines", "Key: Lanterns"])
        self.assertFalse(self.can_reach_location("Sakura Clear Fountain"))
        self.collect_by_name("Key: Park")
        self.assertTrue(self.can_reach_location("Sakura Clear Fountain"))

    def test_hometown_shop_is_open_from_the_start(self) -> None:
        self.collect(self.get_items_by_name("Income Boost (Hometown)"))
        self.assertTrue(self.can_reach_location("Hometown Shop: Medium Bag"))

    def test_shed_key_is_only_for_the_roof(self) -> None:
        self.assertTrue(self.can_reach_location("Hometown Clear Basement"))
        self.assertFalse(self.can_reach_location("Hometown Clear Shed"))

    def test_castle_library_waits_for_dinette(self) -> None:
        self.collect_by_name(["Level: Castle", "Key: Courtyard", "Key: Hallway"])
        self.assertFalse(self.can_reach_location("Castle Clear Library"))
        self.collect_by_name("Key: Dinette")
        self.assertTrue(self.can_reach_location("Castle Clear Library"))
