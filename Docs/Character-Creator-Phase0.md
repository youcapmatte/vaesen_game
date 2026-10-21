# Character Creator – Phase 0 Spike Results

Oct 20, 2026 · Unreal Engine 5.8.3 · branch `feature/character-creator` · tested on i7-9700K, RTX 2080, 1080p

The spike tests the riskiest parts of [Character-Creator.md](Character-Creator.md) before Phase 1. Scripts and test levels are under `Scripts/Editor/Spike/` and `/Game/Spike/`; they're throwaway and will be removed once Phase 1 has its own versions.

## Results

| Test | Result | Summary |
| --- | --- | --- |
| P0-1 Engine | **Partial** | The Win64 editor and dedicated server build on 5.8.3 with the GameLift Server SDK plugin. Linux needs the `v26_clang-20.1.8-rockylinux8` toolchain (not installed). The server hasn't been started yet. |
| P0-2 Base faces | **Pass** | Two faces created, sculpted, skinned and groomed by Python. Auto-rig and assembly were done in the editor after the Epic login. They look good. |
| P0-3 Sculpt sliders | **Pass** | Jaw and nose width clearly visible in both directions, and expressions (face ROM) look right at full strength. Cheekbone height is too subtle: tuning for later. |
| P0-4 Joint offsets | **Pass** | Per-character joint offsets after the face rig move the jaw joints 0.37–0.40 cm outward and inward, symmetrically. Visually good. In the packaged build too: at LOD 1 the jaw joint sits 0.40 cm in, as in the editor. At LOD 2 the joint doesn't exist (see finding 2). |
| P0-5 Skin tone | **Pass (offline and at runtime)** | Moving a face's baked textures from tone A to B, deep and very fair with MetaHuman's bias/gain model looks natural, and head and body match. |
| P0-6 Age | **Partial: age doesn't read convincingly yet** | Two building blocks, both working technically. (1) An "age" slider: landmark sculpt (jowls and mouth corners down, cheeks hollower, brow lower) through the slider pipeline, at most 0.21 cm at weight 1, pushed to 2.5. At age 1.0 the lower face is visibly heavier, but far too subtle. (2) Age wrinkles: the joint-offset node raises the face's wrinkle-map curves (forehead, frown, crow's feet, nasolabial, lips, neck) to a floor of 0.6 × age. The values arrive in the material, but they barely show, even under raking light. **Decision needed:** an aged-skin layer of our own (wrinkle normal, roughness and cavity blend, as the spec planned), and/or base faces in age bands. |
| P0-7 Hair | **Provisional pass** | Hair cards generated from the same groom are acceptable for gameplay, with the same colour. The mesh LOD wasn't shown reliably in the editor. |
| P0-8 Clothing | **Body half: partial pass; clothing not testable yet** | **Body split for the armour system:** the mannequin splitter, generalised (`SplitSkeletalMesh`), splits a MetaHuman body into torso (with arms and neck), hands, legs and feet, every part closed. The MetaHuman toe bones (`indextoe_01_l`...) were taken for fingers at first; fixed. Put back together it looks like the whole body, except for a jagged dark seam at the waist and faintly at the ankles: no clean edge loop there, so the flat caps show. Fix in Phase 1 (pull the caps inward, or cut along the topology). The parts only have LOD 0 so far. **Clothing:** no 19th-century garments in the project yet (only MetaHuman's default garment, which its build does fit to the heavier body type). Waits for garments, e.g. from Fab. |
| P0-9 Animation | **Pass** | Mannequin idle, walk and run retargeted with an IK Retargeter (`IK_Mannequin` → `IK_MH_IKRig`). Stride adapts to the proportions. |
| P0-10 Performance | **60 fps reached; game thread over budget** | Packaged at 1080p: 69–83 fps. Characters cost +4.0–6.2 ms GPU (budget 4) and +3.5–4.5 ms game thread (budget 2), before any optimisation. See below. |
| Neck seam | **Pass at rest; head turns not checked** | SpikeFace_A's head on a second, much heavier and taller parametric body (182 cm, fat +1.2, muscularity +0.6, wider neck): no visible seam, and skin matches. The face rig survives the body change, so no re-rig is needed. But that variant's hair doesn't render: the groom is assigned, but its binding doesn't follow the new body (see finding 10). |

## P0-10: 24 MetaHumans

Standalone game (uncooked, Development) at 1080p, all 24 faces playing the face ROM. The baseline is the same scene without characters: 6.7 ms GPU, 1.6 ms game thread, 139 fps.

| LOD | Characters' GPU cost | Characters' game thread cost | fps |
| --- | --- | --- | --- |
| auto (0–2) | +8.0 ms | +4.9 ms | 63 |
| 1 | +7.1 ms | +4.9 ms | 68 |
| 2 | +5.1 ms | +4.1 ms | 79 |
| 3 | +4.5 ms | +3.9 ms | 83 |
| Budget | ≤ 4 ms | ≤ 2 ms | 60 |

**Packaged build** (cooked, Development), after the NVIDIA driver update to 617.42 (no driver warning any more). The baseline without characters is 7.3 ms GPU, 1.5 ms game thread, 114 fps.

| LOD | Characters' GPU cost | Characters' game thread cost | fps |
| --- | --- | --- | --- |
| auto | +5.1 ms | +4.5 ms | 76 |
| 1 | +6.2 ms | +4.1 ms | 69 |
| 2 | +4.5 ms | +3.7 ms | 79 |
| 3 | +4.0 ms | +3.5 ms | 83 |
| Budget | ≤ 4 ms | ≤ 2 ms | 60 |

**Verdict:** 60 fps is reached at every LOD. The GPU budget is met at LOD 3 and nearly met at LOD 2. The game thread is at about twice its budget, which is what the animation optimisations below are for. Before the driver update, the packaged runs crashed while creating GPU pipelines (`8007000e`).
Not yet tried, all planned in the spec:
- the Animation Budget Allocator and update-rate optimisation;
- the face rig only near the camera (here every face runs the full ROM);
- forcing hair cards.

These are measured again in Phase 1 with `RunSpikeBenchmark.ps1`.

## Findings that change the spec or Phase 1

1. **The face rig's DNA is shared per face mesh.** RigLogic reads it from the skeletal mesh asset, so sliders can't edit DNA per character. They use morph targets plus per-character joint offsets added after the face rig (`FAnimNode_VaesenFaceJointOffsets`). Joint moves are taken from the skin-weighted move of the skin each joint drives. The DNA's own joint data doesn't line up with the mesh skeleton in 5.8.
2. **Sliders must exist at every face LOD.** At gameplay distance faces run at LOD 1–2, and the face rig drops most face joints below LOD 0, so joint offsets are a close-up refinement. **Solved in the slider tool:** it now writes each slider morph into every built face LOD, each from its matching DNA LOD (built LOD 0/1/2/3 = DNA lod1/3/5/7, all fitting exactly), with the same strength. Checked visually at forced LOD 2: jaw and nose width differ clearly.
3. **The built face LOD 0 is DNA `head_lod1_mesh`.** The Optimized High build starts at DNA LOD 1, so the slider tool matches DNA meshes to the built LODs by best fit.
4. **Script sculpting goes through face landmarks.** The face model coefficient array also holds the model's layout and can't be edited blindly. Landmark moves are damped by the face model (about 40% of the move arrives), so slider moves must be generous.
5. **Skin tone.**
   - The built (baked) materials have no tone parameters.
   - Tone is moved per channel in linear colour: detail = (255·albedo − bias) / gain, then new = (bias′ + gain′·detail) / 255, with bias/gain as MetaHuman stores them per tone.
   - At runtime this belongs in a material function in `M_skin_unified_baked`.
   - The MetaHuman component replaces the face materials with its own dynamic instances at startup, so the appearance component must set the parameters on those.
   - The shift must be masked to skin, because the baked underwear changes colour too.
6. **Don't animate the MetaHuman Blueprint directly.**
   - The MetaHuman component asynchronously reloads the face's and body's animation classes after BeginPlay, which overrides single animations and holds the body still (play rate 0).
   - In the game, the MetaHuman's parts follow our character's animated mesh (Copy Pose / Leader Pose).
   - Animation only updates when rendered (`OnlyTickPoseWhenRendered`), and the MetaHuman's LOD Sync component decides every part's LOD.
7. **Cloud steps.** Auto-rig, and the high-resolution textures (needed to build), go through Epic's cloud. After one interactive login, scripts can use them; each auto-rig takes about 50 s.
8. **Hair LOD chain** (Optimized High): strands at LOD 0–1 (screen size ≥ 0.5), cards at LOD 2–4, meshes at LOD 5–7. Gameplay caps it at LOD 2.
9. **The first spike client package** (build, cook, pack) took 36 minutes; repackaging takes a few minutes.
10. **Hair after a body change (open).** On SpikeFace_A_Heavy (the same face rebuilt with another parametric body), the rebuilt updo groom shows only a few stray strands, in the editor and in Simulate (`neck-simulate-hair.png`). Its binding points correctly at the heavy build's face mesh, and the log shows no groom or binding errors. To find out in Phase 1: rebuild the binding, check whether the face mesh changed with the body, or bind at runtime. Body types must be checked with every hairstyle.

## Still open

- **P0-8 Clothing:** one 19th-century outfit on two body types (Chaos Outfit resizing), once garments are in the project; the waist and ankle seams of the split body.
- **P0-1 Linux:** the toolchain.
- **P0-10 again:** in Phase 1, with the optimisations.

## Cleanup (21 October)

The Phase 0 test levels, their scripts and their test-only content are removed: 7.9 GB of `/Game/Spike` became 2 GB. The results and images above stay. Removed:
- the slider variant characters (`SpikeSliders.py variant` rebuilds them; their DNA stays in `Saved/Spike/DNA`);
- the heavy-body, body-split and offline skin-tone tests;
- the retargeted test animations;
- every Phase 0 test map except `L_SpikeCreator`;
- the age-skin builds, except their two head textures each (now in `/Game/Spike/Appearance/AgeSkins`).

The removed test scripts were never committed, so re-running one of those tests means writing it again from the description here. `SpikeCleanup.py` does this cleanup, and reports first without `--delete`.
## Phase 1 groundwork already in place

Built and tested during the spike, on `feature/character-creator` (not committed):

- **API.**
  - `CharacterAppearance` on the C# `Character` (stored as a nested DynamoDB map), sent with `CreateCharacterRequest.Appearance`.
  - `AppearanceRules` validates it: every ID allowed for the character's sex, every number finite and within range, and anything else rejected rather than clamped.
  - The allowed lists live in `Characters:Appearance` configuration, with the spec's placeholder IDs as defaults until the preset table export exists.
  - Tests: `AppearanceTests` (API round trip, 16 rejection cases, configured presets) plus 2 DynamoDB mapping tests. All 130 tests pass.
- **Game data.**
  - `FVaesenAppearance`: replicable, with face sliders as a list. JSON in and out matching the API, tolerant of unknown and missing fields, clamping out-of-range values.
  - `FVaesenCharacter.CharacterAppearance` is read from API responses.
  - `UVaesenApiSubsystem::CreateCharacter` has an overload that sends an appearance.
  - Automation tests `Vaesen.Appearance.*` (3) pass.
- **`UVaesenAppearanceComponent`.**
  - Replicated appearance, applied on clients only, never on a dedicated server. It can dress another actor (`TargetActor`, for the creator's preview).
  - Applies sculpt sliders (morph pairs `slider_<name>_pos/_neg`), their joint offsets (from a `UVaesenFacePresetData` per base face), age (morph plus wrinkle floor) and hair colour (`hairMelanin`, `hairRedness`, `hairDye` on the grooms).
  - Tested end to end on SpikeFace_A (`L_SpikeAppearance`): the values arrive and persist.
  - **Base face swap:** with a `PresetTable`, the component spawns the face row's MetaHuman Blueprint (with its face preset data), attaches it to its owner, swaps it when the face changes, and applies the look again 1.5 s after spawning, once the MetaHuman has settled. Never on a dedicated server. Loading is synchronous for now; it should stream ahead in Phase 1.
- **Presets and NPCs.**
  - `FVaesenAppearancePresetRow` (the rows of `DT_AppearancePresets`: the row name is the stored ID, plus kind, sex, player-selectable, random weight, assets and values).
  - `UVaesenAppearanceLibrary::RandomAppearance`: deterministic per seed, filtered by sex, weighted, with colour jitter and moderate sliders. Used by NPCs and the creator's Randomise.
  - `ToApiOptionsJson` exports the table in the shape of the API's `Characters:Appearance` section. The API reads an optional `appearance-presets.json`, and a test checks the shape.
  - A spike table `DT_AppearancePresets_Spike` (20 rows) and six random NPCs (`L_SpikeNpcs`, `npc-random.png`): seeds 3 and 6 get face B, the others face A, each with its own sliders, age and hair colour.
  - Automation test `Vaesen.Appearance.RandomAppearance`: 200 seeds per sex, all valid; weight 0 never picked; the same seed gives the same look.
  - Palette note: hair melanin 0.6 reads almost black and 0.05 almost white; tune the swatches by eye.
- **Character creator model.**
  - `UVaesenCharacterCreator` (no UI): the five steps with Back/Next and what each step needs; camera framing per step (Body / Face).
  - Presets offered by sex. Changing sex after the body or face was edited asks for confirmation, then resets them.
  - Swatches, sliders, age and hair colour; Randomise (deterministic, keeps sex and body); the API's name rule; Create through the API with the appearance.
  - Automation tests `Vaesen.CharacterCreator.Steps` and `.Appearance` pass.
- **Creator scene and panel.**
  - `AVaesenCharacterCreatorScene`: the preview MetaHuman (through the appearance component), a turntable and the camera. It forces full detail on the preview (LOD Sync LOD 0), so the creator gets strand hair.
  - **Camera:**
    - The mouse wheel zooms between the body and face framings, and the step sets the zoom too (Face and Eyes & Hair zoom in).
    - Both framings put the character left of centre by the same share of the view, beside the panel, at any zoom.
    - The field of view is set from a vertical one (34°) every frame, so any window shape shows the whole body.
    - The face framing follows the preview's head height, read from the face skeleton's reference pose: the male faces' heads are 10–15 cm higher.
  - **`UVaesenCharacterCreatorWidget`:** a panel built in code, with a page per step, Back / Randomise / Next, and drag-to-turn outside the panel.
    - Origins: sex (a sex with no faces says so) and class. Body and skin: body, skin swatches, tone and warmth. Face: base face, a slider per sculpt slider, age. Eyes and hair: eyes, hairstyle, hair swatches, darkness / redness / dye, eyebrows, facial hair. Name: first and last name, a summary.
    - A Widget Blueprint can subclass it with a designed layout later.
  - **Game mode:** `AVaesenCharacterCreatorGameMode` and `AVaesenCharacterCreatorPlayerController` (a front-end mode: no player character or HUD); the arrow keys also turn the preview.
  - **Test level:** `/Game/Spike/Maps/L_SpikeCreator` (`SpikeCreatorLevel.py level`; press Play). `SpikeCreatorLevel.py drive <folder> <stage>` steps it during Simulate.
  - **Hair in captures:** scene captures show no groom while the editor is in the background (its viewport doesn't draw). With the editor window in front, they do. In Play, hair shows normally. Not a game bug.
  - Images: `creator-scene.png` (first version), `creator-panel.webp` (the panel in Play), `creator-head-framing.png` (face framing on face A and the two male faces).
- **Base faces now:** female A and B, and male M_A and M_B (copies of the MetaHuman presets Victor and Lorenzo, made by `MetaHumanBaseFaces.py author|rig|build <face>`, with the grooms they came with).
  - Every face has the sculpt sliders (`SpikeSliders.py --face <face> base|variant|apply|preset`). Face B lacked them at first, which is why its sliders did nothing.
  - The slider regions are drawn on face A; other faces' landmarks are mapped onto it (centre and size) before selecting.
  - `UVaesenFacePresetData::SliderStrength` scales a slider's morph and joint offsets: cheekbone height 2.5 and nose width 1.6, which barely read at 1.
  - The male faces' build used another copy of the MetaHuman common content (`/Game/Spike/MetaHumans/Built/Common`). `SpikeCommonContent.py` applies the Vaesen changes (skin tone parameters, joint-offset node) to every copy; run it after every MetaHuman build.
- **Age is skin only** (the P0-6 decision, 21 October): no morph or joint offsets any more.
  - Each face has a young and an old skin: its own, or a copy of the face built with another of MetaHuman's skin textures (`MetaHumanBaseFaces.py skin <face> <index> <suffix>`; index 151 is the oldest, used by the presets Lorenzo and Lani). The copy keeps the face's skin tone, so the tone shift applies to it as to the face.
  - In the skin material: before the tone, the base colour blends toward the young or old skin's baked colour, and after it the normal toward its baked normal. The weights are `VaesenAgeYoung` / `VaesenAgeOld`, set on the face only (`AddSkinAgeColor`, `AddSkinAgeLayer`, via `SpikeCommonContent.py`).
  - `UVaesenFacePresetData::BaseAge` is how old the face's own skin looks (Lorenzo 0.75, the others 0.25–0.3). Below it the skin goes toward the young skin, above it toward the old one, 85 % at most. The wrinkle-map floor stays.
  - Cost: one more colour and normal texture per face (the young or old skin), and two extra samples in the skin material.
  - Image: `age-skin-and-jaw.png`.
- **Jaw width** now moves the whole lower jaw to the chin: the jaw's and chin's sides spread in proportion to their distance from the middle.
- **Grooms swap**: hairstyles (women 6, men 8, and bald), eyebrows (5), and men's facial hair (10, and none), from the MetaHuman plugin's groom assets.
  - `SpikeGrooms.py` binds every groom of a sex to every face of it: 68 bindings in `/Game/Spike/Appearance/GroomBindings`, listed in each face preset's `GroomBindings`.
  - Each binding is transferred from the head that groom was authored on, which its own plugin binding names. There are three heads: `SKM_Groom_Head_Legacy01` (female: long hair, updos, some brows), `Legacy02` (male) and `MH_Groom_Head` (brush cut). Transferring every groom from `Legacy02` left women's hair and the Natural and Thin brows about 15 cm low.
  - A preset's own facial hair is baked into its face's skin textures when it's built with it; it showed as a blue stain with facial hair set to none. The male faces are rebuilt without it (`MetaHumanBaseFaces.py nofacialhair`).
  - The appearance component swaps a groom only when its ID changes. It clears the Blueprint's material overrides so the groom's own materials apply, and puts facial hair on the moustache or beard component by the groom's name.
  - Image: `groom-swaps.png`.
- **Eye colour**: the built eyes use a baked iris texture and ignore the iris colour parameters (procedural iris switches showed no change). Eye colours are recoloured copies of the baked brown iris (`/Game/Spike/Appearance/Eyes`), set as `Iris Basecolor Baked`. Image: `eye-colours.png`.
- **Hair grey and dye**: the hair material greys every hairstyle by default (`WhiteAmount` 0.3), which made black read light brown with blond strands. Grey is now an appearance value (API `hairColor.grey`), 0 by default, with grey (0.55) and white (1.0) swatches. Dye now works: `hairDye` is a colour, and dye blends it toward a red.- **Hair colour swatches**, tuned by eye (`Saved/Spike/HairColour`):
  - the hair material caps redness at 1;
  - redness tints in proportion to melanin, so blonds need some redness (0.25 / 0.5; without it they read grey) and reds need melanin around 0.45 (0.45 / 1.0);
  - now black, dark brown, brown, light brown, ash brown, blond (0.4 / 0.05), light blond (0.3 / 0.5), red, auburn, greying (grey 0.5), grey (grey 1.0), white (grey 1.0, melanin 0.3).
  - More red than that would need a dye colour (the material's dye), not more redness.- **In the game (Phase 1, 21 October).**
  - **Character select** (`L_CharacterSelect`): the level has an `AVaesenCharacterCreatorScene` in a small studio (`Scripts/Editor/SetupCharacterSelectLevel.py`). It's the view: it shows the selected character's look.
    - Create Character opens the creator there for the selected guild. Create sends the character with its appearance to the API, then the list selects it; Cancel returns. Without a scene in the level the old name-and-class dialog is used.
  - **Joining a world:** the game mode asks the API for the character's appearance (`GET api/sessions/server/characters/{id}`, game-server scope, open in Development) and keeps it on the player state. The player character's `UVaesenAppearanceComponent` (replicated) gets it. A character without one gets a generated look (the same per character ID).
  - **The player character:**
    - It spawns the face's MetaHuman on its mannequin mesh. The mannequin keeps animating (movement, sockets, equipment) but isn't drawn.
    - The MetaHuman body follows it through the IK Retargeter (`UVaesenRetargetAnimInstance`: one Retarget Pose From Mesh node, no animation Blueprint). In first person the owner doesn't draw the MetaHuman's head and grooms.
    - NPCs keep the mannequin.
  - **Settings:** Project Settings > Vaesen > Characters > Appearance: `AppearancePresets`, `SkinToneTable`, `MetaHumanRetargeter` (the spike assets for now), `CharacterCreatorWidget`.
  - **Testing in editor play** (no character ID): `vaesen.TestAppearance <seed>` gives the player a generated look.
  - **Not yet:** armour shows on the hidden mannequin parts only (the MetaHuman body split into the part slots is the next step), and changing a look after creation.- **Runtime skin tone (P0-5 in the engine).**
  - `M_skin_unified_baked` (MetaHuman common content, shared by every built MetaHuman) tones its final base colour: Break Material Attributes, then BaseColor × `VaesenToneScale` + `VaesenToneOffset`, then Set Material Attributes. Identity by default.
  - Inserted and removable by script (`AddSkinToneParameters` / `RemoveSkinToneParameters`).
  - `UVaesenSkinToneTable` (`ST_SkinTones`) holds MetaHuman's bias/gain on a 9 × 9 U/V grid, sampled in the editor. `UVaesenFacePresetData` holds each face's base tone.
  - The appearance component sets scale and offset on the face's and body's material instances.
  - Result (`skin-tone-runtime.png`): face A fair, as built, medium and deep, and face B (synthesized dark) made fair, all at runtime.
  - **Still to do:** a skin mask (the baked underwear is tinted too), and the eye area (dark rings at medium and deep tones).
  - **Risk:** the change sits in MetaHuman common content, which a MetaHuman build might restore or copy unchanged into another folder. Re-run `SpikeCommonContent.py` after builds, or move it into our own material.
  - The first attempt toned the colour texture objects directly, which broke every skin material compile until it was removed.
- **Tools.**
  - The slider tool writes morphs for every face LOD.
  - The mannequin splitter also splits MetaHuman bodies.
  - The Win64 editor and dedicated server build.

Still to do for Phase 1:
- the body and hairstyle swap, and streaming the face swap;
- the skin mask for the tone shift, and the eye area;
- eye colour;
- a designed look for the creator panel;
- the hair cards LOD, which showed no hair in `L_SpikeHair` on 21 October although it did in P0-7 (check with the editor in front: it may be the background-capture effect);
- a dye colour for hair (more vivid reds than the natural range);
- the MetaHuman as the body of `AVaesenBaseCharacter`, following its animated mesh;
- moving the spike classes (`FAnimNode_VaesenFaceJointOffsets` and the slider tool) out of `Spike/`.

## Images

In `Docs/Phase0Images/`:

| Image | Shows |
| --- | --- |
| `base-faces.png` | P0-2, the two built test faces |
| `sliders-at-lod2.png` | Finding 2: base, jaw +, jaw −, nose + at LOD 2 |
| `age-front-light.png`, `age-side-light.png` | P0-6: young, wrinkles only, age 0.5, age 1.0 |
| `neck-front.png`, `neck-side.png` | Neck seam: default body and the heavier body |
| `neck-simulate-hair.png` | Finding 10: the heavier body's missing hair, in Simulate |
| `body-parts.png`, `body-parts-seams.png` | P0-8: the whole body, the parts together, the parts apart |
| `npc-random.png` | Six random NPCs (seeds 1–6): the component spawns face A or B from the table and dresses it |
| `skin-tone-runtime.png` | Runtime skin tone: A fair, as built, medium, deep; B fair |
| `creator-scene.png` | The creator scene's first version: body framing, Face step framing, Randomise turned 25° (no hair: background capture) |
| `creator-panel.webp` | The creator panel in Play (Body and skin step), with strand hair |
| `hair-colour-tuning.png` | Hair colour candidates (melanin m, redness r) |
| `age-skin-and-jaw.png` | Age 0 and 1 on faces A, M_A, M_B (skin only), and jaw width -1 / +1 on face A |
| `groom-swaps.png` | Groom swaps on male face A (hairstyles, mutton chops, bald) and grey hair |
| `eye-colours.png` | Eye colours: blue, green, grey, hazel |
| `creator-head-framing.png` | The face framing following the head height: face A, male faces M_A and M_B |
| `creator-level-placed-hair.png` | A placed MetaHuman in the creator level, with hair |
| `appearance-component.png` | The appearance component: default, wide jaw with red hair, old and narrow with very fair hair |