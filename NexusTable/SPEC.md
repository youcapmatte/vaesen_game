# Nexus Table — Build Specification

Build spec for Claude Code. Version 1, 22 October 2026. Owner: Mattias Persson.

This document is the single source of truth for building **Nexus Table**, a web-based voice tabletop where Claude is the game master for Vaesen-style mysteries. It consolidates five design documents (Mystery Generation Engine, Class Model, KB & Mystery Architecture, Level & Affordance Model, Nexus Table design sketch). Where those documents disagree, this one wins.

Read sections 1 to 3 before writing any code. Section 17 gives the build order.

---

## 1. What we are building, and why

Nexus Game is a long-term multiplayer Unreal Engine game in which an AI is the game master, the first setting being a Vaesen-inspired 19th-century Scandinavia (around 1870). Its heart is a **mystery generation engine**: an AI that generates coherent, fair, non-trivial mysteries, peoples them with characters who remember the players, and keeps a persistent world evolving across many mysteries.

Building 3D levels is expensive, so the engine is tested first at a narrated table. **Nexus Table** is that test:

- Up to four friends join a browser video call. Claude joins as a fifth, audio-only participant and runs a mystery by voice.
- There are no maps, tokens or movement. Places are described in words; a centre panel shows scene text, handouts and discovered clues.
- Every session produces a detailed log that is fed back to Claude afterwards to diagnose and improve the engine.

**The goal of this build is to test the mystery engine, not to make a polished VTT.** Coherence, fairness, spoiler discipline and logging matter most. Voices, visuals and polish are secondary.

### 1.1 Reuse requirement

The engine (domain model, generation pipeline, validator, runtime GM logic) must later be reused by the Unreal game. Therefore:

- Write the engine as **plain C# class libraries with no web, AWS or LiveKit dependencies**. The web API, storage and voice layers depend on the engine, never the reverse.
- Keep the engine **setting-agnostic and data-driven** (section 4.1). Vaesen-specific content lives in data, not in code.
- Anything that only the 3D game needs (navmesh, behaviour trees, NPCs walking about, GameLift, Unreal) is **out of scope**. Where a design concept has a game-only half, build only the half the table needs (section 18).

### 1.2 Scope of the first playtest

In scope:

- One Society (the player group, also called a guild), up to four players, one host.
- Discord login through Auth0, character creation per the Vaesen rulebook, Society creation.
- World seeding on first play; generating, playing and resolving mysteries across several sessions; persistent cast and memory between mysteries; a running season thread.
- Voice play with push-to-talk, an AI game master and NPCs, server-side dice, a full session log with export.

Out of scope for now: maps, tokens, 3D, Unreal, GameLift, competition metrics and leaderboards, Rosenberger sabotage, multiple Societies in play (the data model must still support many), a social hub, monthly release cadence tooling.

---

## 2. Technology stack

| Concern | Choice |
|---|---|
| Engine and API | C# on .NET (current LTS), ASP.NET Core Web API |
| Web front end | React with Mantine UI, TypeScript. Should feel like a game service, not a business app |
| Hosting | AWS throughout |
| Database | DynamoDB, one table for this project (single-table design), separate from the future game's table |
| Large files | S3 (generated case packets, session logs, exports) |
| Login | Auth0 with the Discord social connection |
| Call and video | LiveKit (WebRTC), LiveKit Cloud to start |
| GM voice agent | A small Python service using LiveKit Agents (the framework runs in Python or Node, not .NET) |
| Language models | Anthropic API directly, pay as you go. Model IDs are configuration, never hardcoded |
| Speech to text | Deepgram streaming, one stream per player track |
| Text to speech | ElevenLabs streaming |
| Dice animation | A 3D dice library such as dice-box (Foundry's Dice So Nice style) |

Azure and Cosmos DB appear in older design documents. **They are dropped.** Do not use them.

A template project defining the preferred solution architecture will be added to the repository. Follow its structure and conventions for the API and React app where it does not conflict with this document.

### 2.1 Reference material in the repository

- `DESIGN.md` (this file).
- The Vaesen core rulebook PDF: the reference for character creation, dice, conditions, pushing and other mechanics. It is a rules reference for this private playtest. **Do not copy rulebook prose into seeded data or prompts verbatim**; creature and world content is generated from public folklore and history (section 5.2).
- The template project.

---

## 3. Architecture

### 3.1 Components

```
 Browser (React + Mantine)
   ├─ LiveKit room: video tiles, mics, push-to-talk
   ├─ Centre panel, chat and roll log, character sheets
   └─ REST + server push (SignalR or SSE) ──────────────┐
                                                        ▼
 Nexus.Api (ASP.NET Core) ── uses ──► Nexus.Engine (pure C#)
   │  login, Society, characters, mysteries,              ├─ Domain model
   │  dice, logs, LiveKit tokens, GM turn endpoint        ├─ Generation pipeline + validator
   │                                                      ├─ Runtime GM orchestration
   ├─ Nexus.Infrastructure: DynamoDB, S3, Anthropic       └─ Prompt builders (bounded contexts)
   │                                                     
   ▲  streaming GM turn (HTTP/2 or gRPC stream)          
   │                                                     
 GM agent (Python, LiveKit Agents) — joins the room as audio-only participant
   ├─ subscribes to player tracks; Deepgram STT per track
   ├─ forwards push-to-talk utterances (with speaker) to Nexus.Api
   └─ receives speaker-tagged reply segments, speaks them through ElevenLabs
```

### 3.2 Decision: the GM brain lives in .NET, the Python agent is thin

The Nexus Table sketch put the whole speech-to-Claude-to-speech loop in the Python agent. This spec moves the **GM logic into the .NET engine**: routing, context building, the spoiler guard, tension, beats, failed-roll rerouting, state updates and logging. The Python agent only handles audio: it captures push-to-talk audio, transcribes it, sends the text and speaker to the API, and speaks the returned segments.

Reasons: the engine must be reusable by the game, spoiler discipline must be enforced in one place, and state and logging must not be split across two languages. The extra network hop costs tens of milliseconds and is acceptable if both sides stream.

If latency measurement in milestone 1 shows the hop matters, revisit; do not move engine logic into Python without asking the owner.

### 3.3 Solution layout (suggested; the template project takes precedence)

```
src/
  Nexus.Engine/            pure C#: domain, generation, validation, runtime GM, prompts
  Nexus.Engine.Tests/
  Nexus.Content/           setting data loaders, JSON schemas, seed content for Vaesen
  Nexus.Infrastructure/    DynamoDB repositories, S3, Anthropic client, Deepgram/ElevenLabs config
  Nexus.Api/               ASP.NET Core Web API
  Nexus.Web/               React + Mantine
  Nexus.GmAgent/           Python LiveKit Agents worker
tools/
  Nexus.ContentTool/       CLI to generate and review setting content (section 5)
  Nexus.CaseLab/           CLI to generate cases offline, run the validator, dump case packets
```

`Nexus.CaseLab` matters: it lets the owner generate and inspect many mysteries without starting a voice session. Build it early.

---

## 4. Core principles the code must uphold

These are non-negotiable. Each one is backed by a validator check or a structural guarantee, not by prompt wording alone.

### 4.1 Setting-agnostic and data-driven

No class hierarchy of creatures (no `Nacken : Vaesen`). Creatures, conflicts, places, affordances, hook channels and lore are **data**. A steampunk setting later means new data, not new classes. Where the Vaesen setting needs something special, express it as a field in data.

### 4.2 Shared canon versus per-Society state

- **Canon** is authored once and read by everyone: the setting, its lore, Vaesen definitions, place templates, conflict archetypes, the canonical character roster.
- **Per-Society state** is what one group has done to its own copy of the world. It is partitioned by `societyId`.
- **Canonical characters are templates.** A canonical character (for example an Uppsala police officer or a city innkeeper) exists for every Society, but each Society gets its **own independent instance**. Nothing about one Society's instance (attitude, knowledge, relationships, importance, death) affects another's.
- A character instance's personality, drives, voice and background are fixed once created, so the character stays recognisable. Status, importance, relationships and knowledge change.

### 4.3 Distribute truth, do not script paths

The engine generates a **situation**, not a route. Each load-bearing fact is placed into the world through **at least two independent channels** (a character who knows it, a trace at a place, a document, an overheard remark). There are no quest flags and no required order. A red-herring suspect is cleared by a fact that is true from every angle, not by an event the players must trigger.

### 4.4 Information boundaries are structural

No speaking voice is ever given information it is not entitled to:

- An NPC reply is generated from **that character's records only**: knows, remembers, thinks, wants. Never the full case.
- Narration, recaps and "where were we" summaries are generated from the **discovered-facts ledger only**.
- Only generation, the validator and the orchestrator (beat scheduler, tension) see the whole case. Their output to players always passes through the bounded paths above.

### 4.5 Fair play

Misdirection is built from true facts that license a wrong inference, never from false evidence. Characters may lie or be mistaken; the world's physical evidence never lies. The true culprit always has at least one honest, reachable clue from early in the case.

### 4.6 Failed rolls never strand players

A failed roll changes how and when players learn a fact, never whether they can (section 11.9).

### 4.7 Generation order is enforced in code

Level, then Vaesen, then conflict, then escalation, then humans, then clues. The pipeline is a typed sequence of steps; a later step cannot run before an earlier one has produced validated output.

### 4.8 Structural checks in code, judgement checks by model

Reachability, redundancy, order-independence and fact provenance are **deterministic graph checks in C#**. Only judgements the graph cannot make (is a clue fair, is a red herring honest, does a confrontation land) go to a language-model critic.

---

## 5. Setting content

The engine is generic; the Vaesen setting is a content pack. Content is **AI-generated into fixed schemas**, stored, reviewed where noted, and versioned. The model is expected to draw on its own knowledge of Scandinavian folklore and 1870s Scandinavian history and daily life; the schemas give that knowledge a structure the engine and validator can use.

### 5.1 The setting record

`Setting`: id, name, era (Scandinavia around 1870), and a short list of **world truths** nothing lower may contradict. Initial world truths for Vaesen:

- Modernisation (railways, industry, mills, mines, science) is pushing against the old ways.
- Vaesen have retreated to rural margins as belief has faded; even rural folk only half-believe.
- The recurring engine of conflict is human activity intruding on, or bargaining with, the old world.
- The Society is a group of people with the Sight who investigate such matters, based at a headquarters near Uppsala.

Plus setting-level lists the engine draws from: hook channels (section 7.6), institutions (police, parish, physician, press, landowner, mine or mill company), and period conventions (travel by rail, coach, horse trolley, boat; telegraph and post; money; social ranks).

### 5.2 Vaesen definitions

A `VaesenDefinition` is a data record, generated by the content tool from folklore and stored in the catalogue. The model may also generate **variants and lesser-known beings** that resemble folklore figures without being in the rulebook (for example a creature resembling the myling, the dead-child spirit). Each record must include:

| Field | Meaning |
|---|---|
| `id`, `name`, `family` | Family lets related creatures share lore (for example forest, lake and mine caretaker spirits) |
| `requiredAffordances` | What a level must offer for this creature to appear (water, forest, farm, cave, mine) |
| `domain` | What it is bound to or protects |
| `reasonModel` | Structured: **nature** (disposition before anything happens), **trigger** (what tips it into acting against people, if anything), **expression** (the shape its harm takes), **resolution** (how or whether it can be answered) |
| `reasonKinds` | One or more of: provoked only, self-originating grievance, domain violated, bargain invited, affliction with no fault, none. The engine picks per case |
| `resolutionShape` | Negotiable (has a grievance that can be answered) or not negotiable at the creature (depth moves to a hidden hand, if any) |
| `originSet` | For afflictions: a varied set of possible origins, each stating whether there is a hidden hand and whether it is hostile, desperate or lost control |
| `commonKnowledge` | What everyone assumes. Used as misdirection, never as the solution trail |
| `deepLore` | Lesser-known truths. The solution trail is built from these. Should be rich and varied so cases diverge |
| `giveawayTells` | Signs so iconic they identify the creature on sight, including atmospheric ones (looping paths that will not let you leave the woods are a skogsrå tell) |
| `ambiguousSigns` | Signs that fit this creature **and several others**, safe for openings |
| `confrontationConditions` | None, time-gated (for example only at nightfall or when it transforms), place-gated (for example at its farm or water), or both |
| `weaknesses`, `protections` | Folklore-grounded |
| `compatibleConflictTags` | Which conflict archetypes it fits |

The catalogue starts small: **four creatures** generated and reviewed by the owner before the first session. Recommended starting set: the Näck (water, negotiable), the werewolf (affliction with origins, not negotiable at the creature), the nisse (farm-bound, place-gated), and the bäckahäst or the skogsrå. The catalogue then grows by generation. The engine may also generate a one-off creature for a case, which must pass the same schema and validator before use.

### 5.3 Place templates and text levels

From the Level & Affordance Model. A level is structured, nested data the engine reasons over:

- **Level** contains **places** (a town, the forest around it, outlying farms, a ruin, a logging camp, a mine).
- **Place** contains **interior affordances** (a library has a desk, shelves, books, reading tables, perhaps a lockable archive); some affordances contain more (a desk has a lockable drawer).
- **Place templates** (library, church, inn, farm, smithy, mill, mine, logging camp, herbalist's cottage, croft, railway station, manor) are authored once. Each declares its interior affordances, each with a **condition** against parameters such as settlement size and wealth: a threshold to appear, a scaling description, or a ranged count.
- Affordance **tags are one shared vocabulary**. The mystery engine queries the same tags templates declare. Keep the vocabulary in one file and validate against it.

For the table, **levels are generated as text**: the generator assembles a `LevelDefinition` by choosing a settlement type and size, instantiating templates under their conditions, and writing a short sensory description per place. Each level is tagged with its month of the year (seasonality is baked in at creation). A small library of 6 to 10 text levels is generated and reviewed up front; more can be generated per case later. The same data model will later drive the 3D levels.

### 5.4 The canonical character roster

A one-time set of canonical characters that every Society instantiates: Uppsala and Society-adjacent figures (for example a police officer, an innkeeper, a university scholar, a newspaper editor, a servant at the Society's headquarters). The content tool drafts them; the **owner approves them once** in a review screen, because every Society meets them. Each has the full character record (section 6.4).

### 5.5 Conflict archetypes

`ConflictArchetype`: reusable shapes of dispute (an industry intrudes on a domain, a bargain comes due, an unjust death, a curse laid in anger). Each defines the human role slots it creates and the affordances its provocation needs. Generated and stored like other content.

### 5.6 Content tool

`Nexus.ContentTool` generates content into the schemas, validates it (schema, vocabulary, internal consistency), writes it as JSON for review, and loads approved content into DynamoDB. Every content record carries a version. Never overwrite approved content in place; publish a new version.

---

## 6. Domain model

Names follow the Class Model. Group by where state lives. Use records or classes as fits; keep the engine free of storage attributes (map to DynamoDB in Infrastructure).

### 6.1 Canon (shared, versioned, cached)

| Class | Purpose |
|---|---|
| `Setting` | World truths, setting-level lists (section 5.1) |
| `LoreEntry` | One canonical fact in the world layer |
| `Region`, `Town` | Geography. A town has size, wealth and character |
| `PlaceTemplate` | Template with conditioned interior affordances |
| `AffordanceTag` | Entry in the shared vocabulary |
| `LevelDefinition` | Assembled level: places instantiated from templates, month tag, description |
| `VaesenDefinition` | Section 5.2 |
| `ConflictArchetype` | Section 5.5 |
| `CharacterDefinition` | Canonical identity: `PersonalityProfile` (Big Five style), list of `Drive`, `Voice`, `Background` |
| `SeasonSeed` | Optional authored seeds for season conflicts: motifs, kinds of antagonist force, thread types |

### 6.2 Per-Society state (partitioned by `societyId`)

| Class | Purpose |
|---|---|
| `Society` | The group: name, headquarters, members, roster of investigators, reputation, mystery history, cooldown history (Vaesen, hook channel, hook-giver type) |
| `Investigator` | A roster character, built per the Vaesen rulebook (section 13.3). Any member may play any roster character on a given mystery |
| `CharacterInstance` | One Society's copy of a canonical character, or a character generated for that Society. Status (alive, dead, departed, absent), importance, `CastTier` (core, background, location resident), home location, `LatentHook` list, `NetworkLink` list |
| `LatentHook` | A habit, debt, connection or secret the engine can promote into a plot |
| `NetworkLink` | Reach to another character or institution, possibly in another town, with what influence it can exert |
| `InstitutionState` | An institution's stance toward the Society in a place, constraining what players may do there (for example the police refusing interviews at the inn) |
| `CharacterRelationship` | A link from a character to another character or to the Society: nature of the tie, sentiment, shared history as `MemoryEntry` list. NPC-to-NPC ties live with the instance; ties to the Society are per-Society |
| `MemoryEntry` | One remembered line of what happened |
| `KnowledgeEntry` | A fact a character knows, with source (witnessed, told by whom, found) and when |
| `BeliefEntry` | What a character thinks or suspects, possibly wrong, distinct from knowledge |
| `WorldStateOverlay` | Consequences true only for this Society (a sawmill destroyed). Canon plus overlay equals this Society's world |
| `MysteryInstance` | One generated case (section 8) plus its live state |
| `CastMember` | Binds a character to a `PlotRole` in one mystery, with secrets and reaction curve. One-off characters live here; persistent ones reference a `CharacterInstance` |
| `DiscoveredFactLedger` | Per mystery and per season: every fact the players have uncovered, when, and through which channel |
| `SeasonState` | The current season (section 10) |

### 6.3 Generation and runtime classes

| Class | Purpose |
|---|---|
| `AssignmentBrief` | Output of the assignment stage |
| `MysteryDraft` | Work-in-progress case flowing through the generation steps |
| `IMysteryGenerationStep` | One pipeline stage; strict order enforced by types |
| `ICoherenceValidator` | Returns `CoherenceIssue` list; a step re-rolls until clean or a retry limit is hit |
| `CoherenceIssue` | Code, severity, location, explanation, suggested fix |
| `FactGraph` | Facts as nodes, channels as edges with access conditions; used for structural checks |
| `CharacterContextBuilder` | Assembles one NPC's bounded context from its own records only. **The spoiler guard** |
| `NarrationContextBuilder` | Assembles narration context from the discovered-facts ledger, the current scene and public world facts only |
| `TensionState` | The tension dial and per-character exposure beliefs |
| `BeatScheduler` | Fires authored beats at tension thresholds, honouring confrontation conditions |
| `IntentSelector` | Picks a significant character's off-screen action from its unlocked desperation tier |
| `CharacterAction` | A drive-permitted action with tier, prerequisites and effects |
| `ChannelRouter` | Chooses which channel to push toward players after a missed roll (section 11.9) |
| `RuntimeGuard` | Checks risky replies against the speaker's records before they are spoken |
| `WorldSeedingStep` | Runs once per Society on first play |
| `CastPromotion` | At mystery close, promotes cast members into persistent instances |

### 6.4 The character record

Every significant or persistent character carries, as separate structures:

- **Identity:** name, age, occupation, rank, home, appearance, `PersonalityProfile`, `Voice` (how they speak), `Background`.
- **Knows:** `KnowledgeEntry` list.
- **Remembers:** `MemoryEntry` list with the Society and other characters, including past mysteries.
- **Thinks:** `BeliefEntry` list.
- **Wants:** `Drive` list, fears, guilt, belief about own exposure, current desperation tier, permitted `CharacterAction` set.
- **Ties:** relationships and network links.

Keep the four mind records (knows, remembers, thinks, wants) as separate collections. Prompts are built by selecting from them, never by dumping the whole character.

---

## 7. Mystery generation

Generation runs when the host presses Start for a new mystery, or offline from CaseLab. It uses the large model for creative steps and C# for selection and checks. It produces a **case packet** (section 8) stored in S3, with indexes in DynamoDB.

### 7.1 Assignment stage (deterministic C#, no model)

Inputs: the Society, its history, the current in-world month, the catalogues.

1. Read the Society's history: which Vaesen it has faced, recent hook channels and hook-giver types.
2. Filter levels to those tagged for the current month.
3. Pick a Vaesen the Society has not had recently, ideally never. Enforce a cooldown.
4. Check the Vaesen's required affordances against the level.
5. On failure, re-roll the Vaesen; re-roll the level only if no eligible Vaesen fits.
6. Emit the `AssignmentBrief`: Society, level, Vaesen, month, plus the season's current state and threads due (section 10).

### 7.2 Plot centre and focus

Before creative generation, decide the case's shape:

- Pick a **reason kind** from the creature's `reasonKinds`, and for afflictions an **origin** from its `originSet`.
- **Hidden hand present** (a person caused or controls it): the plot centres on that person; the creature is their weapon or symptom.
- **No hidden hand**: the plot centres on the provocation or the creature's own nature.
- Set the **focus**: *what* (identifying the creature), *who and why* (creature obvious, involvement and cause hidden), or *how* (everything understood, the resolution is the dilemma).
- Spend difficulty on the focus axis only. With no hidden hand, identity is the whole answer, so concealment must be hard. With a hidden hand, identifying the creature can come early; difficulty goes into the hand and the why.

### 7.3 Generation steps (strict order)

1. **Level** (from the brief).
2. **Vaesen** (from the brief).
3. **Conflict.** Read the reason model and build the matching skeleton. The provocation must physically exist in the level's affordances (no forest, no logging). For a hidden hand, fix that person's motive here. If nothing plausible fits, signal a re-roll. The obvious folklore reading of why a creature turned may be used as a decoy, with the real cause built from deep lore.
4. **Escalation.** What the creature (or hand) has concretely done: these become the opening events and the symptoms players meet.
5. **Humans.** Cast people entangled in the conflict, deriving role and moral colour from the conflict, deliberately varied (not a default villain). Draw on the persistent cast first (section 9.3), then promote background characters, then create new ones. Cast to embody deep-lore motifs (the Näck's violin motif yields a fiddler who became a master overnight). Populate the location's ordinary residents.
6. **Clues.** Distribute every load-bearing fact across channels, place red herrings and decoy threads, author beats and climax conditions, write the hook.

Each step's output is validated before the next runs.

### 7.4 The opening and giveaway tells

- The opening must be **supernatural enough to summon the Society** and **ambiguous as to which creature**. Achieve both with a **convergence of several peculiar events**, each deniable alone, inexplicable together, spread across different registers.
- **When identity is the focus,** suppress giveaway tells in the opening and among load-bearing clues; they surface only late as confirmation. Openings use `ambiguousSigns` only. A sign that is one creature's calling card is a giveaway even if it seems atmospheric.
- **When identity is not the focus,** giveaways may appear from the start.
- **Wording matters.** Describe opening events in the plainest terms their mundane reading allows ("went out on an errand and did not return", not "walked into the trees and never came back").
- An identity-focus case uses many quiet oddities rather than one loud event.
- The **true conflict is never a clue.** Players meet symptoms early; the cause is reconstructed late.

### 7.5 Lore registers

Build the solution trail from `deepLore`; treat `commonKnowledge` as misdirection. Include at least one knowledge-gated step: a sign that only resolves once the group understands the creature or finds someone who still knows the old ways. Good clues are indirect.

### 7.6 The hook

- **Channel**, chosen from the setting's list to fit the scale of the trouble: a letter through the Society's contact network (the default, common but never the only route), a newspaper item, an official request, a telegram, an in-person plea, a visit from an old-world being.
- **Hook-giver** is a cast member with their own motive, possibly loosely connected or self-interested. Their account is testimony, not truth: it may lean toward a decoy, but must not fabricate the peculiar events.
- **Techniques** to make it feel wrong: juxtaposition of deniable events, a detail that does not fit, a strained official explanation, the teller's own unease, stakes and urgency.
- It carries **enough convergence to justify the journey and no more.** The arrival adds fresh peculiarities.
- Apply cooldowns on channel and hook-giver type per Society.
- Everything the hook states is clue content and goes through the same checks.

### 7.7 Misdirection

- A red herring may be a **whole false case**: suspects, clues and a climax the players can genuinely solve.
- A case may nest a **decoy Vaesen** with its own full arc.
- Misdirection uses **true facts** that license a wrong inference. No false world evidence.
- The **true culprit has a fair-play clue** reachable early and not gated behind resolving the decoy.
- Counter-clues are a tool, not a rule. Use them where identity is the puzzle; overuse becomes a tell.
- Red-herring density is a tunable setting.

### 7.8 Resolution

- Several **resolution levers** once the truth is understood (tear down the mill, drive the creature off, strike a bargain), each with different consequences for the world and the cast.
- If a hidden hand exists: a **crude solve** is always available (kill the creature), and a **deep solve** (expose the hand, lift the curse, save the victim) is discoverable and recorded as the better outcome. No leaderboard in this build, but record which solve was reached.

### 7.9 Confrontation conditions

If the creature has time or place conditions, the climactic beat is held until tension is high **and** the players bring the confrontation to the right place or hour. Pressing an accusation too early yields denial, an alibi or doubters, not a cheap ending.

### 7.10 Validator

Run after each step and on the final case. Structural checks in C#:

- **Reachability:** from the players' opening knowledge, every load-bearing fact is reachable by walking the fact graph, respecting access conditions, and the final truth is assemblable.
- **Redundancy:** every load-bearing fact has at least two independent channels.
- **Order independence:** traversal from several entry points (testimony-first, scene-first, suspect-first) completes.
- **Provenance:** every `KnowledgeEntry` in every mind has a plausible source.
- **Decoys:** every decoy thread is resolvable; the true culprit's fair-play clue is reachable without resolving the decoy.
- **Affordance fit:** creature and provocation fit the level; every clue location and container exists in the level.
- **Vocabulary:** every tag used exists in the shared vocabulary.
- **Cast coherence:** drawn-in persistent characters behave consistently with their recorded attitude and history; dead characters do not appear.

Judgement checks by a model critic (large model, given the full case):

- The solution trail rests on deep lore, not the obvious answer.
- The case is non-trivial along its focus.
- At least one knowledge-gated step exists.
- Humans follow from the conflict with varied moral colour.
- Difficulty is spent on the focus axis; no giveaway in the opening or load-bearing clues when identity is the focus.
- Hook check: convergence threshold met, neutral wording, no giveaway when identity is in focus, true conflict not stated, plausible channel, not repeating recent channel and hook-giver type.
- Clues are fairly discoverable; red herrings are honest.
- Hidden hand cases have a crude and a deep solve, the deep one discoverable.

A failing step re-rolls with the issues fed back, up to a configurable limit; then the whole case re-rolls; then generation fails visibly with the issues logged. Every validator run is logged.

---

## 8. The case packet

The output of generation, stored as one JSON document in S3, referenced from the `MysteryInstance`. It is the hidden GM file. Define a C# model and a JSON schema; validate on write and read. Fields:

```jsonc
{
  "schemaVersion": 1,
  "mysteryId": "…", "societyId": "…", "seasonId": "…",
  "generatedAt": "…", "models": { "creative": "…", "critic": "…" },
  "brief": { "levelId": "…", "vaesenIds": ["…"], "month": 10 },

  "truth": {
    "summary": "the whole hidden truth in prose, for the engine only",
    "reasonKind": "…", "origin": "…|null",
    "plotCentre": { "kind": "hiddenHand|provocation|nature", "characterId": "…|null" },
    "focus": "what|whoWhy|how",
    "provocation": "…", "escalation": ["…"]
  },

  "hook": {
    "channel": "…", "hookGiverId": "…", "text": "…",
    "assertedFactIds": ["…"], "openingEventIds": ["…"]
  },

  "level": { "levelId": "…", "placesUsed": ["…"], "sceneDescriptions": { "placeId": "…" } },

  "facts": [
    { "id": "f1", "statement": "…", "loadBearing": true,
      "kind": "symptom|cause|identity|alibi|motive|resolution|season",
      "pointsTo": "trueAnswer|decoy:<threadId>|neutral",
      "giveaway": false, "deepLore": true }
  ],

  "channels": [
    { "id": "c1", "factId": "f1",
      "kind": "testimony|trace|document|overheard|knowledgeGated|event",
      "sourceCharacterId": "…|null", "placeId": "…|null", "affordanceId": "…|null",
      "access": { "requires": ["trust>=2", "knowsFact:f7", "roll:investigation", "time:night"] },
      "reveal": "what the players perceive through this channel" }
  ],

  "cast": [
    { "characterId": "…", "instanceRef": "persistent|new|promoted",
      "plotRole": "…", "tier": "significant|resident",
      "knows": [ { "factId": "f1", "source": "witnessed", "when": "…" } ],
      "thinks": [ { "belief": "…", "true": false } ],
      "wants": { "drives": ["…"], "guilt": "…", "exposureBelief": 0, "permittedActions": ["…"] },
      "secrets": ["…"], "lies": [ { "about": "f3", "story": "…" } ],
      "routine": "…", "voiceNotes": "…" }
  ],

  "decoys": [ { "threadId": "d1", "vaesenId": "…|null", "factIds": ["…"], "resolution": "…" } ],

  "beats": [ { "id": "b1", "threshold": 0.4, "description": "…",
               "requires": { "place": "…|null", "time": "…|null" }, "emitsFactIds": ["…"] } ],

  "climax": { "threshold": 0.85, "conditions": { "place": "…|null", "time": "…|null" } },

  "resolutionLevers": [ { "id": "r1", "description": "…", "solveDepth": "crude|deep",
                          "consequences": { "world": ["…"], "attitudes": [ { "characterId": "…", "change": "…" } ] } } ],

  "seasonThreads": [ { "threadId": "…", "factIds": ["…"] } ],

  "tension": { "start": 0.1, "factWeights": { "f1": 0.05 } },

  "validation": { "passed": true, "issues": [], "runs": 3 }
}
```

Facts and channels are the backbone: the fact graph is built from them, NPC knowledge references them, the ledger records them as discovered, and logs cite them.

---

## 9. The persistent world and cast

### 9.1 World seeding (once per Society)

On the Society's first play:

1. Instantiate every canonical character into a `CharacterInstance` for this Society.
2. Generate the Society's own **core cast** (around a dozen, important from the start, with drives, relationships and an attitude toward the Society) and a larger **background cast** (named people with no story yet, each with a few latent hooks).
3. Build the network links and NPC-to-NPC relationships.
4. Validate (no contradictions with world truths, every canonical character present, vocabulary).

### 9.2 What persists per character

Attitude toward the Society and why; network; importance (can rise, fall, end in death or departure); knowledge, beliefs and memories; status.

### 9.3 Mysteries read from the cast

At generation, review the persistent cast and decide who appears, who acts off-screen, who is promoted from the background and who stays quiet. A drawn-in character must stay coherent with their history.

**Influence at a distance:** a character can use their network to affect a mystery they are not in, for example a hostile Uppsala official getting the Society's interview station at a country inn forbidden three mysteries later, or a grateful one opening doors. Express this through `InstitutionState` and network links, applied at generation.

### 9.4 Location residents

Each mystery location gets ordinary residents unconnected to the case (baker, priest, stable hand, inn gossips) with routines, opinions and latent hooks. They provide colour and occasional stray facts. They stay attached to their location and can be met again.

### 9.5 Mysteries write back

At mystery close, the chosen resolution lever sets attitudes. `CastPromotion` promotes characters whose fate warrants it (a debt owed, a grudge earned, a secret shared) into the persistent cast. Consequences go into the `WorldStateOverlay`. Deaths and departures are recorded.

---

## 10. The season

- A season carries an **overarching conflict** beneath its mysteries that intensifies as they come and go.
- **Season length is not fixed.** Nothing may assume a number of mysteries. A season ends when its own conditions are met.
- At season start, generate a **hidden season truth** (a force, a scheme, a deeper wrongness) seeded from the Society's world, cast and history, distributed across people, places and events, not a chapter list.
- **Each mystery stands alone** and carries one or more season threads. Thread density is tunable.
- **Season tension** rises with uncovered threads and with time. The season's forces act off-screen between mysteries, so later cases arrive under heavier pressure.
- It **adapts** to the Society's choices and alliances.
- A **finale** case is generated when enough is uncovered and pressure peaks; its outcome writes into the world and seeds the next season.
- Season facts follow the same rules as case facts: distribution, fair play, information boundaries, validator checks, and never spoiled by a case not meant to reveal them.

For the first playtest: one season, one thread attached to a standalone mystery.

---

## 11. The runtime game master

### 11.1 The turn

1. A player holds push-to-talk and speaks. The agent transcribes and sends `{ sessionId, speakerAccountId, characterId, text, timestamp }`.
2. **Routing** (fast model): classify as in-character speech to a named NPC, a question to the GM ("does he look angry?"), a rules question, an action or investigation, or out-of-character talk to ignore.
3. Build the bounded context for the responder (section 11.3).
4. Generate the reply as an **ordered list of speaker-tagged segments**: `narrator` or a specific NPC id. Speech to an NPC gets an immediate in-character reply with no GM preamble. Narration is added only when something needs explaining that a video game would show.
5. The runtime guard checks risky segments (section 11.6).
6. Stream segments to the agent (spoken) and the browser (shown as text).
7. Apply state updates: new knowledge entries, ledger entries, attitude changes, tension, beats. Larger state updates may run in the background on the large model.
8. Log everything (section 15).

### 11.2 Models

- Routing and NPC dialogue: the fast model (Haiku class). Small, focused prompts.
- Generation, narration of scene changes, recaps, background state updates, the critic: the large model (Sonnet or Opus class).
- Model IDs, temperatures and token limits are configuration.

### 11.3 Bounded contexts

- **NPC context** (`CharacterContextBuilder`): world truths; the town's public facts; the NPC's identity, voice, knows, remembers, thinks and wants; their relationship with each present investigator and the Society; what has happened in this conversation; current desperation tier and permitted behaviours; channels they hold and the conditions for revealing each. **Never** the case truth, other characters' secrets, or the ledger beyond what the NPC witnessed.
- **Narration context** (`NarrationContextBuilder`): the current scene and place description, public world facts, the discovered-facts ledger, present characters' visible state, roll results. **Never** the case truth.
- **Orchestrator context**: the whole case packet. The orchestrator decides beats, tension, channel pushing and state updates. Its decisions reach players only by instructing the bounded generators (for example "the maid approaches the investigators wanting to talk about what she saw at the mill"), never by writing player-facing prose from the full case.

### 11.4 Knowledge grows only through events

A character learns a fact only through a recorded in-world event: they witnessed it, were told by someone (including the investigators), or found evidence. Gossip spreads along recorded links with the same tagging. Everything an investigator tells an NPC becomes a knowledge entry for that NPC.

### 11.5 Discovered-facts ledger

Whenever players perceive a channel's reveal, record the fact, time and channel. The centre panel's clue list and every recap are built from the ledger.

### 11.6 Runtime guard

Before speaking, a segment that touches load-bearing facts, secrets or the case's key names is checked against the speaker's records. A segment that reveals something the speaker does not know is regenerated or trimmed. Every intervention is logged. Keep this fast; skip it for segments that cannot touch case material.

### 11.7 Tension and beats

- The tension dial rises with progress: facts uncovered (weighted per fact), characters cracked, alibis broken.
- **Desperation tiers** (low: lie, deflect; medium: hide or destroy evidence, intimidate, plead; high: flee, confess, help the group, or at the extreme silence someone) unlock with tension and the character's belief about their exposure. Drives choose within the tier; a character can only take actions its drives permit.
- **Who escalates:** exposure pressure applies to whoever has something to hide (the afflicted if the creature is the centre, the caster if a hidden hand is). The creature escalates by its own nature or its controller's direction.
- **Beats** fire at thresholds, honouring place and time requirements. Drastic acts must be motivated and plausible given relationships; the critic checks them; their consequences persist.
- Starting values (dial start, fact weights, tier thresholds, beat spacing) are configuration with sensible defaults, tuned from session logs.

### 11.8 Dice

From the Vaesen rules; the rulebook PDF is authoritative for details.

- The GM calls for a roll when an action is uncertain and the outcome matters. The app builds the pool from the character sheet: attribute plus skill, adjusted by difficulty, gear, talents and conditions. Each six is a success.
- Players roll in the UI with animated 3D dice visible to all, but **results are generated on the server**, then animated, sent to the GM and logged.
- Pushing follows the rules, including its cost in conditions.
- The open question of which rules are enforced strictly and which narrated loosely is listed in section 19; default to enforcing rolls, pushing and conditions, and narrating the rest.

### 11.9 Failed rolls never strand the players

Players rarely return to search a place again after finding nothing; to them a failed search means nothing was there.

1. **Pushing first.** The player may push per the rules.
2. **Register the miss.** If a roll that would reveal a load-bearing fact fails, record that this channel was missed. The fact remains unrevealed.
3. **Push another channel toward the players.** The `ChannelRouter` picks a planned channel for the same fact that the players can reach soon and the orchestrator brings it to them rather than waiting: the maid seeks them out, the letter turns up, the remark is made within earshot.
4. **Only if none fits, open a new channel,** consistent with who knows the fact and how they learned it, checked by the guard against the case and the information boundaries before use.
5. **The failure still costs something:** time, a warned suspect, raised tension, a witness who wants something in return.
6. Log each miss and the channel used instead.

### 11.10 NPCs off-screen at the table

Without a 3D world, there is no walking or navmesh. Significant characters still act off-screen: when tension crosses a tier, the `IntentSelector` picks an action from the unlocked tier, applies its effects to world state, and the orchestrator lets the consequences surface (evidence gone, a witness who left town, a second victim). Ambient NPC-to-NPC conversations are out of scope for the table except as narration the GM may use for colour.

---

## 12. Voice and the call

- LiveKit room with video for up to four players. The GM agent joins as an audio-only participant.
- Players always hear each other. **Table talk is never sent to the AI.**
- Holding a **configurable push-to-talk key** (default: a mouse side button or backtick) routes that audio to the GM; an optional toggle mode exists. The key works only while the tab has focus.
- Deepgram streaming STT per player track, so every utterance carries the speaker.
- ElevenLabs streaming TTS. Start with one narrator voice and one shared NPC voice; because segments are speaker-tagged, distinct NPC voices later are a small change.
- **Latency target: about 1 second** from releasing push-to-talk to first audio. Tactics: streaming end to end, TTS starts on the first complete sentence, prompt caching for stable context (world truths, case-independent instructions, the NPC's identity), extended thinking off for dialogue, compact contexts.
- Recording table talk for the log requires consent (section 16).

---

## 13. Web application

### 13.1 Layout (Roll20-inspired, no map)

- Camera tiles for the four players along one side, with mic and camera status, and missing players shown as missing.
- A large centre panel for the current scene text, handouts and the discovered clues (from the ledger).
- A chat and roll log showing every GM and NPC line as text, with speaker.
- An indicator of who is currently talking to the GM.
- Character sheets one click away.
- A one-click **flag** button during play ("that made no sense") that marks the moment in the log.

### 13.2 Screens

- Login (Discord through Auth0).
- Society: create or join, roster, headquarters, mystery history.
- Character creation and sheet.
- Table (the call).
- Host: settings (API key status, model configuration), content review (approve canonical roster, Vaesen and levels), CaseLab view of generated case packets (host only, spoiler warning), session logs and export.
- Post-session debrief form.

### 13.3 Characters

Built exactly as in the Vaesen rulebook: archetype, age, attributes, skills, talents, motivation, trauma, dark secret, relationships, memento, equipment, plus conditions and experience over time. Note that the engine design cut the dark secret for the eventual game; keep it on the sheet here for rules fidelity, but the engine does not use it.

---

## 14. Session lifecycle and host controls

1. **Joining:** no lobby. The host opens the call for a mystery; players appear as they connect.
2. **Start:** the host presses Start. For a new mystery, generation runs (show progress; it may take a while) unless a pre-generated case is chosen. The GM joins and resumes from saved state, with a recap from the ledger if it is not the first session.
3. **Play:** push-to-talk to the GM. State is saved continuously, not only at the end.
4. **Disconnects:** a dropped player reconnects and reappears. The host can pause the GM.
5. **Ending:** the host can close the call for everyone. State is snapshotted, the log closed, and the next session continues from there. When the mystery is resolved, run write-back (section 9.5) and season updates.
6. **Host controls:** pause and resume the GM, skip or repeat the last reply, force a recap, close the call.

Only the host configures the AI connection and presses Start.

---

## 15. Session logging for engine improvement

Each session produces one exportable log, linked by timestamp, readable directly by Claude, with four parts:

1. **The generated case:** the case packet as produced, including validator runs, and which persistent-cast members were drawn in and why.
2. **The transcript:** everything players said to the GM and to NPCs, every narration and NPC line, rolls, timestamped and attributed. Table talk only with consent.
3. **The GM's reasoning:** for each reply, the routing decision, which facts and records it drew on, what it withheld and why, tension changes, beats fired, channels pushed after misses, guard interventions, and model, token and latency figures.
4. **Player feedback:** in-play flags and the post-session debrief.

Log events are written continuously (DynamoDB for the index, S3 for the bulk). Export produces one file (JSON Lines or Markdown with embedded JSON; choose what Claude reads best and document it). Score with measurable signals as well as feel: facts found, channels used, misses rerouted, time stuck, flags raised.

---

## 16. Data storage

One DynamoDB table for this project. Suggested key design (refine as needed, document final choices):

| Entity | PK | SK |
|---|---|---|
| Account | `ACCOUNT#<id>` | `PROFILE` |
| Society | `SOCIETY#<id>` | `META` |
| Membership | `SOCIETY#<id>` | `MEMBER#<accountId>` |
| Investigator | `SOCIETY#<id>` | `INVESTIGATOR#<id>` |
| Character instance | `SOCIETY#<id>` | `CHAR#<id>` |
| Knowledge, belief, memory | `SOCIETY#<id>#CHAR#<id>` | `KNOW#<ts>#<factId>` / `BELIEF#…` / `MEM#…` |
| Relationship | `SOCIETY#<id>#CHAR#<id>` | `REL#<otherId>` |
| Institution state | `SOCIETY#<id>` | `INST#<placeId>#<institution>` |
| World overlay | `SOCIETY#<id>` | `OVERLAY#<key>` |
| Mystery | `SOCIETY#<id>` | `MYSTERY#<id>` (status, S3 key of case packet, live state) |
| Ledger | `MYSTERY#<id>` | `LEDGER#<ts>#<factId>` |
| Season | `SOCIETY#<id>` | `SEASON#<id>` |
| Session | `MYSTERY#<id>` | `SESSION#<id>` |
| Log event | `SESSION#<id>` | `EVENT#<ts>#<seq>` |
| Canon content | `CANON#<type>` | `<id>#v<version>` |

S3: case packets, full session logs, exports, generated images later. Use one bucket with prefixes per type. Least-privilege IAM role for the API and the agent.

---

## 17. Build order (milestones)

Each milestone ends with something the owner can try.

1. **Voice loop and latency.** LiveKit room; host holds the talk key, speaks, hears Claude reply through ElevenLabs via the .NET turn endpoint with a trivial prompt. Measure latency end to end before building more.
2. **Accounts and Society.** Auth0 with Discord, account creation, Society, character creation per the rulebook, character sheets.
3. **Content pipeline.** Content schemas, shared vocabulary, ContentTool. Generate and review: the four starting Vaesen, a handful of place templates, 6 to 10 text levels, the canonical roster, conflict archetypes.
4. **Mystery engine in CaseLab.** Assignment, plot centre, generation steps, case packet, validator (structural first, then critic). Generate many cases offline and review them with the owner. **Do not wire into voice until the owner is satisfied with generated cases.**
5. **Runtime GM.** Routing, bounded contexts, segments, ledger, knowledge growth, guard, tension and beats, dice with server-side results and pushing, failed-roll rerouting.
6. **Table UI and logging.** Centre panel, chat and roll log, talking indicator, flags, session lifecycle, host controls, full logging and export, debrief.
7. **Persistence and season.** World seeding, write-back and promotion at mystery close, influence at a distance, season state and threads.

The first real session can happen after milestone 6 with one standalone mystery carrying one season thread.

---

## 18. Deliberately out of scope (game-only or later)

- Unreal Engine, 3D levels, PCG, GameLift.
- NPC movement, navmesh, behaviour trees, routine crossings and ambient NPC-to-NPC conversation (the `EncounterScheduler`).
- Competition metrics and leaderboards (but record solve depth and outcomes).
- Rosenberger sabotage, multiple Societies in play, a social hub.
- Distinct voice per NPC (later, small change).
- Generated images for handouts (open question).

---

## 19. Open questions (do not decide silently; ask the owner)

- Which Vaesen rules are enforced strictly versus narrated loosely (conditions, pushing, mental harm). Default in 11.8.
- How clues and handouts appear in the centre panel: text cards only, or generated images.
- How season length is decided, thread density per case, and what happens if a Society ignores the season.
- Red-herring density tuning.
- How knowledge-gated clues are surfaced to a group lacking the lore without hand-holding.
- Retention period for voice transcripts.
- IP: no Nordiska Väsen licence yet. Fine for a private playtest; resolve before any public release (licence, or a legally distinct analogue avoiding the Society name and Uppsala setting).

---

## 20. Security and privacy

- The Anthropic API key, Deepgram, ElevenLabs and LiveKit secrets are server-side only (AWS Secrets Manager or SSM Parameter Store). Set a monthly spend limit in the Anthropic Console.
- Players log in through Discord and Auth0 only.
- Voices and transcripts are personal data: obtain consent at first join, keep retention short, allow deletion.
- Case packets and the CaseLab view are host-only; players must never be able to fetch the hidden GM file through the API.

---

## 21. Glossary

- **Society / guild:** the player group. The persistent unit players build.
- **Investigator:** a roster character a player plays.
- **Vaesen:** a folklore creature or being of the old world.
- **Load-bearing fact:** a fact the case cannot be solved without.
- **Channel:** one way into a fact (testimony, trace, document, overheard remark, knowledge-gated sign, event).
- **Focus:** the axis where a case's puzzle lives (what, who and why, how).
- **Hidden hand:** a person behind the creature (a caster or controller).
- **Giveaway tell:** a sign that identifies a creature on sight.
- **Discovered-facts ledger:** the record of what players have uncovered; the only source for recaps.
- **Case packet:** the hidden GM file for one mystery.
- **Tension dial:** the rising value that drives desperation, beats and the climax.
- **Canon / overlay:** shared authored content / per-Society changes to it.
