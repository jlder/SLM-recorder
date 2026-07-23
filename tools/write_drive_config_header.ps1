param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$ConfigJson,

    [string]$ProjectDirectory = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function ConvertTo-CStringLiteral([string]$Value) {
    if ($null -eq $Value) { return '' }
    foreach ($character in $Value.ToCharArray()) {
        if ([char]::IsControl($character)) {
            throw 'Drive configuration values may not contain control characters.'
        }
    }
    return $Value.Replace('\', '\\').Replace('"', '\"')
}

$config = Get-Content -LiteralPath $ConfigJson -Raw | ConvertFrom-Json
if ([int]$config.version -ne 1) { throw 'Unsupported Drive configuration version.' }

$required = @('client_id', 'client_secret', 'refresh_token', 'root_folder_id')
foreach ($name in $required) {
    if ([string]::IsNullOrWhiteSpace([string]$config.$name)) {
        throw "Drive configuration is missing $name."
    }
}

$tokenUri = [string]$config.token_uri
if ([string]::IsNullOrWhiteSpace($tokenUri)) {
    $tokenUri = 'https://oauth2.googleapis.com/token'
}
if ($tokenUri -ne 'https://oauth2.googleapis.com/token') {
    throw 'Only the Google OAuth token endpoint is allowed.'
}
if (-not ([string]$config.client_id).EndsWith('.apps.googleusercontent.com')) {
    throw 'The OAuth client ID is not a Google application client ID.'
}

$output = Join-Path $ProjectDirectory 'slm_drive_config_private.h'
$content = @"
// GENERATED PRIVATE FILE. DO NOT COMMIT OR SHARE.
#pragma once

#define SLM_DRIVE_CONFIG_ENABLED 1
#define SLM_DRIVE_CLIENT_ID "$(ConvertTo-CStringLiteral ([string]$config.client_id))"
#define SLM_DRIVE_CLIENT_SECRET "$(ConvertTo-CStringLiteral ([string]$config.client_secret))"
#define SLM_DRIVE_REFRESH_TOKEN "$(ConvertTo-CStringLiteral ([string]$config.refresh_token))"
#define SLM_DRIVE_TOKEN_URI "$(ConvertTo-CStringLiteral $tokenUri)"
#define SLM_DRIVE_ROOT_FOLDER_ID "$(ConvertTo-CStringLiteral ([string]$config.root_folder_id))"
"@

[System.IO.File]::WriteAllText($output, $content, [System.Text.UTF8Encoding]::new($false))
Write-Host "Private recorder header created: $output" -ForegroundColor Green
Write-Host 'It is ignored by Git. Keep it private and compile the firmware from this project directory.'
