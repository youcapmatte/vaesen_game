# Vaesen.Framework.Mystery

The mystery engine: canon content, the assignment of a new case, the generation pipeline on the Anthropic API, the structural validator and the model critic, and the case store. Spec: [NexusTable/SPEC.md](../../../NexusTable/SPEC.md), sections 4–8 and 17.

It is a framework library like the others here: Nexus Table registers it now (`AddMysteryServices`), and the game's `Vaesen.Api` can register the same library later. The Unreal game then reaches it through its backend, so nothing has to be ported.

```
Models/        Canon (Setting, AffordanceVocabulary, VaesenDefinition, PlaceTemplate, LevelDefinition,
               ConflictArchetype, CharacterDefinition), the CasePacket (spec 8), AssignmentBrief, CoherenceIssue
Content/       ContentLoader, ContentValidator, and the content packs themselves: Content/vaesen/
Generation/    Assignment (deterministic), the typed steps (conflict -> escalation -> humans -> clues -> critic),
               the step output types, PromptBuilder and the prompt templates (Prompts/*.md)
Validation/    FactGraph and CaseValidator: the structural checks in C#
Ai/            IMysteryModel and the Anthropic implementation (streaming, structured outputs, caching, fallbacks)
Data/          IContentStore, ICasePacketStore (files now; DynamoDB/S3 later)
Services/      MysteryGenerator, ContentGenerator, CaseReport (Markdown), MysteryOptions, the tool host
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
   - **clues**: facts, channels, minds, decoys, beats, the climax, resolution levers and the hook.
3. **Critic**: the judgement checks that code cannot make. Its errors send the clue step round again.
4. **Retries**: a failing step is re-rolled with its issues fed back (`MaxStepRetries`). If it still fails, the whole case is re-rolled with a new seed (`MaxCaseRetries`). If that fails too, generation fails visibly and hands back the last attempt with the full log. Every model call and validator run is logged with its tokens, cache hits and latency.

## The structural checks (`Validation/CaseValidator.cs`)

- **Reachability.** The walk starts from what the hook tells the players. A channel opens once every fact it is gated on (`knowsFact:f7`) is known. Trust, rolls, times and places count as reachable, because a failed roll is rerouted (spec 4.6). Beats fire as the tension from known facts reaches their threshold. Every load-bearing fact must be reached, and so must every beat and the climax.
- **Redundancy.** Every load-bearing fact the hook does not state needs two independent sources: different people, or different spots.
- **Order independence.** Removing any one channel must leave every load-bearing fact reachable. A group that only talks, or only searches, gets a warning if it would reach fewer than half of the load-bearing facts.
- **Provenance.** Every testimony is backed by what the person knows. Anyone who was told something was told by someone who knew it.
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
- **Resolution.** A hidden-hand case has both a crude solve and a discoverable deep solve.
- **Beats.** Thresholds are in order and reachable. A creature with time or place conditions gets a climax that says when or where.

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
| `CreativeModel`, `CreativeEffort` | `claude-opus-5-5`, `high` | conflict, escalation, humans, clues |
| `CriticModel`, `CriticEffort` | `claude-opus-5-5`, `high` | the critic |
| `UseCritic` | `true` | |
| `MaxOutputTokens` | 64000 | per call; steps stream |
| `MaxStepRetries`, `MaxCaseRetries` | 2, 1 | |
| `VaesenCooldown` | 3 | recent mysteries a creature is kept out of |
| `RedHerringDensity` | `low` | `low`, `medium` or `high` |
| `UseDraftContent` | `false` | `true` in Nexus Table Development and in CaseLab (unless `--approved-only`) |
| `DefaultMonth` | 10 | the in-world month when the Society has no calendar yet |
| `ContentPath`, `CasePacketPath` | the library's Content folder; `.data/cases` | |

## Not verified

No case has been generated with the real API: there was no Anthropic key. The pipeline has been tested end to end with a scripted model built from the hand-written sample case (`Tests/.../Fixtures/sample-case-forsby.json`). The Anthropic client has been tested against a fake HTTP server: the request body, streaming, the token limit and the schema-too-complex fallback. Expect the first real runs to need prompt tuning.
