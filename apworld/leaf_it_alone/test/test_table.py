from unittest import TestCase

from ..table import TABLE


class TestTable(TestCase):
    def test_every_shop_entry_is_affordable_with_all_items(self) -> None:
        for level, row in TABLE["income_summary"].items():
            bundles = row["bundles"] * TABLE["bundle_amount"]
            self.assertGreaterEqual(row["income"] + bundles, row["spend"], level)

    def test_ids_are_unique(self) -> None:
        ids = [i["id"] for i in TABLE["items"]] + [x["id"] for x in TABLE["locations"]]
        self.assertEqual(len(ids), len(set(ids)))

    def test_requirements_name_real_items(self) -> None:
        names = {i["name"] for i in TABLE["items"]}
        for row in TABLE["locations"]:
            for req in row["requires"]:
                if req == "TOOL":
                    continue
                base = req.rpartition("x")[0] if req.rpartition("x")[2].isdigit() else req
                self.assertIn(base, names, row["name"])
