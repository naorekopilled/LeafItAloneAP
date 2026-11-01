"""Runs N full generations with fill, half default and half random_starting_level. Run from the Archipelago clone."""
import collections
import sys
import traceback
from argparse import Namespace

from BaseClasses import CollectionState, MultiWorld
from Fill import distribute_items_restrictive
from test.general import gen_steps
from worlds.AutoWorld import AutoWorldRegister, call_all

N = int(sys.argv[1])
world = AutoWorldRegister.world_types["Leaf it Alone"]
ok = fail = beatable = unfilled = 0
starts, totals, by_option = collections.Counter(), collections.Counter(), collections.Counter()
for i in range(N):
    random_start = bool(i % 2)
    try:
        mw = MultiWorld(1)
        mw.game = {1: world.game}
        mw.player_name = {1: "Tester"}
        mw.set_seed(10000 + i)
        args = Namespace()
        for name, option in world.options_dataclass.type_hints.items():
            value = random_start if name == "random_starting_level" else option.default
            setattr(args, name, {1: option.from_any(value)})
        mw.set_options(args)
        mw.state = CollectionState(mw)
        for step in gen_steps:
            call_all(mw, step)
        distribute_items_restrictive(mw)
        locations = mw.get_locations(1)
        unfilled += sum(1 for x in locations if x.item is None)
        totals[len(list(locations))] += 1
        starts[mw.worlds[1].start_level] += 1
        by_option[random_start] += 1
        beatable += mw.can_beat_game()
        ok += 1
    except Exception:
        fail += 1
        traceback.print_exc(limit=4)
print(f"generations={N} ok={ok} fail={fail} beatable={beatable} unfilled_slots={unfilled} "
      f"starts={dict(starts)} random_option={dict(by_option)} locations_per_seed={dict(totals)}")
