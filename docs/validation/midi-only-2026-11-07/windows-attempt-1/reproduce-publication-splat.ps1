$ErrorActionPreference = 'Stop'
$publication = if ($env:GITHUB_REF -eq 'refs/heads/release/full-flysnapper') { @() } else { @('--research') }
& $env:FLYSNAPPER_REPRO_PYTHON $env:FLYSNAPPER_REPRO_PROBE --result running @publication
[string[]] $publication = if ($env:GITHUB_REF -eq 'refs/heads/release/full-flysnapper') { @() } else { @('--research') }
& $env:FLYSNAPPER_REPRO_PYTHON $env:FLYSNAPPER_REPRO_PROBE --result running @publication
