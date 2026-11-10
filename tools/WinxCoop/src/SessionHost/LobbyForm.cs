using System.Diagnostics;
using System.Globalization;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Windows.Forms;

namespace WinxCoop.SessionHost;

// The bundle is borrowed from Main and must outlive the Form. All native
// session/process ownership remains in the separate worker and supervisor.
internal sealed class LobbyForm : Form
{
    private readonly RuntimeBundle _bundle;
    private readonly TextBox _game = new() { Dock = DockStyle.Fill };
    private readonly TextBox _cases = new() { Dock = DockStyle.Fill };
    private readonly TextBox _address = new() { Dock = DockStyle.Fill, Text = "127.0.0.1" };
    private readonly NumericUpDown _port = new() { Minimum = 1, Maximum = 65535, Value = 27045, Width = 90 };
    private readonly NumericUpDown _seconds = new() { Minimum = 60, Maximum = 600, Increment = 60, Value = 600, Width = 90 };
    private readonly Button _browseGame = new() { Text = "Выбрать…", AutoSize = true };
    private readonly Button _browseCases = new() { Text = "Выбрать…", AutoSize = true };
    private readonly Button _find = new() { Text = "Найти в локальной сети", AutoSize = true };
    private readonly Button _create = new() { Text = "Создать лобби", AutoSize = true };
    private readonly Button _join = new() { Text = "Подключиться", AutoSize = true };
    private readonly Button _start = new() { Text = "Начать игру", AutoSize = true };
    private readonly Button _checkpoint = new() { Text = "Контрольная точка", AutoSize = true };
    private readonly Button _load = new() { Text = "Загрузить контрольную точку", AutoSize = true };
    private readonly Button _stop = new() { Text = "Завершить сеанс", AutoSize = true };
    private readonly Button _openCase = new() { Text = "Открыть папку сеанса", AutoSize = true };
    private readonly ListView _lobbies = new()
    { Dock = DockStyle.Fill, View = View.Details, FullRowSelect = true, MultiSelect = false, HideSelection = false, Height = 130 };
    private readonly Label _status = new() { Dock = DockStyle.Fill, AutoSize = false, Text = "Выберите подготовленную копию игры и создайте лобби или подключитесь к серверу." };
    private readonly Label _role = new() { AutoSize = true, Text = "Сервер запускает игру и управляет прохождением." };
    private readonly System.Windows.Forms.Timer _timer = new() { Interval = 100 };
    private readonly ToolTip _tips = new();
    private SessionDiscovery? _discovery;
    private SessionParent? _parent;
    private CancellationTokenSource? _preparationCancellation;
    private bool _busy, _closing, _allowClose, _stopRequested;
    private string _discoverySignature = "", _lastCase = "";

    internal LobbyForm(RuntimeBundle bundle)
    {
        _bundle = bundle;
        Text = "WinxCoop";
        Font = new System.Drawing.Font("Segoe UI", 9F);
        AutoScaleMode = AutoScaleMode.Dpi;
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new System.Drawing.Size(880, 590);
        Size = new System.Drawing.Size(960, 650);
        _cases.Text = Path.Combine(bundle.DirectoryPath, "Cases");
        string prepared = Path.Combine(bundle.DirectoryPath, "PreparedGame");
        if (Directory.Exists(prepared)) _game.Text = prepared;

        var layout = new TableLayoutPanel
        { Dock = DockStyle.Fill, Padding = new Padding(16), ColumnCount = 1, RowCount = 8 };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 66));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        Controls.Add(layout);
        var title = new Label { AutoSize = true, Text = "WinxCoop — совместная игра", Font = new System.Drawing.Font(Font.FontFamily, 15F, System.Drawing.FontStyle.Bold), Margin = new Padding(0, 0, 0, 12) };
        layout.Controls.Add(title, 0, 0);
        var paths = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Top, ColumnCount = 3, RowCount = 3, Margin = new Padding(0, 0, 0, 12) };
        paths.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 170)); paths.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); paths.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        paths.Controls.Add(Caption("Подготовленная копия игры"), 0, 0); paths.Controls.Add(_game, 1, 0); paths.Controls.Add(_browseGame, 2, 0);
        paths.Controls.Add(Caption("Папка сеансов"), 0, 1); paths.Controls.Add(_cases, 1, 1); paths.Controls.Add(_browseCases, 2, 1);
        var note = new Label { AutoSize = true, Text = "Используйте отдельную копию игры, подготовленную WinxCoop. Сейчас доступна Алфея.", Margin = new Padding(0, 8, 0, 0) };
        paths.Controls.Add(note, 0, 2); paths.SetColumnSpan(note, 3); layout.Controls.Add(paths, 0, 1);

        var network = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 6, Margin = new Padding(0, 0, 0, 12) };
        network.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize)); network.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        for (int index = 2; index < 6; ++index) network.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        network.Controls.Add(Caption("Адрес сервера"), 0, 0); network.Controls.Add(_address, 1, 0);
        network.Controls.Add(Caption("Порт"), 2, 0); network.Controls.Add(_port, 3, 0);
        network.Controls.Add(Caption("Лимит сеанса, секунд"), 4, 0); network.Controls.Add(_seconds, 5, 0);
        layout.Controls.Add(network, 0, 2);
        var lobbyButtons = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Top, Margin = new Padding(0, 0, 0, 8) };
        lobbyButtons.Controls.AddRange([_create, _join, _find]); layout.Controls.Add(lobbyButtons, 0, 3);
        _lobbies.Columns.Add("Сервер", 250); _lobbies.Columns.Add("Адрес", 220); _lobbies.Columns.Add("Состояние", 150);
        layout.Controls.Add(_lobbies, 0, 4);
        var gameButtons = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Top, Margin = new Padding(0, 12, 0, 8) };
        gameButtons.Controls.AddRange([_start, _checkpoint, _load, _stop]); layout.Controls.Add(gameButtons, 0, 5);
        _status.Padding = new Padding(8); _status.BorderStyle = BorderStyle.FixedSingle; layout.Controls.Add(_status, 0, 6);
        var footer = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Top, Margin = new Padding(0, 10, 0, 0) };
        footer.Controls.Add(_openCase); footer.Controls.Add(_role); layout.Controls.Add(footer, 0, 7);
        _tips.SetToolTip(_checkpoint, "Сохранить контрольную точку и перезапустить обе игры.");
        _tips.SetToolTip(_load, "Загрузить последнюю принятую контрольную точку сервера и перезапустить обе игры.");
        _tips.SetToolTip(_seconds, "Сеанс автоматически завершится по этому лимиту. По умолчанию — 600 секунд.");

        _browseGame.Click += (_, _) => Browse(_game, "Выберите отдельную подготовленную копию игры");
        _browseCases.Click += (_, _) => Browse(_cases, "Выберите папку для новых сеансов");
        _create.Click += async (_, _) => await BeginSession(true);
        _join.Click += async (_, _) => await BeginSession(false);
        _find.Click += (_, _) => FindLobbies();
        _start.Click += (_, _) => Send("Start");
        _checkpoint.Click += (_, _) => Send(Can("canSave") ? "Save" : "Reload");
        _load.Click += (_, _) => Send("Load");
        _stop.Click += (_, _) => RequestStop();
        _openCase.Click += (_, _) => OpenCase();
        _lobbies.SelectedIndexChanged += (_, _) => SelectLobby();
        _lobbies.DoubleClick += async (_, _) => { if (_lobbies.SelectedItems.Count == 1) await BeginSession(false); };
        _timer.Tick += (_, _) => Pump();
        FormClosing += ClosingForm;
        FormClosed += (_, _) => { _timer.Dispose(); _tips.Dispose(); _discovery?.Dispose(); _preparationCancellation?.Dispose(); };
        RefreshButtons(); _timer.Start();
    }

    private async Task BeginSession(bool host)
    {
        if (_busy || _closing || _parent is { ReadyToDispose: false }) return;
        try
        {
            if (!host && _lobbies.SelectedItems.Count == 1 && _lobbies.SelectedItems[0].Tag is DiscoveredServer selected &&
                selected.Occupied && selected.Address == _address.Text.Trim() && selected.Port == _port.Value)
            { MessageBox.Show(this, "Это лобби уже занято. Выберите свободный сервер.", "WinxCoop", MessageBoxButtons.OK, MessageBoxIcon.Information); return; }
            _parent?.Dispose(); _parent = null; _stopRequested = false;
            string game = SessionConfiguration.AbsoluteAscii(_game.Text.Trim());
            string cases = SessionConfiguration.AbsoluteAscii(_cases.Text.Trim());
            Directory.CreateDirectory(cases); RuntimeBundle.Ordinary(cases, true);
            string casePath = Path.Combine(cases, "Session-" + DateTime.Now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + "-" + Guid.NewGuid().ToString("N")[..8]);
            var configuration = new SessionConfiguration
            {
                BundleDirectory = _bundle.DirectoryPath, GameRoot = game, CaseDirectory = casePath,
                Role = host ? "host" : "guest", BindAddress = "0.0.0.0", ServerAddress = _address.Text.Trim(),
                Port = checked((ushort)_port.Value), Seconds = checked((int)_seconds.Value), Profile = "alfea-28", SmoothingMilliseconds = 50
            };
            configuration.Validate();
            _busy = true; _preparationCancellation = new CancellationTokenSource();
            CancellationToken cancellation = _preparationCancellation.Token;
            _status.Text = "Проверяем отдельную копию игры и готовим сеанс…";
            RefreshButtons();
            var progress = new Progress<string>(message => { if (!IsDisposed && !_closing) _status.Text = message; });
            SessionParent parent = await Task.Run(() =>
            {
                using var qualified = new GameProfile(configuration, _bundle, message =>
                {
                    cancellation.ThrowIfCancellationRequested();
                    ((IProgress<string>)progress).Report(message);
                });
                cancellation.ThrowIfCancellationRequested();
                return SessionParent.Start(configuration, _bundle, qualified);
            });
            _parent = parent; _lastCase = parent.CaseDirectory;
            _role.Text = host ? "Вы — сервер. Управляете запуском и контрольными точками." : "Вы — клиент. Ожидаете решения сервера.";
            _stop.Text = host ? "Завершить сеанс" : "Отключиться";
            if (_closing) RequestStop();
        }
        catch (OperationCanceledException) { _status.Text = "Подготовка отменена."; }
        catch (Exception error) { ReportFailure(error, showDialog: !_closing); }
        finally
        {
            _busy = false; _preparationCancellation?.Dispose(); _preparationCancellation = null;
            RefreshButtons(); TryFinishClosing();
        }
    }

    private void Pump()
    {
        if (_discovery is not null)
        {
            try { _discovery.Pump(); RefreshLobbies(); }
            catch (Exception error)
            {
                SaveError(error); _discovery.Dispose(); _discovery = null;
                if (_parent is null && !_busy) _status.Text = "Поиск недоступен. Можно подключиться по адресу сервера.";
            }
        }
        if (_parent is not null)
        {
            _parent.Pump();
            if (_parent.Failure is not null && !_parent.OuterExited && !_stopRequested) RequestStop();
            if (_closing && !_parent.ReadyToDispose) _status.Text = "Ожидаем завершения и проверки сеанса…";
            else if (_parent.Failure is not null && !_parent.ReadyToDispose) _status.Text = "Сеанс завершился с ошибкой. Ожидаем завершения проверки…";
            else if (_parent.Status?["message"] is JsonValue value && value.TryGetValue<string>(out string? message)) _status.Text = message;
        }
        RefreshButtons(); TryFinishClosing();
    }

    private void RefreshButtons()
    {
        bool idle = !_busy && !_closing && (_parent is null || _parent.ReadyToDispose);
        foreach (Control control in new Control[] { _game, _cases, _address, _port, _seconds, _browseGame, _browseCases, _create, _join }) control.Enabled = idle;
        _find.Enabled = idle && _discovery?.Searching != true;
        _lobbies.Enabled = idle;
        bool active = !_busy && !_closing && _parent is { OuterExited: false, CleanupConfirmed: false } && _parent.Failure is null;
        bool server = active && _parent!.Host;
        _start.Enabled = server && Can("canStart");
        _checkpoint.Enabled = server && (Can("canSave") || Can("canReload"));
        _load.Enabled = server && Can("canLoad");
        _stop.Enabled = active && !_stopRequested;
        _openCase.Enabled = !string.IsNullOrWhiteSpace(_lastCase) && Directory.Exists(_lastCase);
    }
    private bool Can(string name) => _parent?.Status?[name] is JsonValue value && value.TryGetValue<bool>(out bool allowed) && allowed;

    private void Send(string action)
    {
        try { _parent?.Command(action); _status.Text = action == "Start" ? "Ожидаем готовности обеих игр…" : "Сервер выполняет переход. Дождитесь возвращения управления…"; }
        catch (Exception error) { ReportFailure(error, showDialog: !_closing); }
        RefreshButtons();
    }
    private void RequestStop()
    {
        if (_parent is null || _parent.OuterExited || _stopRequested) return;
        try { _parent.Command("Stop"); _stopRequested = true; _status.Text = "Завершаем сеанс. Дождитесь закрытия игровых процессов…"; }
        catch (Exception error) { SaveError(error); }
        RefreshButtons();
    }

    private void FindLobbies()
    {
        try
        {
            _discovery ??= new SessionDiscovery(); _discovery.BeginSearch();
            _status.Text = "Ищем свободные лобби в локальной сети…"; RefreshButtons();
        }
        catch (Exception error) { ReportFailure(error, showDialog: true); }
    }
    private void RefreshLobbies()
    {
        if (_discovery is null) return;
        string signature = string.Join("\n", _discovery.Lobbies.Select(lobby => lobby.Address + ":" + lobby.Port + "|" + lobby.Name + "|" + lobby.Occupied));
        if (signature == _discoverySignature) return;
        _discoverySignature = signature;
        string? selected = _lobbies.SelectedItems.Count == 1 ? _lobbies.SelectedItems[0].Name : null;
        _lobbies.BeginUpdate();
        try
        {
            _lobbies.Items.Clear();
            foreach (DiscoveredServer lobby in _discovery.Lobbies)
            {
                string address = lobby.Address + ":" + lobby.Port.ToString(CultureInfo.InvariantCulture);
                var item = new ListViewItem(lobby.Name) { Name = address, Tag = lobby };
                item.SubItems.Add(address); item.SubItems.Add(lobby.Occupied ? "Занято" : "Свободно");
                _lobbies.Items.Add(item); item.Selected = address == selected;
            }
        }
        finally { _lobbies.EndUpdate(); }
    }
    private void SelectLobby()
    {
        if (_lobbies.SelectedItems.Count != 1 || _lobbies.SelectedItems[0].Tag is not DiscoveredServer lobby) return;
        _address.Text = lobby.Address; _port.Value = lobby.Port;
    }
    private void Browse(TextBox target, string title)
    {
        using var dialog = new FolderBrowserDialog { Description = title, ShowNewFolderButton = true };
        if (Directory.Exists(target.Text)) dialog.SelectedPath = target.Text;
        if (dialog.ShowDialog(this) == DialogResult.OK) target.Text = dialog.SelectedPath;
    }
    private void OpenCase()
    {
        try { if (Directory.Exists(_lastCase)) Process.Start(new ProcessStartInfo { FileName = _lastCase, UseShellExecute = true }); }
        catch (Exception error) { ReportFailure(error, showDialog: true); }
    }

    private void ClosingForm(object? sender, FormClosingEventArgs arguments)
    {
        if (_allowClose) return;
        _closing = true;
        if (_busy) _preparationCancellation?.Cancel();
        RequestStop();
        if (_busy || _parent is { ReadyToDispose: false })
        {
            arguments.Cancel = true; _status.Text = "Завершаем сеанс. Это окно закроется после игровых процессов.";
            RefreshButtons(); return;
        }
        _parent?.Dispose(); _parent = null; _timer.Stop(); _allowClose = true;
    }
    private void TryFinishClosing()
    {
        if (!_closing || _allowClose || _busy || _parent is { ReadyToDispose: false }) return;
        _parent?.Dispose(); _parent = null; _timer.Stop(); _allowClose = true;
        BeginInvoke((Action)Close);
    }
    private void ReportFailure(Exception error, bool showDialog)
    {
        string? path = SaveError(error);
        _status.Text = "Не удалось подготовить или продолжить сеанс. Проверьте копию игры, адрес и пути.";
        if (showDialog) MessageBox.Show(this, "Не удалось подготовить или продолжить сеанс.\nПроверьте отдельную копию игры, адрес сервера и выбранные папки." +
            (path is null ? "" : "\n\nПодробности сохранены: " + path), "WinxCoop", MessageBoxButtons.OK, MessageBoxIcon.Error);
    }
    private string? SaveError(Exception error)
    {
        try
        {
            string directory = Directory.Exists(_lastCase) ? _lastCase : Path.Combine(_bundle.DirectoryPath, "Cases");
            Directory.CreateDirectory(directory);
            string path = Path.Combine(directory, "ui-error-" + Guid.NewGuid().ToString("N") + ".txt");
            using var file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            byte[] bytes = Encoding.UTF8.GetBytes(error + "\n"); file.Write(bytes, 0, bytes.Length); return path;
        }
        catch (Exception) { return null; }
    }
    private static Label Caption(string text) => new() { AutoSize = true, Text = text, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 8, 6) };
}
