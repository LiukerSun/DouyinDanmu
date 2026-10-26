param(
  [Parameter(Mandatory=$true)][string]$ArchivePath,
  [Parameter(Mandatory=$true)][string]$DestinationPath,
  [Parameter(Mandatory=$true)][string]$ExpectedVersion
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$archive = [IO.Path]::GetFullPath($ArchivePath)
$destination = [IO.Path]::GetFullPath($DestinationPath)
if ($ExpectedVersion -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$') { throw 'Invalid release version' }
if (Test-Path -LiteralPath $destination) { throw 'Staging destination already exists' }
$parent = Split-Path -Parent $destination
if (!(Test-Path -LiteralPath $parent -PathType Container)) { throw 'Missing staging parent' }
$cursor = Get-Item -LiteralPath $parent -Force
while ($cursor) {
  if ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked staging path is not allowed' }
  $cursor = $cursor.Parent
}
$allowedDirectories = @('bin','runtime','web','app','licenses')
$allowedFiles = @('DouyinDanmu.exe','start.cmd','start-debug.cmd','stop.cmd','README.txt','VERSION','CHANGELOG.md','manifest.json','update.json')
function Check-RelativePath([string]$relative) {
  if (!$relative -or $relative.Contains('\') -or $relative.StartsWith('/') -or $relative -match '[\x00-\x1f:]') { throw 'Unsafe package path' }
  foreach ($component in $relative.Split('/')) {
    if (!$component -or $component -eq '.' -or $component -eq '..' -or $component -match '[<>"|?*]' -or $component -match '[. ]$' -or $component -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { throw 'Unsafe package path component' }
  }
  $parts = $relative.Split('/')
  if ($parts.Length -eq 1) {
    if ($allowedFiles -cnotcontains $relative -and $allowedDirectories -cnotcontains $relative) { throw 'Unexpected package root item' }
  } elseif ($allowedDirectories -cnotcontains $parts[0]) { throw 'Unexpected package directory' }
}
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
  if ($zip.Entries.Count -eq 0 -or $zip.Entries.Count -gt 100000) { throw 'Invalid package entry count' }
  $files = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
  $paths = [Collections.Generic.Dictionary[string,bool]]::new([StringComparer]::OrdinalIgnoreCase)
  $spellings = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
  $rootName = $null
  [long]$total = 0
  foreach ($entry in $zip.Entries) {
    $name = $entry.FullName
    if (!$name -or $name.Contains('\') -or $name.StartsWith('/') -or $name -match '[\x00-\x1f:]') { throw 'Unsafe ZIP entry' }
    $segments = $name.TrimEnd('/').Split('/')
    foreach ($segment in $segments) {
      if (!$segment -or $segment -eq '.' -or $segment -eq '..' -or $segment -match '[<>"|?*]' -or $segment -match '[. ]$' -or $segment -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { throw 'Unsafe ZIP path component' }
    }
    if (!$rootName) { $rootName = $segments[0] }
    if ($segments[0] -cne $rootName) { throw 'Package must have one enclosing directory' }
    $isDirectory = $name.EndsWith('/')
    if ($segments.Length -eq 1) {
      if (!$isDirectory) { throw 'Package files must be inside an enclosing directory' }
      continue
    }
    $relative = ($segments[1..($segments.Length-1)] -join '/')
    Check-RelativePath $relative
    $prefix = ''
    foreach ($component in $relative.Split('/')) {
      $prefix = if ($prefix) { $prefix+'/'+$component } else { $component }
      if ($spellings.ContainsKey($prefix) -and $spellings[$prefix] -cne $prefix) { throw 'Case-colliding ZIP directory' }
      $spellings[$prefix] = $prefix
    }
    $mode = ($entry.ExternalAttributes -shr 16) -band 0xF000
    if ($mode -ne 0 -and $mode -ne 0x8000 -and $mode -ne 0x4000) { throw 'Linked or special ZIP entry is not allowed' }
    if (($entry.ExternalAttributes -band 0x400) -ne 0) { throw 'Reparse ZIP entry is not allowed' }
    if ($paths.ContainsKey($relative)) { throw 'Duplicate or case-colliding ZIP entry' }
    $paths.Add($relative,$isDirectory)
    [long]$total += $entry.Length
    if ($total -gt 2147483648 -or $entry.Length -gt 536870912) { throw 'Expanded package exceeds allowed size' }
    if (!$isDirectory) { $files.Add($relative,$entry) }
  }
  foreach ($relative in $paths.Keys) {
    $ancestor = $relative
    while ($ancestor.Contains('/')) {
      $ancestor = $ancestor.Substring(0,$ancestor.LastIndexOf('/'))
      if ($paths.ContainsKey($ancestor) -and !$paths[$ancestor]) { throw 'File conflicts with a directory' }
    }
  }
  if (!$files.ContainsKey('manifest.json') -or $files['manifest.json'].Length -gt 16777216) { throw 'Missing or oversized manifest' }
  function Read-EntryText($entry) {
    $stream = $entry.Open()
    try { $reader = [IO.StreamReader]::new($stream,[Text.UTF8Encoding]::new($false,$true));try { return $reader.ReadToEnd() } finally { $reader.Dispose() } }
    finally { $stream.Dispose() }
  }
  $manifest = ConvertFrom-Json -InputObject (Read-EntryText $files['manifest.json'])
  if ($manifest -isnot [Array] -or $manifest.Count -ne ($files.Count-1)) { throw 'Manifest does not match package file count' }
  $listed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
  foreach ($item in $manifest) {
    if ($item.path -isnot [string] -or $item.sha256 -isnot [string] -or $item.sha256 -notmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid manifest record' }
    Check-RelativePath $item.path
    if ($item.path -eq 'manifest.json' -or !$listed.Add($item.path) -or !$files.ContainsKey($item.path) -or $spellings[$item.path] -cne $item.path) { throw 'Invalid manifest path set' }
    $stream = $files[$item.path].Open()
    $hash = [Security.Cryptography.SHA256]::Create()
    try { $actual = [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose();$stream.Dispose() }
    if ($actual -ne $item.sha256.ToLowerInvariant()) { throw 'Package file checksum mismatch' }
  }
  foreach ($required in @('DouyinDanmu.exe','runtime/node.exe','bin/douyin-pipeline.exe','app/portable/launcher.js','app/portable/extract-update.ps1','app/portable/update-package.js','app/auth/server.js','app/auth/update-service.js','web/index.html','VERSION','update.json')) {
    if (!$files.ContainsKey($required)) { throw "Missing required program file: $required" }
  }
  if ($files['VERSION'].Length -gt 128 -or (Read-EntryText $files['VERSION']).Trim() -cne $ExpectedVersion) { throw 'Package version mismatch' }
  if ($files['update.json'].Length -gt 4096) { throw 'Invalid update metadata' }
  $metadata = ConvertFrom-Json -InputObject (Read-EntryText $files['update.json'])
  if ($metadata.format -ne 1 -or $metadata.min_host_protocol -ne 1 -or $metadata.version -cne $ExpectedVersion) { throw 'This update requires a newer launcher; download it from GitHub' }
  [IO.Directory]::CreateDirectory($destination) | Out-Null
  foreach ($relative in $files.Keys) {
    $target = [IO.Path]::GetFullPath((Join-Path $destination $relative.Replace('/',[IO.Path]::DirectorySeparatorChar)))
    if (!$target.StartsWith($destination+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Extraction escaped staging directory' }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    $inputStream = $files[$relative].Open()
    try { $outputStream = [IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try { $inputStream.CopyTo($outputStream);$outputStream.Flush($true) } finally { $outputStream.Dispose() } }
    finally { $inputStream.Dispose() }
  }
} finally { $zip.Dispose() }
