# Vaesen.Framework.Mystery

The mystery engine: canon content, the assignment of a new case, the generation pipeline on the Anthropic API, the structural validator and the model critic, and the case store. Spec: [NexusTable/SPEC.md](../../../NexusTable/SPEC.md), sections 4–8 and 17.

It is a framework library like the others here: Nexus Table registers it now (`AddMysteryServices`), and the game's `Vaesen.Api` can register the same library later. The Unreal game then reaches it through its backend, so nothing has to be ported.

```
Models/        Canon (Setting, AffordanceVocabulary, VaesenDefinition, PlaceTemplate, LevelDefinition,
               ConflictArchetype, CharacterDefinition), the CasePacket (spec 8), AssignmentBrief, CoherenceIssue
Content/       ContentLoader, ContentValidator, and the content packs themselves: Content/vaesen/
Generation/    Assignment (deterministic), the typed steps (conflict -> escalation -> humans -> facts -> minds -> arc -> hook -> critic),
               the step output types, PromptBuilder and the prompt templates (Prompts/*.md)
Validation/    FactGraph and CaseValidator: the structural checks in C#
Ai/            IMysteryModel and the Anthropic implementation (streaming, structured outputs, caching, fallbacks)
Data/          IContentStore, ICasePacketStore, IWorldStore (files now; DynamoDB/S3 later)
Services/      MysteryGenerator, WorldService (seeding, closing, write-back), ContentGenerator, CaseReport (Markdown),
               MysteryOptions, the tool host
```

## How a case is made

1. **Assignment** (`Generation/Assignment.cs`, no model). From the Society's history and the in-world month, the assignment picks:
   - a level of that month;
   - a creature the Society has not met recently (`VaesenCooldown`) whose required affordances the level offers;
   - the conflict archetypes that fit both;
   - a reason kind, and an origin if the creature is an affliction;
   - whether a hidden hand is behind it, which sets the plot centre and the focus (`what`, `whoWhy` or `how`);
   - a hook channel and a hook-giver type, both kept off what the Society saw last.

   The same seed always gives the same brief.
2. **Steps**, each a call to the creative model that is validated before the next one runs. The types enforce the order: each step takes the stage the one before it produced.
   - **conflict**: the provocation, which must physically exist in a place of the level.
   - **escalation**: the symptoms.
   - **humans**: the cast and the residents.
   - **clues**, as four sub-steps, each small enough for the API to enforce its schema (so a fixed-value field cannot get an invented value):
     - **facts**: every fact, the decoy threads, and which facts the events are. Whether a fact is deep lore or a giveaway tell is derived from its lore ids, not asked for.
     - **minds**: what every person knows, thinks and lies about, and every channel into every fact.
     - **arc**: beats, the climax and the resolution levers (crude, partial or deep). The engine computes the tension weights: the load-bearing facts, weighing twice the others, take the dial 15% past the climax.
     - **hook**: the hook and the scene descriptions.
3. **Whole-case checks and the critic**: reachability, redundancy, fair play and the rest, then the judgement checks code cannot make. Each structural problem goes back only to the sub-step it points at (new facts redo everything built on them), up to `MaxReworkRounds`.
   - The critic names the ids each issue touches (`targets`: f3, c7, r2, truth.hiddenHandMotive, npc-anna.knows...). Each issue is routed to the step that owns those ids (a motive to the conflict, a lever to the arc, a channel to minds and channels), split when it spans steps, and fixed in place in pipeline order: a patch of the items named (and any new item it asks for, such as a channel for a fact nothing teaches: target `channels`), or a revised answer that changes only what the issue asks. Nothing downstream is regenerated.
   - A fix that revises an event or a fact sends the items built on it after it: the event's facts, the fact's channels and the people who know it, each patched to agree. Otherwise a corrected date stays wrong in every channel that quotes it.
   - The critic's second look gets its first look's list, at `StandardEffort`: it checks those fixes rather than reviewing from scratch and asking for new detail on a clue it already had fixed.
   - A fix keeps every id it was given (a renamed person keeps their id), and a patch adds an item only when a problem asks for one. A round of the critic's fixes that breaks a case which passed is rolled back, and its objections stay as warnings.
   - The critic may send the case back once (`MaxCriticReworks`). After that its objections are warnings on a case that passes, and those with concrete fixes are applied in one cheap last round on `LightModel`, kept only if the case still passes its checks. Applied warnings are marked in the report.
4. **Repairs and retries**:
   - Near misses in fixed-value fields ("Surface", "half") are repaired in code before validation and logged as warnings.
   - When a step's only errors are field-level ones on a quarter of its items or fewer, a **patch** call asks for just those items and merges them back. Otherwise the step retries in full with its issues fed back (`MaxStepRetries`); every issue about a fixed value lists the allowed values.
   - A step that gives up starts again on its own; steps that passed are kept. An earlier step is re-run only when a structural issue points at it, never on a location the critic mentions. The case is re-rolled from a new brief only when the conflict finds nothing plausible fits (`MaxCaseRetries`), and the focus alone is changed when it does not suit the creature (`supportedFoci`).
   - A rejected case records the step that gave up; its report ends with that step's last attempt. Every model call and check is logged with its kind (full, fresh, patch, check), tokens, cache hits and latency.

### Inputs, caching and cost

- Each step gets only what it works from: its part of the canon (the persistent cast only for humans, all the archetypes only for the conflict and the chosen one for humans, no level for the facts, no creature for the hook) and its view of the case so far (`CaseView`). Only minds and channels get the level's rooms and furniture (one line each); the others get its places. The creature comes whole to the conflict and the facts, as its signs to escalation, as folk belief to humans, and as lore, weaknesses and confrontation to minds and the arc. The critic and the structural checks see everything.
- JSON in prompts is unindented (indentation was about a fifth of every block), and an issue split over several items is stated once with all its items.
- A different output schema invalidates the whole cached prefix (measured: the system prompt too), and every step has its own schema, so the cache cannot carry from one step to the next. First calls therefore do not cache (no write premium); retries, patches and the critic, which repeat the same schema, do. Every call logs a hash of its cached prefix.
- The report shows each call's cache reads and writes and its cost at list prices (`ModelPrices`), and the total.

### Tension

The engine sets the fact weights. Colour and season facts, and what the group knows on arrival, weigh nothing, so the dial starts at its start; decoy facts weigh half, load-bearing ones double. The weights are scaled so the deep solve's path (its lever's facts and what gates them) ends 15% past the climax. A beat with a time fires when its threshold is reached and its time has come, whichever is later.

### Review 3 (case 07)

- **Each fix is checked on its own.** After every code replacement and every step's fix, the structural checks run again. A fix that adds an error the case did not have gets one repair: too few ways in goes to minds and channels as a new channel or a first-hand teller, and anything else goes to the step that owns the broken item. If the repair does not hold, only that fix is undone (an `undo` log entry). The others are kept, and code replacements always stay unless they break something themselves.
- **No blind retries.** An undone fix is not tried again. Only what the verifying look found still standing is retried, with what it found, and an instruction already sent is never sent again.
- **Blocking issues.** The critic marks an issue `blocking` when it would break the case at the table: a scene leak, a hook that contradicts the case, a period error, someone saying what they cannot know, an event told twice. A passed case with a blocking issue no fix took is kept, with `validation.needsReview`. CaseLab's report, the case files list and the case file show "Needs review".
- **New checks.**
  - Scene leaks (`SceneWords`): a scene sharing three or more distinctive words, and a third of them, with a gated reveal is an error for the hook step. The hook prompt now asks for only what anyone sees on arrival, and the critic has a `scenes` check.
  - The hook names people as the case does ("my sister Maja" with no Maja is an error).
  - The hook-giver knows what the hook states. The hook step adds what they lack as parish talk, which is hearsay and never a way in.
  - A beat's actor puts out only what they know, or their own deed.
  - Someone who tells a load-bearing fact has a fear and a permitted action.
  - A tie to no one is an error.
- **Ids are not names (`PersonIds`).**
  - Roster people get `npc-<place>-<role>` ids from code; the model numbers them p1, p2...
  - A new person whose model-given id is made from their name is re-keyed the same way before anything refers to them.
  - Roster people now carry a fear.
- **Residents' patches** run on the small model (`LightModel`).
- **Geography against the map.** A level's `analogueAt` and each neighbour's `at` (latitude, longitude) let the content validator check stated distances and directions. Ekeby's neighbours are corrected. The transport canon splits the Eastern Main Line: Katrineholm to Linköping by 1872, Linköping to Nässjö in 1874. Mjölby's own date is not confirmed, so Ekeby's railhead is Linköping. Mossaby goes by Falköping before 1874.

### Review 2 (case 06)

- **Fixes are verified.** In the last round, nothing is marked applied on trust:
  - a structural warning must no longer be found;
  - a replacement's old text must be gone;
  - a critic issue must be found gone by a verifying look (`FixVerifier`, prompt `verify.md`: the critic model at low effort).

  What did not take is tried again (`FixRetries`, 1) with what still stands. A fix that still does not hold stays a warning marked `unresolved`, never applied. Only one that breaks the table (a second person who fetched the Society) fails the case, unless `UnresolvedFixIsError` is on, when any does. Case 06's regeneration with that on was thrown away over a missing item field. The report and the case file say "fix verified" or "fix did not hold".
- **Mechanical fixes in code.** The critic gives a rename, a date or a wrong id as `replacements` (exact old and new text, `everywhere` or only inside the targets). `TextReplacements` applies them to the packet and every kept output, and logs a `replace` entry; no model is asked.
- **Judgement fixes are patches.**
  - The last round patches on the patch model, never the small one.
  - It never rewrites a whole step: what cannot be a patch is left as it is.
  - The creature's mind is one patchable item, and a target like `nack creatureMinds.blames` is routed to the creature step.
- **One hook-giver.** Another person whose record says they fetched the Society is flagged for a fix to their record. A channel can fix the kind of giver: `old-world-visit` carries a being or its messenger (`oldWorld`).
- **Playable records.**
  - Significant people need drives, fears, a routine, and something they know or think.
  - Residents need a drive; the roster now writes one.
  - Fact ids in prose must exist and be known; the prompts keep them out of prose and turn clues in secrets into facts.
- **Names of 1872.**
  - A son takes his father's first name and -son, a daughter -dotter, and a wife keeps her own patronymic: these are warned on in a case, except for persistent people, whose names are fixed, and are errors when a roster is written.
  - A parent and child are not warned on for sounding alike.
  - A first name that lives only in a house name ("Old Brita's") is flagged where the case speaks of it as a person. Storskog's cottage is renamed.
- **A lever requires at most eight facts** (`ValidationContext.MaxLeverFacts`).
- **Prompt caching is off** (`PromptCache`). Each step's prefix includes its schema with that call's ids, and the critic's second look comes after the five-minute cache has gone: 69k tokens were written to the cache and none read.

### Fixes 4, part C: geography and travel

- **Geography** (`LevelDefinition.Geography`): the real place a level is modelled on (`realAnalogue`, grounding only, never named in a case), province, county, parish, what an educated Swede of the period knows of it, neighbouring towns with rough distances and what they offer, and where the authorities are (sheriff, district court, provincial doctor, post, telegraph, bank, newspaper).
- **Journey** (`LevelDefinition.Journey`): routes from headquarters as legs (from, to, mode, duration, cost, schedule, months, notes); railway, steamer and canal legs name their line. `travel` stays as the one-line summary. `RoutesFor(year, month, setting)` gives the routes whose every leg runs that month on a line open that year; steps see only those (`routesThisMonth`), and the hook may only use them.
- **Period canon** in the setting: `periodTransport` (railways, canals, steamers, telegraphs with the year each opened and the months it runs) and `regionalInstitutions` (county seats, courts, sheriffs, doctors, banks, newspapers).
- **Checks:** the content validator fails a leg on a line not open in the start year, warns on a rail or water leg that names no line, a leg in months its line does not run, a level month with no route, and a level without geography or authorities. The case validator warns when a case speaks of an authority the level lacks, names a known town that is not a neighbour, or names the real analogue; the critic checks the same.
- **Content tool:** `canon` drafts the transport and institutions canon into setting.json for the owner to review once; `geography <levelId>` places a level (geography, journey, travel line) from the canon and puts it back to draft.

### Fixes 4, part B: returning to a place

- **The in-world calendar** only moves forward. A case is set at least `MonthsBetweenCases` after the Society's latest case (queued ones included); without a pinned month the assignment moves on to the first month, up to `MonthsToSearch` ahead, where a level and a creature fit. The date is on the brief and the packet (`year`, `month`).
- **Level cooldown:** after a case on a level, queued or played, the level waits until the Society has *finished* `LevelCooldownMysteries` (3) other mysteries and `LevelCooldownMonths` (3) in-world months have passed. Queued cases count as uses, never as finished, so pre-generated cases cannot clear it. The creature, hook-channel and hook-giver cooldowns already read the same history, queued cases included (`CaseSummary.History`). If every fitting level is on cooldown the cooldown is relaxed, with a note.
- **Return visits** (`GenerationRequest.ReturnLevelId`, CaseLab `--return`) waive the cooldown for one level; the brief carries `returnVisit` and the earlier mysteries there.
- **Month variants:** a level is one record with `monthVariants` (season notes, travel, month-only public facts, place descriptions). Places, owners and history are shared. `LevelDefinition.ForMonth` gives the level as it is in a month; the assignment offers a level in every month it has. `ContentTool variant <levelId> <month>` writes one with the model (the level goes back to draft).
- **A level's people per Society:** the first case on a level writes its roster on the small model (`RosterBuilder`, prompt `roster.md`): who holds every place with an `ownerRole` (keeper, owner, clergy, wise woman), the owner households, the official, the teacher, a few residents with latent hooks. They are kept in the world as `locationResident` characters with `homeLevelId`, but only once the case is stored and `KeepRoster` is on; an unplayed case deleted (CaseLab `discard`) takes a roster no other case used with it. Later cases see them as "the people of this place", the humans step marks who `keeps` a place, and the validator fails a case that gives a held place a new keeper or renames a persistent person. Residents come from the roster in code, without a model call. Other levels' people do not count for name clashes.
- **Public facts explained:** the level's public facts have ids (pf1...). The conflict step lists the ones the case explains (`publicFactsExplained`); a played case writes them to the overlay with the fact's id, and queued cases' explanations are read from their packets. Both reach later cases on the level as what is settled about the place, and explaining a settled fact again is a warning.
- **Room for several cases:** every farm and business has an `ownerRole`; a level of size 2 should have at least two farms, crofts or manors (size 3: three), and works house their workers. The content validator warns otherwise. Gruvbo gained two farms and a charcoal burner's croft, miners' cottages and a January variant.

### Fixes 4, part A

- **Hearsay** (`told:bystander:<role>`, `told:offstage:<id>`) counts for reachability; it only does not count toward a load-bearing fact's two independent ways in, and the error says so. Named people who never appear are listed in `offstagePeople`.
- **Names by role.** The conflict and the events speak of people by role; the humans step names them. A rename after names exist is carried into every text and kept output by code (`Renames`), and logged. The critic is told the name rules and that the cast's name is the right one.
- **Beats do not solve the case:** at most half the deep lever's facts may come from the opening and the beats (an error, sent to the arc). Every fact a beat produces needs an aftermath channel (`afterBeat:<beatId>`).
- **Deeds** name who must do them (`performedBy`: cast ids or `players`), apart from the witnesses.
- **Minds:** the creature's `blames` (whom it holds responsible, and why) instead of false beliefs; the critic checks that minds agree with what they know and that people's records agree with the facts. Someone under a threat must believe something about it.
- **Hook-giver types** carry their meaning and the words that recognise one; the hook-giver is checked against the type asked for.
- **Residents** come from their own step on the small model at low effort, after the case's people.
- **A split issue is applied only if every part was**, so the report never marks a failed fix as applied.

### Ids, deeds and minds

- **Ids as enums.** Every field that refers to a fact, an event, a person, a place, an affordance or a channel requirement is marked `[IdsOf]`. Each call's schema carries the case's ids of that kind as an enum (`CaseIds`), so the model cannot write a fact's title where its id belongs. If a schema with the ids is ever too large to compile, the call retries without them, and only then with the schema in the prompt. A step that defines ids (the facts step its facts) leaves them open. Something the players must have in hand goes in a channel's free `item` field, so `requires` can be a closed list.
- **Deeds.** Every lever lists `requiredDeeds`: what the players must do for it to take effect (where, when, before whom), besides knowing its facts. Closing a mystery can record the deeds done (`deedsDone`). Tracking them live is the runtime's job (milestone 5).
- **Creature minds.** A creature that can be bargained with gets a `creatureMind`, written in a step of its own after the arc (in the arc's schema the grammar is too large for structured outputs): wants, terms (tied to the deep lever's deeds by `termDeedIds`), red lines, what it knows and believes, its voice and how it answers an offering, a threat, steel, a prayer, a bargain, an apology and a lie.
- **Homes.** A cast member may live in a dwelling of the level (`homeAffordanceId`). One whose place has none asks for `newDwelling`, and the engine adds a home with a bed, a hearth and a chest to its copy of the level, recorded in `level.addedAffordances` (`LevelDefinition.ForCase` rebuilds it). Mines, sawmills, mills and logging camps now offer workers' cottages.
- **Warnings that name their items** (an action outside a person's permitted ones, a home trace in someone else's house, a first name the Society's world already has) go to their owning steps with the critic's issues, and in the light last round.

## The Society's world (spec 9)

Each Society has its own world: one `SocietyWorld` document holding its persistent cast and what it has changed.

- **Seeding.** Before a Society's first mystery, `WorldService.EnsureSeededAsync` seeds the world (`Generation/WorldSeeding.cs`, prompt `world.md`):
  - every canonical character gets a copy for this Society, with its starting attitude;
  - the model writes a core cast of `CoreCastSize` people, important from the start, with drives, voices, ties and an attitude toward the Society;
  - it also writes a background cast of `BackgroundCastSize` named faces, each with latent hooks;
  - `WorldValidator` checks the result (every canonical character present, unique ids, values in range, ties that resolve), and a failing seed is re-rolled with its issues fed back.

  The host can also seed from the World page.
- **Reading.** Every case is offered the living persistent characters, canonical and core first, then by importance (`PersistentCastOffered`). For each one it gets their attitude and reason, drives, latent hooks, network, ties, last memories and what they know from before. It's also told who is dead or gone, and what the Society's past mysteries changed in the world. The people step may then:
  - bring a persistent character in (`instanceRef: persistent`);
  - raise a background character into the story (`promoted`);
  - or create someone new.

  The validator rejects a new character who takes a world id, and anyone dead or departed.
- **Closing.** When the host closes a mystery (outcome solved or failed, plus the lever the Society pulled), `WorldWriteBack` writes it back:
  - every cast member gets a memory of the case, what they knew, and what they thought;
  - the case's own people join the world: residents stay with their place, the hook-giver and the hidden hand become core, the rest background;
  - a promoted character moves up to core;
  - the hook-giver's attitude moves +1 if the Society solved it, -1 if not;
  - the chosen lever applies its attitude changes (`delta`), its fates (dead or departed) and its world consequences (the overlay).

  The case is stored as solved or failed, together with a list of everything the write-back changed.

Not built yet: the season thread (spec 10), influence at a distance through `InstitutionState`, and knowledge spreading along ties between mysteries.

## Reviewing a case

A stored case is one JSON file that holds everything about it. Download it from the case's Review tab ("Download everything for review"), or copy it from `{CasePacketPath}/{societyId}/{mysteryId}.json`. It contains:
- the packet;
- the assignment's choices;
- the system prompt and case context it was generated with;
- each step's instruction and the model's raw answer;
- every validator issue;
- the closing.

That's enough for Claude to review a case and propose changes to the prompts, content or checks. The world downloads the same way from the World page.

## The structural checks (`Validation/CaseValidator.cs`)

- **Reachability.** The walk starts from what the hook tells the players. A channel opens once every fact it is gated on (`knowsFact:f7`) is known. Trust, rolls, times and places count as reachable, because a failed roll is rerouted (spec 4.6). Beats fire as the tension from known facts reaches their threshold. Every load-bearing fact must be reached, and so must every beat and the climax.
- **Redundancy.** Every load-bearing fact the hook does not state needs two independent sources: different people, or different spots.
- **Order independence.** Removing any one channel must leave every load-bearing fact reachable. A group that only talks, or only searches, gets a warning if it would reach fewer than half of the load-bearing facts.
- **Provenance.** Every testimony is backed by what the person knows (the error sits on the person, so a patch can add the knowledge). Anyone who was told something was told by someone who knew it, or by a bystander outside the cast (`told:bystander:carter`). Bystander gossip is hearsay: a channel that rests only on it never counts toward a fact's two independent sources.
- **Homes.** A trace or document in a dwelling is in its owner's own home (a warning).
- **Decoys and fair play.**
  - Every decoy can be cleared by a reachable true fact.
  - An honest clue to the true answer is open early and not locked behind a decoy.
  - A hidden hand is never exposed only by their own words.
- **Affordance fit and vocabulary.** The creature, the provocation and every clue fit the level's places and affordances, and every tag is in `vocabulary.json`.
- **Cast coherence.**
  - Every referenced person is in the cast.
  - A persistent character must exist and be alive.
  - A new character has an identity.
- **Openings and giveaways.**
  - The opening has enough converging events, spread over different registers.
  - The hook never states the cause.
  - In a `what` case, giveaway tells are kept out of the opening and gated late.
  - The solution rests on deep lore, and there is at least one knowledge-gated step.
- **Resolution.** A hidden-hand case has both a crude solve and a discoverable deep solve. Every lever has deeds whose places, affordances and witnesses exist, at places the players reach. A creature that can be bargained with has a mind whose terms name deep-lever deeds.
- **Beats.** Thresholds are in order and reachable. A creature with time or place conditions gets a climax that says when or where. A person who acts in a beat or the climax does one of their permitted actions, at a desperation the tension has reached (medium from 0.35, high from 0.65); otherwise a warning.
- **Names.** Two people in a case whose first names match or share a first syllable, or a new significant person whose first name matches or sounds like a persistent character's in the Society's world, is an error at the humans step (the humans step is also given the world's names). It is patched there, before any later step writes the name into prose. A first name inside another's surname is only a warning: it may be a patronymic (Karin Eriksdotter, Erik's daughter), which the truth may already use.

## Content

`Content/vaesen/` holds the setting, the vocabulary, 4 creatures (Näck, nisse, skogsrå, werewolf), 14 place templates, 8 text levels (3 of them in October), 10 conflict archetypes and 6 canonical characters. All of it is **draft**: written by Claude from folklore and history in the content schemas, and not reviewed yet. Generation uses only approved content unless `Mystery:UseDraftContent` is on, which it is in Nexus Table's Development settings.

Each record is one file. An approved record is never changed in place; a change is a new file `{id}.v2.json`, and the loader keeps the highest version.

## Tools

Both tools read `Mystery__*` settings from appsettings.json, user secrets (`vaesen-mystery-tools`) or the environment. The Anthropic key comes from `ANTHROPIC_API_KEY` (or `Mystery__AnthropicApiKey`).

```bash
dotnet run --project VaesenAPI/Tools/Vaesen.Mystery.ContentTool -- validate
```

```bash
dotnet run --project VaesenAPI/Tools/Vaesen.Mystery.ContentTool -- list
```

```bash
dotnet run --project VaesenAPI/Tools/Vaesen.Mystery.ContentTool -- approve nack werewolf
```

```bash
dotnet run --project VaesenAPI/Tools/Vaesen.Mystery.ContentTool -- generate vaesen "the bäckahäst, a river horse"
```

```bash
dotnet run --project VaesenAPI/Tools/Vaesen.Mystery.CaseLab -- assign --count 10
```

```bash
dotnet run --project VaesenAPI/Tools/Vaesen.Mystery.CaseLab -- generate --count 3 --out cases
```

```bash
dotnet run --project VaesenAPI/Tools/Vaesen.Mystery.CaseLab -- validate cases/caselab/<id>.json
```

- **ContentTool** edits the files in the source tree, so every change shows in git. `approve --all-drafts` approves everything at once.
- **CaseLab `generate`** writes `{id}.json` (the stored case: packet, brief, log) and `{id}.md` (a readable report with every fact, channel, mind, beat and the generation log) for each case. A failed case is kept as `.failed.json` and `.failed.md`.
- **CaseLab `assign`** shows what the assignment would choose, without calling the model.

## Settings (`Mystery` section)

| Setting | Default | |
|---|---|---|
| `CreativeModel` | `claude-opus-5-5` | every creative step |
| `CreativeEffort`, `StandardEffort`, `LightEffort` | `high`, `medium`, `low` | high: conflict and the first try of facts; medium: other first tries (minds included) and the critic's second look; low: retries and the hook |
| `RetryModel`, `PatchModel`, `PatchEffort` | (creative model), (creative model), `low` | a smaller model may take retries and patches |
| `PatchMaxOutputTokens`, `PatchShare` | 16000, 0.25 | a patch's output cap; the share of a step's items a patch may touch |
| `MaxReworkRounds` | 2 | how often whole-case and critic issues send sub-steps back |
| `MaxCriticReworks` | 1 | how many of those the critic may cause; then its objections are warnings |
| `LightModel` | `claude-haiku-5-5` | the last round that applies the critic's remaining warnings |
| `CriticModel`, `CriticEffort` | `claude-opus-5-5`, `high` | the critic |
| `UseCritic` | `true` | |
| `MaxOutputTokens` | 64000 | per full step call, thinking included; steps stream |
| `MaxStepRetries`, `MaxCaseRetries` | 2, 1 | |
| `VaesenCooldown` | 3 | recent mysteries a creature is kept out of |
| `RedHerringDensity` | `low` | `low`, `medium` or `high` |
| `UseDraftContent` | `false` | `true` in Nexus Table Development and in CaseLab (unless `--approved-only`) |
| `DefaultMonth` | 10 | the in-world month when the Society has no calendar yet |
| `SeedWorld` | `true` | seed the Society's world before its first mystery |
| `CoreCastSize`, `BackgroundCastSize` | 10, 14 | size of a new world's own cast, besides the canonical characters |
| `PersistentCastOffered` | 24 | how many persistent characters a case is offered |
| `WorldPath` | `.data/worlds` | where worlds are stored (Nexus Table: next to its other local data) |
| `ContentPath`, `CasePacketPath` | the library's Content folder; `.data/cases` | |

## Not verified

No case has been generated with the real API: there was no Anthropic key. The pipeline has been tested end to end with a scripted model built from the hand-written sample case (`Tests/.../Fixtures/sample-case-forsby.json`). The Anthropic client has been tested against a fake HTTP server: the request body, streaming, the token limit and the schema-too-complex fallback. Expect the first real runs to need prompt tuning.
