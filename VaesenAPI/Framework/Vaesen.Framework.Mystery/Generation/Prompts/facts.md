# Step: facts

Decide the facts of the case: everything the players could come to know, true or misleading. Later steps put each fact into the world (who knows it, where it can be found) and write the hook, so state each fact plainly here and do not write channels or the hook.

Facts
- List every fact of the case. Each fact's kind is exactly one of: symptom, cause, identity, alibi, motive, resolution, season, colour. The kind says what the fact is about, not how players find it: lore, trace, testimony, physical, event and pattern are not kinds. An old-lore fact naming what the creature is, is identity. A lore fact about how to appease it is resolution. A physical trace of the broken grave is cause.
- Mark the load-bearing ones (the case cannot be solved without them). Each says what it points to: trueAnswer, neutral, or decoy:<threadId>.
- For every fact that rests on the creature's lore or signs, list the ids it rests on (loreIds). This is how the engine knows which facts are deep lore and which are giveaway tells, so do not leave it empty for a lore-based fact.
- At least one load-bearing identity fact points to the true answer, and the trail to it rests on deep lore: at least one load-bearing true-answer fact lists one of the creature's deep lore ids.
- Map each escalation event to the facts it is (eventFacts). The facts of the opening events are what the players know when they arrive.

Misdirection ({{redHerringDensity}} density)
- {{decoyRule}} Every decoy thread is cleared by true facts (clearedByFactIds), and its own facts point to it (decoy:<threadId>).
- A decoy must survive the first hour of play. Give its suspect at least one true but damning fact that physically places them near the provocation or the harm (seen at the place at the wrong hour, holding something connected to it), not only motive and rumour. The fact must be cleared later by a true fact that explains it innocently.
- {{seasonRule}}
