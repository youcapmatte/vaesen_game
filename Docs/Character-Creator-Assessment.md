# Character Creator – Assessment of the Requirement Specification

Oct 20, 2026 · Review of "Character Creator – Requirement Specification" (Oct 19, 2026)

## Summary

The design is achievable. The data/API side and the stepper UI are straightforward. Two parts of the technical approach need changing before anyone builds on them: base faces as morph targets, and skin tone as a material parameter. Most of the cost will be content (period hairstyles, and clothing for every body type), not code.

## 1. What's solid

- **Storing appearance as data.** Stable IDs, normalised values, `appearanceVersion` and server-side validation are the right design for guild-owned characters.
  - **Addition:** once a base face ships, freeze the face asset itself, not just its ID. If someone edits `face_f_07` later, every character using it changes.
- **One `ApplyAppearance` for both the preview and gameplay.** That's what guarantees "what you create is what loads".
- **The skeleton choice.** The MetaHuman body and the UE5 mannequin share bone names, so retargeting is mostly about proportions. Low risk.
- **Limits on body types and sliders.** Keeping it to 2–3 body types per sex and small slider ranges keeps the clothing and rig work bounded.

## 2. Biggest risk: base faces as morph targets (section 5)

I don't think this will work as written. A MetaHuman face isn't just head geometry. Each MetaHuman has its own **DNA file** that stores where the facial joints sit in the neutral pose, and RigLogic drives the face from those joints. The eyes, teeth, eyelashes and mouth interior are separate meshes that follow those joints.

If you morph the head from face A to face B, the skin moves but the joints stay where face A had them. You get floating eyes, teeth that don't fit the lips, and a jaw that opens in the wrong place. Small sculpt sliders (about ±10–15%) on top of a correct base face are probably fine. Swapping whole faces through morphs isn't.

**Recommended change:**

- **Base face = its own head mesh plus DNA.** All base faces share one topology, one skin material setup and one set of grooms. Load the chosen head on demand with a soft reference, so only the faces of characters actually in a session are in memory.
- **Sculpt and age sliders = small morph targets on top** of the base face, as already planned.
- `DT_AppearancePresets` maps `facePresetId` to a head mesh plus DNA asset, not to a morph target.
- Turn the "verify early" line into a **Phase 0 spike** with a pass/fail result: two base faces, a few sliders, facial animation on, in UE 5.8.

## 3. Second risk: skin tone and age as material parameters (section 5)

Since UE 5.6, MetaHuman Creator synthesises skin textures per character, with the tone baked into the albedo. It doesn't expose skin tone as a runtime parameter.

**Recommended change:**

- Build our own **skin material**: a shared, fairly neutral albedo plus tone parameters (for example melanin/redness), with the same setup on head and body so they always match.
- This saves memory: all faces share one skin texture set instead of each face having its own.
- The age blend (wrinkle normal maps, roughness, cavity) goes into the same material.
- Add this to the spec as explicit shader work. It's doable but currently unaccounted for.

## 4. Performance (a party of players plus about 20 villagers)

The target is achievable on mid-range hardware if these rules hold:

- **Strand hair only in the creator and in dialogue close-ups.** Gameplay always uses card or mesh hair. Strands for 24 characters is the one thing that would sink the frame rate.
- **Use MetaHuman's optimised quality levels** (Medium for players, Low for NPCs) rather than cinematic assets forced to a lower LOD. They save texture memory too, not just triangles.
- **Turn off RigLogic on distant characters.** Facial rig evaluation costs a lot per character, so only characters near the camera should run it. Combine this with the Animation Budget Allocator and update-rate optimisations already in the spec.
- **No meshes on the dedicated server.** The server stores and replicates the appearance struct but skips `ApplyAppearance`, so it never loads heads, grooms or materials.
- **Nanite skinned meshes** may be worth testing in 5.8, but don't count on them working with morph targets plus RigLogic until it's verified.

"Low quality but still beautiful" mostly comes down to good hair cards, a good skin shader and the creator's lighting. Polygon count matters much less.

## 5. Content cost (the real schedule driver)

- **Period hairstyles.** MetaHuman's built-in grooms are modern. 19th-century braids, buns and updos almost certainly need custom grooms, each with card LODs. That's probably the most expensive item on the list. Start with a small launch set.
- **Clothing × body types × sex.** Each garment needs a fitted variant per body type, unless the 5.8 outfit-fitting tools prove reliable. Test that in Phase 0 as well.
- **Hiding the body under clothing.** Add body masks or hidden body sections under armour and clothing to the spec, to stop skin poking through.

## 6. Section 8 (asset setup automation)

- The Python API can create and blend characters. But "beautiful, varied, northern European" faces need a person to pick them; a script can only give you raw options. Plan for a curation pass by a person.
- Auto-rigging and texture synthesis in MetaHuman Creator go through Epic's cloud services, so expect a manual login step. The spec already allows for this.
- The thumbnail commandlet needs a real renderer. It can't run with `-nullrhi`, and grooms won't render without one.

## 7. Suggested phasing

1. **Phase 0 spike (pass/fail):**
   - two base faces as separate head meshes with DNA;
   - sculpt sliders with facial animation running;
   - the custom skin tone material on head and body;
   - one outfit on two body types, including outfit fitting;
   - 24 MetaHumans on screen at the target LODs, with frame time measured.
2. **Phase 1:**
   - `FVaesenAppearance` and `UVaesenAppearanceComponent`;
   - the `appearance` field on the C# `Character` class with server validation, stored in DynamoDB;
   - the five-step stepper with placeholder content.
3. **Phase 2:** the full content (30 base faces, palettes, grooms, clothing and thumbnails).

## 8. Proposed spec changes, in short

| Section | Change |
| --- | --- |
| 4 | Freeze shipped face assets, not just their IDs. |
| 5 | Base face = separate head mesh plus DNA (shared topology), not a morph target. Morphs only for small sculpt and age sliders. |
| 5 | Skin tone needs a custom skin material shared by head and body; MetaHuman's baked albedo can't be re-tinted at runtime. |
| 6 | Strand hair only in the creator and close-ups. Use optimised Medium/Low assemblies. Disable RigLogic at distance. No appearance meshes on the dedicated server. |
| 2 / 6 | Add body masks or hidden body sections under clothing. |
| 7 | Add a Phase 0 spike with pass/fail criteria before Phase 1. |
| 8 | Faces need a curation pass by a person. Thumbnail rendering needs a commandlet with rendering enabled. |
