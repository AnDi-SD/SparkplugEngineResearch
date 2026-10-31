namespace WinxCoop.RuntimeProbe;

/// <summary>Closed original-scene cohorts; a scene selector is not arbitrary level admission.</summary>
internal sealed record NativeSharedPlayerProfile(string Name, uint Level, uint PlayingTable,
    uint TypeGetter, uint ExitEntry, bool LiveResources, bool Story)
{
    internal static NativeSharedPlayerProfile Select(ProbeConfiguration config) => config.TemplateSharedPlayerProfile switch
    {
        null when config.TemplateHealthBattleFixture => new("battle-01", 47, 0x7105C8, 0x5F2080, 0x5F2300, true, false),
        null => new("alfea-28", 28, 0x710874, 0x5F5450, 0x5F86C0, false, false),
        "battle-01-projectile-init" when config.TemplateProjectileManagerFixture =>
            new("battle-01-projectile-init", 47, 0x7105C8, 0x5F2080, 0x5F2300, false, false),
        "alfea-28-input" when config.TemplateBodyInputFixture =>
            new("alfea-28-input", 28, 0x710874, 0x5F5450, 0x5F86C0, true, true),
        "alfea-28-network-input" when config.TemplateNetworkInputFixture =>
            new("alfea-28-network-input", 28, 0x710874, 0x5F5450, 0x5F86C0, true, true),
        "battle-01-ordinary-attack" when config.TemplateOrdinaryAttackFixture =>
            new("battle-01-ordinary-attack", 47, 0x7105C8, 0x5F2080, 0x5F2300, true, false),
        "gardenia-03" => new("gardenia-03", 3, 0x710874, 0x5F5450, 0x5F86C0, true, true),
        _ => throw new InvalidDataException("Unknown bounded shared-player scene profile.")
    };
}
