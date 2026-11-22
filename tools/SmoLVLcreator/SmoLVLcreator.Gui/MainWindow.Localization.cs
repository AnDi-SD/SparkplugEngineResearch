using SmoLVLcreator.Core;
using System.IO;
using System.Windows;
using System.Windows.Controls;

namespace SmoLVLcreator.Gui;

public partial class MainWindow
{
    private bool _changingLanguage;

    internal void InitializeLanguageSelector()
    {
        _changingLanguage = true;
        try
        {
            LanguageSelector.SelectedItem = LanguageSelector.Items.OfType<ComboBoxItem>()
                .FirstOrDefault(item => Equals(item.Tag, _settings.Language));
        }
        finally
        {
            _changingLanguage = false;
        }
    }

    private void LanguageSelector_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_changingLanguage || LanguageSelector?.SelectedItem is not ComboBoxItem { Tag: string preference }
            || preference == _settings.Language || Application.Current is not App)
            return;

        if (_isBusy || _isSaving || _isExporting)
        {
            InitializeLanguageSelector();
            return;
        }

        var updated = _settings with { Language = LevelText.Normalize(preference) };
        try
        {
            SmoLVLcreatorSettingsStore.Save(updated);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            InitializeLanguageSelector();
            MessageBox.Show(this, LevelText.Get("Language.SaveFailed", error.Message),
                LevelText.Get("Language.Title"), MessageBoxButton.OK, MessageBoxImage.Error);
            return;
        }

        ApplyLanguagePreference(updated);
    }

    private void ApplyLanguagePreference(SmoLVLcreatorSettings settings)
    {
        _settings = settings;
        ((App)Application.Current).ApplyLanguage(settings.Language);
        InitializeLanguageSelector();
        StatusText.Text = LevelText.Get("Language.Ready");
        RefreshMainLanguage();
        RefreshProjectLanguage();
        RefreshCatalogLanguage();
    }

}
