[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [switch]$SkipTests,
    [switch]$SkipTeamSpeak,
    [switch]$LaunchArma
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ConfigurationName = $Configuration.ToLowerInvariant()

$ConfigurePreset = "windows-x64-ts3"
$BuildPreset = "windows-x64-ts3-$ConfigurationName"

$SdkHeader = Join-Path $Root ".deps\ts3client-pluginsdk\include\ts3_functions.h"
$ExtensionSource = Join-Path $Root "build\windows-x64-ts3\native\extension\$Configuration\icarus_x64.dll"
$PluginSource = Join-Path $Root "build\windows-x64-ts3\native\teamspeak\$Configuration\icarus.dll"

$HemttDevRoot = Join-Path $Root ".hemttout\dev"
$ExtensionTarget = Join-Path $HemttDevRoot "icarus_x64.dll"

$TeamSpeakPluginRoot = Join-Path $env:APPDATA "TS3Client\plugins"
$PluginTarget = Join-Path $TeamSpeakPluginRoot "icarus.dll"

function Invoke-Checked
{
    param(
        [Parameter(Mandatory)]
        [string]$Command,

        [Parameter()]
        [string[]]$Arguments = @()
    )

    & $Command @Arguments

    if($LASTEXITCODE -ne 0)
    {
        throw "$Command failed with exit code $LASTEXITCODE."
    }
}

Push-Location $Root

try
{
    foreach($Command in @("git", "cmake", "ctest", "hemtt"))
    {
        if($null -eq (Get-Command $Command -ErrorAction SilentlyContinue))
        {
            throw "Required command '$Command' was not found."
        }
    }

    if(-not (Test-Path $SdkHeader))
    {
        throw "TeamSpeak 3 Plugin SDK is missing. Run .\tools\fetch-ts3-sdk.ps1 first."
    }

    if(-not $SkipTeamSpeak)
    {
        $TeamSpeakProcesses = @(
            Get-Process -Name "ts3client_win64", "ts3client" -ErrorAction SilentlyContinue
        )

        if($TeamSpeakProcesses.Count -gt 0)
        {
            throw "TeamSpeak 3 is running. Close it before deployment or use -SkipTeamSpeak."
        }
    }

    Write-Host "Validating ArmA addon..."
    Invoke-Checked "hemtt" @("check", "--pedantic", "--error-on-all")

    Write-Host "Configuring native build..."
    Invoke-Checked "cmake" @("--preset", $ConfigurePreset)

    Write-Host "Building native components..."
    Invoke-Checked "cmake" @("--build", "--preset", $BuildPreset)

    if(-not $SkipTests)
    {
        Write-Host "Running native tests..."
        Invoke-Checked "ctest" @("--preset", $BuildPreset)
    }

    Write-Host "Building HEMTT development mod..."
    Invoke-Checked "hemtt" @("dev")

    if(-not (Test-Path $ExtensionSource))
    {
        throw "ArmA extension was not produced at '$ExtensionSource'."
    }

    Copy-Item $ExtensionSource $ExtensionTarget -Force

    if(-not $SkipTeamSpeak)
    {
        if(-not (Test-Path $PluginSource))
        {
            throw "TeamSpeak plugin was not produced at '$PluginSource'."
        }

        New-Item -ItemType Directory -Force $TeamSpeakPluginRoot | Out-Null
        Copy-Item $PluginSource $PluginTarget -Force
    }

    Write-Host ""
    Write-Host "ICARUS development deployment complete."
    Write-Host "ArmA mod:        $HemttDevRoot"
    Write-Host "ArmA extension:  $ExtensionTarget"

    if(-not $SkipTeamSpeak)
    {
        Write-Host "TeamSpeak plugin: $PluginTarget"
    }
    else
    {
        Write-Host "TeamSpeak plugin: skipped"
    }

    if($LaunchArma)
    {
        Write-Host ""
        Write-Host "Launching ArmA 3..."
        Invoke-Checked "hemtt" @("launch", "-Q")
    }
}
finally
{
    Pop-Location
}