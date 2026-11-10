using System.ComponentModel;
using System.Windows;
using System.Windows.Documents;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Threading;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private object? _stepHelpContentKey;
    private WizardStep? _stepHelpStep;
    private bool _stepHelpLifetimeHooked;
    private long _stepHelpAttentionGeneration;
    private DispatcherOperation? _stepHelpRevealOperation;
    private FrameworkElement? _stepHelpAttentionTarget;
    private StepHelpAttentionAdorner? _stepHelpAttentionAdorner;
    private AdornerLayer? _stepHelpAttentionLayer;
    private DispatcherTimer? _stepHelpAttentionTimer;

    private void RefreshWizardStepHelp()
    {
        if (WizardStepHelpExpander is null) return;
        if (!_stepHelpLifetimeHooked)
        {
            Closing += StepHelpWindowClosing;
            SizeChanged += StepHelpWindowSizeChanged;
            _stepHelpLifetimeHooked = true;
        }
        UpdateStepHelpHeight();
        if (_stepHelpStep != _wizardStep)
        {
            StopStepHelpAttention();
            _stepHelpStep = _wizardStep;
        }
        bool visible = _wizardStep != WizardStep.Models;
        WizardStepHelpExpander.Visibility = visible ? Visibility.Visible : Visibility.Collapsed;
        if (!visible) return;
        WizardStepHelpHeader.Text = "Инструкция · " + (_wizardStep switch
        {
            WizardStep.Preparation => "Подготовка",
            WizardStep.Size => "Размер",
            WizardStep.Rig => "Скелет",
            WizardStep.PostRig => "Защита и детали",
            _ => "Результат"
        });
        object key = GetWizardStepHelpContentKey();
        if (Equals(_stepHelpContentKey, key)) return;
        StopStepHelpAttention();
        _stepHelpContentKey = key;
        BuildWizardStepHelpContent();
        WizardStepHelpFeedback.Visibility = Visibility.Collapsed;
    }

    private void StepHelpLink_Click(object sender, RoutedEventArgs e)
    {
        e.Handled = true;
        StopStepHelpAttention();
        if (_isClosing || sender is not Hyperlink { Tag: WizardStepHelpTarget destination }) return;
        WizardStepHelpFeedback.Visibility = Visibility.Collapsed;
        if (destination.Section is "protection-parts" or "protection-areas")
        {
            if (!_postRigStep || !WizardProtectionModeAvailable)
            {
                ShowStepHelpFeedback("Этот раздел доступен при настройке защиты в режиме «Создать веса».");
                return;
            }
            _wizardProtectionShowParts = destination.Section == "protection-parts";
            RefreshWizardState();
        }
        if (FindName(destination.ElementName) is not FrameworkElement target || !target.IsVisible)
        {
            ShowStepHelpFeedback("Этот элемент сейчас недоступен. Проверьте режим импорта и выбранную деталь.");
            return;
        }
        target.BringIntoView();
        long generation = _stepHelpAttentionGeneration;
        WizardStep step = _wizardStep;
        _stepHelpRevealOperation = Dispatcher.BeginInvoke(DispatcherPriority.Loaded, new Action(() =>
        {
            _stepHelpRevealOperation = null;
            if (generation != _stepHelpAttentionGeneration || _isClosing || _wizardStep != step ||
                !WizardStepHelpExpander.IsExpanded || !target.IsVisible || !target.IsLoaded) return;
            UpdateLayout();
            if (generation != _stepHelpAttentionGeneration || _isClosing || _wizardStep != step ||
                !WizardStepHelpExpander.IsExpanded || !target.IsVisible || !target.IsLoaded) return;
            if (target.ActualWidth <= 0 || target.ActualHeight <= 0)
            {
                ShowStepHelpFeedback("Этот элемент сейчас не отображается в меню.");
                return;
            }
            AdornerLayer? layer = AdornerLayer.GetAdornerLayer(target);
            if (layer is null)
            {
                ShowStepHelpFeedback("Нужный элемент показан в меню.");
                return;
            }
            var adorner = new StepHelpAttentionAdorner(target);
            _stepHelpAttentionTarget = target;
            _stepHelpAttentionAdorner = adorner;
            _stepHelpAttentionLayer = layer;
            target.Unloaded += StepHelpTarget_Unloaded;
            target.IsVisibleChanged += StepHelpTarget_IsVisibleChanged;
            layer.Add(adorner);
            if (SystemParameters.ClientAreaAnimation)
            {
                var pulse = new DoubleAnimation
                {
                    From = 0.15, To = 1, Duration = TimeSpan.FromSeconds(1), AutoReverse = true,
                    RepeatBehavior = new RepeatBehavior(2), FillBehavior = FillBehavior.Stop,
                    EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut }
                };
                pulse.Completed += (_, _) =>
                {
                    if (ReferenceEquals(_stepHelpAttentionAdorner, adorner)) StopStepHelpAttention();
                };
                adorner.BeginAnimation(OpacityProperty, pulse);
            }
            else
            {
                _stepHelpAttentionTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(4) };
                _stepHelpAttentionTimer.Tick += StepHelpAttentionTimer_Tick;
                _stepHelpAttentionTimer.Start();
            }
        }));
    }

    private void ShowStepHelpFeedback(string text)
    {
        WizardStepHelpFeedback.Text = text;
        WizardStepHelpFeedback.Visibility = Visibility.Visible;
    }

    private void StopStepHelpAttention()
    {
        _stepHelpAttentionGeneration++;
        _stepHelpRevealOperation?.Abort();
        _stepHelpRevealOperation = null;
        if (_stepHelpAttentionTimer is { } timer)
        {
            timer.Stop();
            timer.Tick -= StepHelpAttentionTimer_Tick;
            _stepHelpAttentionTimer = null;
        }
        if (_stepHelpAttentionTarget is { } target)
        {
            target.Unloaded -= StepHelpTarget_Unloaded;
            target.IsVisibleChanged -= StepHelpTarget_IsVisibleChanged;
            if (_stepHelpAttentionAdorner is { } adorner)
            {
                adorner.BeginAnimation(OpacityProperty, null);
                if (_stepHelpAttentionLayer?.GetAdorners(target)?.Contains(adorner) == true)
                    _stepHelpAttentionLayer.Remove(adorner);
            }
        }
        _stepHelpAttentionTarget = null;
        _stepHelpAttentionAdorner = null;
        _stepHelpAttentionLayer = null;
    }

    private void WizardStepHelp_Expanded(object sender, RoutedEventArgs e)
    {
        RefreshWizardStepHelp();
        if (!WizardStepHelpExpander.IsVisible) return;
        UpdateLayout();
        WizardStepHelpExpander.BringIntoView();
    }
    private void WizardStepHelp_Collapsed(object sender, RoutedEventArgs e) => StopStepHelpAttention();
    private void WizardStepHelp_Unloaded(object sender, RoutedEventArgs e) => StopStepHelpAttention();
    private void StepHelpTarget_Unloaded(object sender, RoutedEventArgs e) => StopStepHelpAttention();
    private void StepHelpTarget_IsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
    {
        if (ReferenceEquals(sender, _stepHelpAttentionTarget) && e.NewValue is false) StopStepHelpAttention();
    }
    private void StepHelpAttentionTimer_Tick(object? sender, EventArgs e) => StopStepHelpAttention();
    private void StepHelpWindowClosing(object? sender, CancelEventArgs e) => StopStepHelpAttention();
    private void StepHelpWindowSizeChanged(object sender, SizeChangedEventArgs e) => UpdateStepHelpHeight();

    private void UpdateStepHelpHeight()
    {
        double height = ActualHeight > 0 ? ActualHeight : Height;
        WizardFooterInfoScroll.MaxHeight = Math.Clamp(height - 460, 160, 380);
        WizardStepHelpScroll.MaxHeight = Math.Clamp(height - 560, 72, 144);
    }

    private sealed class StepHelpAttentionAdorner : Adorner
    {
        private static readonly Brush Highlight = new SolidColorBrush(Color.FromRgb(255, 204, 128));
        private static readonly Brush Wash = new SolidColorBrush(Color.FromArgb(35, 255, 204, 128));

        public StepHelpAttentionAdorner(UIElement target) : base(target) => IsHitTestVisible = false;

        protected override void OnRender(DrawingContext drawingContext)
        {
            Size size = AdornedElement.RenderSize;
            if (size.Width <= 2 || size.Height <= 2) return;
            drawingContext.DrawRoundedRectangle(Wash, new Pen(Highlight, 2),
                new Rect(1, 1, size.Width - 2, size.Height - 2), 4, 4);
        }
    }
}
