using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Automation;

// External UI Automation probe for an explicitly supplied owned window.
// The parent probe bounds this helper's lifetime with a Job Object.
internal static class PackageUiProbe
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Length < 3) return 2;
        var report = new Dictionary<string, object>();
        try
        {
            var root = AutomationElement.FromHandle(new IntPtr(Int64.Parse(args[0])));
            if (args[1] == "invoke")
            {
                if (args.Length != 4) return 2;
                var target = root.FindFirst(TreeScope.Descendants,
                    new PropertyCondition(AutomationElement.AutomationIdProperty, args[3]));
                if (target == null || !target.Current.IsEnabled)
                    throw new InvalidOperationException("Missing or disabled control: " + args[3]);
                object pattern;
                if (!target.TryGetCurrentPattern(InvokePattern.Pattern, out pattern))
                    throw new InvalidOperationException("Control has no InvokePattern: " + args[3]);
                ((InvokePattern)pattern).Invoke();
                report["invoked"] = args[3];
            }
            else if (args[1] == "select")
            {
                if (args.Length != 5) return 2;
                var combo = root.FindFirst(TreeScope.Descendants,
                    new PropertyCondition(AutomationElement.AutomationIdProperty, args[3]));
                if (combo == null || !combo.Current.IsEnabled)
                    throw new InvalidOperationException("Missing or disabled combo: " + args[3]);
                var expansion = (ExpandCollapsePattern)combo.GetCurrentPattern(ExpandCollapsePattern.Pattern);
                expansion.Expand();
                var item = combo.FindFirst(TreeScope.Descendants,
                    new PropertyCondition(AutomationElement.NameProperty, args[4]));
                if (item == null || !item.Current.IsEnabled)
                    throw new InvalidOperationException("Missing or disabled item: " + args[4]);
                ((SelectionItemPattern)item.GetCurrentPattern(SelectionItemPattern.Pattern)).Select();
                expansion.Collapse();
                report["selected"] = args[4];
            }
            else if (args[1] == "check")
            {
                if (args.Length != 4) return 2;
                var target = root.FindFirst(TreeScope.Descendants, new AndCondition(
                    new PropertyCondition(AutomationElement.NameProperty, args[3]),
                    new PropertyCondition(AutomationElement.ControlTypeProperty, ControlType.CheckBox)));
                if (target == null || !target.Current.IsEnabled)
                    throw new InvalidOperationException("Missing or disabled checkbox: " + args[3]);
                var toggle = (TogglePattern)target.GetCurrentPattern(TogglePattern.Pattern);
                if (toggle.Current.ToggleState != ToggleState.On) toggle.Toggle();
                if (toggle.Current.ToggleState != ToggleState.On)
                    throw new InvalidOperationException("Checkbox is not checked: " + args[3]);
                report["checked"] = args[3];
            }
            else if (args[1] == "snapshot")
            {
                var rows = new List<Dictionary<string, object>>();
                var pending = new Queue<AutomationElement>();
                pending.Enqueue(root);
                var elapsed = Stopwatch.StartNew();
                while (pending.Count > 0 && rows.Count < 700 && elapsed.Elapsed.TotalSeconds < 8)
                {
                    var element = pending.Dequeue();
                    var state = element.Current;
                    var row = new Dictionary<string, object> {
                        {"id", state.AutomationId}, {"name", state.Name},
                        {"type", state.ControlType.ProgrammaticName}, {"enabled", state.IsEnabled}
                    };
                    object pattern;
                    if (element.TryGetCurrentPattern(ValuePattern.Pattern, out pattern))
                        row["value"] = ((ValuePattern)pattern).Current.Value;
                    else if (element.TryGetCurrentPattern(TextPattern.Pattern, out pattern))
                        row["text"] = ((TextPattern)pattern).DocumentRange.GetText(16384);
                    rows.Add(row);
                    for (var child = TreeWalker.ControlViewWalker.GetFirstChild(element);
                         child != null; child = TreeWalker.ControlViewWalker.GetNextSibling(child))
                    {
                        if (pending.Count + rows.Count >= 700) break;
                        pending.Enqueue(child);
                    }
                }
                report["controls"] = rows;
                report["truncated"] = pending.Count > 0;
            }
            else return 2;
            report["status"] = "passed";
        }
        catch (Exception ex)
        {
            report["status"] = "failed";
            report["error"] = ex.ToString();
        }
        File.WriteAllText(args[2], new JavaScriptSerializer().Serialize(report), new UTF8Encoding(false));
        return (string)report["status"] == "passed" ? 0 : 1;
    }
}
