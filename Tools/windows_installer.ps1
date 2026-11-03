# SPDX-License-Identifier: AGPL-3.0-or-later
# Windows PowerShell 5.1, no Python/compiler/external PowerShell modules needed.
[CmdletBinding()]
param(
    [string]$Destination,
    [string]$PackageRoot = $PSScriptRoot,
    [switch]$NoUI,
    [switch]$VerifyOnly,
    [switch]$LibraryOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-FlysnapperFullPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not [IO.Path]::IsPathRooted($Path)) {
        throw 'An absolute directory path is required.'
    }
    return [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
}
function Test-FlysnapperWithin([string]$Child, [string]$Parent) {
    $c = Get-FlysnapperFullPath $Child
    $p = Get-FlysnapperFullPath $Parent
    return $c.Equals($p, [StringComparison]::OrdinalIgnoreCase) -or $c.StartsWith($p + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}
function Assert-FlysnapperNoLinks([string]$Path) {
    $current = [IO.Path]::GetFullPath($Path)
    while (-not [string]::IsNullOrEmpty($current)) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw ('Links/junctions are not accepted: ' + $current)
            }
        }
        $parent = [IO.Path]::GetDirectoryName($current)
        if ($parent -eq $current) { break }
        $current = $parent
    }
}
function Get-FlysnapperInventory([string]$Root) {
    $rootPath = Get-FlysnapperFullPath $Root
    Assert-FlysnapperNoLinks $rootPath
    if (-not (Test-Path -LiteralPath $rootPath -PathType Container)) { throw 'Expected a package directory.' }
    $inventory = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([StringComparer]::OrdinalIgnoreCase)
    $pending = New-Object 'System.Collections.Generic.Stack[string]'
    $pending.Push($rootPath)
    while ($pending.Count -gt 0) {
        foreach ($item in Get-ChildItem -LiteralPath ($pending.Pop()) -Force) {
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw ('Linked item: ' + $item.FullName) }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
            else {
                $relative = $item.FullName.Substring($rootPath.Length + 1).Replace('\','/')
                if ($inventory.ContainsKey($relative)) { throw ('Case-colliding item: ' + $relative) }
                $inventory.Add($relative, (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant())
            }
        }
    }
    return ,$inventory
}
function Assert-FlysnapperMatchingInventory($Actual, $Expected) {
    if ($Actual.Count -ne $Expected.Count) { throw 'File inventory differs from the verified package.' }
    foreach ($key in $Expected.Keys) {
        if (-not $Actual.ContainsKey($key) -or $Actual[$key] -ne $Expected[$key]) { throw ('File checksum failed: ' + $key) }
    }
}
function Assert-FlysnapperPE([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        if ($stream.Length -lt 64) { throw 'Invalid Windows VST3 binary.' }
        $reader = New-Object IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw 'Expected a Windows PE binary.' }
        $stream.Position = 60
        $offset = $reader.ReadUInt32()
        if ($offset + 6 -gt $stream.Length) { throw 'Invalid PE header.' }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'Expected a Windows x64 VST3 binary.' }
    } finally { $stream.Dispose() }
}
function Get-FlysnapperVerifiedPackage([string]$Root) {
    $rootPath = Get-FlysnapperFullPath $Root
    Assert-FlysnapperNoLinks $rootPath
    $manifestPath = Join-Path $rootPath 'release-manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Extract the complete Windows installation ZIP first.' }
    Assert-FlysnapperNoLinks $manifestPath
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.schema -ne 1 -or $manifest.product -ne 'Flysnapper' -or $manifest.platform -ne 'Windows-x64' -or $manifest.tests -ne 'passed') {
        throw 'A tested Flysnapper Windows x64 installation package is required.'
    }
    $expected = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $manifest.files.PSObject.Properties) {
        $name = $entry.Name
        if ([string]::IsNullOrEmpty($name) -or $name.Contains('\') -or $name.Contains(':') -or $name.StartsWith('/') -or $name.Split('/') -contains '..' -or $name.Split('/') -contains '.' -or $entry.Value -notmatch '^[0-9a-f]{64}$') {
            throw ('Invalid manifest path or hash: ' + $name)
        }
        if ($expected.ContainsKey($name)) { throw 'Duplicate manifest path.' }
        $expected.Add($name, [string]$entry.Value)
    }
    $actual = Get-FlysnapperInventory $rootPath
    [void]$actual.Remove('release-manifest.json')
    Assert-FlysnapperMatchingInventory $actual $expected
    $binaryPath = Join-Path $rootPath 'Flysnapper.vst3/Contents/x86_64-win/Flysnapper.vst3'
    Assert-FlysnapperPE $binaryPath
    $build = Get-Content -LiteralPath (Join-Path $rootPath 'Validation/build-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($build.platform -ne 'Windows-x64' -or $build.tests -ne 'passed' -or $build.windows_verified -ne $true -or $build.pitch_detection_implemented -ne $true -or $build.version -ne $manifest.version) {
        throw 'Build proof does not match a complete Windows installation package.'
    }
    $bundle = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $expected.GetEnumerator()) {
        if ($entry.Key.StartsWith('Flysnapper.vst3/', [StringComparison]::Ordinal)) { $bundle.Add($entry.Key.Substring('Flysnapper.vst3/'.Length), $entry.Value) }
    }
    if ($bundle.Count -eq 0) { throw 'The VST3 bundle is missing.' }
    return @{ Root = $rootPath; Version = [string]$manifest.version; Bundle = $bundle }
}
function Copy-FlysnapperTree([string]$Source, [string]$DestinationPath) {
    Assert-FlysnapperNoLinks $Source
    Assert-FlysnapperNoLinks $DestinationPath
    if (Test-Path -LiteralPath $DestinationPath) { throw ('Copy destination already exists: ' + $DestinationPath) }
    New-Item -ItemType Directory -Path $DestinationPath | Out-Null
    $sourceRoot = Get-FlysnapperFullPath $Source
    $pending = New-Object 'System.Collections.Generic.Stack[string]'
    $pending.Push($sourceRoot)
    while ($pending.Count -gt 0) {
        foreach ($item in Get-ChildItem -LiteralPath ($pending.Pop()) -Force) {
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Linked source item.' }
            $relative = $item.FullName.Substring($sourceRoot.Length + 1)
            $target = Join-Path $DestinationPath $relative
            if ($item.PSIsContainer) {
                [IO.Directory]::CreateDirectory($target) | Out-Null
                $pending.Push($item.FullName)
            } else {
                [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
                [IO.File]::Copy($item.FullName, $target, $false)
            }
        }
    }
}
function Move-FlysnapperDirectory([string]$Source, [string]$Target) { [IO.Directory]::Move($Source, $Target) }
function Remove-FlysnapperDirectory([string]$Path) { Remove-Item -LiteralPath $Path -Recurse -Force }
function Install-FlysnapperPackage([string]$Root, [string]$TargetDirectory, [string]$BackupDirectory = '') {
    $package = Get-FlysnapperVerifiedPackage $Root
    $destinationPath = Get-FlysnapperFullPath $TargetDirectory
    Assert-FlysnapperNoLinks $destinationPath
    if ($destinationPath.EndsWith('.vst3', [StringComparison]::OrdinalIgnoreCase) -or $destinationPath.Length -le 3) {
        throw 'Choose the VST3 parent directory, not a drive root or a plugin bundle.'
    }
    if (Test-FlysnapperWithin $destinationPath $package.Root) { throw 'Choose a VST3 directory outside the extracted installer package.' }
    if ([string]::IsNullOrEmpty($BackupDirectory)) {
        $BackupDirectory = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Flysnapper/Backups'
    }
    $backupRoot = Get-FlysnapperFullPath $BackupDirectory
    if (Test-FlysnapperWithin $backupRoot $destinationPath) { throw 'Backup directory must be outside the VST3 scan directory.' }
    Assert-FlysnapperNoLinks $backupRoot
    [IO.Directory]::CreateDirectory($destinationPath) | Out-Null
    $target = Join-Path $destinationPath 'Flysnapper.vst3'
    if ((Test-Path -LiteralPath $target) -and -not (Test-Path -LiteralPath $target -PathType Container)) { throw 'Flysnapper.vst3 exists as a file; no files were overwritten.' }
    Assert-FlysnapperNoLinks $target
    $id = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff') + '-' + [Guid]::NewGuid().ToString('N')
    $stage = Join-Path $destinationPath ('.Flysnapper-install-' + $id)
    $rollback = Join-Path $destinationPath ('.Flysnapper-rollback-' + $id)
    $backup = ''
    $movedOld = $false; $movedNew = $false; $committed = $false
    try {
        Copy-FlysnapperTree (Join-Path $package.Root 'Flysnapper.vst3') $stage
        Assert-FlysnapperMatchingInventory (Get-FlysnapperInventory $stage) $package.Bundle
        if (Test-Path -LiteralPath $target) {
            $oldInventory = Get-FlysnapperInventory $target
            $backup = Join-Path (Join-Path $backupRoot $id) 'Flysnapper.vst3'
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup)) | Out-Null
            Copy-FlysnapperTree $target $backup
            Assert-FlysnapperMatchingInventory (Get-FlysnapperInventory $backup) $oldInventory
            Move-FlysnapperDirectory $target $rollback; $movedOld = $true
        }
        Move-FlysnapperDirectory $stage $target; $movedNew = $true
        Assert-FlysnapperMatchingInventory (Get-FlysnapperInventory $target) $package.Bundle
        $committed = $true
        if ($movedOld) {
            try { Remove-FlysnapperDirectory $rollback; $movedOld = $false }
            catch { Write-Warning ('Installation verified; temporary old-tree cleanup failed. Recovery copy: ' + $backup) }
        }
        return @{ Installed = $target; Backup = $backup; Version = $package.Version }
    } catch {
        # Remove only the new tree created by this transaction, then restore the
        # renamed original. The independently verified recovery copy remains.
        if (-not $committed -and $movedNew -and (Test-Path -LiteralPath $target)) { Remove-Item -LiteralPath $target -Recurse -Force }
        if (-not $committed -and $movedOld -and (Test-Path -LiteralPath $rollback)) { Move-FlysnapperDirectory $rollback $target }
        throw
    } finally {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    }
}
function New-FlysnapperFolderDialog {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Velg VST3-mappen som Ableton bruker. Bare Flysnapper.vst3 blir installert.'
    $dialog.ShowNewFolderButton = $true
    $common = $env:CommonProgramW6432
    if ([string]::IsNullOrEmpty($common)) { $common = [Environment]::GetFolderPath('CommonProgramFiles') }
    $dialog.SelectedPath = Join-Path $common 'VST3'
    return $dialog
}
function Select-FlysnapperDestination {
    $dialog = New-FlysnapperFolderDialog
    try {
        if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { return $null }
        return $dialog.SelectedPath
    } finally { $dialog.Dispose() }
}
function Test-FlysnapperAccessDenied($ErrorRecord) {
    $exception = $ErrorRecord.Exception
    while ($null -ne $exception) {
        if ($exception -is [UnauthorizedAccessException] -or $exception.HResult -eq -2147024891) { return $true }
        $exception = $exception.InnerException
    }
    return $false
}
if ($LibraryOnly) { return }
try {
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'Flysnapper requires Windows x64.' }
    $verified = Get-FlysnapperVerifiedPackage $PackageRoot
    if ($VerifyOnly) { Write-Host ('Verified Flysnapper ' + $verified.Version); exit 0 }
    if ([string]::IsNullOrWhiteSpace($Destination)) {
        if ($NoUI) { throw '-Destination is required with -NoUI.' }
        $Destination = Select-FlysnapperDestination
        if ([string]::IsNullOrWhiteSpace($Destination)) { Write-Host 'Installation cancelled; no files changed.'; exit 2 }
    }
    try { $result = Install-FlysnapperPackage $PackageRoot $Destination }
    catch {
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = New-Object Security.Principal.WindowsPrincipal($identity)
        if (-not $NoUI -and (Test-FlysnapperAccessDenied $_) -and -not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            $arguments = @('-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-STA','-File',('"' + $PSCommandPath + '"'),'-PackageRoot',('"' + $verified.Root + '"'),'-Destination',('"' + (Get-FlysnapperFullPath $Destination) + '"'))
            $process = Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $arguments -Verb RunAs -Wait -PassThru
            exit $process.ExitCode
        }
        throw
    }
    Write-Host ('Installed Flysnapper ' + $result.Version + ': ' + $result.Installed)
    if (-not [string]::IsNullOrEmpty($result.Backup)) { Write-Host ('Recovery copy: ' + $result.Backup) }
    if (-not $NoUI) {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show(('Flysnapper ' + $result.Version + ' er installert. Start Ableton og skann plugins pa nytt.'),'Flysnapper')
    }
    exit 0
} catch {
    Write-Error ('Installation stopped. Close Ableton/other plugin hosts and try again. ' + $_.Exception.Message) -ErrorAction Continue
    exit 1
}
