namespace WinxCoop.RuntimeProbe;

internal sealed record NativeTemplateDescriptor(uint Id, uint Type, uint ClassId, uint Table,
    int Offset, int PropertyOffset, int PropertyBytes);

// Pinned diagnostic inputs, not an alternate game-template serializer.
// Derived init, retained neutral and controlled movement require explicit modes;
// the derived input does not inherit stock walk/turn permission.
internal sealed record NativeTemplateLayout(string Name, string Sha256, int Bytes,
    uint MachineClassId, uint MachineTable, uint? ExpectedKind, int? KindSourceOffset,
    bool IsDerivedBloom, IReadOnlyList<NativeTemplateDescriptor> Descriptors)
{
    internal const string StockSha256 = "617EF85C33E9E447565CBF83A08FA2476D22F1C24570B59A6D21D4BEA61A7296";
    internal const string BloomInitSha256 = "517238FA954F087A22ED7DFEE090800A85718761187D5A8C7188AC67B4F3DA9A";
    internal const string BloomBodySha256 = "9E1EC20BFC1D426D838BEDC7C35B92C55FD78F2F620AEB70F96F47DB6A2FEF49";
    internal const string PlayerTraitsBodySha256 = "5272566F84FD0E51E58DEC8E0153917828B33A792DF50CB9E41768596354B1E9";
    internal const string TargetRegionBodySha256 = "21A604B591E328DAE83566C6C1935826AB53CE8AD84B541AD02AE80F67E99C84";
    internal const string ProjectileManagerBodySha256 = "70818B42F9592109F276B379176E8F212F1D2F21BCE11D628306E27D53EA15E6";
    internal bool IsBloomBody => Sha256 is BloomBodySha256 or PlayerTraitsBodySha256 or TargetRegionBodySha256 or ProjectileManagerBodySha256;
    internal uint TraitsClassId => Descriptors.Single(d => d.ClassId is 0x1BC6108E or 0x28C56217).ClassId;

    private static readonly NativeTemplateLayout Stock = new("stock-static-seresa", StockSha256, 4298,
        0x78F35E7F, 0x6FB280, 0x40, 2669, false, Array.AsReadOnly<NativeTemplateDescriptor>(
        [
            new(5, 0, 0x695C0F65, 0x6DC4F4, 76, 0, 0),
            new(1, 1, 0x0003CC73, 0x6F6438, 532, 988, 215),
            new(2, 1, 0x1BC6108E, 0x6F67F0, 1203, 1659, 400),
            new(0, 1, 0x78F35E7F, 0x6FB280, 2059, 2515, 182),
            new(4, 1, 0xCD2B2876, 0x6F670C, 2697, 3153, 127),
            new(3, 1, 0x680838E4, 0x6F64F8, 3280, 3736, 562)
        ]));

    private static readonly NativeTemplateLayout BloomInit = new("derived-seresa-bloom", BloomInitSha256, 4281,
        0x37810000, 0x6FA288, null, null, true, Array.AsReadOnly<NativeTemplateDescriptor>(
        [
            new(5, 0, 0x695C0F65, 0x6DC4F4, 76, 0, 0),
            new(1, 1, 0x0003CC73, 0x6F6438, 532, 988, 215),
            new(2, 1, 0x1BC6108E, 0x6F67F0, 1203, 1659, 400),
            new(0, 1, 0x37810000, 0x6FA288, 2059, 2515, 165),
            new(4, 1, 0xCD2B2876, 0x6F670C, 2680, 3136, 127),
            new(3, 1, 0x680838E4, 0x6F64F8, 3263, 3719, 562)
        ]));

    // This fixture changes only the fixed body-asset path. Its native graph is
    // a separate qualification; only explicit body modes admit retained/control use.
    private static readonly NativeTemplateLayout BloomBody = BloomInit with
        { Name = "derived-bloom-body", Sha256 = BloomBodySha256 };

    // E3 admission candidate: one complete stock PlayerTraits descriptor.
    // Parse/scoped init/delete does not imply retained-player lifecycle safety.
    private static readonly NativeTemplateLayout PlayerTraitsBody = new("derived-bloom-playertraits-body", PlayerTraitsBodySha256, 4632,
        0x37810000, 0x6FA288, null, null, true, Array.AsReadOnly<NativeTemplateDescriptor>(
        [
            new(5, 0, 0x695C0F65, 0x6DC4F4, 76, 0, 0),
            new(1, 1, 0x0003CC73, 0x6F6438, 532, 988, 215),
            new(2, 1, 0x28C56217, 0x6F6990, 1203, 1659, 751),
            new(0, 1, 0x37810000, 0x6FA288, 2410, 2866, 165),
            new(4, 1, 0xCD2B2876, 0x6F670C, 3031, 3487, 127),
            new(3, 1, 0x680838E4, 0x6F64F8, 3614, 4070, 562)
        ]));

    private static readonly NativeTemplateLayout TargetRegionBody = PlayerTraitsBody with
    {
        Name = "derived-bloom-playertraits-targetregion-body", Sha256 = TargetRegionBodySha256, Bytes = 5377,
        Descriptors = Array.AsReadOnly(PlayerTraitsBody.Descriptors.Concat(new NativeTemplateDescriptor[]
        { new(6, 1, 0x4307193A, 0x6F7ED0, 4632, 5088, 289) }).ToArray())
    };

    // E4 structural candidate: append the complete, unmodified stock manager.
    // Its presence does not admit attacks, pools, a player role or live input.
    private static readonly NativeTemplateLayout ProjectileManagerBody = TargetRegionBody with
    {
        Name = "derived-bloom-playertraits-targetregion-projectile-manager-body",
        Sha256 = ProjectileManagerBodySha256, Bytes = 7227,
        Descriptors = Array.AsReadOnly(TargetRegionBody.Descriptors.Concat(new NativeTemplateDescriptor[]
        { new(9, 1, 0x551015C7, 0x6F73D8, 5377, 5833, 1394) }).ToArray())
    };

    internal static NativeTemplateLayout Select(ProbeConfiguration config)
    {
        string expected = config.TemplateSha256 ?? StockSha256;
        if (config.TemplateProjectileManagerFixture)
        {
            if (config.TemplatePlayerTraitsFixture && config.TemplateTargetRegionFixture &&
                expected == ProjectileManagerBodySha256 &&
                (config.Mode == "template-bloom-body-hair-init" && !config.TemplateBodyInputFixture && !config.TemplateOrdinaryAttackFixture && !config.TemplateNetworkInputFixture ||
                 config.Mode == "template-bloom-body-hair-idle" && config.TemplatePlayerTraitsPickupFx &&
                 config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias &&
                 (config.TemplateBodyInputFixture && config.TemplateSharedPlayerProfile == "alfea-28-input" ||
                  config.TemplateNetworkInputFixture && config.TemplateSharedPlayerProfile == "alfea-28-network-input" ||
                  config.TemplateOrdinaryAttackFixture && config.TemplateSharedPlayerProfile == "battle-01-ordinary-attack")) &&
                !string.IsNullOrWhiteSpace(config.TemplateFile)) return ProjectileManagerBody;
            throw new InvalidDataException("Projectile manager body requires its exact scoped init/delete or separate Alfea retained-input fixture.");
        }
        if (config.TemplateTargetRegionFixture)
        {
            if (config.TemplatePlayerTraitsFixture && expected == TargetRegionBodySha256 &&
                (config.Mode == "template-bloom-body-hair-init" || config.Mode == "template-bloom-body-hair-idle" &&
                config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias) &&
                !string.IsNullOrWhiteSpace(config.TemplateFile)) return TargetRegionBody;
            throw new InvalidDataException("TargetRegion body requires its exact scoped init/delete fixture.");
        }
        if (config.TemplatePlayerTraitsFixture)
        {
            if (expected == PlayerTraitsBodySha256 && (config.Mode == "template-bloom-body-hair-init" ||
                config.Mode == "template-bloom-body-hair-idle" && config.TemplatePlayerTraitsPickupFx) &&
                !string.IsNullOrWhiteSpace(config.TemplateFile)) return PlayerTraitsBody;
            throw new InvalidDataException("E3 PlayerTraits body requires its exact scoped init/delete fixture.");
        }
        if (expected == BloomBodySha256 && config.Mode is ("template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump") &&
            !string.IsNullOrWhiteSpace(config.TemplateFile)) return BloomBody;
        if (expected == StockSha256 && config.Mode is not ("template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn")) return Stock;
        if (expected == BloomInitSha256 && config.Mode is ("template-init" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn") &&
            !string.IsNullOrWhiteSpace(config.TemplateFile)) return BloomInit;
        throw new InvalidDataException("Template pin/mode mismatch; derived Bloom requires an explicit supported Bloom mode or init.");
    }
}
