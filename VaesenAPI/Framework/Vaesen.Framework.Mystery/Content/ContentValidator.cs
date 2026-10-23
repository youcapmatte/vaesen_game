using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Content;

/// <summary>
/// Checks a content pack before it is used (spec 5.6): every tag is in the vocabulary, every reference resolves,
/// enumerated values are known, and the catalogues fit together (each creature has a level and a conflict it can use).
/// </summary>
public static class ContentValidator
{
    public static List<CoherenceIssue> Validate(ContentPack pack)
    {
        var issues = new List<CoherenceIssue>();
        var vocab = pack.Vocabulary;

        CheckVocabulary(vocab, issues);
        CheckSetting(pack.Setting, issues);
        CheckUniqueIds(pack, issues);

        foreach (var vaesen in pack.Vaesen)
        {
            CheckVaesen(vaesen, pack, issues);
        }

        foreach (var template in pack.PlaceTemplates)
        {
            CheckTemplate(template, vocab, issues);
        }

        foreach (var level in pack.Levels)
        {
            CheckLevel(level, pack, issues);
        }

        foreach (var conflict in pack.Conflicts)
        {
            CheckConflict(conflict, vocab, issues);
        }

        foreach (var character in pack.Characters)
        {
            CheckCharacter(character, pack, issues);
        }

        return issues;
    }

    private static void CheckVocabulary(AffordanceVocabulary vocab, List<CoherenceIssue> issues)
    {
        foreach (var duplicate in vocab.Tags.GroupBy(t => t.Id).Where(g => g.Count() > 1))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Vocabulary, $"vocabulary:{duplicate.Key}", "The tag is defined more than once."));
        }

        foreach (var tag in vocab.Tags)
        {
            if (!AffordanceKinds.All.Contains(tag.Kind))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Vocabulary, $"vocabulary:{tag.Id}", $"Unknown tag kind '{tag.Kind}'.{CoherenceIssue.Allowed(AffordanceKinds.All)}"));
            }

            foreach (var implied in tag.Implies.Where(i => !vocab.Contains(i)))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Vocabulary, $"vocabulary:{tag.Id}", $"Implies unknown tag '{implied}'."));
            }
        }
    }

    private static void CheckSetting(Setting setting, List<CoherenceIssue> issues)
    {
        if (setting.WorldTruths.Count == 0)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, "setting", "The setting has no world truths."));
        }

        if (setting.HookChannels.Count == 0)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, "setting", "The setting has no hook channels."));
        }

        foreach (var channel in setting.HookChannels)
        {
            foreach (var scale in channel.Scales.Where(s => s is not (HookScales.Private or HookScales.Local or HookScales.Regional or HookScales.National)))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"setting:hookChannel:{channel.Id}", $"Unknown scale '{scale}'."));
            }
        }
    }

    private static void CheckUniqueIds(ContentPack pack, List<CoherenceIssue> issues)
    {
        var all = pack.Vaesen.Cast<ContentRecord>()
            .Concat(pack.PlaceTemplates).Concat(pack.Levels).Concat(pack.Conflicts).Concat(pack.Characters);
        foreach (var record in all.Where(r => string.IsNullOrWhiteSpace(r.Id) || string.IsNullOrWhiteSpace(r.Name)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"content:{record.Id}", "Every record needs an id and a name."));
        }

        foreach (var record in all.Where(r => r.Status is not (ContentStatus.Draft or ContentStatus.Approved or ContentStatus.Retired)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"content:{record.Id}", $"Unknown status '{record.Status}'."));
        }
    }

    private static void CheckVaesen(VaesenDefinition v, ContentPack pack, List<CoherenceIssue> issues)
    {
        var at = $"vaesen:{v.Id}";
        CheckTags(v.RequiredAffordances, pack.Vocabulary, at, issues);
        CheckTags(v.ConfrontationConditions.PlaceTags, pack.Vocabulary, at, issues);

        if (v.RequiredAffordances.Count == 0)
        {
            issues.Add(CoherenceIssue.Warning(IssueCodes.Content, at, "No required affordances: it fits every level."));
        }

        foreach (var kind in v.ReasonKinds.Where(k => !ReasonKinds.All.Contains(k)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Unknown reason kind '{kind}'.{CoherenceIssue.Allowed(ReasonKinds.All)}"));
        }

        if (v.ReasonKinds.Count == 0)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "No reason kinds."));
        }

        if (!ResolutionShapes.All.Contains(v.ResolutionShape))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Unknown resolution shape '{v.ResolutionShape}'.{CoherenceIssue.Allowed(ResolutionShapes.All)}"));
        }

        if (v.ReasonKinds.Contains(ReasonKinds.Affliction) && v.OriginSet.Count == 0)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "An affliction needs an origin set."));
        }

        foreach (var origin in v.OriginSet)
        {
            if (origin.HiddenHand && !HandDispositions.All.Contains(origin.HandDisposition ?? ""))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"{at}:origin:{origin.Id}", "A hidden-hand origin needs a hand disposition."));
            }
        }

        if (v.DeepLore.Count < 4)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "Too little deep lore to build varied solution trails (need at least 4)."));
        }

        if (v.AmbiguousSigns.Count < 4)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "Too few ambiguous signs for openings (need at least 4)."));
        }

        if (v.GiveawayTells.Count == 0)
        {
            issues.Add(CoherenceIssue.Warning(IssueCodes.Content, at, "No giveaway tells listed, so the validator cannot keep them out of openings."));
        }

        foreach (var sign in v.AmbiguousSigns.Where(s => s.AlsoFits.Count == 0))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"{at}:sign:{sign.Id}", "An ambiguous sign must name what else it fits."));
        }

        foreach (var sign in v.AmbiguousSigns.Concat(v.GiveawayTells).Where(s => !SignRegisters.All.Contains(s.Register)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"{at}:sign:{sign.Id}", $"Unknown register '{sign.Register}'.{CoherenceIssue.Allowed(SignRegisters.All)}"));
        }

        var loreIds = v.AllLore.Select(l => l.Id).Concat(v.AmbiguousSigns.Select(s => s.Id)).Concat(v.GiveawayTells.Select(s => s.Id)).ToList();
        foreach (var duplicate in loreIds.GroupBy(i => i).Where(g => g.Count() > 1))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Lore or sign id '{duplicate.Key}' is used twice."));
        }

        if (!ConfrontationKinds.All.Contains(v.ConfrontationConditions.Kind))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Unknown confrontation kind '{v.ConfrontationConditions.Kind}'.{CoherenceIssue.Allowed(ConfrontationKinds.All)}"));
        }

        foreach (var month in v.ExcludedMonths.Where(m => m is < 1 or > 12))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Excluded month {month} is not 1-12."));
        }

        if (pack.Conflicts.Count > 0 && !pack.Conflicts.Exists(c => c.Tags.Intersect(v.CompatibleConflictTags).Any()))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "No conflict archetype carries any of its compatible conflict tags."));
        }

        if (pack.Levels.Count > 0 && !pack.Levels.Exists(l => LevelFits(l, v, pack.Vocabulary)))
        {
            issues.Add(CoherenceIssue.Warning(IssueCodes.Content, at, "No level offers all its required affordances, so it can never be assigned."));
        }
    }

    /// <summary>True if the level offers every affordance the creature requires (with implied tags).</summary>
    public static bool LevelFits(LevelDefinition level, VaesenDefinition vaesen, AffordanceVocabulary vocab)
    {
        var offered = vocab.ExpandAll(level.AllTags());
        return vaesen.RequiredAffordances.All(offered.Contains);
    }

    private static void CheckTemplate(PlaceTemplate t, AffordanceVocabulary vocab, List<CoherenceIssue> issues)
    {
        var at = $"place:{t.Id}";
        if (vocab.Find(t.Tag)?.Kind != AffordanceKinds.Place)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Vocabulary, at, $"Template tag '{t.Tag}' is not a place tag in the vocabulary."));
        }

        CheckTags(t.Tags, vocab, at, issues);
        foreach (var type in t.SettlementTypes.Where(s => !SettlementTypes.All.Contains(s)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Unknown settlement type '{type}'."));
        }

        var pending = new Stack<AffordanceSlot>(t.Affordances);
        while (pending.Count > 0)
        {
            var slot = pending.Pop();
            CheckTags([slot.Tag], vocab, $"{at}:{slot.Id}", issues);
            if (slot.Condition.MinCount > slot.Condition.MaxCount)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"{at}:{slot.Id}", "minCount is above maxCount."));
            }

            foreach (var inner in slot.Contains)
            {
                pending.Push(inner);
            }
        }
    }

    private static void CheckLevel(LevelDefinition level, ContentPack pack, List<CoherenceIssue> issues)
    {
        var at = $"level:{level.Id}";
        if (level.Month is < 1 or > 12)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Month {level.Month} is not 1-12."));
        }

        if (!SettlementTypes.All.Contains(level.SettlementType))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Unknown settlement type '{level.SettlementType}'."));
        }

        CheckTags(level.Terrain, pack.Vocabulary, at, issues);
        if (level.Places.Count == 0)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "A level needs places."));
        }

        var ids = new HashSet<string>(StringComparer.Ordinal);
        foreach (var place in level.Places)
        {
            var placeAt = $"{at}:{place.Id}";
            if (!ids.Add(place.Id))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Content, placeAt, "Place id is used twice in the level."));
            }

            var template = pack.FindTemplate(place.TemplateId);
            if (template is null)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.UnknownReference, placeAt, $"Unknown place template '{place.TemplateId}'."));
            }
            else if (!place.Tags.Contains(template.Tag))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Content, placeAt, $"A place from template '{template.Id}' must carry the tag '{template.Tag}'."));
            }

            CheckTags(place.Tags, pack.Vocabulary, placeAt, issues);
            foreach (var affordance in place.AllAffordances())
            {
                if (!ids.Add(affordance.Id))
                {
                    issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"{placeAt}:{affordance.Id}", "Affordance id is used twice in the level."));
                }

                CheckTags([affordance.Tag], pack.Vocabulary, $"{placeAt}:{affordance.Id}", issues);
            }
        }
    }

    private static void CheckConflict(ConflictArchetype c, AffordanceVocabulary vocab, List<CoherenceIssue> issues)
    {
        var at = $"conflict:{c.Id}";
        CheckTags(c.ProvocationAffordances, vocab, at, issues);
        foreach (var kind in c.ReasonKinds.Where(k => !ReasonKinds.All.Contains(k)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, $"Unknown reason kind '{kind}'.{CoherenceIssue.Allowed(ReasonKinds.All)}"));
        }

        if (c.RoleSlots.Count == 0)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "A conflict needs human role slots."));
        }

        if (c.Tags.Count == 0)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "A conflict needs tags to match creatures against."));
        }
    }

    private static void CheckCharacter(CharacterDefinition c, ContentPack pack, List<CoherenceIssue> issues)
    {
        var at = $"character:{c.Id}";
        if (c.StartingAttitude is < -3 or > 3)
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "Starting attitude must be -3 to 3."));
        }

        var p = c.Personality;
        if (new[] { p.Openness, p.Conscientiousness, p.Extraversion, p.Agreeableness, p.Neuroticism }.Any(v => v is < 1 or > 5))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "Personality traits must be 1-5."));
        }

        foreach (var hook in c.LatentHooks.Where(h => !LatentHookKinds.All.Contains(h.Kind)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, $"{at}:{hook.Id}", $"Unknown latent hook kind '{hook.Kind}'."));
        }

        var institutions = pack.Setting.Institutions.Select(i => i.Id).ToHashSet();
        foreach (var link in c.NetworkLinks.Where(l => !institutions.Contains(l.TargetId) && pack.FindCharacter(l.TargetId) is null))
        {
            issues.Add(CoherenceIssue.Warning(IssueCodes.UnknownReference, at, $"Network link to '{link.TargetId}', which is neither an institution nor a canonical character."));
        }

        if (c.Drives.Count == 0 || string.IsNullOrWhiteSpace(c.Voice.Register))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Content, at, "A character needs drives and a voice."));
        }
    }

    private static void CheckTags(IEnumerable<string> tags, AffordanceVocabulary vocab, string at, List<CoherenceIssue> issues)
    {
        foreach (var tag in tags.Where(t => !vocab.Contains(t)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Vocabulary, at, $"Tag '{tag}' is not in the vocabulary."));
        }
    }
}
