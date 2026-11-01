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
