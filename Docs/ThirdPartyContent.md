# Third-party content

Asset packs from Fab are **not in git**. What the project uses of a pack is kept in `Content/Vaesen/ThirdParty/<pack>/…` (committed); the pack itself is ignored (`.gitignore`) and reinstalled from Fab by whoever needs it.

## The packs

| Pack (Fab) | Install into | Used by the project (in `Vaesen/ThirdParty`) | Needed to |
| --- | --- | --- | --- |
| Food Pack 01 | `Content/ThirdParty/Food_Pack_01` | the challah mesh and its materials and textures (the rye bread) | add more food |
| One Hand Sword Attacks And Finishers (RamsterZ) | `Content/1HM_Sword_Attacks_Finishers` | nothing (the sword animations are retargeted copies in `Vaesen/Animations/Sword`) | rerun the sword import (`-run=VaesenAnimSetup -rebuild`), self-test screenshots |
| Shooter Rifle Animations | `Content/RifleAnims` | the UE4 mannequin and rifle (the retargeting source), two fire animations | re-retarget the rifle animations |
| Distance Gun Sounds: WeaponVerse1 | `Content/WeaponVerse1` | the waves, cues and attenuations the gun sounds play | rebuild the gun sounds (`-run=VaesenSoundSetup -rebuild`) |
| Combat Sounds - Bundle Collection (Audios With Love) | `Content/Combat_SFX_Bundle` | the waves the sword and hit sounds play | rebuild the blade and hit sounds |

`Content/Characters` (Epic's mannequins) and `Content/ThirdPerson` (the template's input) stay in git: nearly all of them is used.

A clone without any of the packs builds, plays and passes the self-test and the automation tests.

## Adding a pack, or using more of one

1. Install the pack into its folder under `Content` and add that folder to `.gitignore`.
2. Use it as usual (or let a setup tool build from it).
3. With the editor closed, list what's used of each pack, and move it into `Vaesen/ThirdParty`:

```
UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenContentAudit
UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenContentAudit -move -packs=<PackFolder>
UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenContentAudit -fixrefs
```

`-move` moves what's used (and what that needs inside the pack: materials, textures, waves) and points everything at the new places; one pack per run. `-fixrefs` re-saves anything of ours that still points at a moved asset's old place. Run the audit again afterwards: every ignored pack should show **used: 0**.

The setup tools look for a pack's assets in `Vaesen/ThirdParty` first, then in the pack.

## Note on the repository's history

Ignoring a pack stops it growing the repository from now on. Packs committed earlier (the food pack, about 770 MB, and the two animation packs) are still in the history; removing them from it means rewriting history (`git filter-repo` and a force push), which everyone with a clone has to follow. That's a separate decision.
