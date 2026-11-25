<#
.SYNOPSIS
  Delega un encargo (.md) a Gemini mediante Antigravity CLI (agy) en modo headless.

.DESCRIPTION
  Claude escribe el encargo en .agy-tasks\<tarea>.md y llama a este script.
  agy lee el encargo y el proyecto, pero NO escribe nada: este script anade
  su respuesta al final del mismo .md y los errores a <tarea>.err.
  Cada llamada es una conversacion nueva (Gemini no recuerda tareas anteriores).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts/agy-task.ps1 -Task .agy-tasks/2026-11-24-revisar-player.md -Model gemini-4-hydrogen-high -Agent Juno
#>
param(
  [Parameter(Mandatory = $true)][string]$Task,
  [string]$Model = "gemini-4-hydrogen-high",
  [string]$Timeout = "15m",
  [string]$Effort = "",
  [string]$Agent = "Ayudante"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Task)) { Write-Error "No existe el encargo: $Task"; exit 2 }
if (-not (Get-Command agy -ErrorAction SilentlyContinue)) {
  Write-Error "agy no esta en el PATH. Instala Antigravity CLI: irm https://antigravity.google/cli/install.ps1 | iex"
  exit 2
}

$taskPath = (Resolve-Path $Task).Path
$errPath  = [IO.Path]::ChangeExtension($taskPath, ".err")
$relTask  = Resolve-Path -Relative $taskPath

$prompt = @"
Eres un agente ayudante. Lee el encargo del archivo $relTask y cumplelo.
Reglas: NO modifiques, crees ni borres ningun archivo; solo lee y analiza.
Responde unicamente con el resultado en Markdown, siguiendo el formato que pida el encargo.
Si te falta informacion o permisos, dilo en una seccion titulada Bloqueos.
"@

$agyArgs = @("-p", $prompt, "--model", $Model, "--sandbox", "--print-timeout", $Timeout)
if ($Effort) { $agyArgs += @("--effort", $Effort) }

$started = Get-Date -Format "yyyy-MM-dd HH:mm"
$statusPath = Join-Path (Split-Path $taskPath) "_estado.md"
function Set-AgentStatus([string]$state, [string]$note) {
  # Una linea por agente: | Agente | estado | modelo | encargo | nota | hora |
  $line = "| $Agent | $state | $Model | $([IO.Path]::GetFileName($taskPath)) | $note | $(Get-Date -Format 'HH:mm') |"
  $rows = @()
  if (Test-Path $statusPath) { $rows = Get-Content $statusPath -Encoding utf8 | Where-Object { $_ -notmatch "^\| $([regex]::Escape($Agent)) \|" -and $_ -match "^\| " } }
  $rows = $rows | Where-Object { $_ -notmatch "^\| Agente \|" -and $_ -notmatch "^\|---" }
  $header = @("| Agente | Estado | Modelo | Encargo | Nota | Hora |", "|---|---|---|---|---|---|")
  Set-Content -Path $statusPath -Value ($header + $rows + $line) -Encoding utf8
}
Set-AgentStatus "working" ""
$ErrorActionPreference = "Continue"   # en PowerShell 5.1 el stderr de agy no debe abortar el script
$output = & agy @agyArgs 2> $errPath
$code = $LASTEXITCODE

if ($code -eq 0 -and $output) {
  $block = @(
    "",
    "---",
    "## Respuesta de Gemini ($Model, $started)",
    "",
    ($output -join "`n")
  ) -join "`n"
  Add-Content -Path $taskPath -Value $block -Encoding utf8
  if ((Test-Path $errPath) -and (Get-Item $errPath).Length -eq 0) { Remove-Item $errPath }
  Set-AgentStatus "done" ""
  Write-Output "OK: respuesta anadida a $relTask"
  exit 0
} else {
  Add-Content -Path $errPath -Value "`n[agy-task] codigo de salida $code, salida vacia=$(-not $output), $started" -Encoding utf8
  $note = if (("" + (Get-Content $errPath -Raw -ErrorAction SilentlyContinue)) -match "(?i)quota|429|RESOURCE_EXHAUSTED") { "quota exceeded" } else { "error, ver .err" }
  Set-AgentStatus "error" $note
  Write-Output "ERROR: revisa $([IO.Path]::GetFileName($errPath))"
  exit 1
}
