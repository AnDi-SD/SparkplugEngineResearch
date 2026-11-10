using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Media;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record WizardStepHelpTarget(string ElementName, string? Section = null);

    private object GetWizardStepHelpContentKey() => (_wizardStep, ReplacementIsSmo, ReplacementIsObj, EffectivePortingMode,
        IsStaticReplacementPair, WizardCanAdjustScale, WizardCanAdjustPlacement,
        UsesAdaptDonorWeightsPortingMode, ManualAdaptWeights, HasWizardTargetSkeleton,
        WizardRigDefinition is not null, WizardRigSupportsHumanoid, _wizardRigHumanoidMode,
        WizardProtectionModeAvailable, WizardProtectionVisibilityPanel.Visibility,
        _wizardProtectionState.Components.Values.Any(choice => choice.Attachment == WizardProtectionAttachment.Wings),
        WizardResultChooseGameButton.Visibility);

    private void BuildWizardStepHelpContent()
    {
        WizardStepHelpContent.Children.Clear();
        if (_preparationStep)
            BuildPreparationStepHelp();
        else if (_sizeStep)
            BuildSizeStepHelp();
        else if (_rigStep)
            BuildRigStepHelp();
        else if (_postRigStep)
            BuildProtectionStepHelp();
        else if (_preparedStep)
            BuildResultStepHelp();
    }

    private void BuildPreparationStepHelp()
    {
        if (ReplacementIsSmo && !IsStaticReplacementPair)
            AddHelpParagraph("1. Проверьте совместимость. ", "SMO переносится напрямую. Результат проверки показан в ",
                HelpLink("способе импорта", "PreparationModeHintText"), ".");
        else if (IsStaticReplacementPair)
            AddHelpParagraph("1. Проверьте способ. ", HelpLink("Способ импорта", "PortingModeChoices"),
                " определяет статическую замену без создания скелета и весов.");
        else if (UsesGeneratedWeightsPortingMode)
            AddHelpParagraph("1. Проверьте способ. ", HelpLink("«Создать веса»", "PortingModeChoices"),
                " готовит донора для игрового скелета. Расчёт начнётся после подгонки размера и позы.");
        else if (UsesAdaptDonorWeightsPortingMode)
            AddHelpParagraph("1. Проверьте способ. ", HelpLink("«Адаптировать скелет»", "PortingModeChoices"),
                " сопоставляет кости донора с игровыми и сохраняет его исходные веса.");
        else if (UsesPreparedModelPortingMode)
            AddHelpParagraph("1. Проверьте способ. ", HelpLink("«Готовая модель»", "PortingModeChoices"),
                " использует уже подготовленные для игры кости и веса донора.");
        else
            AddHelpParagraph("1. Выберите способ. ", HelpLink("«Автовыбор»", "PortingModeChoices"),
                " использует рекомендуемый способ. Доступность остальных вариантов зависит от скелета и весов модели.");

        AddHelpParagraph("2. Проверьте изображения. ",
            HelpLink("Список текстур", ReplacementIsSmo ? "NativeDonorTextureList" : "TextureList"),
            " показывает связанные детали при выборе строки. Перетащите изображение на конкретную строку для точной замены.");
        if (!ReplacementIsSmo)
            AddHelpParagraph("Автоподбор. ", HelpLink("Добавить изображения", "AutomaticTextureDropSurface"),
                " можно сразу несколько: они сопоставляются по именам. Это удобно для набора файлов; точную замену выполняйте на нужной строке.");
        if (ReplacementIsObj)
            AddHelpParagraph("Материалы OBJ. ", "Если выбран неверный файл материалов, укажите его через ",
                HelpLink("«Выбрать MTL…»", "ChooseObjMaterialButton"), ". Для других форматов этот выбор не требуется.");
        AddHelpParagraph("Дальше. ", "Жёлтое замечание о недоступной текстуре допускает продолжение без неё. Проверьте внешний вид и нажмите ",
            HelpLink("«Далее»", "WizardNextButton"), " — откроется подгонка размера.");
        AddHelpControls("ПКМ — вращать вид · Колесо — приблизить · Home — показать модель целиком.");
    }

    private void BuildSizeStepHelp()
    {
        if (WizardCanAdjustScale)
            AddHelpParagraph("1. Сравните размеры. ", "Проверьте исходную модель и донора. Задайте ",
                HelpLink("масштаб в процентах", "WizardScalePercentTextBox"),
                "; кнопка «Автоподгонка» предлагает начальное совпадение по высоте и центру.");
        else
            AddHelpParagraph("1. Проверьте совпадение. ", "Для этой пары изменение масштаба недоступно. Осмотрите модели с разных сторон перед продолжением.");

        if (UsesAdaptDonorWeightsPortingMode && !ManualAdaptWeights)
            AddHelpParagraph("2. Положение. ", "В адаптации размещение вычисляется автоматически. При необходимости включите ",
                HelpLink("ручную настройку положения", "WizardManualPlacementButton"), ", затем поправьте поворот и смещение.");
        else if (WizardCanAdjustPlacement)
            AddHelpParagraph("2. Совместите модели. ", "Используйте ", HelpLink("поля поворота и смещения", "WizardRotationXBox"),
                " по X, Y, Z. Маркеры в предпросмотре позволяют менять положение мышью, когда они доступны.");
        else
            AddHelpParagraph("2. Учитывайте режим. ", "Поворот и смещение недоступны для текущей пары. Доступные настройки определяет способ импорта.");

        if (WizardCanAdjustScale || WizardCanAdjustPlacement)
            AddHelpParagraph("3. Примите числа. ", "После ручного ввода нажмите ", HelpLink("«Применить»", "WizardTransformApplyButton"),
                ". Кнопка активна при неприменённых корректных значениях. «Сбросить» возвращает исходную подгонку.");
        AddHelpParagraph("Дальше. ", "Когда совпадение устраивает, нажмите ", HelpLink("«Далее»", "WizardNextButton"),
            HasWizardTargetSkeleton ? " для настройки скелета." : " для подготовки готовой статической модели.");
        AddHelpControls("ЛКМ — тянуть доступный маркер · Esc — отменить перетаскивание · ПКМ — вращать вид · Колесо — приблизить.");
    }

    private void BuildRigStepHelp()
    {
        if (WizardRigDefinition is null)
        {
            AddHelpParagraph("1. Проверьте скелет. ", "Если скелет недоступен, вернитесь ",
                HelpLink("назад", "WizardBackButton"), " и проверьте выбранную модель и способ импорта.");
            AddHelpParagraph("2. Не меняйте исходники. ", "Все настройки выполняются в предпросмотре; исходные файлы сохраняются.");
        }
        else
        {
            if (WizardRigSupportsHumanoid && _wizardRigHumanoidMode)
                AddHelpParagraph("1. Совместите позу. ", "Начните с ", HelpLink("положения рук", "WizardRigArmRaiseSlider"),
                    ", затем поправьте локти, ноги и корпус. Ориентируйтесь на совпадение игрового скелета с донором.");
            else
                AddHelpParagraph("1. Выберите кость. ", HelpLink("Список костей", "WizardRigJointList"),
                    " поддерживает поиск. Выберите кость в списке или щёлкните её в предпросмотре, затем настройте углы X, Y, Z.");

            if (WizardRigSupportsHumanoid)
                AddHelpParagraph("2. Выберите точность. ", HelpLink("Переключатели режима", "WizardRigModesPanel"),
                    " позволяют перейти между общей позой человека и правкой отдельных костей. Изменения видны справа сразу.");
            else
                AddHelpParagraph("2. Проверьте результат. ", "Поворачивайте отдельные кости постепенно и осматривайте модель с разных сторон. Избегайте резких неестественных сгибов.");
            AddHelpParagraph("3. При необходимости начните заново. ", HelpLink("«Сбросить позу»", "WizardRigResetButton"),
                " возвращает исходное положение костей.");
            AddHelpParagraph("Дальше. ", HelpLink("«Далее»", "WizardNextButton"),
                " принимает текущую позу. К ней можно вернуться кнопкой «Назад».");
        }
        AddHelpControls("ЛКМ — выбрать кость в режиме отдельных костей · ПКМ — вращать вид · Колесо — приблизить · Home — в кадр.");
    }

    private void BuildProtectionStepHelp()
    {
        if (!WizardProtectionModeAvailable)
        {
            AddHelpParagraph("1. Учитывайте способ импорта. ", "Ручная защита областей и назначение деталей доступны при создании весов для человеческого скелета.");
            AddHelpParagraph("2. Проверьте позу. ", "Для текущего режима дополнительные настройки здесь не требуются. При необходимости вернитесь ",
                HelpLink("к предыдущему шагу", "WizardBackButton"), ".");
            AddHelpParagraph("Дальше. ", HelpLink("«Далее»", "WizardNextButton"), " открывает готовую модель.");
            AddHelpControls("ПКМ — вращать вид · Колесо — приблизить · Home — в кадр.");
            return;
        }

        AddHelpParagraph("1. Проверьте защитные области. ", "В разделе ", HelpLink("«Области»", "WizardProtectionRegionList", "protection-areas"),
            " выберите плечо, голову или кисти и поправьте доступные границы. Проверяйте силуэт рядом с суставами.");
        AddHelpParagraph("2. Назначьте отдельные детали. ", "В «Детали и крылья» выделите нужные части. Для крыльев выберите обе стороны и укажите ",
            HelpLink("«Крылья»", "WizardProtectionAttachmentWingsButton", "protection-parts"),
            "; остальные отдельные элементы при необходимости можно назначить голове.");
        bool wings = _wizardProtectionState.Components.Values.Any(choice => choice.Attachment == WizardProtectionAttachment.Wings);
        if (wings)
            AddHelpParagraph("3. Подгоните крылья. ", HelpLink("Высота", "WizardProtectionWingHeight", "protection-parts"),
                " задаётся в процентах от тела; ", HelpLink("поворот", "WizardProtectionWingRotation", "protection-parts"),
                " идёт в плоскости крыла вокруг корня у спины. Минус опускает, плюс поднимает. Нули возвращают автоматическую подгонку.");
        else
            AddHelpParagraph("3. Проверьте назначения. ", "После назначения крыльев появятся отдельные настройки их высоты и поворота. Крылья подгоняются по исходной модели либо образцу BloomX.");
        if (WizardProtectionVisibilityPanel.Visibility == Visibility.Visible)
            AddHelpParagraph("Видимость. ", HelpLink("Галочки частей", "WizardProtectionVisibilityPanel"),
                " временно скрывают тело, крылья или голову только здесь. Скрытые детали не выбираются; в готовую модель входят все части.");
        AddHelpParagraph("Дальше. ", "Дождитесь окончания расчёта в предпросмотре и нажмите ",
            HelpLink("«Далее»", "WizardNextButton"), " для принятия настроек.");
        AddHelpControls("ЛКМ — деталь или рамка · Ctrl — добавить/убрать · Esc — отменить рамку · ПКМ — вращать вид · Колесо — приблизить.");
    }

    private void BuildResultStepHelp()
    {
        AddHelpParagraph("1. Осмотрите результат. ", "Здесь всегда показана полная готовая модель с текстурами. Для исправлений вернитесь ",
            HelpLink("назад", "WizardBackButton"), ".");
        AddHelpParagraph("2. Сохраните отдельный SMO. ", HelpLink("«Экспортировать SMO…»", "WizardResultExportButton"),
            " сохраняет показанную модель. Исходные файлы не изменяются; прежний файл по выбранному пути получает резервную копию.");
        AddHelpParagraph("3. Проверьте совместимость. ", HelpLink("«Проверить загрузку в игре»", "WizardResultCheckButton"),
            " проверяет загрузку файла, без просмотра сцены в игре.");
        if (WizardResultChooseGameButton.Visibility == Visibility.Visible)
            AddHelpParagraph("Путь к игре. ", "Если игра не найдена автоматически, ",
                HelpLink("укажите исполняемый файл", "WizardResultChooseGameButton"), ".");
        if (HasWizardTargetSkeleton)
            AddHelpParagraph("Анимации. ", "Выберите ", HelpLink("тестовую анимацию", "WizardResultAnimationCombo"),
                ", затем используйте воспроизведение, стоп и перемотку. Если список пуст, выберите папку кнопкой «Папка…». Проверка анимации не меняет экспортируемый SMO.");
        AddHelpControls("Это последний шаг: «Далее» отключена. ПКМ — вращать вид · Колесо — приблизить · Home — в кадр.");
    }

    private void AddHelpParagraph(string heading, params object[] parts)
    {
        var paragraph = new TextBlock
        {
            TextWrapping = TextWrapping.Wrap,
            Foreground = new SolidColorBrush(Color.FromRgb(213, 221, 232)),
            FontSize = 12,
            LineHeight = 18,
            Margin = new Thickness(0, 0, 0, 5)
        };
        paragraph.Inlines.Add(new Run(heading) { FontWeight = FontWeights.SemiBold, Foreground = Brushes.NavajoWhite });
        foreach (object part in parts)
            paragraph.Inlines.Add(part is Inline inline ? inline : new Run((string)part));
        WizardStepHelpContent.Children.Add(paragraph);
    }

    private void AddHelpControls(string text) => WizardStepHelpContent.Children.Add(new TextBlock
    {
        Text = text,
        TextWrapping = TextWrapping.Wrap,
        FontSize = 11,
        Foreground = new SolidColorBrush(Color.FromRgb(155, 170, 189)),
        Margin = new Thickness(0, 4, 0, 0)
    });

    private Hyperlink HelpLink(string label, string elementName, string? section = null)
    {
        var link = new Hyperlink(new Run(label))
        {
            Tag = new WizardStepHelpTarget(elementName, section),
            Foreground = new SolidColorBrush(Color.FromRgb(136, 207, 255)),
            ToolTip = "Показать элемент в интерфейсе"
        };
        link.Click += StepHelpLink_Click;
        return link;
    }
}
