<#
.SYNOPSIS
    Installe HomeServerHub en démarrage automatique sous Windows.

.DESCRIPTION
    HomeServerHub est une application console : plutôt qu'un « vrai » service
    Windows (qui exigerait un wrapper externe comme NSSM), ce script utilise le
    Planificateur de tâches intégré pour lancer le hub au démarrage de la
    machine, en arrière-plan, et ouvre le pare-feu sur le port d'écoute.

    Il copie le binaire et la config dans C:\ProgramData\HomeServerHub, crée une
    tâche planifiée « HomeServerHub » exécutée par le compte SYSTEM, ouvre le
    port dans le pare-feu Windows, puis démarre le hub.

.PARAMETER ExePath
    Chemin du HomeServerHub.exe compilé. Par défaut : build-mingw\HomeServerHub.exe.

.PARAMETER Uninstall
    Supprime la tâche planifiée et la règle de pare-feu.

.EXAMPLE
    # Clic droit sur PowerShell > « Exécuter en tant qu'administrateur », puis :
    .\scripts\windows\install-service.ps1

.EXAMPLE
    .\scripts\windows\install-service.ps1 -Uninstall
#>
[CmdletBinding()]
param(
    [string]$ExePath,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$TaskName  = 'HomeServerHub'
$InstallDir = Join-Path $env:ProgramData 'HomeServerHub'
$ExeDest    = Join-Path $InstallDir 'HomeServerHub.exe'
$ConfDest   = Join-Path $InstallDir 'config.json'

# --- Doit être administrateur --------------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
          ).IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "Ce script doit être lancé dans un PowerShell OUVERT EN ADMINISTRATEUR."
    exit 1
}

# --- Désinstallation ------------------------------------------------------
if ($Uninstall) {
    Write-Host "Désinstallation de $TaskName…"
    if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
        Stop-ScheduledTask   -TaskName $TaskName -ErrorAction SilentlyContinue
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
    }
    Get-NetFirewallRule -DisplayName $TaskName -ErrorAction SilentlyContinue | Remove-NetFirewallRule
    Write-Host "Tâche et règle de pare-feu supprimées. (Fichiers conservés dans $InstallDir)"
    Write-Host "Pour tout retirer :  Remove-Item -Recurse -Force '$InstallDir'"
    return
}

# --- Localiser le binaire -------------------------------------------------
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $ExePath) {
    $ExePath = Join-Path $repoRoot 'build-mingw\HomeServerHub.exe'
}
if (-not (Test-Path $ExePath)) {
    Write-Error @"
Binaire introuvable : $ExePath
Compile d'abord :
    cmake --preset mingw
    cmake --build --preset mingw
…ou passe le chemin :  .\scripts\windows\install-service.ps1 -ExePath C:\chemin\HomeServerHub.exe
"@
    exit 1
}
Write-Host "Binaire      : $ExePath"

# --- Installer binaire + config -------------------------------------------
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item $ExePath $ExeDest -Force
Write-Host "Installé     : $ExeDest"

if (-not (Test-Path $ConfDest)) {
    $exampleConf = Join-Path $repoRoot 'config.example.json'
    if (Test-Path $exampleConf) {
        Copy-Item $exampleConf $ConfDest -Force
    } else {
        @{ host = '0.0.0.0'; port = 8080; dataDir = 'data'; token = '' } |
            ConvertTo-Json | Set-Content -Path $ConfDest -Encoding UTF8
    }
    Write-Host "Config créée : $ConfDest  (édite-la pour le port ou un token)"
} else {
    Write-Host "Config       : $ConfDest  (existante, conservée)"
}

# Port d'écoute lu depuis la config (pour la règle de pare-feu et le message).
$port = 8080
try { $port = [int]((Get-Content $ConfDest -Raw | ConvertFrom-Json).port) } catch {}

# --- Tâche planifiée : démarrage automatique, compte SYSTEM ---------------
if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
}
$action    = New-ScheduledTaskAction -Execute $ExeDest -Argument "`"$ConfDest`"" -WorkingDirectory $InstallDir
$trigger   = New-ScheduledTaskTrigger -AtStartup
$principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
$settings  = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)
Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
    -Principal $principal -Settings $settings -Description 'HomeServerHub — socle de synchronisation' | Out-Null
Write-Host "Tâche        : '$TaskName' (démarrage automatique, compte SYSTEM)"

# --- Pare-feu : autoriser le port en entrée (indispensable pour le LAN) ---
Get-NetFirewallRule -DisplayName $TaskName -ErrorAction SilentlyContinue | Remove-NetFirewallRule
New-NetFirewallRule -DisplayName $TaskName -Direction Inbound -Action Allow `
    -Protocol TCP -LocalPort $port -Profile Private,Domain | Out-Null
Write-Host "Pare-feu     : port $port/TCP autorisé en entrée (profils Privé + Domaine)"

# --- Démarrer maintenant --------------------------------------------------
Start-ScheduledTask -TaskName $TaskName
Start-Sleep -Seconds 2

Write-Host ""
try {
    $health = Invoke-RestMethod -Uri "http://localhost:$port/api/health" -TimeoutSec 5
    Write-Host "Vérification : OK -> status=$($health.status) version=$($health.version)"
} catch {
    Write-Host "Vérification : pas encore de réponse sur http://localhost:$port/api/health"
    Write-Host "  Voir l'état :  Get-ScheduledTask -TaskName $TaskName | Get-ScheduledTaskInfo"
}

$ip = (Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
       Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' } |
       Select-Object -First 1).IPAddress
if ([string]::IsNullOrEmpty($ip)) { $ip = '<ip-windows>' }
Write-Host ""
Write-Host "Depuis un autre poste :  curl http://${ip}:$port/api/health"
