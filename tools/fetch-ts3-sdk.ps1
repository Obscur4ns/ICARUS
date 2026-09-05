[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$Repository = "https://github.com/teamspeak/ts3client-pluginsdk.git"
$Revision = "b2f4b8e"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Target = Join-Path $Root ".deps\ts3client-pluginsdk"

if(-not (Test-Path (Join-Path $Target ".git")))
{
    New-Item -ItemType Directory -Force -Path (Split-Path $Target) | Out-Null
    git clone $Repository $Target

    if($LASTEXITCODE -ne 0)
    {
        throw "Failed to clone the TeamSpeak 3 Plugin SDK."
    }
}
else
{
    git -C $Target fetch origin

    if($LASTEXITCODE -ne 0)
    {
        throw "Failed to update the TeamSpeak 3 Plugin SDK."
    }
}

git -C $Target checkout --detach $Revision

if($LASTEXITCODE -ne 0)
{
    throw "Failed to check out TeamSpeak 3 Plugin SDK revision $Revision."
}

Write-Host "TeamSpeak 3 Plugin SDK API 26 ready at $Target"
