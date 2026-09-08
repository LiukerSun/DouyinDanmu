param([switch]$SkipBuild, [string]$OutputDirectory, [int]$Jobs = 4)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $project '.cache/windows-build'
$build = Join-Path $project 'backend/build_portable'
$nodeVersion = '22.23.2'
$nodeName = "node-v$nodeVersion-win-x64"
$nodeDirectory = Join-Path $cache $nodeName
$node = Join-Path $nodeDirectory 'node.exe'
$npm = Join-Path $nodeDirectory 'node_modules/npm/bin/npm-cli.js'
function Check-Exit([string]$Step) { if ($LASTEXITCODE -ne 0) { throw "$Step failed (exit $LASTEXITCODE)" } }
New-Item -ItemType Directory -Force $cache | Out-Null
$archive = Join-Path $cache "$nodeName.zip"
$sums = Join-Path $cache "node-v$nodeVersion-SHASUMS256.txt"
if (!(Test-Path $archive)) { Invoke-WebRequest "https://nodejs.org/dist/v$nodeVersion/$nodeName.zip" -OutFile $archive }
if (!(Test-Path $sums)) { Invoke-WebRequest "https://nodejs.org/dist/v$nodeVersion/SHASUMS256.txt" -OutFile $sums }
$expected = ((Get-Content $sums | Where-Object { $_.EndsWith("  $nodeName.zip") }) -split '\s+')[0]
if (!$expected -or (Get-FileHash $archive -Algorithm SHA256).Hash -ne $expected) { throw 'Node runtime SHA256 mismatch' }
if (!(Test-Path $node)) { Expand-Archive -LiteralPath $archive -DestinationPath $cache }
$nodeHash = ((Get-Content $sums | Where-Object { $_.EndsWith('  win-x64/node.exe') }) -split '\s+')[0]
if (!$nodeHash -or (Get-FileHash $node -Algorithm SHA256).Hash -ne $nodeHash) { throw 'Extracted Node runtime SHA256 mismatch' }
$previousPath = $env:PATH
try {
  $env:PATH = "$nodeDirectory;$previousPath"
  if (!$SkipBuild) {
    $cmakeArgs = @('-S', (Join-Path $project 'backend/pipeline'), '-B', $build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=ON', '-DCMAKE_POLICY_VERSION_MINIMUM=3.10')
    $cachedBoost = Join-Path $cache 'boost_1_85_0'
    if (Test-Path (Join-Path $cachedBoost 'boost/version.hpp')) { $cmakeArgs += "-DFETCHCONTENT_SOURCE_DIR_BOOST_HEADERS=$($cachedBoost.Replace('\','/'))" }
    & cmake @cmakeArgs; Check-Exit 'Configure native backend'
    & cmake --build $build --target douyin-pipeline portable-host metadata-test gift-store-test parser-test -j $Jobs; Check-Exit 'Build native backend'
    & ctest --test-dir $build --output-on-failure; Check-Exit 'Native tests'
    & $node $npm ci --prefix (Join-Path $project 'frontend'); Check-Exit 'Frontend dependencies'
    & $node $npm run build --prefix (Join-Path $project 'frontend'); Check-Exit 'Build frontend'
  }
  if (!$OutputDirectory) { $OutputDirectory = Join-Path $project ('release/DouyinDanmu-win-x64-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
  $output = [IO.Path]::GetFullPath($OutputDirectory)
  if (Test-Path $output) { throw 'Output directory already exists. Use a new empty release path; existing data will never be overwritten.' }
  foreach ($folder in @('', 'bin', 'runtime', 'web', 'app/portable', 'app/auth', 'app/collector/proto', 'licenses')) { New-Item -ItemType Directory -Path (Join-Path $output $folder) -Force | Out-Null }
  Copy-Item -LiteralPath (Join-Path $build 'douyin-pipeline.exe') -Destination (Join-Path $output 'bin')
  Copy-Item -LiteralPath (Join-Path $build 'DouyinDanmu.exe') -Destination $output
  Copy-Item -LiteralPath $node -Destination (Join-Path $output 'runtime')
  Copy-Item -LiteralPath (Join-Path $nodeDirectory 'LICENSE') -Destination (Join-Path $output 'licenses/Node-LICENSE.txt')
  Copy-Item -Path (Join-Path $project 'frontend/dist/*') -Destination (Join-Path $output 'web') -Recurse
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'launcher.js') -Destination (Join-Path $output 'app/portable')
  foreach ($name in @('server.js','portable-web.js')) { Copy-Item -LiteralPath (Join-Path $project "auth/$name") -Destination (Join-Path $output 'app/auth') }
  foreach ($name in @('index.js','room-info.js','auth-cookie.js','settings-server.js','session-control.js','durable-files.js','package.json','package-lock.json')) { Copy-Item -LiteralPath (Join-Path $project "collector/$name") -Destination (Join-Path $output 'app/collector') }
  Copy-Item -LiteralPath (Join-Path $project 'backend/scripts/sign.js') -Destination (Join-Path $output 'app/collector')
  Copy-Item -Path (Join-Path $project 'backend/proto/*.proto') -Destination (Join-Path $output 'app/collector/proto')
  & $node $npm ci --omit=dev --prefix (Join-Path $output 'app/collector'); Check-Exit 'Package collector dependencies'
  foreach ($name in @('start.cmd','start-debug.cmd','stop.cmd')) {
    $content = [IO.File]::ReadAllText((Join-Path $PSScriptRoot $name)).Replace("`r`n","`n").Replace("`n","`r`n")
    [IO.File]::WriteAllText((Join-Path $output $name), $content, [Text.UTF8Encoding]::new($false))
  }
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.txt') -Destination $output
  Copy-Item -Path (Join-Path $PSScriptRoot 'licenses/*') -Destination (Join-Path $output 'licenses')
  foreach ($license in @(@('protobuf','LICENSE'),@('nlohmann_json','LICENSE.MIT'),@('httplib','LICENSE'),@('zlib','README'))) {
    Copy-Item -LiteralPath (Join-Path $build "_deps/$($license[0])-src/$($license[1])") -Destination (Join-Path $output "licenses/$($license[0])-LICENSE.txt")
  }
  $boostLicense = Join-Path $cache 'boost_1_85_0/LICENSE_1_0.txt'
  if (!(Test-Path $boostLicense)) { $boostLicense = Join-Path $build '_deps/boost_headers-src/LICENSE_1_0.txt' }
  Copy-Item -LiteralPath $boostLicense -Destination (Join-Path $output 'licenses/Boost-LICENSE.txt')
  [IO.File]::WriteAllText((Join-Path $output 'licenses/SQLite.txt'), 'SQLite is in the public domain. https://sqlite.org/copyright.html')
  foreach ($modules in @((Join-Path $project 'frontend/node_modules'), (Join-Path $output 'app/collector/node_modules'))) {
    Get-ChildItem -LiteralPath $modules -File -Recurse | Where-Object { $_.Name -match '^(LICENSE|LICENCE|COPYING|NOTICE)(\.|$)' } | ForEach-Object {
      $relative = $_.FullName.Substring($modules.Length).TrimStart('\','/').Replace('\','_').Replace('/','_')
      Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $output "licenses/npm-$relative")
    }
  }
  $manifest = Get-ChildItem -LiteralPath $output -File -Recurse | ForEach-Object { [ordered]@{ path = $_.FullName.Substring($output.Length+1).Replace('\','/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() } }
  [IO.File]::WriteAllText((Join-Path $output 'manifest.json'), ($manifest | ConvertTo-Json -Depth 3), [Text.UTF8Encoding]::new($false))
  Compress-Archive -LiteralPath $output -DestinationPath "$output.zip" -CompressionLevel Optimal
  (Get-FileHash -LiteralPath "$output.zip" -Algorithm SHA256).Hash | Set-Content -LiteralPath "$output.zip.sha256"
  Write-Host "Portable release: $output.zip"
} finally { $env:PATH = $previousPath }
