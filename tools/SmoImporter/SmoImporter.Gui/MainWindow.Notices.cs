using System.IO;
using System.Windows;
using System.Windows.Media;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private string? _shownNoticeError;
    private IReadOnlyList<string>? _shownNoticeWarnings;

    // Wizard notices stay visible independently of the old editor's diagnostic fields.
    private void RefreshWizardNotices()
    {
        if (WizardNoticePanel is null) return;
        WizardCancelLoadButton.Visibility = _inputActionRunning || _replacementLoadRunning ? Visibility.Visible : Visibility.Collapsed;
        WizardCancelLoadButton.IsEnabled = _sourceLoadCancellation is { IsCancellationRequested: false } ||
            _replacementLoadCancellation is { IsCancellationRequested: false };
        string? error = _inputError ?? (_preparedStep
            ? _wizardResultIssue ?? _wizardResultActionIssue ?? WizardResultPreviewIssue ?? WizardResultAnimationIssue
            : _postRigStep ? WizardProtectionIssue : _preparationStep && HasInputSource && HasInputDonor
                ? GetPortingModeBlockMessage() : null);
        IReadOnlyList<string> warnings = _replacementScene?.ImportWarnings ?? Array.Empty<string>();
        if (string.Equals(error, _shownNoticeError, StringComparison.Ordinal) && ReferenceEquals(warnings, _shownNoticeWarnings)) return;
        _shownNoticeError = error;
        _shownNoticeWarnings = warnings;
        bool failed = !string.IsNullOrWhiteSpace(error);
        WizardNoticePanel.Visibility = failed || warnings.Count > 0 ? Visibility.Visible : Visibility.Collapsed;
        if (!failed && warnings.Count == 0) return;

        WizardNoticePanel.Background = new SolidColorBrush(failed ? Color.FromRgb(61, 30, 34) : Color.FromRgb(57, 45, 25));
        WizardNoticePanel.BorderBrush = failed ? Brushes.Salmon : Brushes.DarkGoldenrod;
        WizardNoticeTitle.Foreground = failed ? Brushes.Salmon : Brushes.NavajoWhite;
        WizardNoticeTitle.Text = failed ? "Не удалось завершить действие" : $"Замечания при чтении файла: {warnings.Count}";
        WizardNoticeSummary.Text = failed ? ShortNotice(error!) :
            "Модель загружена. Можно продолжить; проверьте отмеченные материалы и текстуры на шаге «Подготовка». " + ShortNotice(warnings[0]);
        WizardNoticeDetails.Text = (failed ? error + Environment.NewLine + Environment.NewLine : string.Empty) +
            (warnings.Count > 0 ? "Замечания при чтении донора:" + Environment.NewLine + string.Join(Environment.NewLine, warnings) : string.Empty);
    }

    private static string ShortNotice(string text) => text.Length <= 420 ? text : text[..417] + "…";

    private void WizardCancelLoad_Click(object sender, RoutedEventArgs e)
    {
        CancelSourceLoad();
        _replacementLoadCancellation?.Cancel();
        WizardCancelLoadButton.IsEnabled = false;
        WizardMessageText.Text = "Запрошена отмена. Дождитесь завершения чтения текущего блока файла…";
        WizardMessageText.Visibility = Visibility.Visible;
    }

    private static string DescribeUserError(Exception exception, string operation, string? path = null)
    {
        while ((exception is System.Reflection.TargetInvocationException or AggregateException) && exception.InnerException is not null)
            exception = exception.InnerException;
        string action = exception switch
        {
            _ when exception.Message.Contains("enabled Head plane", StringComparison.Ordinal) =>
                "Плоскость головы не выделяет пригодную область. Проверьте подгонку и позу головы на шаге «Скелет», затем повторите расчёт. Частичный результат не принят.",
            OperationCanceledException => "Действие отменено. Можно повторить его или выбрать другой файл.",
            FileNotFoundException or DirectoryNotFoundException => "Файл или папка недоступны. Выберите их снова.",
            UnauthorizedAccessException => "Нет доступа к файлу или папке. Выберите доступную копию или другую папку для результата.",
            OutOfMemoryException => "Недостаточно памяти. Уменьшите модель или текстуры и повторите действие.",
            NotSupportedException => "Эта возможность файла не поддерживается. Используйте другой режим или подготовьте другую копию модели.",
            SmoViewer.Core.SmoFormatException or InvalidDataException or EndOfStreamException or FormatException or OverflowException or System.Text.Json.JsonException =>
                "Файл повреждён или содержит недопустимые данные. Исправьте его в редакторе или выберите другую копию.",
            IOException => "Не удалось прочитать или записать файл. Проверьте доступ, свободное место и выбранную папку.",
            _ => "Проверьте параметры и повторите действие. Подробности можно открыть в журнале."
        };
        string context = string.IsNullOrWhiteSpace(path) ? operation : $"{operation}: {Path.GetFileName(path)}";
        string reason = exception.Message.Trim();
        if (reason.Length > 1800) reason = reason[..1800] + "…";
        return context + ". " + action + (reason.Length == 0 ? string.Empty : Environment.NewLine + "Причина: " + reason);
    }
}
