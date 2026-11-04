# Leaf it Alone inventory

Steam build ID: 26427508. Unity 6000.5.6f1. Status: implemented. `apworld/leaf_it_alone/table.py` is the generated source of truth for items and locations.


## Summary counts

| Group | Count |
|---|---|
| Upgrade tier locations | 106 |
| Shop item locations | 8 |
| Area reward locations | 45 |
| Objective locations | 38 |
| Clean-up locations holding `Level Cleared` | 3 |
| Total locations | 200 |
| Items before filler, with the start level and 3 `Level Cleared` excluded from the pool | 176 |
| Filler slots per seed | 21 |
| Ambiguous objectives left out | 6 |
| Meta objectives, excluded | 7 |

Generation report: 160 of 160 seeds filled and were beatable, 0 unfilled slots, 200 locations in every seed.

The level order comes from `StoryModeSceneProperty`: Hometown, then Castle, then Sakura.

## Terms

- The brief's "level reward" maps to `PlotArea` progress rewards. Each area pays out when cleared (`progressRewards`, `requiredProgress` 1.0).
- The brief's "keys" map to game features. A reward unlocks a feature such as `Dinette`. The player then opens a door, which unlocks `UnlockedDinette`.
- Money values appear to be stored in hundredths: `MoneyManager.CanAfford` multiplies by a constant before comparing with an integer. The constant's value is unverified.
- Hometown's upgrades are all separate from Castle's and Sakura's (`RakeSweepWidth`, `RakeSweepWidth_Castle`).

## Item pool

| Group | Items |
|---|---|
| Progressive upgrades | 27 lines, 106 copies, one line per upgrade per level |
| Tools | Rake, Leaf Blower, Mini Tractor. Bow is not randomized, it sits in the Castle archery range |
| Bags | `Progressive Bag (Hometown)` x2, `Progressive Bag (Castle)` x2. Starting bags stay vanilla |
| Area unlocks | Hometown: Shed, Grill, Greenhouse, Secret Room. Castle: Courtyard, Hallway, Dinette, Love Garden, Throne Room Love Seal, Throne Room Leaf Seal. Sakura: Temple, Shrines, Lanterns, Park, Dog Run, Tree |
| Abilities | Sprint, Crouch, Bag Sealing, Lantern Lighting |
| Activities | Sliding Puzzle, Memory Game, Mosaics, Archery Range, Petal Sort |
| Stat rewards | Income Boost x3 per Hometown and Castle, Move Speed x3, Sprint Speed x2, Upgrade Discount x2, Amplify Upgrades, Amplify Suction Range |
| Level unlocks | Hometown, Castle, Sakura |
| Money Bundle | Castle x10, Sakura x10, 25 money each, needed so every shop entry is affordable |
| Filler | Money (5 in the current level), Nothing |

The Garage is not a key: a free button inside the Shed building opens it, and you need Sprint to reach it in time. `Key: Shed` opens the roof gate and the small area beside the Shed. The Shed's own leaves stay frozen until the Garage is unlocked, so the Shed region sits behind the Garage. `Park` is one item although vanilla accepts either of two features.

Vanilla money rewards (Courtyard 15, Ruins 40, Shrines 40, Grill 15, Garage 7.5) are withheld. Their slots hold normal items.

## Access logic

Start pockets come from a collision flood of each scene from its spawn point, with doors shut. Anything the flood could not place sits later than it may need to, so logic can ask for more than the game does, never less. Castle's outer walls leak in the flood, so Castle leans on vanilla reward and objective order. None of this has been walked in game.

| Level | Region | Areas | Requires |
|---|---|---|---|
| Hometown | Yard | Entrance, Frontyard, Driveway, Porch, Court, shop | Nothing. The Shed building and its Garage button are open |
| Hometown | Garage | Garage, Basement, Swings | `Ability: Sprint`, to reach the door after pressing the button |
| Hometown | Shed | Shed, Shed roof | Garage plus `Key: Shed`. The Garage feature unfreezes the Shed's leaves |
| Hometown | Backyard | Grill, Pool | Shed plus `Key: Grill` |
| Hometown | Greenhouse | Greenhouse, Maze | Backyard plus `Key: Greenhouse` |
| Hometown | Secret Room | Secret Room objective | Greenhouse, Garage and `Key: Secret Room` |
| Castle | Gate | Gate | Nothing |
| Castle | Courtyard | Courtyard, Stairs | `Key: Courtyard` |
| Castle | Hallway | Hallway, Fresco, Range, shop | Courtyard plus `Key: Hallway`. Range also needs `Activity: Archery Range` |
| Castle | Dinette | Dinette, Terrace, Armory, Library | Hallway plus `Key: Dinette` |
| Castle | Love Garden | Love Garden, Ruins, Labyrinth, Leaf Garden | Dinette plus `Key: Love Garden` |
| Castle | Throne Room | Throne Room | Love Garden plus both Throne Room seals |
| Sakura | Statues | Statues | Nothing |
| Sakura | Temple | Temple | `Key: Temple` |
| Sakura | Shrines, Lanterns | Shrines, Lanterns | Temple plus their keys |
| Sakura | Inner Gardens | Garden, Playground | Shrines and Lanterns |
| Sakura | Park | Fountain, Alley, Open Air, Picnic | Inner Gardens plus `Key: Park` |
| Sakura | Dog Run | Dog Run, Recreation, Mini Tractor shop | Park plus `Key: Dog Run` |
| Sakura | Tree | Tree | Dog Run, `Key: Tree` and 500 money |

## Money model

| Level | Income with all boosts | Shop plus Tree door | Money Bundles |
|---|---|---|---|
| Hometown | 716.80 | 544.97 | 0 |
| Castle | 982.02 | 1113.52 | 10 |
| Sakura | 2600.96 | 2574.99 | 10 |

Income counts leaves in areas reachable with held items, times leaf price, times income boosts, plus bundles. Each shop entry needs the running total of all entries at or below its price, so cheap entries unlock first. Bundles cover 110% of spend. Money is stored per level inside one save (`GameState.StatesBySceneId`) and in hundredths (`MoneyDataModel`).

## Hook points

Addresses are in `client/src/gen_rva.h`.

| Purpose | Method | RVA |
|---|---|---|
| Upgrade purchase | `ShopUpgradeEntry.BuyNextUpgrade` | `0x7ed130` |
| Item purchase | `ItemShopView.BuyItem` | `0x804e80` |
| Area reward | `RewardManager.Unlock` | `0x80ac30` |
| Objective | `ObjectiveManager.OnCompleted` | `0x8c6ec0` |
| Effect gate | `ModifierSettings.ClaimModifier`, `ClaimReclaimableModifier` | `0x8dff60` |
| Shop record | `ModifierManager.Unlock` | `0x8df670` |
| Tool lock | `EquipmentBehaviour.EquipItem` | `0x87b280` |
| Start level | `StoryMode.Create` | `0x7cfa00` |
| Saves | `DesktopPersistentDataService.SaveGameSave` | `0x9193d0` |
| Steam blocks | `steam_api64.dll` flat `ISteamUserStats` exports | by name |

## Online features

Leaderboard, stat and achievement calls are blocked at `steam_api64.dll` while the mod is loaded.

## Objective decisions

Counted as locations: `ReassembleMosaic`, `RingBell`, `ScoreExactArcheryPoints`, `SellWorthyBag`, `CollectLeavesAtDrivingSpeed`. `SellWorthyBag` is an excluded location, so only filler lands there, because its bag value threshold was not verified as reachable.

Left out: `BuyAllUpgrades`, `FullyUpgradeTractor`, `AcquireAbilities`, `ObtainAnyItem`, `BuyTractor`, `CompleteStoryMode`. The three `CleanUpAllLeaves_*` objectives are locations with a locked `Level Cleared`.

Also left out: inlet repairs, and the 8 collectables per level as individual locations.

## Goal

Three `Level Cleared` items, one from cleaning each level. No other goal options were built.

## Generated tables

### Hometown

| Upgrade (progressive item) | Tiers | Costs | Key |
|---|---|---|---|
| HandHoldInteraction | 1 | 0.4 | - |
| HandPickupInterval | 5 | 0.25, 0.75, 1.5, 2.75, 4.5 | Upgrades/QUICK_GRAB |
| HandCapacity | 5 | 0.1, 0.5, 1.75, 3.5, 5.75 | Upgrades/GREEDY_GRIP |
| RakeHandleLength | 4 | 3.5, 6.0, 8.5, 11.0 | Upgrades/HANDLE_LENGTH |
| RakeSweepWidth | 5 | 4.0, 6.75, 9.5, 12.25, 14.0 | Upgrades/SWEEP_WIDTH |
| RakeInteractionStrength | 4 | 5.0, 6.5, 8.0, 9.5 | Upgrades/LEAF_STICKINESS |
| LeafBlowerAirstreamWidth | 5 | 28.0, 35.0, 40.0, 46.0, 50.0 | Upgrades/AIRSTREAM_WIDTH |
| LeafBlowerPower | 4 | 15.0, 20.0, 24.0, 27.0 | Upgrades/POWER |
| LeafBlowerSpreadFactor | 4 | 10.0, 16.0, 21.0, 25.0 | Upgrades/AIR_SPREAD |

Upgrade tiers: 37. Total upgrade cost: 483.25. Leaves: 102400. Leaf price: 0.004. Base income from all leaves: 409.60.

| Objective | Type | Class | Value |
|---|---|---|---|
| CompleteAllObjectives | Meta (excluded) | CompleteAllObjectives | - |
| EarnBigMoney | Meta (excluded) | EarnMoney | {'requiredMoney': 700} |
| InletlessRun | Meta (excluded) | CompleteGameWithoutInlets | - |
| Speedrun | Meta (excluded) | CompleteGameWithinTime | {'secondsToComplete': 2400.0} |
| AcquireAbilities | Left out | AcquireAbilities | - |
| BuyAllUpgrades | Left out | BuyAllUpgrades | - |
| CleanUpAllLeaves_Hometown | Check | CleanUpAllLeaves | - |
| FindAllCollectables_Hometown | Check | FindAllCollectables | - |
| JumpIntoLeafPile | Check | JumpIntoLeafPile | {'requiredLeavesCount': 1000} |
| ObtainAnyItem | Left out | BuyAnyItem | - |
| ScoreBasketball | Check | ScoreBasketball | - |
| SealBags | Check | SealLeaves | {'requiredCount': 5000} |
| ThrowLeafBag | Check | ThrowLeafBag | {'distanceToReach': 15} |
| UnlockGarage | Check | UnlockGameFeature | {'gameFeature': 'Garage'} |
| UnlockGreenhouse | Check | UnlockGameFeature | {'gameFeature': 'UnlockedGreenhouseDoors'} |
| UnlockGrill | Check | UnlockGameFeature | {'gameFeature': 'UnlockedGrillDoors'} |
| UnlockSecretRoom | Check | UnlockGameFeature | {'gameFeature': 'UnlockedSecretRoom'} |
| UnlockShed | Check | UnlockGameFeature | {'gameFeature': 'UnlockedShedGate'} |

Objective checks: 11. Left out: 3. Meta excluded: 4.

| Area | Leaves | Reward |
|---|---|---|
| Grill | 10000 | Grill_1: money 15.0 |
| Frontyard | 3000 | Frontyard_1: ModifyActorStatEffect MoveSpeed |
| Court | 3900 | Court_1: remove SprintLocked; feature Sprint |
| Shed | 3500 | Shed_1: ModifyActorStatEffect MoveSpeed / Shed_2: feature Grill |
| Porch | 1000 | Porch_1: remove BagSealingLocked; feature BagSealing |
| Greenhouse | 29000 | Greenhouse_1: feature SecretRoom |
| Maze | 6500 | Maze_1: ModifyActorStatEffect ; ModifyActorStatEffect ; ModifyActorStatEffect  |
| Other | 0 | - |
| Entrance | 4500 | Entrance_1: LeafIncomeMultiplier 25.0 |
| Pool | 18000 | Pool_1: LeafIncomeMultiplier 25.0 / Pool_2: feature Greenhouse |
| Basement | 5000 | Basement_1: LeafIncomeMultiplier 25.0 / Basement_2: feature Shed |
| Swings | 12500 | Swings_1: remove CrouchLocked; feature Crouch |
| Driveway | 3000 | Driveway_1: ModifyActorStatEffect MoveSpeed |
| Garage | 2500 | Garage_1: money 7.5 |

Area reward locations: 16.

### Castle

| Upgrade (progressive item) | Tiers | Costs | Key |
|---|---|---|---|
| BagSealingTIme_Castle | 4 | 1.2, 3.6, 6.0, 8.4 | Upgrades/BAG_MASTER |
| HandPickupInterval_Castle | 4 | 4.0, 5.5, 7.0, 9.0 | Upgrades/QUICK_GRAB |
| BagCapacity_Castle | 5 | 3.0, 3.5, 4.1, 4.75, 5.5 | Upgrades/BAG_CAPACITY |
| RakeHandleLength_Castle | 4 | 8.0, 10.5, 13.0, 16.0 | Upgrades/HANDLE_LENGTH |
| RakeSweepWidth_Castle | 5 | 10.0, 14.0, 18.0, 22.0, 26.0 | Upgrades/SWEEP_WIDTH |
| RakeUsageCooldown_Castle | 4 | 12.0, 17.0, 22.0, 27.0 | Upgrades/USAGE_COOLDOWN |
| LeafBlowerAirstreamWidth_Castle | 5 | 30.0, 47.5, 62.5, 80.0, 90.0 | Upgrades/AIRSTREAM_WIDTH |
| LeafBlowerPower_Castle | 4 | 25.0, 45.0, 65.0, 87.5 | Upgrades/POWER |
| InletSuctionRange_Castle | 4 | 20.0, 37.5, 60.0, 85.0 | Upgrades/INLET_SUCTION_RANGE |

Upgrade tiers: 39. Total upgrade cost: 1016.05. Leaves: 140288. Leaf price: 0.004. Base income from all leaves: 561.15.

| Objective | Type | Class | Value |
|---|---|---|---|
| UnlockCourtyard | Check | UnlockGameFeature | {'gameFeature': 'UnlockedCourtyard'} |
| UnlockDinette | Check | UnlockGameFeature | {'gameFeature': 'UnlockedDinette'} |
| UnlockHallway | Check | UnlockGameFeature | {'gameFeature': 'UnlockedHallway'} |
| UnlockLoveGarden | Check | UnlockGameFeature | {'gameFeature': 'UnlockedLoveGarden'} |
| UnlockThroneRoom | Check | UnlockGameFeature | {'gameFeature': 'UnlockedThroneRoom'} |
| PassArcheryTest | Check | PassArcheryTest | {'requiredScore': 70} |
| SortPetals | Check | SortPetals | - |
| CompleteMemoryGame | Check | CompleteMemoryGame | - |
| AssembleMosaics | Check | AssembleMosaics | - |
| SolveSlidingPuzzle | Check | SolveSlidingPuzzle | - |
| CleanUpAllLeaves_Castle | Check | CleanUpAllLeaves | - |
| FindAllCollectables_Castle | Check | FindAllCollectables | - |
| SellWorthyBag | Left out | SellLeafBag | {'minBagValue': 10.0} |
| CompleteAllObjectives_Castle | Meta (excluded) | CompleteAllObjectives | - |
| ReassembleMosaic | Left out | ReassembleMosaic | - |
| ScoreExactArcheryPoints | Left out | ScoreExactArcheryPoints | {'points': 67} |
| RingBell | Left out | RingBell | - |
| PutBooksOnBookshelf | Check | FillShelfSlots | - |

Objective checks: 13. Left out: 4. Meta excluded: 1.

| Area | Leaves | Reward |
|---|---|---|
| Fresco | 12000 | Fresco_1: LeafIncomeMultiplier 25.0 / Fresco_2: feature Dinette |
| Armory | 3000 | Armory_1: remove ArcheryRangeLocked; start PassArcheryTest |
| Courtyard | 6000 | Courtyard_1: money 15.0 |
| Stairs | 9000 | Stairs_1: feature Hallway |
| Ruins | 15000 | Ruins_1: money 40.0 |
| ThroneRoom | 0 | - |
| LeafGarden | 22144 | LeafGarden_1: feature ThroneRoom_LeafGarden |
| Labyrinth | 12000 | Labyrinth_1: remove SlidingPuzzleLocked; start SolveSlidingPuzzle |
| LoveGarden | 22144 | LoveGarden_1: feature ThroneRoom_LoveGarden |
| Library | 5000 | Library_1: remove MosaicLocked; start AssembleMosaics |
| Range | 4000 | Range_1: LeafIncomeMultiplier 25.0 |
| Hallway | 7000 | Hallway_1: ModifyActorStatEffect SprintSpeedPercentageComponent / Hallway_2: remove PetalSortLocked; start SortPetals |
| Gate | 3000 | Gate_1: feature Courtyard |
| Shop | 0 | - |
| Terrace | 14000 | Terrace_1: LeafIncomeMultiplier 25.0 / Terrace_2: feature LoveGarden |
| Dinette | 6000 | Dinette_1: remove MemoryGameLocked; start CompleteMemoryGame |

Area reward locations: 17.

### Sakura

| Upgrade (progressive item) | Tiers | Costs | Key |
|---|---|---|---|
| BagCapacity_Sakura | 6 | 10.0, 15.0, 20.0, 25.0, 25.0, 25.0 | Upgrades/BAG_CAPACITY |
| LeafBlowerSuctionMode_Sakura | 1 | 60.0 | - |
| LeafBlowerSuctionRate_Sakura | 4 | 26.0, 35.0, 45.0, 56.0 | Upgrades/UNDERPRESSURE |
| TractorInletRange_Sakura | 5 | 59.0, 79.0, 89.0, 95.0, 99.0 | Upgrades/INLET_SUCTION_RANGE |
| TractorStartupTime_Sakura | 3 | 45.0, 59.0, 75.0 | Upgrades/TRACTOR_STARTUP_TIME |
| TractorSwiftMode_Sakura | 1 | 95.0 | - |
| TractorAcceleration_Sakura | 3 | 65.0, 80.0, 95.0 | Upgrades/TRACTOR_ACCELERATION |
| TractorTopSpeed_Sakura | 4 | 65.0, 85.0, 95.0, 99.0 | Upgrades/TRACTOR_TOP_SPEED |
| TractorSteering_Sakura | 3 | 70.0, 85.0, 99.0 | Upgrades/TRACTOR_STEERING |

Upgrade tiers: 30. Total upgrade cost: 1875.00. Leaves: 260096. Leaf price: 0.01. Base income from all leaves: 2600.96.

| Objective | Type | Class | Value |
|---|---|---|---|
| UnlockTemple | Check | UnlockGameFeature | {'gameFeature': 'UnlockedTemple'} |
| UnlockShrines | Check | UnlockGameFeature | {'gameFeature': 'UnlockedShrines'} |
| UnlockLanterns | Check | UnlockGameFeature | {'gameFeature': 'UnlockedLanterns'} |
| UnlockPark | Check | UnlockGameFeature | {'gameFeature': 'UnlockedPark'} |
| UnlockDogRun | Check | UnlockGameFeature | {'gameFeature': 'UnlockedDogRun'} |
| UnlockTree | Check | UnlockGameFeature | {'gameFeature': 'UnlockedTree'} |
| SuckUpLeaves | Check | CollectLeaves | {'leafCollectionType': 1, 'requiredLeavesCount': 10000} |
| FindAllCollectables_Sakura | Check | FindAllCollectables | - |
| CleanUpAllLeaves_Sakura | Check | CleanUpAllLeaves | - |
| AccumulateMoney | Meta (excluded) | AccumulateMoney | {'requiredAccumulatedMoney': 250} |
| CompleteAllObjectives_Sakura | Meta (excluded) | CompleteAllObjectives | - |
| CompleteStoryMode | Left out | UnlockGameFeature | {'gameFeature': 'StoryModeCompletion'} |
| SpinAllPinwheels | Check | SpinAllPinwheels | - |
| BuyTractor | Left out | UnlockGameFeature | {'gameFeature': 'MiniTractor'} |
| FullyUpgradeTractor | Left out | BuyUpgrades | - |
| CollectLeavesAtDrivingSpeed | Left out | CollectLeavesAtDrivingSpeed | {'minSpeed': 6} |
| DumpPresetLeafBags | Check | DumpPresetLeafBags | {'leafBagSettings': 'BigLeafBag'} |
| LightUpLanterns | Check | LightUpLanterns | - |

Objective checks: 12. Left out: 4. Meta excluded: 2.

| Area | Leaves | Reward |
|---|---|---|
| Shrines | 4000 | Shrines_1: money 40.0 |
| DogRun | 29000 | DogRun_1: feature Tree |
| OpenAir | 35000 | OpenAir_1: feature DogRun |
| Temple | 14000 | Temple_1: feature Shrines; feature Lanterns |
| Recreation | 65096 | Recreation_1: UpgradeDiscount 0.10000000149011612 |
| Picnic | 50000 | Picnic_1: UpgradeDiscount 0.10000000149011612 |
| Alley | 20000 | Alley_1: ModifyActorStatEffect LeafInletSuctionRange |
| Fountain | 9800 | Fountain_1: ModifyActorStatEffect SprintSpeedPercentageComponent |
| Playground | 8000 | Playground_1: feature Park_Playground |
| Statues | 13200 | Statues_1: feature Temple |
| Garden | 8000 | Garden_1: feature Park_Garden |
| Lanterns | 4000 | Lanterns_1: feature LanternLighting |

Area reward locations: 12.

### Items sold

| Item | Prices by level |
|---|---|
| Tractor | {'Sakura': 199.99} |
| MediumLeafBag | {'Hometown': 0.75} |
| LeafBlower | {'Hometown': 49.99, 'Castle': 74.99} |
| Bow | not sold |
| BigLeafBag | {'Hometown': 3.49, 'Castle': 7.49} |
| Hand | not sold |
| SmallLeafBag | {'Hometown': 0.75} |
| Rake | {'Hometown': 7.49} |
| GiantLeafBag | {'Castle': 14.99} |

### Counts

| Level | Upgrade tiers | Objective checks | Left out | Meta excluded | Area rewards |
|---|---|---|---|---|---|
| Hometown | 37 | 11 | 3 | 4 | 16 |
| Castle | 39 | 13 | 4 | 1 | 17 |
| Sakura | 30 | 12 | 4 | 2 | 12 |
