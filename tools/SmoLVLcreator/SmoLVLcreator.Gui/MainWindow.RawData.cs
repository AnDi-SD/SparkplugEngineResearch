using SmoViewer.Core;
using System.Buffers.Binary;
using System.Globalization;
using System.Numerics;
using System.Text;
using System.Windows;

namespace SmoLVLcreator.Gui;

public partial class MainWindow
{
    private void UpdateRawData(
        params (string Label, int? ObjectIndex)[] targets)
    {
        if (RawDataText is null)
            return;
        if (_workspace is null)
        {
            RawDataText.Text = "Нет данных: откройте SMO и выберите объект.";
            return;
        }

        (string Label, int ObjectIndex)[] valid = targets
            .Where(target => target.ObjectIndex is int index &&
                (uint)index < (uint)_workspace.Document.Objects.Count)
            .Select(target => (
                Label: target.Label,
                ObjectIndex: target.ObjectIndex!.Value))
            .DistinctBy(target => target.ObjectIndex)
            .ToArray();
        if (valid.Length == 0)
        {
            RawDataText.Text =
                "Объект существует только в сессии редактора. Его SBOO-поля " +
                "будут назначены во время сохранения.";
            return;
        }

        var text = new StringBuilder();
        foreach ((string label, int objectIndex) in valid)
        {
            if (text.Length > 0)
                text.AppendLine().AppendLine();
            AppendRawObject(text, label, objectIndex);
        }
        RawDataText.Text = text.ToString();
    }

    private void AppendRawObject(
        StringBuilder text,
        string label,
        int objectIndex)
    {
        SmoDocument document = _workspace!.Document;
        SmoObjectEntry entry = document.Objects[objectIndex];
        string name = entry.Name.TrimEnd('\0');
        text.Append('[').Append(label).Append("] object [")
            .Append(entry.Index).Append("] ").AppendLine(name);
        text.Append("id 0x").Append(entry.Id.ToString("X8", CultureInfo.InvariantCulture))
            .Append("  class ").Append(entry.ClassName ?? "unknown")
            .Append("  hash 0x")
            .AppendLine(entry.TypeHash.ToString("X8", CultureInfo.InvariantCulture));
        text.Append("table 0x").Append(entry.TableOffset.ToString("X", CultureInfo.InvariantCulture))
            .Append("  logical 0x").Append(entry.LogicalOffset.ToString("X", CultureInfo.InvariantCulture))
            .Append("  physical 0x").Append(entry.PhysicalOffset.ToString("X", CultureInfo.InvariantCulture))
            .Append("  size ").Append(entry.SerializedSize.ToString(CultureInfo.InvariantCulture))
            .AppendLine(" bytes");
        text.Append("parent ").Append(entry.ParentIndex?.ToString(CultureInfo.InvariantCulture) ?? "—")
            .Append("  depth ").Append(entry.NestingDepth.ToString(CultureInfo.InvariantCulture))
            .Append("  signature ").AppendLine(entry.SignatureMatches ? "SBOO OK" : "INVALID");

        try
        {
            SmoObjectCapabilities capabilities = SmoSchemaRegistry.Describe(document, entry);
            text.Append("fields ").Append(capabilities.RawFields.Count)
                .Append("  schema properties ").AppendLine(capabilities.Properties.Count.ToString(CultureInfo.InvariantCulture));
            for (int fieldIndex = 0; fieldIndex < capabilities.RawFields.Count; fieldIndex++)
            {
                SmoObjectField field = capabilities.RawFields[fieldIndex];
                text.Append("  f").Append(field.FieldType)
                    .Append('[').Append(field.Occurrence).Append(']')
                    .Append("  ").Append(field.SizeKind);
                if (SmoSerializedFieldRegistry.TryDescribeOwnField(
                        entry.TypeHash,
                        capabilities.RawFields,
                        fieldIndex,
                        out SmoSerializedFieldDescriptor? serializedField) &&
                    serializedField is not null)
                {
                    text.Append("  ").Append(serializedField.Key)
                        .Append(" (").Append(serializedField.PayloadLayout).Append(')');
                }
                text
                    .Append("  rel 0x").Append(field.RelativePayloadOffset.ToString("X", CultureInfo.InvariantCulture))
                    .Append("  abs 0x").Append(field.AbsolutePayloadOffset.ToString("X", CultureInfo.InvariantCulture))
                    .Append("  ").Append(field.PayloadSize.ToString(CultureInfo.InvariantCulture))
                    .Append(" bytes  ").AppendLine(HexPreview(field.Payload.Span));

                foreach (SmoPropertyCapability property in capabilities.Properties
                             .Where(candidate => candidate.Descriptor.Field.Matches(field)))
                {
                    text.Append("    ").Append(property.Descriptor.Key)
                        .Append(" = ").Append(FormatPropertyValue(field, property.Descriptor))
                        .Append("  [").Append(property.CanWrite ? "writable" : "read-only")
                        .AppendLine("]");
                }
            }

            foreach (SmoPropertyCapability property in capabilities.Properties
                         .Where(property => !property.IsPresent))
            {
                text.Append("  ").Append(property.Descriptor.Key)
                    .Append(" = <absent>  [")
                    .Append(property.CanWrite ? "materializable" : "unsupported")
                    .AppendLine("]");
            }
            if (capabilities.Properties.Count == 0)
                text.AppendLine("  schema: класс пока не имеет подтверждённых редактируемых свойств");
        }
        catch (Exception exception)
        {
            text.Append("RAW READ ERROR: ").AppendLine(exception.Message);
        }
    }

    private static string FormatPropertyValue(
        SmoObjectField field,
        SmoPropertyDescriptor descriptor)
    {
        if (descriptor.PayloadOffset < 0 ||
            descriptor.PayloadOffset > field.Payload.Length - descriptor.ValueSize)
        {
            return "<payload out of range>";
        }
        ReadOnlySpan<byte> value = descriptor.ValueSize == 0
            ? field.Payload.Span
            : field.Payload.Span.Slice(descriptor.PayloadOffset, descriptor.ValueSize);
        return descriptor.ValueKind switch
        {
            SmoPropertyValueKind.Vector3 => FormatVector3(value),
            SmoPropertyValueKind.Quaternion => FormatQuaternion(value),
            SmoPropertyValueKind.Matrix4x4 => FormatMatrix(value),
            _ => HexPreview(value)
        };
    }

    private static string FormatVector3(ReadOnlySpan<byte> value) =>
        $"({ReadSingle(value, 0):0.######}, {ReadSingle(value, 4):0.######}, {ReadSingle(value, 8):0.######})";

    private static string FormatQuaternion(ReadOnlySpan<byte> value) =>
        $"({ReadSingle(value, 0):0.######}, {ReadSingle(value, 4):0.######}, " +
        $"{ReadSingle(value, 8):0.######}, {ReadSingle(value, 12):0.######})";

    private static string FormatMatrix(ReadOnlySpan<byte> value)
    {
        var matrix = new Matrix4x4(
            ReadSingle(value, 0), ReadSingle(value, 4), ReadSingle(value, 8), ReadSingle(value, 12),
            ReadSingle(value, 16), ReadSingle(value, 20), ReadSingle(value, 24), ReadSingle(value, 28),
            ReadSingle(value, 32), ReadSingle(value, 36), ReadSingle(value, 40), ReadSingle(value, 44),
            ReadSingle(value, 48), ReadSingle(value, 52), ReadSingle(value, 56), ReadSingle(value, 60));
        return string.Create(
            CultureInfo.InvariantCulture,
            $"[{matrix.M11:0.###} {matrix.M12:0.###} {matrix.M13:0.###} {matrix.M14:0.###}; " +
            $"{matrix.M21:0.###} {matrix.M22:0.###} {matrix.M23:0.###} {matrix.M24:0.###}; " +
            $"{matrix.M31:0.###} {matrix.M32:0.###} {matrix.M33:0.###} {matrix.M34:0.###}; " +
            $"{matrix.M41:0.###} {matrix.M42:0.###} {matrix.M43:0.###} {matrix.M44:0.###}]");
    }

    private static float ReadSingle(ReadOnlySpan<byte> value, int offset) =>
        BitConverter.Int32BitsToSingle(
            BinaryPrimitives.ReadInt32LittleEndian(value[offset..]));

    private static string HexPreview(ReadOnlySpan<byte> value)
    {
        const int limit = 24;
        string result = Convert.ToHexString(value[..Math.Min(value.Length, limit)]);
        return value.Length > limit ? result + "…" : result;
    }

    private void ControlsHelp_Click(object sender, RoutedEventArgs e)
    {
        const string help =
            "КАМЕРА\n" +
            "ПКМ — вращение/свободный обзор; Shift+ПКМ — панорама; колесо — приближение.\n" +
            "WASD + Q/E — полёт; F — показать выделенное целиком.\n\n" +
            "ВЫДЕЛЕНИЕ И ПРАВКА\n" +
            "ЛКМ — объект целиком; Alt+ЛКМ — отдельная mesh-часть; Ctrl+ЛКМ — несколько объектов.\n" +
            "Move/Rotate/Scale — гизмо; Ctrl при перетаскивании — привязка.\n" +
            "Ctrl+C/V/D — копировать, вставить и дублировать размещение; Delete — удалить.\n" +
            "Ctrl+Z/Y — отмена и повтор. MOVE LINKED двигает связанную коллизию вместе с моделью.\n\n" +
            "КАТАЛОГ И ФАЙЛЫ\n" +
            "Двойной клик по карточке показывает ресурс; модель можно перетащить в сцену или разместить кнопкой.\n" +
            "Ctrl+S — сохранить; Ctrl+Shift+S — сохранить как; Ctrl+E — быстрый экспорт.\n" +
            "Перед проверкой в игре сохраняйте копию уровня; рядом всегда создаётся подробный журнал.";
        MessageBox.Show(this, help, "SmoLVLcreator — управление",
            MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void About_Click(object sender, RoutedEventArgs e) =>
        MessageBox.Show(
            this,
            $"SmoLVLcreator {App.VersionText}\nРедактор уровней Sparkplug SMO\n\n" +
            "Использует общие ядра SmoViewer, SmoImporter и SmoExporter.",
            "О программе",
            MessageBoxButton.OK,
            MessageBoxImage.Information);
}
