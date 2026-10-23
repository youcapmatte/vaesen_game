# Step: minds and channels

Put the facts into the world: what every person knows, thinks and lies about, and every way the players can come to each fact. The facts are fixed: they are in the case so far, and you refer to them by id.

Minds
- For every cast member: what they know, as fact ids (f1, f2…) from the facts list given above, never a title or description, each with source and when. What they think (beliefs, true or false) and what they lie about and the story they tell.
- Every testimony or overheard channel is backed by a knows entry of its source. Anyone who was told something was told by someone who knows it.
- when is a few words ("the night of the felling", "two days before arrival"), on the same calendar as the facts and the events.

Channels
- Channel kind is exactly one of: testimony, trace, document, overheard, knowledgeGated, event. Something players see for themselves at a place is a trace.
- Every load-bearing fact that is not a fact of an opening event gets at least two independent channels: different people or different spots. Losing any single channel must still leave every load-bearing fact reachable.
- Testimony and overheard channels name the person (sourceCharacterId), who must know the fact. Traces and documents name the place and the affordance id they are in (placeId, affordanceId: ids from the level).
- Gate later discoveries on earlier ones with knowsFact, but keep at least one honest clue to the true answer open from the start (no knowsFact gate, or gated only on facts of the opening events). Include at least one knowledgeGated channel.
- {{giveawayRule}}
- Mix testimony and physical traces: a group that only talks, or only searches, should still get far.
- reveal is what the players perceive, in plain words, not the fact statement: one or two short sentences.
- Keep to the budget: two channels for each load-bearing fact (a third only when it opens a different kind of way in), one for each other fact and decoy fact. A case with more channels is not fairer, only longer to run.
