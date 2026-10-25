function Copy-WinxRemixSourceSnapshot {
    param(
        [Parameter(Mandatory=$true)][string]$RepositoryRoot,
        [Parameter(Mandatory=$true)][string]$ToolDestination
    )
    $repository = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\','/')
    $tool = Join-Path $repository 'tools/WinxRemix'
    $destination = [IO.Path]::GetFullPath($ToolDestination)
    # Preserve the repository layout so quoted shared-source includes resolve
    # against frozen inputs, including when Build-Probe uses this snapshot.
    if (-not $destination.Replace('\','/').EndsWith('/tools/WinxRemix', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Snapshot destination must end in tools/WinxRemix'
    }
    $snapshotRoot = [IO.Path]::GetFullPath((Join-Path $destination '../..'))
    $pending = [Collections.Generic.Queue[string]]::new()
    foreach ($directory in @('src','tests')) {
        Get-ChildItem -LiteralPath (Join-Path $tool $directory) -Recurse -File |
            Where-Object { $_.Extension -in '.h','.cpp','.def' -and $_.FullName -notlike '*\tests\patches\*' } |
            ForEach-Object { $pending.Enqueue($_.FullName) }
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while ($pending.Count) {
        $file = [IO.Path]::GetFullPath($pending.Dequeue())
        if (-not $file.StartsWith($repository + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Source include escapes repository: $file"
        }
        if (-not $seen.Add($file)) { continue }
        $relative = $file.Substring($repository.Length + 1)
        $target = Join-Path $snapshotRoot $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $file -Destination $target
        if ((Get-FileHash -LiteralPath $file).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
            throw "Source changed while freezing snapshot: $relative"
        }
        foreach ($match in [regex]::Matches([IO.File]::ReadAllText($file), '(?m)^\s*#include\s+"([^"]+)"')) {
            $include = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $file) $match.Groups[1].Value))
            if (Test-Path -LiteralPath $include -PathType Leaf) { $pending.Enqueue($include) }
        }
    }
}
