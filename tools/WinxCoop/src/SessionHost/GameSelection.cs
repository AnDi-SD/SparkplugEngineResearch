namespace WinxCoop.SessionHost;

// Fast installed-game selection only. This object grants no executable/content or peer
// compatibility qualification; the retained worker performs the complete SHA.
internal sealed class GameSelection
{
    internal string GameRoot { get; }
    internal string Executable => Path.Combine(GameRoot, "WinxClub.exe");
    internal string Media => Path.Combine(GameRoot, "Media");

    internal GameSelection(string gameRoot, uint initialLevel = 28)
    {
        GameRoot = SessionConfiguration.AbsoluteAscii(gameRoot);
        RuntimeBundle.Ordinary(GameRoot, true);
        RuntimeBundle.Ordinary(Media, true);
        RuntimeBundle.Ordinary(Path.Combine(GameRoot, "Shaders"), true);
        RuntimeBundle.Ordinary(Executable, false);
        Require(initialLevel is 28 or CampaignSceneProfile.FirstLevel, "Unknown initial scene.");
        // Native scene admission remains explicit, but selecting an installed
        // game neither reads nor changes its original startup configuration.
    }

    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
}
