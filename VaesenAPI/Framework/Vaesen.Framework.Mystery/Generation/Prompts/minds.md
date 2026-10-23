# Step: minds and channels

Put the facts into the world: what every person knows, thinks and lies about, and every way the players can come to each fact. The facts are fixed: they are in the case so far, and you refer to them by id.

Minds
- A person under a threat thinks why it is real (one of their thinks): "No one in the parish would take a stoker's word over the master of Östansjö, and the strap is the only proof."
- For every cast member: what they know, as fact ids (f1, f2…) from the facts list given above, never a title or description, each with source and when. What they think (beliefs, true or false) and what they lie about and the story they tell.
- Every testimony or overheard channel is backed by a knows entry of its source. Anyone who was told something was told by someone who knows it.
- Knowledge told by a named person who never appears (in offstagePeople) is told:offstage:<id>.
- Knowledge may come from someone anonymous outside the cast only as told:bystander:<role> (told:bystander:carter, told:bystander:miner's wife). Use it for gossip. It never makes a load-bearing fact reachable on its own: a load-bearing fact needs two channels from people who know it first-hand (witnessed, self, lore, or told by a cast member who knows it) or from traces and documents.
- when is a few words ("the night of the felling", "two days before arrival"), on the same calendar as the facts and the events.

Channels
- Channel kind is exactly one of: testimony, trace, document, overheard, knowledgeGated, event. Something players see for themselves at a place is a trace.
- Every load-bearing fact that is not a fact of an opening event gets at least two independent channels: different people or different spots. Losing any single channel must still leave every load-bearing fact reachable.
- Testimony and overheard channels name the person (sourceCharacterId), who must know the fact. Traces and documents name the place and the affordance id they are in (placeId, affordanceId: ids from the level).
- A trace or document in someone's home names them as sourceCharacterId and sits in their own home: their homeAffordanceId or something inside it, never another person's house or a shared building they do not live in.
- Gate later discoveries on earlier ones with knowsFact, but keep at least one honest clue to the true answer open from the start (no knowsFact gate, or gated only on facts of the opening events). Include at least one knowledgeGated channel.
- {{giveawayRule}}
- Mix testimony and physical traces: a group that only talks, or only searches, should still get far.
- A fact a beat may put into the world later still needs a way to learn it afterwards; the arc adds those once the beats exist.
- reveal is what the players perceive, in plain words, not the fact statement: one or two short sentences.
- Keep to the budget: two channels for each load-bearing fact (a third only when it opens a different kind of way in), one for each other fact and decoy fact. A case with more channels is not fairer, only longer to run.
