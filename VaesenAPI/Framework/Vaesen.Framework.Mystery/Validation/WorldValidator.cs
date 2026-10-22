using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Validation;

/// <summary>
/// Checks a Society's world (spec 9.1, step 4): every canonical character present, ids unique, values in range, ties
/// pointing at people who exist. Run after seeding and after every write-back.
/// </summary>
public static class WorldValidator
{
    public static List<CoherenceIssue> Validate(SocietyWorld world, ContentPack pack)
    {
        var issues = new List<CoherenceIssue>();
        foreach (var canon in pack.Characters.Where(c => !world.Characters.Exists(i => i.DefinitionId == c.Id)))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.CastCoherence, $"world:{canon.Id}", $"The canonical character {canon.Name} is missing from the world."));
        }

        foreach (var duplicate in world.Characters.GroupBy(c => c.Id).Where(g => g.Count() > 1))
        {
            issues.Add(CoherenceIssue.Error(IssueCodes.Schema, $"world:{duplicate.Key}", "Character id is used twice.", "Give each person their own id."));
        }

        var institutions = pack.Setting.Institutions.Select(i => i.Id).ToHashSet();
        foreach (var c in world.Characters)
        {
            var at = $"world:{c.Id}";
            if (string.IsNullOrWhiteSpace(c.Id) || string.IsNullOrWhiteSpace(c.Name))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, "A character needs an id and a name."));
            }

            if (c.DefinitionId is null && pack.FindCharacter(c.Id) is not null)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, "The id belongs to a canonical character.", "Use a new npc-firstname-lastname id."));
            }

            if (!WorldTiers.All.Contains(c.Tier))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, $"Unknown tier '{c.Tier}'."));
            }

            if (!CharacterStatuses.All.Contains(c.Status))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, $"Unknown status '{c.Status}'."));
            }

            if (c.Attitude is < -3 or > 3)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, "Attitude must be -3 to 3."));
            }

            if (c.Importance is < 0 or > 3)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, "Importance must be 0 to 3."));
            }

            var p = c.Personality;
            if (new[] { p.Openness, p.Conscientiousness, p.Extraversion, p.Agreeableness, p.Neuroticism }.Any(v => v is < 1 or > 5))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, "Personality traits must be 1-5."));
            }

            if (c.Tier is WorldTiers.Core or WorldTiers.Canonical && (c.Drives.Count == 0 || string.IsNullOrWhiteSpace(c.Voice.Register)))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, "A core character needs drives and a voice."));
            }

            if (c.Tier == WorldTiers.Background && c.LatentHooks.Count == 0)
            {
                issues.Add(CoherenceIssue.Warning(IssueCodes.Schema, at, "A background character should carry latent hooks the engine can promote."));
            }

            foreach (var rel in c.Relationships.Where(r => world.Find(r.CharacterId) is null))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.UnknownReference, at, $"Relationship to '{rel.CharacterId}', who is not in the world.", "Point it at a character of this world, or drop it."));
            }

            foreach (var rel in c.Relationships.Where(r => r.Sentiment is < -3 or > 3))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, $"Sentiment toward '{rel.CharacterId}' must be -3 to 3."));
            }

            foreach (var link in c.NetworkLinks.Where(l => !institutions.Contains(l.TargetId) && world.Find(l.TargetId) is null))
            {
                issues.Add(CoherenceIssue.Warning(IssueCodes.UnknownReference, at, $"Network link to '{link.TargetId}', which is neither an institution nor a character of this world."));
            }
        }

        return issues;
    }
}
