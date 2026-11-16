using System.Text.Json;
using System.Runtime.InteropServices;

namespace WinxCoop.SessionHost;

internal static class SessionMain
{
    [STAThread]
    private static int Main(string[] arguments)
    {
        LauncherLocalState? local = null;
        bool gui = arguments.Length == 0 && !File.Exists((Environment.ProcessPath ?? "") + ".json");
        try
        {
            local = new LauncherLocalState();
            local.Write("process-start", new { executable = Environment.ProcessPath,
                framework = RuntimeInformation.FrameworkDescription, gui, argumentCount = arguments.Length,
                localData = local.RootDirectory, launcherLog = local.LogPath });
            string adjacent = (Environment.ProcessPath ?? throw new InvalidOperationException("Missing application path.")) + ".json";
            if (arguments.Length == 2 && arguments[0] == "--worker" || arguments.Length == 0 && File.Exists(adjacent))
            {
                string path = arguments.Length == 2 ? arguments[1] : adjacent;
                var config = JsonSerializer.Deserialize<SessionConfiguration>(File.ReadAllText(path), SessionConfiguration.Json)
                    ?? throw new InvalidDataException("Missing worker configuration.");
                config.Validate();
                SessionConfiguration.Require(config.Worker, "A native-parent worker configuration is required.");
                local.Write("worker-start", new { config.Role, config.CaseDirectory });
                int result = SessionWorker.Run(config);
                local.TryWrite("worker-exit", new { exitCode = result, config.Role, config.CaseDirectory });
                return result;
            }
            if (arguments.Length == 4 && arguments[0] == "--check-profile" && arguments[2] == "--report")
            {
                string directory = AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
                var config = new SessionConfiguration { BundleDirectory = directory, GameRoot = Path.GetFullPath(arguments[1]), CaseDirectory = local.CasesDirectory };
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
            local.Write("program-bundle-opened", new { source = uiBundle.DirectoryPath,
                uiBundle.Manifest.Architecture, uiBundle.Manifest.SourceProfileSha256 });
            LauncherLocalState logger = local;
            AppDomain.CurrentDomain.UnhandledException += (_, failure) =>
                logger.TryWrite("unhandled-exception", new { error = failure.ExceptionObject.ToString(), failure.IsTerminating });
            Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
            Application.ThreadException += (_, failure) =>
            {
                logger.TryWrite("gui-unhandled-exception", new { error = failure.Exception.ToString() });
                MessageBox.Show(failure.Exception.Message + "\n\n" + logger.LogPath, "WinxCoop",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            };
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new LobbyForm(uiBundle, local));
            local.TryWrite("gui-closed");
            return 0;
        }
        catch (Exception error)
        {
            // Worker writes its own complete report; an early CLI failure is
            // retained without opening a dialog over an unattended game test.
            local?.TryWrite("process-failed", new { error = error.ToString() });
            if (gui)
                MessageBox.Show(error.Message + (local is null ? "" : "\n\n" + local.LogPath), "WinxCoop", MessageBoxButtons.OK, MessageBoxIcon.Error);
            else
            {
                try { Console.Error.WriteLine(error); }
                catch (IOException) { }
            }
            return 4;
        }
        finally { local?.Dispose(); }
    }
}
