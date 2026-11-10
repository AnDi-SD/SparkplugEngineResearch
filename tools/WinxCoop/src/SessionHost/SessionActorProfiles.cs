using System.Text.Json.Nodes;

namespace WinxCoop.SessionHost;

// Explicit native admission profiles. These values compose the existing
// qualified adapters; a lobby level number cannot broaden their native gates.
internal static class SessionActorProfiles
{
    internal static JsonObject Alfea(SessionConfiguration session, GameProfile game, uint epoch, uint generation,
        string? template, byte[]? seed = null, byte[]? witness = null, string? savedSha256 = null)
    {
        Require(epoch > 0 && generation > 0 && (seed is null) == (witness is null) &&
            (seed is null || seed.Length == 448 && witness!.Length == 64), "Invalid authoritative entry seed/witness.");
        Require(session.Host || seed is not null, "Guest must receive the actual Host entry before Actor construction.");
        Require(savedSha256 is null || session.Host && RuntimeBundle.Digest(savedSha256) && seed is not null,
            "Only a continued Host may consume a server checkpoint.");
        var config = new JsonObject
        {
            ["Executable"] = game.Executable, ["WorkingDirectory"] = game.GameRoot,
            ["MediaDirectory"] = game.Media, ["SampleEvery"] = 15,
            ["Mode"] = session.Host ? "template-bloom-body-hair-idle" : "client-presentation-live",
            ["EnableWorldEntry"] = true, ["WorldEntryCaptureOrdinal"] = savedSha256 is null ? 1 : 2,
            ["EnableProgressionAuthority"] = true, ["UseProcessLocalMediaPath"] = true,
            ["ProgressionSceneLevel"] = 28, ["WorldBattlePresentation"] = false,
            ["TemplateNetworkInputBackpressureFixture"] = false,
            ["ProgressionSavedReload"] = session.Host,
            ["ProgressionPreWaitingReloadPreparation"] = session.Host,
            ["ProgressionHostContinuationSeed"] = session.Host && seed is not null
        };
        if (seed is not null)
        {
            config["WorldEntrySeed"] = Convert.ToBase64String(seed);
            config["WorldEntryExpectedWitness"] = Convert.ToBase64String(witness!);
        }
        if (savedSha256 is not null) config["InitialSaveSha256"] = savedSha256;
        if (session.Host)
        {
            Require(template is not null, "The Host requires its locally generated body template.");
            config["TemplateFile"] = template;
            config["TemplateSha256"] = "70818B42F9592109F276B379176E8F212F1D2F21BCE11D628306E27D53EA15E6";
            config["TemplateSharedPlayerProfile"] = "alfea-28-network-input";
            config["TemplateHostBackgroundFixture"] = true;
            config["TemplateNetworkInputFixture"] = true;
            config["TemplateProjectileManagerFixture"] = true;
            config["TemplatePlayerTraitsFixture"] = true;
            config["TemplatePlayerTraitsPickupFx"] = true;
            config["TemplateTargetRegionFixture"] = true;
            config["TemplateConfigureTargetRegion"] = true;
            config["TemplateSharedHealthAlias"] = true;
        }
        else
        {
            config["DiagnosticGuestCaptureFixture"] = true;
            config["DiagnosticInputHeartbeatMilliseconds"] = 100;
            config["LimitContinuousGuestInput"] = true;
            config["WorldPresentationSmoothingMilliseconds"] = session.SmoothingMilliseconds;
            config["GuestPresentationBackgroundFixture"] = true;
            config["ProgressionNeutralCaptureWhileUnfocused"] = true;
        }
        return config;
    }

    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
}
