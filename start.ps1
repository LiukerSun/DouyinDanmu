param([switch]$DebugMode)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
Write-Host '正在构建并启动直播工作台…' -ForegroundColor Cyan
$composeOptions = @('-f', (Join-Path $PSScriptRoot 'compose.yaml'))
if ($DebugMode) { $composeOptions += @('-f', (Join-Path $PSScriptRoot 'compose.debug.yaml')) }
docker compose @composeOptions up -d --build
if ($LASTEXITCODE -ne 0) { throw '启动失败，请检查 Docker Desktop 和上方错误信息。' }
docker compose @composeOptions ps
Write-Host $(if ($DebugMode) { 'Debug 模式：协议诊断已开启。' } else { '普通模式：协议诊断已隐藏。' })
Write-Host '工作台：http://localhost:3000' -ForegroundColor Green
Write-Host '首次访问请创建管理员账号，之后使用账号密码登录。'
Write-Host '停止服务：docker compose stop；账号和采集数据会保留。'
