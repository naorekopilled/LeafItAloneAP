# Leaf it Alone AP

An [Archipelago](https://archipelago.gg) randomizer for [Leaf it Alone](https://store.steampowered.com/app/3981100/Leaf_it_Alone/) on Steam.

Upgrades, tools, area keys, abilities and levels are shuffled into the multiworld. Buying from a shop, clearing an area and finishing an objective send checks.

Singleplayer. Tested on Linux through Proton. Windows has not been tested.

## What you need

- Leaf it Alone on Steam, **Steam build 26384472** (the game shows `v1.1.4` in the corner of the main menu)
- Archipelago 0.6.8 or newer
- `version.dll` and `leaf_it_alone.apworld` from the [releases page](../../releases)

The mod only works on that exact game build, because it needs the addresses of game functions and they change with every update. On any other build the panel shows "Unsupported game build: update the mod" and the game runs as normal.

## Install

1. Right-click Leaf it Alone in Steam, then Manage, then Browse local files. Put `version.dll` in that folder, next to `Leaf it Alone.exe`.
2. On Linux, open the game's Properties and set Launch Options to:

   ```
   WINEDLLOVERRIDES="version=n,b" %command%
   ```

3. Install `leaf_it_alone.apworld` like any other apworld.

To remove the mod, delete `version.dll`, `lia_ap.log` and `lia_ap.cfg` from the game folder and clear the launch option. No game file is changed.

## Options

```yaml
name: YourName
game: Leaf it Alone
Leaf it Alone:
  random_starting_level: false
```

| Option | Default | What it does |
| --- | --- | --- |
| `random_starting_level` | off | The seed picks one of the three levels as your start. Off starts at Hometown. |

A seed has 200 locations: 106 upgrade tiers, 8 shop items, 45 area rewards, 38 objectives and 3 clean-ups.

## Play

1. Start the game. The panel at the top right is the client, and F8 hides it.
2. At the main menu, fill in host, port, slot and password, then press Connect. For an archipelago.gg room, the host is `archipelago.gg` and the port is the room's number.
3. Once connected, the menu only shows this seed's saves. Press New Game, pick Story and press Play. Do not use Custom. The seed decides your starting level, whichever card is shown.
4. To change level, pause, press Save & Exit, open Load Game, select your slot, pick the map with the arrows and press Load.

Always connect before you press Continue, New Game or Load Game. If you are not connected, the game uses your normal saves.

Saves go to `AppData/LocalLow/Eternity/Leaf it Alone/AP/<seed>_<slot>/`, so your normal saves are not touched. `lia_ap.cfg` in the game folder keeps your connection details, including the password in plain text.

### How the shuffle works in game

- Buying an upgrade tier or a shop item costs money and sends its check, but gives you nothing. The next tier opens because you bought the last one, so every tier stays reachable.
- Shop entries show which item they hold and who it is for.
- Upgrades are progressive, with one item line per level. A Hometown upgrade does nothing in Castle or Sakura.
- Clearing an area and finishing an objective send checks. Their normal rewards are withheld and come from the multiworld instead.
- Tools you have not received cannot be equipped, including the Rake lying in Castle and the tools you start Sakura with.
- Money is per level. The Money filler goes to the level you are in, and a Money Bundle waits for its own level.
- Doors need their key item. The Garage has no key: the button in the Shed building opens it, but you need Sprint to reach it in time. `Key: Shed` opens the shed roof and the small area beside the Shed. The inside of the Shed, where the shop is, is open from the start.

The goal is to clean up all leaves in all three levels. Each level gives one `Level Cleared`, and three of them finish your seed.

Leaderboard uploads, Steam stats and achievements are blocked for as long as `version.dll` is loaded, connected or not.

## Known limits

- No seed has been played from start to goal yet. Purchases, level changes, locks, reconnects and the goal report were tested in the real game, some of them with scripted triggers. Which areas are open comes from a collision check of each level's scene, and how much money you can really earn comes from the game data. Neither was walked through by hand. If you get stuck, please open an issue with your `lia_ap.log` from the game folder.
- `Castle Objective: Sell Worthy Bag` only holds filler, because I could not confirm it can be reached.
- A game update breaks the mod until a new release is out. Steam can hold updates back if you set the game to update only when you launch it, but it will still update then.
- The server warns that the client does not support compressed connections. It works, the warning is only that.

## Building from source

The client is C++ and cross-compiles to Windows from Linux:

```bash
scripts/setup_deps.sh
scripts/package.sh
```

This writes `dist/version.dll` and `dist/leaf_it_alone.apworld`.

For the tests, copy `apworld/leaf_it_alone` into `worlds/` of an Archipelago checkout, then:

```bash
AP_TEST_WORLDS=leaf_it_alone python -m pytest worlds/leaf_it_alone
```

Run from that checkout, `scripts/gen_many.py 160` fills 160 seeds and reports the counts.

All items and locations live in one table, `apworld/leaf_it_alone/table.py`, and the client uses a copy in `client/src/gen_table.h`. Both come from `scripts/gen_table.py`, which holds the logic and reads data taken from the game's asset bundles with `scripts/assets_dump.py`. That game data is not in this repository. `docs/inventory.md` lists every location and item.

## Credits

`version.dll` includes [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause), [Dear ImGui](https://github.com/ocornut/imgui) (MIT) and [nlohmann/json](https://github.com/nlohmann/json) (MIT). It is built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw). See [THIRD_PARTY_LICENSES.txt](THIRD_PARTY_LICENSES.txt).

## License

MIT, see [LICENSE](LICENSE). Leaf it Alone belongs to its developers. This project is not affiliated with them.
