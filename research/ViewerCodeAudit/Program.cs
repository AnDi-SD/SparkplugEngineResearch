using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Xml.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

// Syntax inventory, not a proof of semantic equivalence or reachability.
// Reads source only; no assemblies/game files are executed or corpus scanned.
string root = Path.GetFullPath(args[0]), output = Path.GetFullPath(args[1]);
string viewer = Path.Combine(root, "tools", "SmoViewer");
var excluded = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "bin", "obj", ".git", "build", "node_modules" };
bool Source(string path) => !Path.GetRelativePath(root, path).Split(Path.DirectorySeparatorChar).Any(excluded.Contains);
string Relative(string path) => Path.GetRelativePath(root, path).Replace('\\', '/');
string Hash(string value) => Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(value)));
var files = new List<object>();
var methods = new List<MethodRow>();
var identifiers = new Dictionary<string, int>(StringComparer.Ordinal);
var projects = new List<object>();
var graph = new Dictionary<string, string[]>(StringComparer.OrdinalIgnoreCase);
foreach (string path in Directory.EnumerateFiles(viewer, "*.csproj", SearchOption.AllDirectories).Where(Source).Order())
{
    XDocument document = XDocument.Load(path);
    graph[Path.GetFullPath(path)] = document.Descendants("ProjectReference")
        .Select(e => Path.GetFullPath(Path.Combine(Path.GetDirectoryName(path)!, e.Attribute("Include")!.Value))).ToArray();
    projects.Add(new { path = Relative(path), references = document.Descendants("ProjectReference")
        .Select(e => Relative(Path.GetFullPath(Path.Combine(Path.GetDirectoryName(path)!, e.Attribute("Include")!.Value)))).ToArray() });
}
var productionDirectories = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
void Visit(string project)
{
    if (!productionDirectories.Add(Path.GetDirectoryName(project)!)) return;
    if (graph.TryGetValue(project, out var references)) foreach (string reference in references) Visit(reference);
}
Visit(Path.Combine(viewer, "SmoViewer", "SmoViewer.csproj"));
foreach (string path in Directory.EnumerateFiles(Path.Combine(root, "tools"), "*.cs", SearchOption.AllDirectories).Where(Source).Order())
{
    string content = File.ReadAllText(path);
    var syntax = CSharpSyntaxTree.ParseText(content).GetRoot();
    foreach (var token in syntax.DescendantTokens().Where(t => t.IsKind(SyntaxKind.IdentifierToken)))
        identifiers[token.ValueText] = identifiers.GetValueOrDefault(token.ValueText) + 1;
    if (!Path.GetFullPath(path).StartsWith(viewer + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) continue;
    string project = Path.GetRelativePath(viewer, path).Split(Path.DirectorySeparatorChar)[0];
    bool production = productionDirectories.Contains(Path.Combine(viewer, project));
    var declared = syntax.DescendantNodes().OfType<BaseTypeDeclarationSyntax>().Select(t => t.Identifier.ValueText).ToArray();
    files.Add(new { path = Relative(path), sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))),
        project, production, lines = syntax.SyntaxTree.GetText().Lines.Count, declared });
    foreach (var method in syntax.DescendantNodes().OfType<MethodDeclarationSyntax>())
    {
        SyntaxNode? body = method.Body ?? (SyntaxNode?)method.ExpressionBody;
        if (body is null) continue;
        var tokens = body.DescendantTokens().ToArray();
        methods.Add(new(Relative(path), method.Identifier.ValueText,
            method.Ancestors().OfType<BaseTypeDeclarationSyntax>().FirstOrDefault()?.Identifier.ValueText ?? "",
            method.GetLocation().GetLineSpan().StartLinePosition.Line + 1, production,
            method.Modifiers.Any(SyntaxKind.PrivateKeyword), tokens.Length,
            Hash(string.Join(" ", tokens.Select(t => t.Text)))));
    }
}
var duplicates = methods.Where(m => m.Production && m.BodyTokens >= 60).GroupBy(m => m.BodyHash)
    .Where(g => g.Count() > 1).Select(g => g.ToArray()).ToArray();
var unusedCandidates = methods.Where(m => m.Production && m.Private && identifiers.GetValueOrDefault(m.Name) == 1).ToArray();
Directory.CreateDirectory(Path.GetDirectoryName(output)!);
File.WriteAllText(output, JsonSerializer.Serialize(new { kind = "viewer-source-inventory", files, projects,
    methods = methods.Count, duplicateBodyCandidates = duplicates, privateSingleIdentifierCandidates = unusedCandidates,
    limitations = new[] { "Syntax/identifier matches only; different-looking methods may implement the same algorithm.",
        "XAML, reflection, interfaces, tests outside tools and conditional compilation require caller review before deletion.",
        "Core/Scene are shared by other applications; production marking denotes the Viewer application project set." } }, new JsonSerializerOptions { WriteIndented = true }));
Console.WriteLine($"Inventoried {files.Count} Viewer C# files, {methods.Count} methods; {duplicates.Length} identical-body groups, {unusedCandidates.Length} private single-name candidates.");
record MethodRow(string Path, string Name, string Owner, int Line, bool Production, bool Private, int BodyTokens, string BodyHash);
