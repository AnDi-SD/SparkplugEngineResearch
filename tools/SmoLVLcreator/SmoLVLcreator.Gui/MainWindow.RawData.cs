using SmoLVLcreator.Core;
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
            RawDataText.Text = LevelText.Get("Main.Inspector.RawEmpty");
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
                LevelText.Get("Main.Raw.TheObjectExistsOnlyInThis");
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
        text.Append('[').Append(label).Append(LevelText.Get("Main.Raw.Object"))
            .Append(entry.Index).Append("] ").AppendLine(name);
        text.Append(LevelText.Get("Main.Raw.IdX")).Append(entry.Id.ToString("X8", CultureInfo.InvariantCulture))
            .Append(LevelText.Get("Main.Raw.Class")).Append(entry.ClassName ?? LevelText.Get("Main.Raw.Unknown"))
            .Append(LevelText.Get("Main.Raw.HashX"))
            .AppendLine(entry.TypeHash.ToString("X8", CultureInfo.InvariantCulture));
        text.Append(LevelText.Get("Main.Raw.TableX")).Append(entry.TableOffset.ToString("X", CultureInfo.InvariantCulture))
            .Append(LevelText.Get("Main.Raw.LogicalX")).Append(entry.LogicalOffset.ToString("X", CultureInfo.InvariantCulture))
            .Append(LevelText.Get("Main.Raw.PhysicalX")).Append(entry.PhysicalOffset.ToString("X", CultureInfo.InvariantCulture))
            .Append(LevelText.Get("Main.Raw.Size")).Append(entry.SerializedSize.ToString(CultureInfo.InvariantCulture))
            .AppendLine(LevelText.Get("Main.Raw.Bytes"));
        text.Append(LevelText.Get("Main.Raw.Parent")).Append(entry.ParentIndex?.ToString(CultureInfo.InvariantCulture) ?? "—")
            .Append(LevelText.Get("Main.Raw.Depth")).Append(entry.NestingDepth.ToString(CultureInfo.InvariantCulture))
            .Append(LevelText.Get("Main.Raw.Signature")).AppendLine(LevelText.Get(entry.SignatureMatches
                ? "Main.Raw.ValidSignature" : "Main.Raw.INVALID"));

        try
        {
            SmoObjectCapabilities capabilities = SmoSchemaRegistry.Describe(document, entry);
            text.Append(LevelText.Get("Main.Raw.Fields")).Append(capabilities.RawFields.Count)
                .Append(LevelText.Get("Main.Raw.SchemaProperties")).AppendLine(capabilities.Properties.Count.ToString(CultureInfo.InvariantCulture));
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
                    .Append(LevelText.Get("Main.Raw.RelX")).Append(field.RelativePayloadOffset.ToString("X", CultureInfo.InvariantCulture))
                    .Append(LevelText.Get("Main.Raw.AbsX")).Append(field.AbsolutePayloadOffset.ToString("X", CultureInfo.InvariantCulture))
                    .Append("  ").Append(field.PayloadSize.ToString(CultureInfo.InvariantCulture))
                    .Append(LevelText.Get("Main.Raw.Bytes2")).AppendLine(HexPreview(field.Payload.Span));

                foreach (SmoPropertyCapability property in capabilities.Properties
                             .Where(candidate => candidate.Descriptor.Field.Matches(field)))
                {
                    text.Append("    ").Append(property.Descriptor.Key)
                        .Append(" = ").Append(FormatPropertyValue(field, property.Descriptor))
                        .Append("  [").Append(property.CanWrite ? LevelText.Get("Main.Raw.Writable") : LevelText.Get("Main.Raw.ReadOnly"))
                        .AppendLine("]");
                }
            }

            foreach (SmoPropertyCapability property in capabilities.Properties
                         .Where(property => !property.IsPresent))
            {
                text.Append("  ").Append(property.Descriptor.Key)
                    .Append(LevelText.Get("Main.Raw.Absent"))
                    .Append(property.CanWrite ? LevelText.Get("Main.Raw.Materializable") : LevelText.Get("Main.Raw.Unsupported"))
                    .AppendLine("]");
            }
            if (capabilities.Properties.Count == 0)
                text.AppendLine(LevelText.Get("Main.Raw.SchemaThisClassHasNoConfirmed"));
        }
        catch (Exception exception)
        {
            text.Append(LevelText.Get("Main.Raw.RAWREADERROR")).AppendLine(exception.Message);
        }
    }

    private static string FormatPropertyValue(
        SmoObjectField field,
        SmoPropertyDescriptor descriptor)
    {
        if (descriptor.PayloadOffset < 0 ||
            descriptor.PayloadOffset > field.Payload.Length - descriptor.ValueSize)
        {
            return LevelText.Get("Main.Raw.PayloadOutOfRange");
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
        string help =
            LevelText.Get("Main.Help.ControlsText");
        MessageBox.Show(this, help, LevelText.Get("Main.Help.ControlsTitle"),
            MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void About_Click(object sender, RoutedEventArgs e) =>
        MessageBox.Show(
            this,
            LevelText.Get("Main.Help.AboutText", App.VersionText),
            LevelText.Get("Main.Menu.About"),
            MessageBoxButton.OK,
            MessageBoxImage.Information);
}
