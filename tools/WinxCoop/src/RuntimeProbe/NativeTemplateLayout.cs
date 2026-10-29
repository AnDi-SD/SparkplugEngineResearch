namespace WinxCoop.RuntimeProbe;

internal sealed record NativeTemplateDescriptor(uint Id, uint Type, uint ClassId, uint Table,
    int Offset, int PropertyOffset, int PropertyBytes);

// Two pinned diagnostic inputs, not an alternate game-template serializer.
// Derived init, retained neutral and controlled movement require explicit modes;
// the derived input does not inherit stock walk/turn permission.
internal sealed record NativeTemplateLayout(string Name, string Sha256, int Bytes,
    uint MachineClassId, uint MachineTable, uint? ExpectedKind, int? KindSourceOffset,
    bool IsDerivedBloom, IReadOnlyList<NativeTemplateDescriptor> Descriptors)
{
    internal const string StockSha256 = "617EF85C33E9E447565CBF83A08FA2476D22F1C24570B59A6D21D4BEA61A7296";
    internal const string BloomInitSha256 = "517238FA954F087A22ED7DFEE090800A85718761187D5A8C7188AC67B4F3DA9A";

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

    internal static NativeTemplateLayout Select(ProbeConfiguration config)
    {
        string expected = config.TemplateSha256 ?? StockSha256;
        if (expected == StockSha256 && config.Mode is not ("template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn")) return Stock;
        if (expected == BloomInitSha256 && config.Mode is ("template-init" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn") &&
            !string.IsNullOrWhiteSpace(config.TemplateFile)) return BloomInit;
        throw new InvalidDataException("Template pin/mode mismatch; derived Bloom requires an explicit supported Bloom mode or init.");
    }
}
