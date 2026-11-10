using System.Windows;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private readonly record struct WizardProtectionVisibility(bool Body, bool Wings, bool Head);

    private WizardProtectionVisibility? _shownProtectionVisibility;
    private WizardProtectionState? _shownProtectionVisibilityState;
    private GeneratedSkinningAnalysis? _shownProtectionVisibilityAnalysis;
    private bool _resettingWizardProtectionVisibility;

    private WizardProtectionVisibility ReadWizardProtectionVisibility() => new(
        WizardProtectionShowBodyCheckBox?.IsChecked != false,
        WizardProtectionShowWingsCheckBox?.IsChecked != false,
        WizardProtectionShowHeadCheckBox?.IsChecked != false);

    private void RefreshWizardProtectionVisibility(bool donorOnly)
    {
        if (WizardProtectionVisibilityPanel is not null)
            WizardProtectionVisibilityPanel.Visibility = _postRigStep && donorOnly && WizardProtectionPreparation is not null
                ? Visibility.Visible : Visibility.Collapsed;
    }

    private void ResetWizardProtectionVisibility()
    {
        _resettingWizardProtectionVisibility = true;
        try
        {
            if (WizardProtectionShowBodyCheckBox is not null)
                WizardProtectionShowBodyCheckBox.IsChecked = true;
            if (WizardProtectionShowWingsCheckBox is not null)
                WizardProtectionShowWingsCheckBox.IsChecked = true;
            if (WizardProtectionShowHeadCheckBox is not null)
                WizardProtectionShowHeadCheckBox.IsChecked = true;
            _shownProtectionVisibility = null;
        }
        finally
        {
            _resettingWizardProtectionVisibility = false;
        }
    }

    private void WizardProtectionVisibility_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded || _resettingWizardProtectionVisibility)
            return;

        EndWizardProtectionRectangleSelection();
        RefreshWizardProtectionPreview();
        RefreshWizardResultPreview();
    }

    private bool IsWizardProtectionComponentVisible(GeneratedSkinningComponentInfo component)
    {
        WizardProtectionVisibility visibility = ReadWizardProtectionVisibility();
        return GetWizardProtectionComponentAttachment(component.ComponentIndex) switch
        {
            WizardProtectionAttachment.Wings => visibility.Wings,
            WizardProtectionAttachment.Head => visibility.Head,
            _ => GetGeneratedComponentDisplayBinding(component) switch
            {
                GeneratedComponentDisplayBinding.Head => visibility.Head,
                GeneratedComponentDisplayBinding.UpperBack => visibility.Wings,
                _ => visibility.Body
            }
        };
    }

    private uint[][] FilterWizardProtectionTriangles(ImportedScene scene, GeneratedSkinningAnalysis? analysis)
    {
        var triangles = scene.Meshes.Select(mesh => mesh.TriangleIndices).ToArray();
        WizardProtectionVisibility visibility = ReadWizardProtectionVisibility();
        if (analysis is null || visibility is { Body: true, Wings: true, Head: true })
            return triangles;

        // Analysis covers complete connected donor surfaces. Preserve mesh and vertex
        // indices so fitting, component selection and the export scene stay unchanged.
        var hiddenByMesh = new Dictionary<int, HashSet<int>>();
        foreach (GeneratedSkinningComponentInfo component in analysis.Components.Where(component => !IsWizardProtectionComponentVisible(component)))
        foreach (TargetRigBodyVertexMembership membership in component.VerticesByMesh)
        {
            if (!hiddenByMesh.TryGetValue(membership.MeshIndex, out HashSet<int>? vertices))
                hiddenByMesh.Add(membership.MeshIndex, vertices = []);
            vertices.UnionWith(membership.VertexIndices);
        }

        foreach ((int meshIndex, HashSet<int> hidden) in hiddenByMesh)
        {
            if ((uint)meshIndex >= (uint)triangles.Length)
                continue;
            uint[] original = triangles[meshIndex];
            var visible = new List<uint>(original.Length);
            for (int index = 0; index + 2 < original.Length; index += 3)
            {
                uint a = original[index], b = original[index + 1], c = original[index + 2];
                if (hidden.Contains(checked((int)a)) && hidden.Contains(checked((int)b)) && hidden.Contains(checked((int)c)))
                    continue;
                visible.Add(a);
                visible.Add(b);
                visible.Add(c);
            }
            triangles[meshIndex] = visible.ToArray();
        }
        return triangles;
    }
}
