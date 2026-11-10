using System.Text.Json;

namespace WinxCoop.SessionHost;

internal static class SessionMain
{
    [STAThread]
    private static int Main(string[] arguments)
    {
        try
        {
            string adjacent = (Environment.ProcessPath ?? throw new InvalidOperationException("Missing application path.")) + ".json";
            if (arguments.Length == 2 && arguments[0] == "--worker" || arguments.Length == 0 && File.Exists(adjacent))
            {
                string path = arguments.Length == 2 ? arguments[1] : adjacent;
                var config = JsonSerializer.Deserialize<SessionConfiguration>(File.ReadAllText(path), SessionConfiguration.Json)
                    ?? throw new InvalidDataException("Missing worker configuration.");
                config.Validate();
                SessionConfiguration.Require(config.Worker, "A native-parent worker configuration is required.");
                return SessionWorker.Run(config);
            }
            if (arguments.Length == 4 && arguments[0] == "--check-profile" && arguments[2] == "--report")
            {
                string directory = AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
                var config = new SessionConfiguration { BundleDirectory = directory, GameRoot = Path.GetFullPath(arguments[1]), CaseDirectory = directory };
                config.Validate();
                using var bundle = new RuntimeBundle(directory);
                using var profile = new GameProfile(config, bundle);
                using var file = new FileStream(Path.GetFullPath(arguments[3]), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
                JsonSerializer.Serialize(file, new { passed = true, profile = config.Profile, gameRoot = profile.GameRoot,
                    gameResourceDigest = profile.MediaDigest, filesHashed = profile.FilesHashed, bytesHashed = profile.BytesHashed,
                    peerCompatibilityDigest = Convert.ToHexString(profile.CompatibilityDigest), gameStarted = false }, SessionConfiguration.Json);
                return 0;
            }
            SessionConfiguration.Require(arguments.Length == 0, "Use the launcher, --worker <config>, or --check-profile <game> --report <new.json>.");
            using var uiBundle = new RuntimeBundle(AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar));
            uiBundle.InstallNativeResolver();
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new LobbyForm(uiBundle));
            return 0;
        }
        catch (Exception error)
        {
            // Worker writes its own complete report; an early CLI failure is
            // retained without opening a dialog over an unattended game test.
            if (arguments.Length == 0 && !File.Exists((Environment.ProcessPath ?? "") + ".json"))
                MessageBox.Show(error.Message, "WinxCoop", MessageBoxButtons.OK, MessageBoxIcon.Error);
            else
            {
                try { File.AppendAllText(Path.Combine(AppContext.BaseDirectory, "WinxCoop.SessionHost.error.log"), error + Environment.NewLine); }
                catch (IOException) { }
            }
            return 4;
        }
    }
}
