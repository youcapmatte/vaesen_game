# Character Creator – Requirement Specification (rev. 2)

Oct 20, 2026 · @Mattias Persson · Unreal Engine 5.8.3

Revision 2 replaces the Oct 19 draft. It applies the assessment of that draft (base faces, skin tone, performance, clothing, phasing) and these decisions:

- **Performance comes first.** Smooth multiplayer on mid-range PCs takes priority over maximum visual quality.
- **Strand hair only in the creator and in dialogue close-ups.** Gameplay uses hair cards generated from the same grooms.
- **NPCs use the same appearance system**, with randomised values.
- **Base face assets are frozen once shipped.** A player character's uniqueness comes only from its stored slider values.
- **Engine:** Unreal Engine 5.8.3 (source build) with the GameLift Server SDK plugin.

Section 9 lists the findings from the UE 5.8.3 engine source that these decisions rest on.

## 1. Purpose and context

Replace the current modal "Create character" dialog with a step-by-step character creator built on MetaHumans. A character's appearance is saved as data and rebuilt the same way on every client.

- **Where it starts:** the Character Select level. The player picks a guild, sees that guild's characters and clicks **Create character**.
- **Who owns characters:** the guild, not the player. Any guild member may play any guild character in a mystery, so appearance must never depend on the creating player's machine or local files.
- **Today:** a modal dialog with first name, last name and class. These move into the stepper, and sex is added.

## 2. Character pipeline

Every human character, player or NPC, is a MetaHuman driven by the same appearance data. Monsters and vaesen are separate characters on their own skeletons and don't use the appearance system.

### 2.1 Base faces and bodies are pre-built, frozen MetaHumans

- **Base face:** a complete MetaHuman head (head mesh, its DNA and face rig, its skin textures) authored in the editor and built with the MetaHuman Optimized pipeline. Base faces are **not** morph targets on a shared head: each MetaHuman's DNA stores its own facial joint positions, and the eyes, teeth and mouth interior follow those joints.
- **Shared:** all base faces share the MetaHuman head topology, one skin material setup, the eye, teeth and lash materials, and the groom list.
- **Loading:** a base face is loaded on demand through a soft reference. Only the faces of characters in the current session are in memory.
- **Frozen:** once a base face ships, neither its ID nor its asset may change, because every character using it would change with it. A visual fix to a shipped face means shipping a new face with a new ID. A data validation check (section 8) enforces this.
- **Body types:** 2–3 MetaHuman body types per sex (height and build), built the same way and frozen the same way. Every garment needs a fitted variant per body type, so this number stays small.

### 2.2 Uniqueness comes from stored values

Two characters with the same base face differ only through their stored values: sculpt sliders, age, skin tone, eyes, hair and body type. Section 5 says how each is applied at runtime.

### 2.3 Skeleton and animation

- **Skeleton:** the MetaHuman body skeleton (`metahuman_base_skel`). Its main bones use the same names as the UE5 mannequin (Manny), so mannequin animations retarget with an IK Retargeter, mostly a direct mapping.
- **Missing IK bones:** the MetaHuman skeleton has no `ik_foot_root` or `ik_hand_*` bones. The animation Blueprint must not depend on them, or must add them as virtual bones.
- **Extra bones:** about 116 corrective and twist bones are driven by the MetaHuman body's post-process logic. At low detail levels that logic can be switched off.

### 2.4 Modular body and armour

The existing equipment system dresses a modular body. Today that's the mannequin split into Head, Torso (with arms), Hands, Legs and Feet by `VaesenMannequinSplitter`, plus a worn-over layer per part and a Back layer for cloaks.

- **Body:** the MetaHuman body for each body type is split into Torso, Hands, Legs and Feet the same way (`VaesenMannequinSplitter::SplitSkeletalMesh`, in place since Phase 0). Each part stays a closed surface, so a part hidden on its own leaves no hole. Still to do: a jagged seam at the waist and ankles when the parts sit together (no clean edge loop there), and the parts' lower LODs.
- **Head:** the base face's head is the Head part. Hats and hoods are worn-over layers; nothing replaces the face.
- **Hiding the body under clothing:** a garment that replaces a part hides that part entirely. A worn-over garment that would still let skin poke through hides it with a body mask on the part's material.
- **Armour meshes:** `UVaesenArmorItem::ArmorMesh` targets the MetaHuman body skeleton instead of the mannequin. It has one fitted variant per body type, chosen by the character's `bodyPresetId`.
- **Clothing workflow:** garments come from third-party tools (Marvelous Designer, Blender and so on) and are skinned to the MetaHuman body. Fitting each garment to the other body types uses Chaos Outfit resizing if it proves reliable (Phase 0, test P0-8); otherwise each variant is fitted by hand.

### 2.5 Licensing

MetaHuman assets may be used in Unreal Engine projects free of charge.

## 3. Stepper flow

The creator is a full-screen stepper with Back, Next and a step indicator. Every step updates a live preview MetaHuman on a turntable (drag to rotate). The preview runs at full quality: LOD0, strand hair, full face rig.

1. **Basics:** sex and class. Sex filters the body and face presets offered later. Changing sex after later steps resets the face and body steps, after a confirmation.
2. **Body and skin:** body type and skin tone. Full-body camera.
3. **Face:** pick one of the base faces for the chosen sex, or **Randomise**. The camera zooms in on the face. The sculpt sliders follow, including Age (section 5).
4. **Eyes and hair:** eye colour, hairstyle from a fixed list, hair colour (natural palette plus a dye slider), and optionally eyebrows and facial hair. Face camera.
5. **Name and review:** first and last name, a full-body review and **Create**. Create sends the record to the API. On success the stepper closes and the new character appears in the guild list.

- **Cameras:** two framings, Body and Face, with a smooth blend between them (for example, CineCamera actors and `SetViewTargetWithBlend`). Lighting in the creator scene is fixed, so colours read the same for everyone.
- **Randomise** on the Face step picks a random base face, small random slider offsets, and a random skin tone, eye and hair combination from the allowed palettes. It uses the same generator as NPCs (section 6.3).
- **Preview loading:** switching base face or hairstyle loads the asset asynchronously and keeps the old one on screen until the new one is ready.

## 4. Appearance data and storage

**Requirement:** a character's full appearance is stored as one compact `appearance` object on the C# `Character` class in the Web API, saved in DynamoDB with the character. Any client must be able to rebuild the character from this object alone. No meshes, textures or local files are stored or required.

- **IDs, not assets:** presets are stored by stable string IDs (for example `face_f_07`, `hair_m_short_02`). The game maps IDs to assets through the data table `DT_AppearancePresets`. An ID is never renamed or reused once shipped, and its asset is frozen (section 2.1).
- **Normalised numbers:** sliders are floats from −1 to 1 or from 0 to 1. Colours are palette IDs or normalised values, never engine structs.
- **Versioned:** `appearanceVersion` lets the client migrate old records when sliders or presets change. Unknown fields are ignored and missing fields use defaults.
- **Validated on the server:** the API checks IDs against the allowed lists for the record's sex and clamps every number to its range, so a modified client can't save broken data. The allowed lists come from the API's configuration, exported from `DT_AppearancePresets` (section 8).
- **Small:** about 1 KB, stored as a nested map attribute on the character item.

| Field | Type | Example |
| --- | --- | --- |
| appearanceVersion | int | 1 |
| sex | string | "female" |
| bodyPresetId | string | "body_f_slim" |
| skinTone | u, v (floats 0–1) | 0.42, 0.30 |
| facePresetId | string | "face_f_07" |
| faceSliders | map of string → float (−1 to 1) | jawWidth: 0.2, cheekboneHeight: −0.3 |
| age | float 0–1 (young adult to elderly) | 0.35 |
| eyeColorId | string | "eye_grey_blue" |
| hairStyleId | string | "hair_f_braid_01" |
| hairColor | melanin, redness, dye (floats 0–1) | 0.7, 0.3, 0 |
| eyebrowStyleId | string | "brow_f_03" |
| facialHairId | string or null | null |

- **Skin tone** is two numbers because MetaHuman skin tone is a 2D (U, V) coordinate. The creator shows about 12 palette swatches plus fine adjustment, and the server clamps U and V to the allowed region.
- **Sex** moves into the appearance object. Today it isn't stored on the character at all.

**Load flow:** when a mystery starts, the dedicated server loads the character from the API, spawns the pawn and replicates the appearance struct. Each client then calls `ApplyAppearance` locally, so every player sees the same face. The creator preview uses the same function, so what is created is exactly what loads.

Changing appearance after creation is out of scope for now (perhaps a barber feature later).

## 5. Runtime implementation

One C++ component, `UVaesenAppearanceComponent`, owns a replicated `FVaesenAppearance` struct that mirrors the stored record. Its `ApplyAppearance()` runs on every client (on rep-notify) and in the creator preview.

- **Dedicated server:** `ApplyAppearance` does nothing on a dedicated server. The server stores and replicates the struct but never loads heads, grooms, textures or materials.

### 5.1 Base face and body

`facePresetId` and `bodyPresetId` select pre-built assets through `DT_AppearancePresets` and are loaded asynchronously. The body parts (section 2.4) follow the torso's pose (leader pose), as today.

- **In place since Phase 0:** given the preset table, the appearance component spawns the face row's MetaHuman Blueprint, attaches it, swaps it when the face changes, and applies the look again once the MetaHuman has settled (its component sets its parts up asynchronously after spawning). Loading is still synchronous.
- **Don't animate the MetaHuman Blueprint directly:** its component replaces single animations on face and body and holds the body still. Its parts must follow the character's animated mesh.

### 5.2 Sculpt sliders

There are 8–12 sliders: jaw width, chin length, cheekbone height and width, nose width and length, brow height, eye spacing, lip fullness. Each runs from −1 to 1. Each slider does two things:

1. **Shape:** a pair of morph targets on the base face's head mesh, `slider_<name>_pos` and `slider_<name>_neg`, on **every face LOD** (each LOD from its matching DNA LOD; without that, sliders vanish at gameplay distance). Generated in the editor (section 8): the base face's MetaHuman is sculpted in each direction through its face landmarks, auto-rigged, and the resulting DNA compared with the base face's. The morph has the slider's joint moves (point 2) already subtracted, so the skin isn't moved twice.
2. **Joints (P0-4 passed, editor and packaged build):** the matching change in facial joint positions, added per character after the face rig. A node in the face's post-process animation Blueprint (`FAnimNode_VaesenFaceJointOffsets`, inserted after the face rig) offsets the joints. This keeps the eyes, teeth and mouth corners aligned with the sculpted skin. Each joint's move is the skin-weighted average move of the skin it drives (the 5.8 DNA's joint data doesn't line up with the mesh skeleton). The face rig drops most face joints below LOD 0, so joints are a close-up refinement and the morphs carry the shape at a distance.

   The DNA itself can't be edited per character: RigLogic reads it from the head's skeletal mesh asset, and every character with the same base face shares that asset. Editing it would change them all.

Ranges stay moderate, because the face rig is tuned to each base face. In Phase 0, jaw width ±0.64 cm and nose width ±0.28 cm looked right through all expressions at full strength. Landmark sculpts are damped by the face model (about 40% of a landmark move arrives), so slider definitions must move landmarks generously; cheekbone height was too subtle at 1 cm.

### 5.3 Age

Age runs from young adult (0) to elderly (1). MetaHuman has no age parameter, so the slider drives custom things together:

- **an age morph per base face** (sagging jowls, hollow cheeks, heavier brow), made like a slider; pushed past weight 1 (`AgeMorphAtFullAge`, 2.5 in Phase 0), because the face model damps the sculpt;
- **a wrinkle floor**: the joint-offset node raises the face rig's wrinkle-map curves (forehead, frown lines, crow's feet, nasolabial folds, lips, neck) to 0.6 × age, so they show permanently and deepen with expressions;
- optional greying: hair melanin shifts toward grey.

**Phase 0 result (P0-6, partial):** both work technically but don't read convincingly as age. **Decision needed** before Phase 2:
- an aged-skin layer of our own (wrinkle normal, roughness and cavity maps blended in the skin material, as first planned); and/or
- base faces in age bands (young, middle-aged, old), with the sliders on top.

### 5.4 Skin tone

MetaHuman synthesises skin textures per character with the tone baked in, and its materials have no runtime skin-tone parameter. So:

- each base face's textures are synthesised at its own tone, stored in its face preset (MetaHuman's bias and gain);
- the **skin material** shared by head and body (`M_skin_unified_baked`) tones its final base colour: colour × `VaesenToneScale` + `VaesenToneOffset` (identity by default), inserted at the material output;
- a **tone table** (`UVaesenSkinToneTable`) holds MetaHuman's bias and gain on a U/V grid, sampled once in the editor. For a target tone, scale = gain′ / gain and offset = (bias′ − bias × scale) / 255 per channel;
- the appearance component sets them on the face's and body's material instances.
- **Phase 0 result (P0-5 passed, offline and at runtime):** a fair face moves naturally to deep tones and a dark one to fair, with head and body matching. The 3-band fallback isn't needed.
- **Still to do:**
  - a skin mask, because the baked underwear is tinted too (mostly hidden by clothing);
  - the eye area, which gets dark rings at medium and deep tones;
  - the material change sits in MetaHuman common content, which a MetaHuman build may restore. Re-apply it, or move it into our own material.

### 5.5 Eyes

Iris colour parameters on the eye material, picked from a palette.

### 5.6 Hair

- A fixed list of grooms per sex. Each groom has **strands** for the creator and close-ups, and **cards and a mesh** generated from the same groom for gameplay. The groom's level-of-detail chain goes from strands to cards to mesh.
- Cards for MetaHuman grooms come from the MetaHuman pipeline. Cards for custom period grooms come from the Hair Card Generator, which is experimental and editor-only, so generated cards are checked in as assets.
- Hair colour sets the groom material's `hairMelanin`, `hairRedness` and dye parameters on every level of detail. Eyebrows and facial hair work the same way.

### 5.7 Mapping

`DT_AppearancePresets` maps each stored ID to its base face, body, groom or palette value, and lists the sliders with their ranges.

## 6. Performance and detail levels

Smooth multiplayer on mid-range PCs is the priority. Visual quality is reduced before frame rate is.

### 6.1 Targets

- **Hardware:** a mid-range PC (for example, RTX 3060 or RX 6600, a 6-core CPU, 16 GB RAM) at 1080p.
- **Scene:** a full party of players plus about 20 visible villagers, at a steady 60 fps.
- **Budgets:**
  - characters' share of GPU frame time: at most 4 ms;
  - animation and face rig on the game thread, including worker threads: at most 2 ms;
  - character texture memory: within the texture streaming pool at the "High" scalability setting.
- **Measured in Phase 0** (packaged, RTX 2080 at 1080p, 24 MetaHumans all running the face ROM, before any optimisation):
  - 69–83 fps;
  - characters +4.0–6.2 ms GPU, met at LOD 3;
  - characters +3.5–4.5 ms game thread, about twice the budget.

  The animation measures in 6.2 are what closes the gap; measure again in Phase 1 with `RunSpikeBenchmark.ps1`.

### 6.2 Detail levels

| Situation | Mesh detail | Hair | Face rig | Textures |
| --- | --- | --- | --- | --- |
| Creator, dialogue close-up | LOD0 | Strands | Full | Full resolution |
| Gameplay, player characters | LOD1–2 | Cards | On within about 10 m of the camera | Streamed |
| NPCs, distant characters | LOD3 and below | Cards, then mesh | Off (body animation only) | Streamed |

- **One asset per base face:** every base face is built once with enough levels of detail for the creator. Gameplay caps the highest level of detail (a minimum LOD), so there's no separate gameplay asset.
- **Groom switching:** a groom switches from strands to cards automatically outside the creator and close-ups. Strands are never rendered for more than one or two characters at once.
- **Animation budget:** use the Animation Budget Allocator and update-rate optimisation, so off-screen and distant characters tick less often. The face rig (RigLogic) and the body's corrective-bone logic only run where the table says.
- **Scalability:** a graphics setting caps the highest MetaHuman detail level for low-end PCs.
- **Not in the plan:** Nanite skinned meshes, until it's verified that they work with morph targets and the face rig.

### 6.3 NPCs

NPCs use the same `FVaesenAppearance` and `ApplyAppearance` as players.

- **Generating an appearance:** the server generates the NPC's appearance with the same randomiser the creator uses (`UVaesenAppearanceLibrary::RandomAppearance`, in place), from a seed (for example, the NPC's spawn ID), and replicates it. The same seed gives the same look on every machine. A designer can instead set a fixed appearance on a named NPC.
- **Detail level:** NPCs never go above the gameplay detail levels.
- **MetaHumanCrowd:** not used for now. It's experimental and built on Mass, which doesn't fit our character actors. It can be reconsidered later for background crowds.

## 7. Multiplayer and acceptance

**Multiplayer:** the creator runs entirely on the client until **Create**. The API is the source of truth. In a mystery, the dedicated server reads the appearance from the API and replicates it. Clients never send appearance during play.

**Acceptance criteria**

- [ ] The modal dialog is replaced by the five-step stepper, with Back and Next on every step.
- [ ] The camera blends between body and face framing on the right steps.
- [ ] Randomise produces a valid, natural-looking face.
- [ ] Create saves the appearance object through the API to DynamoDB, and the server rejects invalid IDs and out-of-range values.
- [ ] Another guild member who loads the character sees exactly the same appearance on a different machine.
- [ ] An old record with a lower `appearanceVersion` still loads.
- [ ] Armour and clothing equip correctly on the MetaHuman body for every body type offered, with no skin poking through.
- [ ] Mannequin animations play correctly on the MetaHuman through retargeting.
- [ ] NPCs get valid random appearances from the same system.
- [ ] The performance target in section 6.1 is met.
- [ ] The dedicated server loads no appearance meshes, grooms or textures.

**Out of scope:** changing appearance after creation, free-form face sculpting beyond the listed sliders, monster customisation.

**Open questions**

- How many players in a full party?
- How many body types per sex, given that each needs fitted clothing?
- Which hairstyles fit the 19th-century setting, and how many at launch?
- Is the class tied to any appearance, such as starting clothes?

## 8. Phases

### Phase 0: spike (pass/fail before Phase 1)

Each test passes or fails. A failure either has the fallback named in this spec, or changes the spec before Phase 1 starts.

| Test | What is verified | Pass when |
| --- | --- | --- |
| P0-1 Engine | The editor and dedicated server build on 5.8.3 with the GameLift Server SDK plugin, Win64 and Linux. | All targets build and the server starts. |
| P0-2 Base faces | Two base faces per sex are created and built by Python script (create, sculpt, skin, groom, auto-rig, build with the Optimized pipeline), then spawned in a test level. | Both faces build, animate and look right. Any manual step is written down. |
| P0-3 Sculpt sliders | Slider morphs generated for one base face, with facial animation playing. | Expressions look right across the chosen slider range. |
| P0-4 Joint offsets | Per-character joint offsets after the face rig (`FAnimNode_VaesenFaceJointOffsets`), with compensated slider morphs, in a cooked (non-editor) build. | Eyes, teeth and mouth stay aligned with sculpted skin. If it fails, slider ranges shrink (section 5.2). |
| P0-5 Skin tone | The custom skin material on head and body across the whole tone range. | Fair to deep tones all look natural and head and body match. Otherwise use the 3-band fallback (section 5.4). |
| P0-6 Age | An age morph plus aged-skin blend on one base face. | 0 to 1 reads as young adult to elderly, with acceptable expressions. |
| P0-7 Hair | One MetaHuman groom and one custom groom: strands in the creator, cards in gameplay, the same colour on both. | Cards are visually acceptable from the gameplay camera and the switch isn't distracting. |
| P0-8 Clothing | One outfit fitted to two body types through Chaos Outfit resizing, worn on the split MetaHuman body. | It fits without skin poking through, and the split body parts hide cleanly. |
| P0-9 Animation | Mannequin locomotion retargeted to the MetaHuman body. | It plays without foot sliding or broken hands. |
| P0-10 Performance | 24 MetaHumans in a test level at the section 6.2 detail levels, with frame time measured on mid-range hardware. | The budgets in section 6.1 are met. |

### Phase 1: systems

- `FVaesenAppearance`, `UVaesenAppearanceComponent` and `ApplyAppearance`.
- `DT_AppearancePresets`, plus an editor check that shipped presets are unchanged.
- The `appearance` object on the C# `Character` class, with server validation and the allowed lists, stored in DynamoDB.
- The five-step stepper with placeholder content.
- The NPC randomiser.

### Phase 2: content

- 15 base faces per sex.
- Slider and age morphs per face.
- Palettes: about 12 skin tone swatches; about 10 eye colours (blues, greys, greens, hazels, browns); 6–8 period hairstyles per sex, plus eyebrows and facial hair; about 12 natural hair colours plus the dye range.
- Fitted clothing per body type.
- Thumbnails.

#### Asset setup notes

- **Faces need a person.** The base faces can be generated by Python script, but picking "beautiful, varied" faces suited to 19th-century Scandinavia (plus some travellers and outsiders) needs a curation pass by a person.
- **Epic login.** Auto-rig, and downloading high-resolution textures, go through Epic's cloud services and need an Epic account login in the editor. Both are required before a MetaHuman can be built: the build needs a rigged face and the high-resolution texture sources, which include the animated wrinkle maps.
- **Thumbnails need a renderer.** The thumbnail commandlet (extending `VaesenCreateWidgetsCommandlet`, or a new one) must run with rendering enabled, not `-nullrhi`, because grooms and skin don't render without it.
- **Exported allowed lists.** A commandlet exports the allowed IDs and slider ranges from `DT_AppearancePresets` to the API's configuration, so client and server validation never drift apart.

## 9. Engine findings (UE 5.8.3 source)

**Python authoring (MetaHumanCharacter plugin, Beta).** `MetaHumanCharacterEditorSubsystem` exposes:

- face model coefficients and landmarks;
- body constraints;
- skin settings (tone U/V, freckles), eye and makeup settings;
- wardrobe items (grooms, outfits);
- `request_auto_rigging` and `build_meta_human`.

Preset blending isn't exposed to Python. Script sculpting goes through face landmarks (`translate_face_landmarks`). The face model coefficient array also holds the model's layout, so it can't be edited blindly.

**Cloud steps.** Auto-rig and high-resolution texture sources go through Epic cloud services and need an Epic login. Building a MetaHuman requires both. Sculpting, body fitting and base texture synthesis run locally. Without a saved login, a script's auto-rig request opens the browser for an Epic account verification code and waits.

**Runtime DNA.**

- DNACalib and RigLogic are runtime modules, so DNA joint neutral positions can be changed in a cooked game and handed to RigLogic with `UDNAAsset::SetDNAReader`.
- But the RigLogic animation node reads its DNA from the skeletal mesh asset (`UDNAAssetUserData` → `UDNA`, or the legacy `UDNAAsset`), so a DNA edit applies to every character sharing that mesh. Per-character joint changes are therefore offsets added after RigLogic in the animation graph.
- Rebuilding mesh geometry, morph targets or the reference skeleton from DNA (`USkelMeshDNAUtils`) is editor-only. Base faces must therefore be pre-built, and slider shapes must be morph targets.

**Quality.**

- Build quality is Low, Medium, High or Cinematic, through the Optimized pipeline.
- Quality sets the face LOD count, groom decimation, texture resolution and the hair LOD transition (strands, then cards, then mesh).

**Skin.**

- Tone is a (U, V) coordinate, synthesised into per-character textures.
- There is no runtime skin-tone parameter.
- Wrinkle detail exists as animated maps driven by the face rig.
- There is no age parameter.

**Plugin status.**

| Plugin | Status | Used for |
| --- | --- | --- |
| MetaHumanCharacter | Beta | Authoring |
| ChaosOutfitAsset | Beta | Outfit resizing (applied at build time for MetaHumans) |
| MeshResizing | Experimental | Outfit resizing |
| HairCardGenerator | Experimental, editor-only | Cards for custom grooms |
| MetaHumanCrowd | Experimental, Mass-based | Not used |

**Body.**

- 18 fixed legacy body types plus a continuous parametric body, driven by measurement constraints.
- The skeleton matches Manny's main bone names but has no IK bones.
