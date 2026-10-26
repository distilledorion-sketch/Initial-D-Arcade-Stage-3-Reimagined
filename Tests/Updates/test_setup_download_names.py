"""Check the actual setup downloader's version-to-asset mapping in PowerShell 5.1."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DownloadNameTests(unittest.TestCase):
    def test_pinned_and_future_versions_match_release_names(self):
        # Load only the production function through the PowerShell AST; never
        # execute the installer, access the network or create a game folder.
        script = r'''
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($args[0],[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Installer parse failed'}
$fn=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-ReleaseArchiveNames'},$true)
if(!$fn){throw 'Missing production download-name function'}
Invoke-Expression $fn.Extent.Text
$source=[IO.File]::ReadAllText($args[0])
if($source -notmatch '\$names=Get-ReleaseArchiveNames \$version'){throw 'Downloader does not use version mapping'}
# Use a literal-dollar pattern to avoid expanding a PowerShell variable.
$pin=[regex]::Match($source,'(?m)^\$version=''([^'']+)'';')
if(!$pin.Success){throw 'Missing pinned version'}
$rows=@()
foreach($v in @($pin.Groups[1].Value,'0.3.95-community-replays.53','1.2.3-community-replays.104')){
    $rows+=@{version=$v;names=@(Get-ReleaseArchiveNames $v)}
}
foreach($v in @('','../0.3.95','0.3.95.52','0.3.95-community-replays.52/evil')){
    $rejected=$false
    try{Get-ReleaseArchiveNames $v|Out-Null}catch{$rejected=$true}
    if(!$rejected){throw 'Malformed version accepted'}
}
ConvertTo-Json -InputObject $rows -Depth 5 -Compress
'''
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'download-names.ps1'
            path.write_text(script, encoding='utf-8')
            shell = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
            run = subprocess.run([str(shell), '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
                                  '-File', str(path), str(ROOT / 'Tools/Install-InitialD.ps1')],
                                 capture_output=True, text=True, timeout=30,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        rows = json.loads(run.stdout)
        self.assertEqual(len(rows), 3)
        for row in rows:
            major, revision = row['version'].split('-community-replays.')
            self.assertEqual(row['names'], [f'Initial-D-Arcade-Stage-3-Reimagined-{major}.{revision}-Windows-x64.zip',
                                           f'Initial-D-Additional-Courses-{row["version"]}.zip'])


if __name__ == '__main__':
    unittest.main()
