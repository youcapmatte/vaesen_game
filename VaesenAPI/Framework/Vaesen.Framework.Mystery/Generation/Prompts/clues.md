# Step: clues

Distribute the truth into the world, then write the hook. This is the step the validator checks hardest.

Facts
- List every fact of the case: symptoms, causes, the creature's identity, alibis, motives, how it can be resolved, any season thread, and colour. Mark the load-bearing ones (the case cannot be solved without them). Each says what it points to: trueAnswer, neutral, or decoy:<threadId>. Mark giveaway tells (giveaway) and facts built from the creature's deep lore (deepLore), and list the lore ids they rest on (loreIds, from the creature's lore and sign ids).
- At least one load-bearing identity fact points to the true answer, and the trail to it rests on deep lore.

Channels
- Every load-bearing fact the players do not already know from the hook gets at least two independent channels: different people or different spots. Losing any single channel must still leave every load-bearing fact reachable.
- Testimony and overheard channels name the person (sourceCharacterId), who must know the fact. Traces and documents name the place and the affordance id they are in (placeId, affordanceId: ids from the level).
- Gate later discoveries on earlier ones with knowsFact, but keep at least one honest clue to the true answer open from the start (no knowsFact gate, or gated only on what the hook tells). Include at least one knowledgeGated channel.
- {{giveawayRule}}
- Mix testimony and physical traces: a group that only talks, or only searches, should still get far.
- reveal is what the players perceive, in plain words, not the fact statement.

Minds
- For every cast member: what they know (fact ids, with source and when), what they think (beliefs, true or false) and what they lie about and the story they tell. Every testimony channel is backed by a knows entry. Anyone who was told something was told by someone who knows it.

Misdirection ({{redHerringDensity}} density)
- {{decoyRule}} Every decoy thread is cleared by true facts that can be reached (clearedByFactIds), and its own facts point to it (decoy:<threadId>).

Beats, climax and resolution
- Three to six beats at rising tension thresholds (0-1), each with what happens, who acts (actorId), where and when it requires, and the facts it puts into the world. Use them for the off-screen actions of the desperate and the creature's escalation.
- The climax comes at a threshold of about 0.8; {{climaxRule}}
- Tension starts around 0.1 and rises with each fact found (factWeights); make sure the facts players can reach add up past the climax threshold.
- Two to four resolution levers with different consequences for the world and the cast. {{leverRule}} Each lever lists the facts the players need to see it (requiresFactIds).

Hook and scenes
- The hook arrives by {{hookChannel}} from the hook-giver. It carries just enough convergence to justify the journey: it reports opening events (openingEventIds) and states facts (assertedFactIds), never the cause. Use a detail that does not fit, a strained official explanation, the teller's own unease, stakes and urgency. Everything it states is a fact like any other.
- Map each escalation event to the facts it is (eventFacts).
- List the places the case uses (placesUsed) and write a scene description for each, in this month's weather and light, with the case's traces woven in.
- {{seasonRule}}
