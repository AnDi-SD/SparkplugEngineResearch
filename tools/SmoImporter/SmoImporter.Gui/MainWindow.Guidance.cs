using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private const string PreviewNavigationHelp = "ПКМ — вращать · Колесо — приблизить · Home — в кадр";
    private readonly Dictionary<Button, (bool Active, bool Animate)> _modelChoiceAttention = new();

    private void RefreshPreviewGuidance()
    {
        if (SourcePreviewGuidanceText is null) return;
        bool choosing = _wizardStep == WizardStep.Models && !WizardBusy && !_isClosing;
        SetModelChoiceAttention(SourceSelectionButton, choosing && !HasInputSource);
        SetModelChoiceAttention(DonorSelectionButton, choosing && !HasInputDonor);

        SourcePreviewGuidanceText.Text = HasInputSource
            ? "Перетащите SMO · " + PreviewNavigationHelp
            : "Выберите SMO или перетащите его сюда.";
        string? action = _sizeStep ? "ЛКМ — тянуть маркер · Esc — отмена" :
            _rigStep ? "ЛКМ — выбрать кость" :
            _postRigStep && WizardProtectionModeAvailable ? "ЛКМ / рамка — выбор · Ctrl — добавить/убрать · Esc — отмена" :
            _wizardStep == WizardStep.Models ? "Перетащите модель" : null;
        DonorPreviewGuidanceText.Text = !HasInputDonor
            ? "Выберите донора или перетащите SMO / FBX / GLB / OBJ сюда."
            : action is null ? PreviewNavigationHelp : action + "\n" + PreviewNavigationHelp;
        RefreshWizardStepHelp();
    }

    private void SetModelChoiceAttention(Button button, bool active)
    {
        bool animate = active && SystemParameters.ClientAreaAnimation;
        var state = (active, animate);
        if (_modelChoiceAttention.TryGetValue(button, out var previous) && previous == state) return;
        _modelChoiceAttention[button] = state;
        if (button.Background is SolidColorBrush oldBackground && !oldBackground.IsFrozen)
            oldBackground.BeginAnimation(SolidColorBrush.ColorProperty, null);
        if (button.BorderBrush is SolidColorBrush oldBorder && !oldBorder.IsFrozen)
            oldBorder.BeginAnimation(SolidColorBrush.ColorProperty, null);
        button.ClearValue(Control.BackgroundProperty);
        button.ClearValue(Control.BorderBrushProperty);
        if (!active) return;

        var background = new SolidColorBrush(Color.FromRgb(53, 43, 33));
        var border = new SolidColorBrush(Color.FromRgb(140, 90, 45));
        button.Background = background;
        button.BorderBrush = border;
        if (!animate) return;
        Pulse(background, Color.FromRgb(99, 64, 30));
        Pulse(border, Color.FromRgb(233, 174, 104));
    }

    private static void Pulse(SolidColorBrush brush, Color highlight) =>
        brush.BeginAnimation(SolidColorBrush.ColorProperty, new ColorAnimation
        {
            From = brush.Color, To = highlight, Duration = TimeSpan.FromSeconds(1.6),
            AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever,
            EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut }
        });

    private void ModelSelectionButton_Unloaded(object sender, RoutedEventArgs e) =>
        SetModelChoiceAttention((Button)sender, active: false);
}
