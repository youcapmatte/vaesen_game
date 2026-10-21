# Vaesen Combat Test Plan

Test checklist for the weapons framework and combat (branch `feature/combat-weapons`), built from *Vaesen Combat Spec — Weapons Framework and Ranged Combat*. It covers every feature, property and dropdown value in the spec, edge cases, multiplayer and empty asset slots.

Each test lists its **setup**, **steps**, the **expected** result, and whether it **runs now** (with the grey-box assets) or **needs assets** (animations, sounds, effects, final meshes). Unless a test says otherwise, run it in the editor with **2 players and a dedicated server** (Play > Net Mode: *Play As Client*, Number of Players: 2) on **`L_CombatTest`**, and check that **both clients** see the same thing.

---

## 0. Before testing

### 0.1 Build the assets (once, or after pulling)

Close the editor, then run the commandlet. It only creates what's missing, so running it again is safe.

```
UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenCreateWidgets
```

It creates:

| What | Where |
| --- | --- |
| Grey-box weapon meshes with sockets (`Grip`, `LeftHandIK`, `Muzzle`, `Eject`, `TraceStart`, `TraceEnd`) | `/Game/Vaesen/Combat/GreyBox/SM_GB_*` |
| Mannequin sockets `hand_r_weapon`, `hand_l_weapon`, `Sheath_BackUpper`, `Sheath_BackLower`, `Sheath_BackSling`, `Sheath_HipLeft`, `Sheath_HipRight`, `Sheath_Belt` | `SK_Mannequin` (adjust them in the Skeleton editor) |
| Physical materials Flesh, Wood, Stone, Metal, Earth, Water, Cloth | `/Game/Vaesen/Combat/PhysicalMaterials/PM_*` |
| `DA_WeaponCategoryDefaults`, `DA_ImpactEffects`, `DA_WeaponSounds_Default` | `/Game/Vaesen/Combat` |
| `GE_Damage`, `GE_StaminaCost`, `GE_Stagger`, `GE_StaminaRegen` | `/Game/Vaesen/Combat/Effects` |
| `GA_FireWeapon`, `GA_Reload`, `GA_CycleAction`, `GA_Aim`, `GA_DrawBow`, `GA_MeleeLight`, `GA_MeleeHeavy`, `GA_Block`, `GA_Throw`, `GA_Holster`, `GA_SwapWeapon` (granted by `BP_VaesenPlayerCharacter`) | `/Game/Vaesen/Combat/Abilities` |
| Input actions `IA_Attack`, `IA_HeavyAttack` (+ `IA_HeavyModifier`), `IA_TriggerAttack`, `IA_AimBlock`, `IA_Reload`, `IA_Throw`, `IA_Holster`, `IA_SwapWeapon`, mapped in `IMC_Default` | `/Game/Vaesen/Input` |
| Test weapons | `/Game/Vaesen/Blueprints/Items/Weapons` |
| Ammo and the cartridge box | `/Game/Vaesen/Blueprints/Items/Ammo` |
| Stone and bottle | `/Game/Vaesen/Blueprints/Items/Throwables` |
| `BP_TrainingDummy`, `BP_TrainingDummy_Armored`, `BP_TrainingDummy_Attacker` | `/Game/Vaesen/Blueprints/Combat` |
| `L_CombatTest` | `/Game/Vaesen/Maps` |

### 0.2 The test map

`L_CombatTest`:

- Two player starts near the origin.
- A row of pickups to the left of the start: every test weapon, every ammo, the cartridge box, 5 stones and 3 bottles.
- Two plain dummies, an armored dummy and the attacker dummy in front.
- A stone wall (left) and a wooden wall (right) at about 9 m.
- A plain dummy and a wooden wall at about 50 m, for long shots.

### 0.3 Test items

| Item | Category | Settings |
| --- | --- | --- |
| `BP_Weapon_Revolver` | Revolver (Sidearm, hip right) | InstantHit, action Revolver, Cylinder reload 0.6 s/round, 6 rounds, casings **OnReload** |
| `BP_Weapon_KragRifle` | Rifle (two-handed, back sling) | **Projectile** 300 m/s, **BoltAction**, Magazine 5 (2.4 s), casings **OnCycle** |
| `BP_Weapon_MuzzleLoader` | Rifle (two-handed, back sling) | InstantHit, SingleShot, **MuzzleLoading** 7 s, 1 round, casings **None**, smoke ×3 |
| `BP_Weapon_RollingBlockPistol` | Pistol (Sidearm) | InstantHit, SingleShot, **SingleRound** 1.4 s, 1 round, casings **OnReload** |
| `BP_Weapon_HuntingBow` | Bow (two-handed, back upper) | Projectile 60 m/s at full draw, action **Draw**, arrows from the inventory |
| `BP_Weapon_Crossbow` | Crossbow (two-handed) | Projectile 70 m/s, SingleShot, SingleRound 3.5 s |
| `BP_Weapon_OldSword` | OneHandedSword (hip left) | Durability 60; throwable Overhand, Slashing 10 |
| `BP_Weapon_Dagger` | Dagger (Sidearm, belt) | Throwable PointFirst, Piercing 12 |
| `BP_Weapon_HandAxe` | OneHandedAxe (back lower) | Throwable Spinning, Slashing 14 |
| `BP_Weapon_Club` | Club (back lower) | Throwable Overhand, Blunt 8 |
| `BP_Weapon_Warhammer` | Warhammer (back upper) | Durability 120 |
| `BP_Weapon_ChairLeg` | Improvised (belt) | Durability 25 |
| `BP_Weapon_Buckler` | **Shield** (off hand, back upper) | Durability 200; blocks projectiles |
| `BP_Ammo_RevolverCartridge` | Ammo | Piercing 18 |
| `BP_Ammo_KragCartridge` | Ammo | Piercing 30 |
| `BP_Ammo_PowderAndBall` | Ammo | Piercing 34 + Blunt 6 |
| `BP_Ammo_Arrow`, `BP_Ammo_CrossbowBolt` | Ammo | Piercing 22 / 28; stick and can be recovered |
| `BP_Item_CartridgeBox` | Pouch | 4 slots; accepts `Item.Ammo` |
| `BP_Item_Stone` | Throwable (stacks to 10) | Lob, Blunt 6 |
| `BP_Item_Bottle` | Throwable | Lob, Blunt 3, breaks on impact |

Built-in attack numbers (from `DA_WeaponCategoryDefaults`):

| Category | Light | Heavy | Block (Slashing / Piercing / Blunt) | Parry window |
| --- | --- | --- | --- | --- |
| Unarmed | Blunt 4, 5 stamina | Blunt 7, 12 stamina | 15% / 10% / 25% | 0.15 s |
| Dagger | Slashing 5, 6 st | Piercing 11, 12 st | 40 / 30 / 10% | 0.30 s |
| OneHandedSword | Slashing 12, 10 st | Piercing 16, 18 st | 70 / 50 / 25% | 0.25 s |
| OneHandedAxe | Slashing 15, 14 st | Slashing 22, 22 st | 40 / 30 / 30% | 0.15 s |
| Club | Blunt 10, 10 st | Blunt 16, 18 st | 30 / 20 / 40% | 0.10 s |
| Warhammer | Blunt 14, 14 st | Piercing 18, 22 st | 30 / 30 / 40% | 0.12 s |
| Improvised | Blunt 7, 8 st | Blunt 11, 14 st | 25 / 25 / 25% | 0.10 s |
| Shield | Blunt 4 (bash), 8 st | — | 80 / 80 / 80%, projectiles | 0.20 s |
| Pistol / Revolver | Blunt 5 (pistol-whip), heavy input | — | none | — |
| Rifle | Blunt 8 (butt), heavy input | — | 30 / 20 / 30% | 0.10 s |
| Bow / Crossbow | Blunt 3 / 6, heavy input | — | 10% / 20% | 0 / 0.05 s |

Dummy protections: `BP_TrainingDummy` has none. `BP_TrainingDummy_Armored` has Slashing 8, Piercing 4 and Blunt **−2** (a weakness).

### 0.4 Controls

| Action | PC | Gamepad |
| --- | --- | --- |
| Light attack / fire | Left mouse | Right bumper; right trigger fires with a gun drawn |
| Heavy attack (gun: butt stroke) | Shift + left mouse | Right trigger with a melee weapon |
| Aim (ranged) / block and parry (melee, bare hands) | Right mouse (hold) | Left trigger (hold) |
| Reload / work the bolt | R | X / Square |
| Throw (hold to aim, release) | G | Left bumper |
| Draw / sheathe | H | Hold Y / Triangle |
| Draw the other carried weapon (sidearm ↔ main hand) | Q | — |
| Crouch (toggle) | C | B / Circle |

### 0.5 Debug tools

- **`vaesen.combat.debug 1`** (console) draws:
  - melee sweeps (green, red when they hit);
  - unarmed punches;
  - shot lines (orange);
  - projectile and thrown paths (yellow, cyan);
  - hit points (red spheres).
- **Dummies** show floating numbers on every machine: per damage type, the amount dealt and what got through (`Slashing 12 > 4`), with BLOCKED or PARRIED on top. Their health shows above them.
- **Log**: `LogVaesenCombat` has every resolved hit, with packet, protection, block and the final number.
- **Network emulation**: `net pktlag=150`, `net pktloss=5` in the client console. Or Editor Preferences > Level Editor > Play > Multiplayer Options > Enable Network Emulation.

### 0.6 Automated tests

Session Frontend > Automation, filter **`Vaesen.Combat`** (24 tests), or headless:

```
UnrealEditor-Cmd.exe VaesenGame.uproject -ExecCmds="Automation RunTests Vaesen.Combat; Quit" -unattended -nullrhi
```

All 24 passed when this plan was written.

| Test | Covers |
| --- | --- |
| `Damage.Protection` | Each entry minus its protection, minimum zero; a weakness adds |
| `Damage.TypeWithoutProtection` | Damage.Iron isn't reduced by armor |
| `Damage.Block` | Block share per type; types missing from the profile aren't reduced |
| `Damage.ProtectionThenBlock` | Order: protection first, then block |
| `Damage.Parry` | Negates everything, no wear |
| `Damage.BlockProfileLookup` | Most specific tag wins, a parent tag covers children, clamped to 1 |
| `Damage.BlockFacing` | Front, front-left, side, behind; height ignored |
| `Damage.Packet` | Same-type entries add up, scaling, main type |
| `Damage.ExecutionOnDummy` | GE_Damage → execution → IncomingDamage → Health; death; reset |
| `Durability` | Wear never below zero; a fresh item is whole |
| `CategoryDefaults` | Every category has an attack with a sane hit window; every sheath location exists; shields block projectiles, swords don't |
| `ImpactTable` | Row priority: type + surface > surface > type > any |
| `Ammo.Accepts` | Accepting by class and by tag; cycle step; interruptible reload |
| `Ammo.CountingThroughPouches` | A cartridge box takes the ammo; counting, taking and finding ammo through it |
| `Flow.EquipDrawLoadFire` | Equip spawns the actor, sheathed. Draw by timer fallback. Load from the inventory, fire hits the dummy for 20, fire interval enforced, implausible aim refused. Sheathe. Unequip keeps the loaded rounds and drops the actor |
| `Flow.BoltActionNeedsCycle` | Fire → needs cycle → blocked → cycle → fire |
| `Flow.BlockParryDurability` | Unblocked, blocked (70%, wear 7), from behind, unblockable, projectile vs sword, parry (no damage, no wear, attacker staggered, stagger wears off), block until broken (can't block, still throwable) |
| `Flow.StaminaCostAndRegen` | Abilities granted. Regen up to max, paused while blocking. A punch costs its stamina; no stamina, no punch |
| `Abilities.ReloadFireInterrupt` | Through the real abilities and inputs (AI-controlled character): H draws, dry fire, R without ammo, cylinder reload round by round, fire hits, fire interval, firing interrupts the reload, right mouse aims (not blocks), sheathing cancels a reload |
| `Abilities.AutoCycle` | Magazine reload, fire, bolt worked by itself, no firing while cycling |
| `Abilities.MeleeLightHeavyPunch` | Punch, sword light and heavy (damage, stamina, wear), no stamina or broken: no swing |
| `Abilities.BlockAndParryInput` | Right mouse blocks; parry window then plain block; slower while blocking; stamina drains; release; attacking drops the block |
| `Abilities.Throw` | Nothing to throw; stone aimed then thrown off the stack, hits, lands as a pickup; a drawn sword is unequipped and thrown |
| `Abilities.BowDraw` | No arrows; early release lets down; full draw hits, uses an arrow, costs stamina, sticks and becomes a pickup |
| `WeaponFitRig` | The rig holds the Krag shouldered; the left palm sits on LeftGrip (place and turn), aimed and from the hip; moving the weapon while aimed sets Sight (it stays where put); sliding it from the hip moves Grip; moving and turning the LeftGrip handle moves the hand with it |

### 0.7 In-map self-test

In a running game (PIE with a dedicated server is fine), open the console and type **`vaesen.combat.selftest`**. On the server it spawns:

- an AI-controlled `BP_VaesenPlayerCharacter`, which uses the same `GA_` Blueprints and input as a player;
- three `BP_TrainingDummy`s, 15 m to the side of the first player start.

It first checks that the interaction trace finds every pickup in the map and picks up a rifle through its interaction. It then runs the test Blueprints: revolver (draw, reload, fire, armored target), Krag (left hand on LeftGrip from the hip and aimed, shouldered, bolt cycles by itself), punch, sword (stamina, block, parry, wear), stone throw, and bow.

Every check prints PASS or FAIL on screen and in the log, and everything it spawned is removed at the end. In `L_CombatTest` it reported **43 passed, 0 failed**.

It checks what the C++ tests can't: the generated assets, the Blueprints and the map's collision together. Both clients should see the fighter do everything. The sword swing is reported as INFO until an attack montage exists, because without one the blade stays where the hand hangs.

This replaces functional-test actors. The engine's FunctionalTesting module is developer-only and would break shipping builds.

### 0.8 How weapons are held (sockets), and the Weapon Fit rig

Every weapon is held by sockets on its mesh. The rule is the same for every weapon: **X toward the muzzle, Z up**.

| Socket | Where | What follows it |
| --- | --- | --- |
| `Grip` | Where the right palm closes (pistol grip, wrist of the stock) | The weapon sits in the right hand here (skeleton socket `hand_r_weapon`) |
| `LeftGrip` | Where the left palm holds (fore-end), turned as the palm should be | The left hand: IK moves it there, and it is turned to match (skeleton socket `hand_l_weapon`) |
| `Sight` | On the line of the sights; its X is that line | Aiming: that line lies along the aim and passes through the eye (skeleton socket `Eye_R` on `head`) |
| `Stock` | The butt plate (long guns) | Aiming: goes in the shoulder pocket (skeleton socket `ShoulderPocket` on `clavicle_r`, in front of the right shoulder joint) |

Aiming, the **Vaesen Shoulder Aim** anim node holds it like a shooter: the butt in the shoulder pocket and the eye down the sights. With a straight stock both can only hold if the body meets the rifle, so the right shoulder lifts and the head bows and leans onto the stock. **Shoulder Share** (Weapon > Hold) splits that between them: 0.4 means the shoulder takes 40%. The node also solves both arms onto the rifle.

IK solves the arms and elbows, and the fingers close by the category's **Grip Pose** (DA_WeaponCategoryDefaults; rifles use the pack's aimed rifle grip). A weapon with these four sockets right needs nothing else. The grey-box long guns already have them: `hand_l_weapon` and `LeftGrip` are worked out from how the rifle pack holds its rifle.

**Checking and setting the sockets: the Weapon Fit rig.** Open **`L_WeaponFit`** (Content/Vaesen/Maps). You don't need to press Play. To use it in another level, place *Vaesen Weapon Fit Rig* from Place Actors.

1. Select the rig. Under **Weapon Fit** in Details, pick the **Weapon** (an item Blueprint) and the **Pose**: Hip, Aim, Crouch or CrouchAim.
   - **Aim Pitch** looks up or down.
   - **Speed** and **Direction** walk or run on the spot.
   - **Preview Animation** shows any animation on the upper body (reload, fire, draw), held at **Preview Time**. **Play Preview** plays it instead.
2. The small shapes on the weapon are its sockets: **cube** `Grip`, **sphere** `LeftGrip`, **cone** `Sight`, **cylinder** `Stock`. Click one (click again if the whole rig is selected) and move or turn it with the gizmo (W / E). The hands follow at once:
   - Move or turn `LeftGrip` and the left hand moves and turns with it.
   - Move `Grip` and the weapon slides in the right hand.
3. Or move the **weapon** itself:
   - In Hip or Crouch, it slides or turns in the hand, and `Grip` is set to match.
   - In Aim, you are setting how it lies against the shoulder: the butt stays in the shoulder pocket, so `Stock` (and the line of `Sight`) are set to match.
4. Untick **Show Sockets** to see the hands without the shapes. **Save Weapon** saves the weapon's mesh (the sockets) and its Blueprint.

Sockets belong to the mesh, so every weapon using that mesh gets them.

Put `LeftGrip` under the fore-end, within reach. The left arm doesn't stretch, so a grip much more than about 30 cm in front of `Grip` leaves the hand a few cm short. The in-map self-test measures it (the left hand holds LeftGrip ... cm off).

Extra adjustments for one weapon only (normally zero) are in the weapon Blueprint under **Weapon > Hold**: In Hand, Left Hand Offset, Shoulder Offset, Aim Sight Offset (weapons without a stock), Aim Rotation, Recoil.

`-run=VaesenAnimSetup` only adds sockets that are missing. Add `-resetsockets` to work them out again from the pack, and `-rebuild` to rebuild the anim Blueprints.

The interaction trace (the debug capsule) is hidden while a weapon is drawn, so it doesn't cover the crosshair.

---

## 1. Equipment, holding and sheathing

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 1.1 | Revolver in inventory | Right-click it | Goes to the **Sidearm** slot; appears in `Sheath_HipRight`, sheathed; the other client sees it there | Now |
| 1.2 | 1.1 | Press **H** | At the grab moment (≈0.2 s, timer fallback) it moves to the right hand; crosshair appears; other client sees it drawn | Now |
| 1.3 | 1.2 | Press **H** again | Back to the hip socket; crosshair gone | Now |
| 1.4 | Krag and revolver in inventory | Equip both | Krag on `Sheath_BackSling` (diagonal), revolver on the hip, both visible to both clients | Now |
| 1.5 | 1.4 | H, then **Q**, then Q | H draws the rifle (main-hand set). Q sheathes it and draws the revolver. Q again goes back to the rifle | Now |
| 1.6 | Weapon drawn | Press **I** | Weapon sheathes automatically; the inventory opens | Now |
| 1.7 | Weapon drawn, facing a pickup | Press **E** | Weapon sheathes; the pickup is taken | Now |
| 1.8 | Sword + buckler equipped | H | Sword to the right hand, buckler to the left (`hand_l_weapon`) | Now |
| 1.9 | Only the buckler equipped | H | Buckler drawn to the left hand (counts as the drawn weapon) | Now |
| 1.10 | Krag (two-handed) equipped, buckler in inventory | Equip the buckler | The two-handed rifle is pushed back to the inventory (existing rule) | Now |
| 1.11 | Every weapon | Equip each; look at its sheathed position | Each sits in its category's socket (sword hip left, dagger belt, axe and club back lower, warhammer and buckler back upper, rifles and crossbow back sling, bow back upper, pistol and revolver hip right) | Now (positions need tuning in the skeleton) |
| 1.12 | `BP_Weapon_OldSword` open in the editor | Change Sheath Socket (dropdown lists the six sockets), Sheath Offset | Dropdown shows the sockets from `DA_WeaponCategoryDefaults`; the offset moves it in the socket | Now |
| 1.13 | Weapon Blueprint | Change Category | Slot and sheath socket follow (Pistol/Revolver → Sidearm hip right, Dagger → Sidearm belt, Shield → OffHand, Rifle/Crossbow → two-handed back sling, Bow → back upper) | Now |
| 1.14 | A weapon whose Sheath Offset puts Grip > 25 cm from its location's grab point, no own draw montage | Right-click > Asset Actions > Validate | Warning "…the arm will stretch…" (spec test 6) | Now (set the grab points once draw montages exist) |
| 1.15 | Mesh without a `Grip` socket on a weapon | Validate | Warning "no Grip socket; it's held by its origin" | Now |
| 1.16 | Weapon drawn | Drop it (drag to the Drop box) | Leaves the hand; falls as a pickup; the drawn state clears on both clients | Now |
| 1.17 | Weapon drawn | Unequip it into the inventory (right-click its slot) | Actor removed; nothing drawn; item back in the inventory with its durability and loaded rounds | Now |
| 1.18 | Sword drawn | Equip a club from the inventory (same slot) | The club replaces the sword in the hand, no flicker; sword back to the inventory | Now |
| 1.19 | Inventory full | Unequip a drawn weapon | Refused (rejected sound); weapon stays | Now |
| 1.20 | H pressed twice quickly | — | Ends in the last requested state; no weapon stuck between sockets | Now |
| 1.21 | Draw montages set in `DA_WeaponCategoryDefaults` > Sheath Locations, with `AN_WeaponGrab` | H | Montage plays on all machines; the weapon moves exactly at the notify frame | Needs assets |
| 1.22 | 1.21, montage without the notify | H | Moves at the location's Grab Time | Needs assets |
| 1.23 | 1.21, montage with a `HandIK_Sheath` curve, ABP reparented to `VaesenAnimInstance` and using `SheathHandIKTransform` / `SheathHandIKAlpha` | Draw a sword (BackUpper) and a club (BackLower) with the same montage | The hand lands on each weapon's real handle | Needs assets |
| 1.24 | Anim layer set for a category | Draw / sheathe | Layer linked on draw, unarmed layer back on sheathe | Needs assets |
| 1.25 | Two-handed rifle drawn, ABP uses `LeftHandIKTransform` / `LeftHandIKAlpha` | Look at the left hand | On the fore-grip (`LeftHandIK` socket); off while reloading | Needs assets |
| 1.26 | Equip sound set on an item / defaults in Project Settings | Equip | Heard by that player only | Now with placeholder sounds |

## 2. Ranged

### 2.1 Firing and validation

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 2.1.1 | Revolver drawn, 12 cartridges | R, then fire at a plain dummy | Hit marker (white); dummy shows `Piercing 18 > 18`; HUD shows `5 / 6` | Now |
| 2.1.2 | 2.1.1 | Fire at the armored dummy | `Piercing 18 > 14` (protection 4) | Now |
| 2.1.3 | Revolver empty | Fire | Dry-fire click (if set); no shot; no round used | Now (silent until sound set) |
| 2.1.4 | Revolver loaded | Click as fast as possible | At most one shot per 0.4 s, on the server and the HUD count | Now |
| 2.1.5 | Client with `net pktlag=150` | Fire | Flash/sound/montage at once on the shooter; damage number and hit marker ~150 ms later | Now (cosmetics need assets) |
| 2.1.6 | 2.1.5, `net pktloss=10` | Fire 20 times | Every confirmed shot used a round; no shots that weren't fired by the server dealt damage | Now |
| 2.1.7 | Third person, a wall between camera and character | Fire | Nothing behind the character is hit (aim starts level with the character) | Now |
| 2.1.8 | Muzzle-loader | Fire at the plain dummy | `Piercing 34 > 34`, `Blunt 6 > 6`; smoke effect (when set) heavy and lingering | Now (smoke needs effect) |
| 2.1.9 | Krag, dummy at 50 m | Fire at it | Projectile visibly travels and drops (aim slightly high to hit) — spec test 13 | Now (`vaesen.combat.debug 1` shows the path) |
| 2.1.10 | Shooting the stone wall with the Krag | — | Projectile disappears; impact for Piercing/Stone (with table entries) | Now |
| 2.1.11 | Shoot the far dummy with the revolver | Compare with close range | Damage falls toward half past Effective Range (30 m) | Now |
| 2.1.12 | Revolver | Hip fire standing, moving, jumping; aim (RMB) standing | Crosshair gap grows with movement/jumping, tightest aimed; shots spread accordingly | Now |
| 2.1.13 | RMB with a gun | Hold | Camera zooms (weapon's Aim FOV), moves over the right shoulder, body strafes (faces the aim), walk slower | Now |
| 2.1.14 | First person, RMB | Hold | FOV zooms; `bAiming` true in the anim instance (weapon raise is animation) | Now (raise needs animation) |
| 2.1.15 | Gun sheathed | LMB | No shot (a punch instead) — drawing is required | Now |
| 2.1.16 | Weapon with Misfire Chance 1 (set on a copy) | Fire | Round spent, no shot, misfire sound | Now |
| 2.1.17 | Weapon with Wear Per Shot 10 | Fire 10 times | Durability drops by 10 each; at 0 it's broken and won't fire | Now |
| 2.1.18 | Gun drawn | Shift + LMB near a dummy | Butt stroke: Blunt 5 (pistol) / 8 (rifle) | Now |
| 2.1.19 | Gamepad, gun drawn | Right trigger | Fires; with a sword drawn, it's a heavy attack | Now |

### 2.2 Reload styles

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 2.2.1 | Revolver (Cylinder) at 2/6, 12 cartridges | R | Spent casings out first (4, cosmetic), then a round each 0.6 s until 6/6; HUD counts up | Now |
| 2.2.2 | 2.2.1 mid-reload | Fire | The reload stops between rounds; a loaded round fires | Now |
| 2.2.3 | Pistol (SingleRound) empty | R | Casing out (OnReload), one round after 1.4 s | Now |
| 2.2.4 | Krag (Magazine) at 2/5 | R | All 3 go in at once after 2.4 s | Now |
| 2.2.5 | Muzzle-loader (MuzzleLoading) | R, then fire, H, RMB during the 7 s | Nothing interrupts it; loaded after 7 s | Now |
| 2.2.6 | Cartridges only in the **cartridge box** | R | Taken from the box; box count and HUD update (spec test 9) | Now |
| 2.2.7 | No ammo at all | R | Nothing happens, no error (spec test 10) | Now |
| 2.2.8 | Full magazine | R | Nothing happens | Now |
| 2.2.9 | Wrong ammo only (Krag cartridges, revolver drawn) | R | Nothing (not accepted) | Now |
| 2.2.10 | Reload in progress | Sheathe (H) | Reload cancelled; rounds already in stay | Now (muzzle-loader: can't be cancelled) |
| 2.2.11 | Reload montage set | R | Montage plays, sped/slowed to fit Reload Time | Needs assets |
| 2.2.12 | Reload | Watch the other client | Remote sees the montage and the casings (OnReload weapons) | Now (casings), needs assets (montage) |

### 2.3 Actions

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 2.3.1 | Krag (BoltAction), loaded | Fire | Bolt worked automatically (0.8 s); can't fire until done; casing ejects on **cycle**, not on fire (spec test 11) | Now |
| 2.3.2 | Krag, last round fired | — | No automatic cycle; R reloads | Now |
| 2.3.3 | Krag, spent case in the chamber, rounds left (cycle interrupted by sheathing) | Draw, press R | R works the bolt (not a reload) | Now |
| 2.3.4 | A rifle set to LeverAction | Fire | Same as bolt action (cycle step) | Now |
| 2.3.5 | Revolver (Revolver action) | Fire repeatedly | No cycle step; fires every 0.4 s | Now |
| 2.3.6 | Muzzle-loader / pistol (SingleShot) | Fire | No cycle; reload needed | Now |
| 2.3.7 | Bow (Draw) | See 9 | — | — |
| 2.3.8 | Cycle with `net pktlag=150` | Fire repeatedly | Shooter can't double-fire before the cycle; cycle runs on the shooter's machine | Now |

### 2.4 Casings

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 2.4.1 | Casing Ejection = None (muzzle-loader) | Fire, reload | No casings | Now |
| 2.4.2 | = OnFire (set on a test copy of the revolver) | Fire | One casing per shot | Now |
| 2.4.3 | = OnCycle (Krag) | Fire | Casing when the bolt is worked | Now |
| 2.4.4 | = OnReload (revolver, pistol) | Fire 6, reload | 6 casings out at reload start | Now |
| 2.4.5 | Many shots on stone and on grass | — | Casings bounce (one bounce sound each), settle, gone after 20 s; never more than 40 (oldest removed) (spec test 17) | Now (bounce sound needs asset) |
| 2.4.6 | Second client watching | — | Casings appear there too but aren't replicated actors (each machine spawns its own) (spec test 18) | Now |
| 2.4.7 | Fire montage with `AN_EjectCasing` (Moment Fire) on a weapon set to OnCycle | Fire | No casing from the notify (setting doesn't match); casing on cycle | Needs assets |
| 2.4.8 | Montage with `AN_EjectCasing` matching | Fire / cycle / reload | Casing at the notify frame, not also from the code | Needs assets |
| 2.4.9 | Casing Mesh empty | Fire | Small grey cylinder | Now |

### 2.5 Trigger finger

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 2.5.1 | ABP reparented, finger curl driven by `TriggerCurl` | Fire | Index finger curls and releases | Needs assets |
| 2.5.2 | Fire montage with a `TriggerFinger` curve | Fire | Curve drives the curl | Needs assets |

## 3. Melee and damage types

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 3.1 | Sword drawn, plain dummy | LMB | `Slashing 12 > 12`; stamina −10 (spec test 19) | Now (timer hit window) |
| 3.2 | 3.1 | Shift + LMB | `Piercing 16 > 16`; stamina −18 (spec test 20) | Now |
| 3.3 | Armored dummy | Light attacks with sword, club, dagger; heavies with axe and warhammer | Sword `Slashing 12 > 4`. Club `Blunt 10 > 12` (weakness). Dagger `Slashing 5 > 0`. Axe heavy `Slashing 22 > 14`. Warhammer heavy `Piercing 18 > 14` (spec test 21) | Now |
| 3.4 | Two dummies side by side | Swing through both | Each hit once (spec test 22) | Now (blade must sweep both; use the debug lines) |
| 3.5 | Stamina 0 (block until empty) | Attack | Doesn't activate (spec test 23) | Now |
| 3.6 | Fast swing past a thin target | — | Still hits (sub-steps fill in between frames; debug shows several lines per frame) (spec test 24) | Now |
| 3.7 | Nothing drawn | LMB / Shift + LMB | Punch Blunt 4 / 7 | Now |
| 3.8 | Every melee category | Light and heavy on the plain dummy | Matches the table in 0.3; categories without a heavy attack (shield) do nothing on heavy | Now |
| 3.9 | Attack while one is running | Spam LMB | No overlap; next attack only after duration + cooldown | Now |
| 3.10 | Swing into a wall | — | No damage; impact for the damage type and surface (with table entries) | Now |
| 3.11 | Attack montage with `ANS_MeleeHitWindow` | Attack | Hits only while the notify state runs | Needs assets |
| 3.12 | Weapon without `TraceStart`/`TraceEnd` sockets | Attack | Traces along the mesh's longest side from the grip | Now |
| 3.13 | Hit lands | Both clients | Damage numbers and hit effect on both; hit marker only on the attacker | Now (effects need assets) |
| 3.14 | Weapon with its own attacks (Override Attacks) | Attack | Uses its own instead of the category's | Now |
| 3.15 | Attack with `net pktlag=150` | Swing at a dummy | Swing at once locally; numbers when the server confirms | Now |
| 3.16 | Each hit | Watch durability in the tooltip | −0.5 per hit (attack's Durability Cost Per Hit) | Now |
| 3.17 | An attack with two damage types (iron blade: Slashing 10 + Iron 4, set on a copy) | Hit the armored dummy | Two lines: `Slashing 10 > 2`, `Iron 4 > 4` | Now |
| 3.18 | A new tag `Damage.Fire` added in Project Settings, used on an attack | Hit | Dealt with no protection; no code change needed | Now |
| 3.19 | Heavy club or warhammer swing, shield bash, rifle butt at the other player | Pushed away from the attacker (club heavy 350, warhammer heavy 400, axe heavy 250, shield bash 300, butt 200); half when blocked, none when parried | Now |
| 3.20 | Equip or unequip a weapon with Equip / Unequip montages set | The montage plays (not when joining, or for NPC outfits) | Needs assets |

## 4. Block, parry, durability, shields

Use `BP_TrainingDummy_Attacker`. It swings every 3 s: the club rises for 0.7 s, then strikes with Blunt 10.

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 4.1 | Sword drawn, facing the attacker | Hold RMB before the swing | Damage `Blunt 10 > 7.5` (25%); BLOCKED marker (blue); sword −2.5 durability; moving slower; stamina drains 5/s and doesn't regen (spec test 25) | Now |
| 4.2 | 4.1 | Press RMB just before the strike (within 0.25 s) | PARRIED (gold); 0 damage; no durability loss; attacker staggered and skips its next swing (spec test 26) | Now |
| 4.3 | Back to the attacker, blocking | — | Full damage (spec test 27) | Now |
| 4.4 | Block until the sword (60) breaks | — | Break sound; tooltip "Broken"; can't attack or block with it; HUD warning; G still throws it (spec test 28) | Now |
| 4.5 | Durability below 20% | — | Creak sound once; HUD "close to breaking"; tooltip "(close to breaking)" | Now |
| 4.6 | Buckler in the off hand + sword | Block the attacker | 80% absorbed by the **buckler** (its profile, its durability) (spec test 29) | Now |
| 4.7 | Buckler | Block a revolver shot from the other player | 80% absorbed; sword alone: not blocked (spec test 30) | Now |
| 4.8 | Unarmed | Block the attacker | 25% of Blunt absorbed | Now |
| 4.9 | Gun drawn | RMB | Aims, never blocks | Now |
| 4.10 | Attacker with Swing Can Be Blocked off | Block | Full damage (unblockable) | Now |
| 4.11 | Club (0.1 s window) vs dagger (0.3 s) | Try parrying with each | Dagger far easier to parry with | Now |
| 4.12 | Block held until stamina 0 | — | Block drops | Now |
| 4.13 | Parried attacker mid-swing (player vs player) | — | Attacker's swing stops; stagger montage (if set); can't attack for 0.8 s | Now (montage needs asset) |
| 4.14 | Block montage set | RMB | Plays; legs keep walking (upper body slot) | Needs assets |
| 4.15 | Shield bash | LMB with only the buckler drawn | Blunt 4 | Now |
| 4.16 | Block and parry with `net pktlag=150` | — | Decided by the server (parry window timed from the server's view of the press) | Now |

## 5. Throwing

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 5.1 | 5 stones in the inventory | Hold G at a dummy | Arc preview on this player's screen only; crosshair (spec test 31) | Now |
| 5.2 | 5.1 | Release | Stone flies in an arc; `Blunt 6`; lands as a pickup where it stops; stack 4 (spec tests 31, 33) | Now |
| 5.3 | Sword drawn | G | Sword unequipped first, flies end over end, Slashing 10, lands as a pickup (spec test 32) | Now |
| 5.4 | Dagger drawn | G | Point first (rotation follows the flight), Piercing 12 | Now |
| 5.5 | Hand axe drawn | G | Spins flat, Slashing 14 | Now |
| 5.6 | Bottle | Throw at a wall | Breaks (break sound/effect if set); no pickup | Now |
| 5.7 | Nothing throwable | G | Nothing happens | Now |
| 5.8 | A broken sword drawn | G | Thrown | Now |
| 5.9 | Stamina below the throw cost | G, release | No throw | Now |
| 5.10 | A thrown pouch (set Can Be Thrown on the herb pouch) | Throw, pick up | Keeps its contents | Now |
| 5.11 | Throw lands on a table edge | — | Tumbles off and settles (pickup physics) | Now |
| 5.12 | Throw montage set | G | Plays on both clients | Needs assets |
| 5.13 | Thrown with lag | — | Lands in the same place on both clients | Now |

## 6. Bow and crossbow

| # | Setup | Steps | Expected | Runs |
| --- | --- | --- | --- | --- |
| 6.1 | Bow drawn, 10 arrows | Hold LMB | Nock (0.35 s), then strength builds over 1 s; stamina drains 6/s and doesn't regen | Now |
| 6.2 | 6.1 | Release very early (< 15%) | Arrow let down, no shot, no arrow used | Now |
| 6.3 | 6.1 | Release at ~50% vs 100% | Weaker: slower, shorter, lower damage (×0.5 vs ×1) (spec test 34) | Now |
| 6.4 | Shoot an arrow into the wooden wall | — | Sticks; becomes a pickup there; E picks it up into the inventory (spec test 36) | Now |
| 6.5 | Arrow into a dummy | — | Piercing damage; sticks, recoverable | Now |
| 6.6 | Strafe and run while drawing | — | Legs move, upper body holds the draw (spec test 35) | Needs assets (upper-body layering) |
| 6.7 | Hold drawn until stamina 0 | — | Arrow let down | Now |
| 6.8 | Other client watching a draw | — | Bow's `DrawAlpha` follows (Blueprint `OnDrawAlphaChanged` bends the string) | Needs assets |
| 6.9 | Crossbow, 5 bolts | R, fire | Long reload (3.5 s), then fires at once; bolt sticks | Now |
| 6.10 | Bow, no arrows | LMB | Nothing | Now |
| 6.11 | HUD with the bow | — | `- / 10` (arrows carried) | Now |

## 7. Damage types × protection

Expected numbers with the armored dummy (Slashing 8, Piercing 4, Blunt −2). Run each.

| Source | Packet | Expected |
| --- | --- | --- |
| Sword light | Slashing 12 | 4 |
| Sword heavy | Piercing 16 | 12 |
| Dagger light / heavy | Slashing 5 / Piercing 11 | 0 / 7 |
| Axe light / heavy | Slashing 15 / 22 | 7 / 14 |
| Club light / heavy | Blunt 10 / 16 | 12 / 18 |
| Warhammer light / heavy | Blunt 14 / Piercing 18 | 16 / 14 |
| Chair leg light | Blunt 7 | 9 |
| Punch light / heavy | Blunt 4 / 7 | 6 / 9 |
| Revolver | Piercing 18 | 14 |
| Krag | Piercing 30 | 26 |
| Muzzle-loader | Piercing 34 + Blunt 6 | 30 + 8 = 38 |
| Arrow full draw | Piercing 22 | 18 |
| Stone | Blunt 6 | 8 |
| Thrown sword | Slashing 10 | 2 |

Then the same against each **block profile** (blocker = a second player or `Flow.BlockParryDurability` in automation):

- sword: 70 / 50 / 25 %;
- dagger: 40 / 30 / 10 %;
- axe: 40 / 30 / 30 %;
- club: 30 / 20 / 40 %;
- warhammer: 30 / 30 / 40 %;
- improvised: 25 % each;
- shield: 80 % each, projectiles too;
- unarmed: 15 / 10 / 25 %.

Each against **parry** (0 damage). Expected: final = (raw − protection) × (1 − block share), never below zero.

## 8. Edge cases

| # | Case | Expected | Runs |
| --- | --- | --- | --- |
| 8.1 | Empty ammo, fire | Dry fire, no error | Now |
| 8.2 | Zero stamina: attack, block, throw, bow | None activate; firearms still fire (no cost) | Now |
| 8.3 | Broken weapon: attack, fire, block | Refused; throw allowed | Now |
| 8.4 | Full inventory: unequip, pick up a thrown item, recover an arrow | Unequip refused; pickups stay on the ground | Now |
| 8.5 | Drop the drawn weapon | Sheathed state clears; nothing in the hand | Now |
| 8.6 | Throw the drawn weapon | Unequipped first; nothing in the hand | Now |
| 8.7 | Sheathe mid-reload / mid-cycle / mid-aim / mid-block / mid-swing | That action stops; no stuck tags (aim zoom resets) | Now |
| 8.8 | Swap (Q) mid-action | Same as 8.7 | Now |
| 8.9 | Unequip mid-reload | Reload stops; rounds loaded so far stay on the item | Now |
| 8.10 | Dummy dies mid-attack (overkill) | "Killed" marker (red) once; dummy resets after 4 s; later hits during death do nothing | Now |
| 8.11 | Player health 0 | State.Dead; abilities cancelled; logged (no death handling yet) | Now |
| 8.12 | Player disconnects while reloading / drawing a bow / with a projectile in flight | Server cleans up; projectile still hits (it has no owner checks beyond the instigator) | Now |
| 8.13 | Weapon actor arrives on a client after the drawn state | Still attaches to the right socket | Now |
| 8.14 | Equip the same weapon twice quickly | One actor only | Now |
| 8.15 | Open the inventory while drawing a bow | Bow let down, weapon sheathed | Now |
| 8.16 | Fire / swing while staggered | Blocked for the stagger's 0.8 s | Now |
| 8.17 | Interact (E) while blocking | Weapon sheathes; block ends | Now |
| 8.18 | Two players hit the same dummy at once | Both see both damage numbers; each gets only their own hit marker | Now |
| 8.19 | `vaesen.combat.debug 1` on a client | Traces, paths and hit points drawn there (server melee traces are sent to clients) (spec test 41) | Now |

## 9. Multiplayer

| # | Case | Expected | Runs |
| --- | --- | --- | --- |
| 9.1 | Client A draws, B watches | B sees montage (when set) and the swap at the grab moment | Now |
| 9.2 | **Late join** with A's rifle drawn and pistol sheathed | B joins later and sees the rifle in hand, pistol on the hip, without replaying the draw | Now |
| 9.3 | Late join after a casing-heavy fight | No old casings appear on the late joiner (not replicated) | Now |
| 9.4 | Loaded rounds, durability | Same on both clients and in the tooltip | Now |
| 9.5 | `net pktlag=150` + `net pktloss=5` for 5 minutes of mixed combat | No desync of loaded rounds, durability or drawn state; no stuck abilities | Now |
| 9.6 | Listen server host as player | Fires, blocks, throws like a client; shots aren't doubled | Now |
| 9.7 | Rotation while aiming/blocking at 150 ms | Smooth (owner and server agree on strafing) | Now |
| 9.8 | Dedicated server performance with 2 players swinging | Melee traces run on the server (mesh ticks bones there) | Now |
| 9.9 | Hit, block, parry, break sounds | Only heard after the server confirms | Now with placeholder sounds |

## 10. Sound

Fill `DA_WeaponSounds_Default` (every slot is listed, empty) with placeholder sounds. Set it as a category's Sounds in `DA_WeaponCategoryDefaults`, or on a weapon.

| # | Case | Expected | Runs |
| --- | --- | --- | --- |
| 10.1 | Everything with sounds unset | Silent, no errors or warnings (spec test 37) | Now |
| 10.2 | Each slot with a placeholder | Draw/Sheath at grab, Swing (no-montage fallback or `AN_WeaponSound`), Fire + DistantTail + Echo, DryFire, Reload steps (`AN_WeaponSound`, or ReloadInsert/Ramrod fallback), Cycle, CasingBounce, BowNock/BowRelease, ArrowStick, Block, Parry, Break, NearBreak, Misfire (spec test 38) | Now with placeholders |
| 10.3 | Fire far from the other player | Fire with Gunshot Attenuation carries; distant tail and echo slots (spec test 39) | Needs assets (attenuation) |
| 10.4 | Impact table with sounds per type × surface | Sword on the dummy (Flesh), arrow into wood, bullet on stone with ricochet chance | Now with placeholders |
| 10.5 | Effort and pain vocals on `BP_VaesenPlayerCharacter` | Heavy attack and throw grunt; pain when hit, not when blocked or parried | Now with placeholders |
| 10.6 | Concurrency set | 40 casings bouncing don't stack sounds | Needs assets |
| 10.7 | Flyby: stand within 3 m of the other player's line of fire (bullets and arrows) | Flyby plays once as it passes, not for the shooter | Now with placeholders |
| 10.8 | Shoot an arrow into wood, then press E on it | Arrow pull (bow category's set) for the one who takes it | Now with placeholders |
| 10.9 | Drop a weapon (or throw a stone) onto the floor | Drop/land sound once on first touch, on both clients (item's Land Sound, else the weapon's DropLand) | Now with placeholders |
| 10.10 | Walk and run with a rifle slung and a pistol holstered | Gear rattle per stride, louder when running; none with nothing sheathed. With `AN_Footstep` in the locomotion it follows the footsteps | Now with placeholders |
| 10.11 | HammerCock | From montages with `AN_WeaponSound` | Needs assets |

## 11. Empty-slot robustness

| # | Case | Expected | Runs |
| --- | --- | --- | --- |
| 11.1 | A weapon with no mesh at all (no Weapon Mesh, no Pickup Mesh) | Equips, draws, fires from the actor's origin, swings along the character's reach; no crash (spec test 40) | Now |
| 11.2 | No montages anywhere | Every action works on timers | Now |
| 11.3 | No anim layers | Draw/sheathe don't change the anim | Now |
| 11.4 | No sockets on the skeleton (delete `hand_r_weapon` and `Sheath_*`) | Weapons use `hand_r` / `hand_l` and the sheath locations' fallback bones | Now |
| 11.5 | No `DA_WeaponCategoryDefaults` (clear the setting and delete the asset) | Built-in grey-box defaults are used | Now |
| 11.6 | No `DA_ImpactEffects` | No impact effects; no errors | Now |
| 11.7 | No effects on a gun (flash, smoke, tracer) | Nothing spawned; no errors | Now |
| 11.8 | No ammo class on a gun (Accepted Ammo and tag empty) | Can't reload; dry fire | Now |
| 11.9 | No Projectile Mesh on ammo | Small grey sphere | Now |
| 11.10 | No GE_ / GA_ Blueprints | The C++ classes are used | Now |
| 11.11 | No combat input assigned on the character | No combat input; no errors | Now |
| 11.12 | ABP not reparented to `VaesenAnimInstance` | Everything except IK, aim offset and trigger finger works | Now |

## 12. Tests that can't run yet

Each of these needs something not in the project yet:

- Anything marked **Needs assets** above. Draw/sheath montages with `AN_WeaponGrab`/`AN_WeaponRelease` and the `HandIK_Sheath` curve; attack montages with `ANS_MeleeHitWindow`; fire/reload/cycle montages with `AN_EjectCasing` and `AN_WeaponSound`; anim layers per category; Control Rig or additive finger curl; bow string bone; sounds, attenuation and concurrency; Niagara effects.
- **The anim Blueprint isn't reparented** to `VaesenAnimInstance` and has no upper-body slot or layered blend. Until it is: no IK, no aim offset, no trigger finger, and montages play full-body.
- **Motion matching**: `LocomotionDatabase` per category is exposed but empty. The locomotion graph will read it when the motion matching setup comes.
- **Socket placement** on the mannequin (`hand_r_weapon`, `Sheath_*`) is estimated. Weapons will sit oddly until adjusted in the Skeleton editor. The grab points (`ReferenceGrabPoint`) are zero, so the grip-distance warning (1.14) only becomes meaningful once they're set with the draw montages.
- **The self-test's sword hit** is INFO, not a check, until there is an attack montage.
- **Health and damage reactions**: there's no condition system yet. Player death only sets State.Dead and logs.

---

## Notes on how this was built

These go beyond the spec, or depart from it:

- **GAS was added, not extended.** The spec says GAS already exists; the project had none. It now has:
  - an ability system component on every character (`UVaesenAbilitySystemComponent`);
  - `UVaesenAttributeSet` with Health, MaxHealth, Stamina, MaxStamina, StaminaRegenRate, ProtectionSlashing/Piercing/Blunt and IncomingDamage;
  - the GE classes in C++, with Blueprint children made by the commandlet.
- **Sidearm slot.** A new equipment slot, `Sidearm`, so a rifle (main hand, two-handed) and a pistol can be carried together. H draws the main-hand set, or the sidearm if that's all there is. **Q** swaps between them; it isn't in the spec.
- **Timing.** Reload and cycle are timed by the weapon's numbers; their montages are sped up or slowed down to fit. Hit windows and grab frames come from notifies when present, with timer fallbacks otherwise.
- **Bow montages.** The bow uses its Reload montage slot for nock-and-draw and its Fire montage for the release.
- **Hit cues.** Hit GameplayCues (`GameplayCue.Hit.*`) are handled in C++ on the hit actor, so they need no GameplayCue assets. GameplayCue Notify assets under `/Game/Vaesen/Combat` are also picked up.
- **Damage** always goes through `GE_Damage` and `UVaesenDamageExecution` (protection → block → parry). There's no separate `ApplyDamagePacket` damage path; `UVaesenCombatLibrary::ApplyDamagePacket` only builds and applies the effect spec.
- **What gets thrown.** Without a hotbar, G throws the drawn weapon if it's throwable; otherwise the first throwable stack in the inventory (pouches included).
